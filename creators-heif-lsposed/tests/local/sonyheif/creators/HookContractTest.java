package local.sonyheif.creators;

import java.lang.reflect.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicBoolean;
import de.robv.android.xposed.XC_MethodHook;
import jp.co.sony.ips.portalapp.transfer.*;
import jp.co.sony.ips.portalapp.transfer.ptp.*;
import jp.co.sony.ips.portalapp.mtp.Copy;
import jp.co.sony.ips.portalapp.mtp.browse.MtpObjectDownloader;
import jp.co.sony.ips.portalapp.common.setting.EnumTransferImageSize;

public class HookContractTest {
    static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    static class RuntimeBoundary implements CreatorsHooks.Registrar {
        final Map<Member,XC_MethodHook> hooks=new HashMap<>();
        int attempts,failAt=-1;
        public AutoCloseable hook(Member method,XC_MethodHook callback){
            if(attempts++==failAt)throw new IllegalStateException("deliberate registration failure");
            hooks.put(method,callback);return ()->hooks.remove(method);
        }
        Object invoke(Member member,Object receiver,Object... args) throws Throwable {
            XC_MethodHook.MethodHookParam p=new XC_MethodHook.MethodHookParam();p.method=member;p.thisObject=receiver;p.args=args;
            XC_MethodHook callback=hooks.get(member);
            if(callback!=null)dispatch(callback,"beforeHookedMethod",p);
            Field early=p.getClass().getDeclaredField("returnEarly");early.setAccessible(true);
            if(!early.getBoolean(p)){
                try{p.setResult(member instanceof Method?((Method)member).invoke(receiver,p.args):((Constructor<?>)member).newInstance(p.args));}
                catch(InvocationTargetException e){p.setThrowable(e.getCause());}
            }
            if(callback!=null)dispatch(callback,"afterHookedMethod",p);
            if(p.hasThrowable())throw p.getThrowable();return p.getResult();
        }
        void dispatch(XC_MethodHook callback,String name,XC_MethodHook.MethodHookParam p) throws Throwable {
            Method method=XC_MethodHook.class.getDeclaredMethod(name,p.getClass());method.setAccessible(true);
            try{method.invoke(callback,p);}catch(InvocationTargetException e){throw e.getCause();}
        }
    }
    public static void main(String[] args) throws Throwable {
        RuntimeBoundary runtime=new RuntimeBoundary();List<AutoCloseable> installed=CreatorsHooks.install(HookContractTest.class.getClassLoader(),runtime);
        Method canCopy=PtpPullItem.class.getDeclaredMethod("canCopy");PtpPullItem image=new PtpPullItem();
        check(Boolean.TRUE.equals(runtime.invoke(canCopy,image)),"manual HEIF checkbox must become selectable");
        image.format=EnumFileFormat.JPEG;check(Boolean.TRUE.equals(runtime.invoke(canCopy,image)),"JPEG remains copyable");
        image.format=EnumFileFormat.UNSPECIFIED;check(Boolean.FALSE.equals(runtime.invoke(canCopy,image)),"unknown remains rejected");
        check(Boolean.FALSE.equals(runtime.invoke(canCopy,new PtpSimpleDifferencePullItem())),"automatic subclass unchanged");
        check(Boolean.FALSE.equals(runtime.invoke(PtpPullFileToTransfer.class.getDeclaredMethod("canCopy"),new PtpPullFileToTransfer())),"unproven file-level HEIF remains rejected");
        Method prepare=Copy.CopyContentRunnable.class.getDeclaredMethod("prepareDownloadContent");
        PtpPullItem heif=new PtpPullItem();heif.duplicateThumbnail=true;heif.throwThumbnail=true;
        Copy.CopyContentRunnable work=new Copy.CopyContentRunnable(heif);runtime.invoke(prepare,work);
        check(work.started==1&&work.skipped==0&&work.originalCalls==0,"HEIF continues once despite duplicate completion and exception");
        PtpPullItem before=new PtpPullItem();before.throwBeforeThumbnail=true;
        Copy.CopyContentRunnable beforeWork=new Copy.CopyContentRunnable(before);runtime.invoke(prepare,beforeWork);
        check(beforeWork.started==1&&beforeWork.originalCalls==0,"failure before thumbnail delivery uses original failure continuation");
        Copy.CopyContentRunnable usb=new Copy.CopyContentRunnable(new jp.co.sony.ips.portalapp.mtp.mtpobject.MtpItem());runtime.invoke(prepare,usb);
        check(usb.originalCalls==1&&usb.skipped==1&&usb.started==0,"USB original rejection retained");
        PtpPullItem jpeg=new PtpPullItem();jpeg.format=EnumFileFormat.JPEG;Copy.CopyContentRunnable ordinary=new Copy.CopyContentRunnable(jpeg);runtime.invoke(prepare,ordinary);
        check(ordinary.originalCalls==1&&ordinary.started==1,"JPEG original workflow retained");
        Constructor<?> ctor=MtpObjectDownloader.class.getConstructors()[0];
        EnumTransferImageSize global=EnumTransferImageSize.TwoMegaPixels;
        MtpObjectDownloader downloaded=(MtpObjectDownloader)runtime.invoke(ctor,null,heif,"unused",new AtomicBoolean(),global,true,new Copy.MtpObjectDownloaderCallback());
        check(downloaded.transferSize==global&&global==EnumTransferImageSize.TwoMegaPixels,"constructor outside transfer scope remains unchanged");
        MtpObjectDownloader other=(MtpObjectDownloader)runtime.invoke(ctor,null,jpeg,"unused",new AtomicBoolean(),global,true,new Copy.MtpObjectDownloaderCallback());
        check(other.transferSize==EnumTransferImageSize.TwoMegaPixels,"JPEG size untouched");
        for(AutoCloseable handle:installed)handle.close();check(runtime.hooks.isEmpty(),"all handles unhook");
        for(int index=0;index<7;index++){
            RuntimeBoundary failed=new RuntimeBoundary();failed.failAt=index;
            try{CreatorsHooks.install(HookContractTest.class.getClassLoader(),failed);throw new AssertionError("failure must propagate");}
            catch(IllegalStateException expected){check(failed.hooks.isEmpty(),"partial hooks rolled back at every registration");}
        }
        RuntimeBoundary mismatch=new RuntimeBoundary();
        ClassLoader missing=new ClassLoader(HookContractTest.class.getClassLoader()){
            protected Class<?> loadClass(String name,boolean resolve)throws ClassNotFoundException{
                if(name.endsWith("Copy$CopyContentRunnable$prepareDownloadContent$1"))throw new ClassNotFoundException(name);
                return super.loadClass(name,resolve);
            }
        };
        try{CreatorsHooks.install(missing,mismatch);throw new AssertionError("missing contract must fail");}
        catch(ClassNotFoundException expected){check(mismatch.attempts==0,"shape mismatch before any registration");}
        System.out.println("host reflection/hook contracts passed (not ART integration)");
    }
}
