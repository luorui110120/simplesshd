package org.galexander.util;

import android.util.Log;

import java.io.FileWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
public class LogUtils {
    public static boolean bFlags = true;
    public static boolean bFileLogFlags = false;
    public static String sFileLogPath = "/data/data/com.yiqixie.kem/utilslog.txt";
    private  static String TAG="mlog";
    private static LogUtils mInstance = null;

    public static void d(String tag, String content){
        if(bFlags)
        {
            Log.d(tag, content);
            write("DEBUG", getInstance().getPrefixName(), content);
        }
    }
    public static void d(String content){
        d(TAG,content);
    }
    public static void e(String tag, String content){
        if(bFlags)
        {
            Log.e(tag, content);
            write("ERROR", getInstance().getPrefixName(), content);
        }
    }
    public static void e(String content){
        e(TAG,content);
    }
    private static LogUtils getInstance() {
        if (mInstance == null) {
            synchronized (LogUtils.class) {
                if (mInstance == null) {
                    mInstance = new LogUtils();
                }
            }
        }
        return mInstance;
    }

    private String getPrefixName() {
        StackTraceElement[] sts = Thread.currentThread().getStackTrace();
        if (sts == null || sts.length == 0) {
            return "[ minify ]";
        }
        try {
            for (StackTraceElement st : sts) {
                if (st.isNativeMethod()) {
                    continue;
                }
                if (st.getClassName().equals(Thread.class.getName())) {
                    continue;
                }
                if (st.getClassName().equals(this.getClass().getName())) {
                    continue;
                }
                if (st.getFileName() != null) {
                    return "[ " + Thread.currentThread().getName() +
                            ": " + st.getFileName() + ":" + st.getLineNumber() +
                            " " + st.getMethodName() + " ]";
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "[ minify ]";
    }

    private static void write(String level, String prefix, String content) {
        if (!bFileLogFlags)
            return;
        try {
            // 打开一个写文件器，构造函数中的第二个参数true表示以追加形式写文件
            FileWriter writer = new FileWriter(sFileLogPath, true);
            SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault());
            String time = sdf.format(new Date());
            writer.write(time + ": " + level + "/" + prefix + ": " + content + "\n");
            writer.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
