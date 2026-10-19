package local.sonyheif.creators;

import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

public final class PolicyCallbackTest {
    public interface Callback {
        void onBitmapImageAcquired(long id,Object image);
        void onBitmapImageAcquisitionFailed(long id);
    }
    static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args) throws Throwable {
        if(args.length==0||!args[0].equals("callback")){
            check(HookPolicy.applies("jp.co.sony.ips.portalapp","jp.co.sony.ips.portalapp","3.5.0",350607302L),"verified main process must be enabled");
            check(!HookPolicy.applies("jp.co.sony.ips.portalapp","jp.co.sony.ips.portalapp:remote","3.5.0",350607302L),"subprocess must stay disabled");
            check(!HookPolicy.applies("other","jp.co.sony.ips.portalapp","3.5.0",350607302L),"other package must stay disabled");
            check(!HookPolicy.applies("jp.co.sony.ips.portalapp","jp.co.sony.ips.portalapp","3.5.1",350607302L),"version name mismatch");
            check(!HookPolicy.applies("jp.co.sony.ips.portalapp","jp.co.sony.ips.portalapp","3.5.0",350607303L),"version code mismatch");
            check(!HookPolicy.applies(null,null,null,0),"null metadata");
            for(String cls:new String[]{"jp.co.sony.ips.portalapp.transfer.ptp.PtpPullItem","jp.co.sony.ips.portalapp.transfer.ptp.PtpPullFileToTransfer"}){
                check(HookPolicy.allowHeif(cls,"HEIF"),"manual PTP HEIF allowed");
                for(String format:new String[]{"JPEG","RAW","MP4","UNSPECIFIED",null})check(!HookPolicy.allowHeif(cls,format),"non-HEIF untouched");
            }
            for(String cls:new String[]{"jp.co.sony.ips.portalapp.transfer.ptp.PtpPushItem","jp.co.sony.ips.portalapp.transfer.ptp.PtpSimpleDifferencePullItem","jp.co.sony.ips.portalapp.mtp.mtpobject.MtpItem",null})
                check(!HookPolicy.allowHeif(cls,"HEIF"),"push, auto and USB untouched");
            System.out.println("exact process/version and PTP-format policy passed");
        }
        AtomicInteger delivered=new AtomicInteger();
        Callback original=new Callback(){
            public void onBitmapImageAcquired(long id,Object image){check(id==42,"original id");delivered.incrementAndGet();}
            public void onBitmapImageAcquisitionFailed(long id){check(id==42,"original failure id");delivered.incrementAndGet();}
        };
        OnceImageCallback guard=new OnceImageCallback(Callback.class,original);
        Callback proxy=(Callback)guard.proxy();proxy.toString();proxy.hashCode();proxy.equals(proxy);
        check(proxy.equals(proxy),"proxy identity must remain reflexive without consuming completion");
        proxy.onBitmapImageAcquired(42,new Object());guard.fail(42);proxy.onBitmapImageAcquisitionFailed(42);
        check(delivered.get()==1,"success followed by failure must continue once");
        delivered.set(0);OnceImageCallback concurrent=new OnceImageCallback(Callback.class,original);
        ExecutorService pool=Executors.newFixedThreadPool(8);CountDownLatch start=new CountDownLatch(1);
        try{
            Future<?>[] requests=new Future<?>[16];
            for(int i=0;i<requests.length;i++)requests[i]=pool.submit(()->{try{start.await();concurrent.fail(42);}catch(Throwable e){throw new RuntimeException(e);}});
            start.countDown();for(Future<?> result:requests)result.get();
        }finally{pool.shutdownNow();}
        check(delivered.get()==1,"racing failures must continue once");
        System.out.println("success/failure and racing one-shot callback tests passed");
    }
}
