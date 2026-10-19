package de.robv.android.xposed;
import java.lang.reflect.Member;
public final class XposedBridge {
    private XposedBridge(){}
    public static XC_MethodHook.Unhook hookMethod(Member method,XC_MethodHook callback){throw new UnsupportedOperationException("compile-only API");}
    public static void log(String message){System.err.println(message);}
}
