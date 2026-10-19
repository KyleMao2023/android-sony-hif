package de.robv.android.xposed;
import java.lang.reflect.Member;
// Compile-time API declarations plus host-test parameter state; NEVER package.
public abstract class XC_MethodHook {
    protected void beforeHookedMethod(MethodHookParam p) throws Throwable {}
    protected void afterHookedMethod(MethodHookParam p) throws Throwable {}
    public static final class MethodHookParam {
        public Member method;
        public Object thisObject;
        public Object[] args;
        private Object result;
        private Throwable throwable;
        boolean returnEarly;
        public Object getResult(){return result;}
        public Throwable getThrowable(){return throwable;}
        public boolean hasThrowable(){return throwable!=null;}
        public void setResult(Object value){result=value;throwable=null;returnEarly=true;}
        public void setThrowable(Throwable value){throwable=value;result=null;returnEarly=true;}
    }
    public class Unhook {public void unhook(){throw new UnsupportedOperationException("compile-only API");}}
}
