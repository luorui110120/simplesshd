#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <jni.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

const char *conf_path = "", *conf_shell = "", *conf_home = "", *conf_env = "",
	*conf_lib = "";
int conf_rsyncbuffer = 0;

/* NB - this will leak memory like crazy if called often.... */
const char *
conf_path_file(const char *fn)
{
	char *ret = malloc(strlen(conf_path)+strlen(fn)+20);
	sprintf(ret, "%s/%s", conf_path, fn);
	return ret;
}


static JNIEnv *g_env;
static jclass cl_string;
static jclass cl_simplesshdservice;

extern int dropbear_main(int argc, char **argv);

static int
jni_init(JNIEnv *env_)
{
	g_env = env_;
#define CLASS(var, id) \
	cl_##var = (*g_env)->FindClass(g_env, id); \
	if (!cl_##var) return 0;
#define METHOD(var, mycl, id, sig) \
	mid_##var = (*g_env)->GetMethodID(g_env, cl_##mycl, id, sig); \
	if (!mid_##var) return 0;
#define FIELD(var, mycl, id, sig) \
	fid_##var = (*g_env)->GetFieldID(g_env, cl_##mycl, id, sig); \
	if (!fid_##var) return 0;
#define STFIELD(var, mycl, id, sig) \
	fid_##var = (*g_env)->GetStaticFieldID(g_env, cl_##mycl, id, sig); \
	if (!fid_##var) return 0;

	CLASS(string, "java/lang/String")
	CLASS(simplesshdservice, "org/galexander/sshd/SimpleSSHDService")

	return 1;
}

/* split str into argv entries, honoring " and \ (but nothing else) */
static int
split_cmd(const char *in, char **argv, int max_argc)
{
	char curr[1000];
	int curr_len = 0;
	int in_quotes = 0;
	int argc = 0;

	if (!in) {
		argv[argc] = NULL;
		return 0;
	}
	while (1) {
		char c = *in++;
		if (!c || (curr_len+10 >= sizeof curr) ||
		    (!in_quotes && isspace(c))) {
			if (curr_len) {
				curr[curr_len] = 0;
				if (argc+2 >= max_argc) {
					break;
				}
				argv[argc++] = strdup(curr);
				curr_len = 0;
			}
			if (!c) {
				break;
			}
		} else if (c == '"') {
			in_quotes = !in_quotes;
		} else {
			if (c == '\\') {
				c = *in++;
				switch (c) {
					case 'n': c = '\n'; break;
					case 'r': c = '\r'; break;
					case 'b': c = '\b'; break;
					case 't': c = '\t'; break;
					case 0: in--; break;
				}
			}
			curr[curr_len++] = c;
		}
	}
	argv[argc] = NULL;
	return argc;
}

static const char *
from_java_string(JNIEnv *env, jobject s)
{
	const char *ret, *t;
	if (!s) {
		return "";
	}
	t = (*env)->GetStringUTFChars(env, s, NULL);
	if (!t) {
		return "";
	}
	ret = strdup(t);
	(*env)->ReleaseStringUTFChars(env, s, t);
	return ret;
}


/* this makes sure that no previously-added atexit gets called (some users have
 * an atexit registered by libGLESv2_adreno.so */
static void
null_atexit(void)
{
	_Exit(0);
}


JNIEXPORT jint JNICALL
Java_org_galexander_sshd_SimpleSSHDService_start_1sshd(JNIEnv *env_,
	jclass cl,
	jint port, jobject jpath, jobject jshell, jobject jhome, jobject jextra,
	jint rsyncbuffer, jobject jenv, jobject jlib)
{
	pid_t pid;
	const char *extra;

	if (!jni_init(env_)) {
		return 0;
	}
	conf_path = from_java_string(env_,jpath);
	conf_shell = from_java_string(env_,jshell);
	conf_home = from_java_string(env_,jhome);
	extra = from_java_string(env_,jextra);
	conf_rsyncbuffer = rsyncbuffer;
	conf_env = from_java_string(env_,jenv);
	conf_lib = from_java_string(env_,jlib);

	pid = fork();
	if (pid == 0) {
		char *argv[100] = { "sshd", NULL };
		int argc = 1;
		const char *logfn;
		const char *logfn_old;
		int logfd;
		int retval;
		int i;

		atexit(null_atexit);

		logfn = conf_path_file("dropbear.err");
		logfn_old = conf_path_file("dropbear.err.old");
		unlink(logfn_old);
		rename(logfn, logfn_old);
		unlink(logfn);
		logfd = open(logfn, O_CREAT|O_WRONLY, 0666);
		if (logfd != -1) {
			/* replace stderr, so the output is preserved... */
			dup2(logfd, 2);
		}
		for (i = 3; i < 255; i++) {
			/* close all of the dozens of fds that android typically
			 * leaves open */
			close(i);
		}

		argv[argc++] = "-R";	/* enable DROPBEAR_DELAY_HOSTKEY */
		argv[argc++] = "-F";	/* don't redundant fork to background */
		if (port) {
			argv[argc++] = "-p";
			argv[argc] = malloc(20);
			sprintf(argv[argc], "%d", (int)port);
			argc++;
		}
		argc += split_cmd(extra, &argv[argc],
				(sizeof argv / sizeof *argv) - argc);
		fprintf(stderr, "starting dropbear\n");
		retval = dropbear_main(argc, argv);
		/* NB - probably not reachable */
		fprintf(stderr, "dropbear finished (%d)\n", retval);
		exit(0);
	}
	return pid;
}

JNIEXPORT void JNICALL
Java_org_galexander_sshd_SimpleSSHDService_kill(JNIEnv *env_, jclass cl,
			jint pid)
{
	kill(pid, SIGKILL);
}

JNIEXPORT int JNICALL
Java_org_galexander_sshd_SimpleSSHDService_waitpid(JNIEnv *env_, jclass cl,
			jint pid)
{
	int status;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return 0;
}


JNIEXPORT jstring JNICALL
Java_org_galexander_sshd_SimpleSSHDService_api_1mkfifo(JNIEnv *env_,jclass cl,
	jstring jpath)
{
	if (!jni_init(env_)) {
		return NULL;
	}

	const char *path = from_java_string(env_,jpath);
	char *buf = malloc(strlen(path)+100);
	sprintf(buf, "%s/api", path);
	if ((mkdir(buf, 0700) < 0) && (errno != EEXIST)) {
		perror(buf);
		return NULL;
	}
	sprintf(buf, "%s/api/cmd", path);
	unlink(buf);
	if (mkfifo(buf, 0666) < 0) {
		perror(buf);
		return NULL;
	}
	return (*g_env)->NewStringUTF(g_env, buf);
}
