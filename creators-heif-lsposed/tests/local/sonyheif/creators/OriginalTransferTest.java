package local.sonyheif.creators;
import java.lang.reflect.*;
import java.util.concurrent.atomic.AtomicBoolean;
import jp.co.sony.ips.portalapp.mtp.*;
import jp.co.sony.ips.portalapp.mtp.browse.MtpObjectDownloader;
import jp.co.sony.ips.portalapp.transfer.ptp.*;
import jp.co.sony.ips.portalapp.common.setting.EnumTransferImageSize;
public class OriginalTransferTest {
    static int fileRequests,previewRequests;
    static EnumTransferImageSize callbackSize,downloaderSize;
    static void dispatch(HookContractTest.RuntimeBoundary runtime,jp.co.sony.ips.portalapp.transfer.AbstractCameraItem item,boolean simple,boolean fail)throws Throwable {
        Copy$CopyContentRunnable$$ExternalSyntheticLambda0 work=new Copy$CopyContentRunnable$$ExternalSyntheticLambda0(new Copy.CopyContentRunnable(item));
        work.body=()->{
            try{
                // Real caller retains a local size BEFORE constructing callback and downloader.
                EnumTransferImageSize saved=(EnumTransferImageSize)runtime.invoke(EnumTransferImageSize.class.getDeclaredMethod(simple?"readImageSizeSimpleDifference":"readImageSize"),null);
                callbackSize=saved;
                MtpObjectDownloader downloader=(MtpObjectDownloader)runtime.invoke(MtpObjectDownloader.class.getConstructors()[0],null,item,"unused",new AtomicBoolean(),saved,true,new Copy.MtpObjectDownloaderCallback());
                downloaderSize=downloader.transferSize;
                if(fail)throw new IllegalStateException("continuation exception");
                if(saved==EnumTransferImageSize.Original)fileRequests++;else previewRequests++;
            }catch(Throwable e){if(e instanceof RuntimeException)throw (RuntimeException)e;throw new AssertionError(e);}
        };
        runtime.invoke(work.getClass().getDeclaredMethod("run"),work);
    }
    public static void main(String[] args)throws Throwable {
        HookContractTest.RuntimeBoundary runtime=new HookContractTest.RuntimeBoundary();
        CreatorsHooks.install(OriginalTransferTest.class.getClassLoader(),runtime);
        dispatch(runtime,new PtpPullItem(),false,false);
        HookContractTest.check(fileRequests==1&&previewRequests==0&&callbackSize==EnumTransferImageSize.Original&&downloaderSize==EnumTransferImageSize.Original,"2M caller must select original bytes AND Original callback/downloader metadata");
        dispatch(runtime,new PtpPullItem(),true,false);
        HookContractTest.check(fileRequests==2&&previewRequests==0,"both preference readers are scoped");
        Object manual=((java.util.List<?>)runtime.invoke(PtpPullItem.class.getDeclaredMethod("getFilesToTransfer"),new PtpPullItem())).get(0);
        dispatch(runtime,(PtpPullFileToTransfer)manual,false,false);
        HookContractTest.check(fileRequests==3&&previewRequests==0,"manually expanded file requests original bytes");
        Object automatic=((java.util.List<?>)runtime.invoke(PtpSimpleDifferencePullItem.class.getDeclaredMethod("getFilesToTransfer"),new PtpSimpleDifferencePullItem())).get(0);
        dispatch(runtime,(PtpPullFileToTransfer)automatic,false,false);
        HookContractTest.check(previewRequests==1,"automatic file does not receive original override even if invoked independently");
        PtpPullItem jpeg=new PtpPullItem();jpeg.format=jp.co.sony.ips.portalapp.transfer.EnumFileFormat.JPEG;
        dispatch(runtime,jpeg,false,false);
        HookContractTest.check(previewRequests==2,"JPEG preference unchanged");
        // A nested non-HEIF continuation must shadow, then restore, the outer scope.
        Copy$CopyContentRunnable$$ExternalSyntheticLambda0 outer=new Copy$CopyContentRunnable$$ExternalSyntheticLambda0(new Copy.CopyContentRunnable(new PtpPullItem()));
        outer.body=()->{
            try{
                dispatch(runtime,jpeg,false,false);
                HookContractTest.check(runtime.invoke(EnumTransferImageSize.class.getDeclaredMethod("readImageSize"),null)==EnumTransferImageSize.Original,"outer scope restored after nested JPEG");
                final Object[] otherThread=new Object[1];
                Thread thread=new Thread(()->{try{otherThread[0]=runtime.invoke(EnumTransferImageSize.class.getDeclaredMethod("readImageSize"),null);}catch(Throwable e){throw new AssertionError(e);}});
                thread.start();thread.join();
                HookContractTest.check(otherThread[0]==EnumTransferImageSize.TwoMegaPixels,"transfer context must not leak across threads");
            }catch(Throwable e){throw new AssertionError(e);}
        };
        runtime.invoke(outer.getClass().getDeclaredMethod("run"),outer);
        HookContractTest.check(previewRequests==3,"nested JPEG still requests preview");
        try{dispatch(runtime,new PtpPullItem(),false,true);throw new AssertionError("expected exception");}catch(IllegalStateException expected){}
        HookContractTest.check(runtime.invoke(EnumTransferImageSize.class.getDeclaredMethod("readImageSize"),null)==EnumTransferImageSize.TwoMegaPixels&&EnumTransferImageSize.preference==EnumTransferImageSize.TwoMegaPixels,"exception cleans scope; global preference unchanged");
        System.out.println("real caller original-versus-preview decision regression passed");
    }
}
