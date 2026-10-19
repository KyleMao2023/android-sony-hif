package local.sonyheif.creators;
import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;
import de.robv.android.xposed.XC_MethodHook;
import de.robv.android.xposed.XposedBridge;
final class CreatorsHooks {
    interface Registrar {AutoCloseable hook(Member method,XC_MethodHook callback) throws Throwable;}
    static List<AutoCloseable> install(ClassLoader loader) throws Throwable {
        return install(loader,(member,callback)->{
            XC_MethodHook.Unhook handle=XposedBridge.hookMethod(member,callback);
            return handle::unhook;
        });
    }
    static List<AutoCloseable> install(ClassLoader loader,Registrar registrar) throws Throwable {
        final Bindings b=new Bindings(loader); // Resolve every contract before touching ART.
        final AtomicBoolean enabled=new AtomicBoolean();
        final ThreadLocal<Deque<Boolean>> transfers=new ThreadLocal<>();
        List<AutoCloseable> handles=new ArrayList<>();
        try{
            XC_MethodHook selectable=new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    if(!enabled.get()||p.hasThrowable()||!Boolean.FALSE.equals(p.getResult()))return;
                    try{if(b.eligible(p.thisObject))p.setResult(true);}catch(Throwable e){log("selection left original",e);}
                }
            };
            handles.add(registrar.hook(b.pullCanCopy,selectable));
            handles.add(registrar.hook(b.fileCanCopy,selectable));
            handles.add(registrar.hook(b.expandFiles,new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    if(!enabled.get()||p.hasThrowable()||p.thisObject.getClass()!=b.pullType)return;
                    try{
                        for(Object item:(List<?>)p.getResult()){
                            if(item!=null&&item.getClass()==b.fileType&&b.isHeif(item))b.manualFiles.add(item);
                        }
                    }catch(Throwable e){log("file provenance left original",e);}
                }
            }));
            handles.add(registrar.hook(b.prepare,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    if(!enabled.get())return;
                    final Object item;
                    final OnceImageCallback guard;
                    final long id;
                    try{
                        item=b.content.get(p.thisObject);
                        if(!b.eligible(item))return;
                        Object delegate=b.callbackConstructor.newInstance(p.thisObject);
                        id=(Long)b.uniqueId.invoke(item);
                        guard=new OnceImageCallback(b.callbackType,delegate);
                    }catch(Throwable e){log("preparation left original",e);return;}
                    // After takeover an error must not run the original rejection or
                    // deliver a second continuation if thumbnail delivery already began.
                    p.setResult(null);
                    try{b.thumbnail.invoke(item,guard.proxy());}
                    catch(Throwable e){
                        log("thumbnail completion fallback",e);
                        try{guard.fail(id);}catch(Throwable failure){log("original continuation threw",failure);}
                    }
                }
            }));
            handles.add(registrar.hook(b.downloadRun,new XC_MethodHook(){
                @Override protected void beforeHookedMethod(MethodHookParam p){
                    boolean eligible=false;
                    try{eligible=enabled.get()&&b.eligible(b.content.get(b.downloadOwner.get(p.thisObject)));}
                    catch(Throwable e){log("transfer scope left original",e);}
                    Deque<Boolean> stack=transfers.get();
                    if(stack==null){stack=new ArrayDeque<>();transfers.set(stack);}
                    stack.push(eligible); // A nested non-HEIF run shadows the outer scope.
                }
                @Override protected void afterHookedMethod(MethodHookParam p){
                    Deque<Boolean> stack=transfers.get();
                    if(stack!=null){stack.pop();if(stack.isEmpty())transfers.remove();}
                }
            }));
            XC_MethodHook originalSize=new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    Deque<Boolean> stack=transfers.get();
                    if(enabled.get()&&!p.hasThrowable()&&stack!=null&&Boolean.TRUE.equals(stack.peek()))p.setResult(b.originalSize);
                }
            };
            // Change the caller's saved local BEFORE callback/downloader construction
            // and downloadFile/downloadScreennail branching. Never write preferences.
            handles.add(registrar.hook(b.readSize,originalSize));
            handles.add(registrar.hook(b.readSimpleSize,originalSize));
            enabled.set(true); // No partial feature is observable during registration.
            return Collections.unmodifiableList(handles);
        }catch(Throwable failure){
            enabled.set(false);
            for(int i=handles.size()-1;i>=0;--i){try{handles.get(i).close();}catch(Throwable e){log("rollback failed",e);}}
            throw failure;
        }
    }
    static void log(String event,Throwable error){
        XposedBridge.log("CreatorsHeif: "+event+" ["+error.getClass().getSimpleName()+"]");
    }
    private static final class Bindings {
        private static final String ROOT="jp.co.sony.ips.portalapp.";
        final Method pullCanCopy,fileCanCopy,prepare,thumbnail,uniqueId,format,expandFiles,downloadRun,readSize,readSimpleSize;
        final Field content,downloadOwner;
        final Class<?> callbackType,pullType,fileType;
        final WeakIdentitySet manualFiles=new WeakIdentitySet();
        final Constructor<?> callbackConstructor,downloader;
        final Object originalSize;
        Bindings(ClassLoader loader)throws ReflectiveOperationException {
            Class<?> pull=type(loader,"transfer.ptp.PtpPullItem");
            Class<?> file=type(loader,"transfer.ptp.PtpPullFileToTransfer");
            pullType=pull;fileType=file;
            Class<?> item=type(loader,"transfer.ICameraItem");
            Class<?> fileFormat=type(loader,"transfer.EnumFileFormat");
            Class<?> copy=type(loader,"mtp.Copy$CopyContentRunnable");
            callbackType=type(loader,"transfer.IGetBitmapImageCallback");
            Class<?> callback=type(loader,"mtp.Copy$CopyContentRunnable$prepareDownloadContent$1");
            Class<?> size=type(loader,"common.setting.EnumTransferImageSize");
            if(!fileFormat.isEnum()||!size.isEnum()||!callbackType.isInterface()||
               !item.isInterface()||!item.isAssignableFrom(pull)||!item.isAssignableFrom(file)||
               !callbackType.isAssignableFrom(callback))throw new NoSuchMethodException("incompatible class contracts");
            pullCanCopy=method(pull,"canCopy",boolean.class);
            fileCanCopy=method(file,"canCopy",boolean.class);
            expandFiles=method(pull,"getFilesToTransfer",List.class);
            prepare=method(copy,"prepareDownloadContent",void.class);
            format=method(item,"getFileFormat",fileFormat);
            uniqueId=method(item,"getUniqueId",long.class);
            thumbnail=method(item,"getThumbnail",void.class,callbackType);
            method(callbackType,"onBitmapImageAcquisitionFailed",void.class,long.class);
            method(callbackType,"onBitmapImageAcquired",void.class,long.class,Class.forName("android.graphics.drawable.BitmapDrawable",false,loader));
            content=copy.getDeclaredField("content");content.setAccessible(true);
            if(content.getType()!=item||Modifier.isStatic(content.getModifiers()))throw new NoSuchFieldException("incompatible content field");
            callbackConstructor=callback.getDeclaredConstructor(copy);callbackConstructor.setAccessible(true);
            Field original=size.getDeclaredField("Original");
            if(original.getType()!=size||!Modifier.isStatic(original.getModifiers()))throw new NoSuchFieldException("incompatible Original enum");
            originalSize=original.get(null);
            if(!(originalSize instanceof Enum)||!"Original".equals(((Enum<?>)originalSize).name()))throw new NoSuchFieldException("missing Original enum");
            downloader=type(loader,"mtp.browse.MtpObjectDownloader").getDeclaredConstructor(item,String.class,AtomicBoolean.class,size,boolean.class,type(loader,"mtp.Copy$MtpObjectDownloaderCallback"));
            downloader.setAccessible(true);
            Class<?> download=type(loader,"mtp.Copy$CopyContentRunnable$$ExternalSyntheticLambda0");
            downloadRun=method(download,"run",void.class);
            downloadOwner=download.getDeclaredField("f$0");downloadOwner.setAccessible(true);
            if(downloadOwner.getType()!=copy||Modifier.isStatic(downloadOwner.getModifiers()))throw new NoSuchFieldException("incompatible download owner");
            readSize=staticMethod(size,"readImageSize",size);
            readSimpleSize=staticMethod(size,"readImageSizeSimpleDifference",size);
            // Both exact PTP classes return false here in 3.5.0, so the
            // non-size-selectable branch already chooses Original without a hook.
            method(pull,"shouldDownloadScn",boolean.class);
            method(file,"shouldDownloadScn",boolean.class);
        }
        boolean eligible(Object item)throws ReflectiveOperationException {
            if(item==null||!HookPolicy.allowHeif(item.getClass().getName(),"HEIF"))return false;
            if(item.getClass()==fileType&&!manualFiles.contains(item))return false;
            return isHeif(item);
        }
        boolean isHeif(Object item)throws ReflectiveOperationException {
            Object value=format.invoke(item);
            return value instanceof Enum&&HookPolicy.allowHeif(item.getClass().getName(),((Enum<?>)value).name());
        }
        static Method staticMethod(Class<?> owner,String name,Class<?> result)throws NoSuchMethodException {
            Method method=owner.getDeclaredMethod(name);method.setAccessible(true);
            if(method.getReturnType()!=result||!Modifier.isStatic(method.getModifiers()))throw new NoSuchMethodException("incompatible "+name);
            return method;
        }
        static Class<?> type(ClassLoader loader,String suffix)throws ClassNotFoundException {return Class.forName(ROOT+suffix,false,loader);}
        static Method method(Class<?> owner,String name,Class<?> result,Class<?>... args)throws NoSuchMethodException {
            Method method=owner.getDeclaredMethod(name,args);method.setAccessible(true);
            if(method.getReturnType()!=result||Modifier.isStatic(method.getModifiers()))throw new NoSuchMethodException("incompatible "+name);
            return method;
        }
    }
}
