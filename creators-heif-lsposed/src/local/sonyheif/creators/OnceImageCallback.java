package local.sonyheif.creators;

import java.lang.reflect.*;
import java.util.concurrent.atomic.AtomicBoolean;

final class OnceImageCallback implements InvocationHandler {
    private final Object delegate,proxy;
    private final Method failure;
    private final AtomicBoolean completed=new AtomicBoolean();
    OnceImageCallback(Class<?> type,Object delegate) throws NoSuchMethodException {
        this.delegate=delegate;
        failure=type.getMethod("onBitmapImageAcquisitionFailed",long.class);
        proxy=Proxy.newProxyInstance(type.getClassLoader(),new Class<?>[]{type},this);
    }
    Object proxy(){return proxy;}
    void fail(long id) throws Throwable {failure.invoke(proxy,id);}
    public Object invoke(Object unused,Method method,Object[] args) throws Throwable {
        if(method.getDeclaringClass()==Object.class){
            if(method.getName().equals("equals"))return unused==args[0];
            if(method.getName().equals("hashCode"))return System.identityHashCode(unused);
            if(method.getName().equals("toString"))return "CreatorsHeif one-shot callback";
        }
        if(!completed.compareAndSet(false,true))return null;
        try{return method.invoke(delegate,args);}catch(InvocationTargetException e){throw e.getCause();}
    }
}
