package local.sonyheif.creators;
import java.lang.reflect.*;
import java.util.*;
import jp.co.sony.ips.portalapp.mtp.Copy;
import jp.co.sony.ips.portalapp.transfer.ptp.*;
public class TransferOriginTest {
    public static void main(String[] args)throws Throwable {
        HookContractTest.RuntimeBoundary runtime=new HookContractTest.RuntimeBoundary();
        CreatorsHooks.install(TransferOriginTest.class.getClassLoader(),runtime);
        Method manualFiles=PtpPullItem.class.getDeclaredMethod("getFilesToTransfer");
        Method autoFiles=PtpSimpleDifferencePullItem.class.getDeclaredMethod("getFilesToTransfer");
        Method prepare=Copy.CopyContentRunnable.class.getDeclaredMethod("prepareDownloadContent");
        Object automatic=((List<?>)runtime.invoke(autoFiles,new PtpSimpleDifferencePullItem())).get(0);
        Copy.CopyContentRunnable autoWork=new Copy.CopyContentRunnable((PtpPullFileToTransfer)automatic);
        runtime.invoke(prepare,autoWork);
        HookContractTest.check(autoWork.originalCalls==1&&autoWork.skipped==1&&autoWork.started==0,"automatic parent expanded to ordinary file must retain HEIF rejection");
        Object manual=((List<?>)runtime.invoke(manualFiles,new PtpPullItem())).get(0);
        Copy.CopyContentRunnable manualWork=new Copy.CopyContentRunnable((PtpPullFileToTransfer)manual);
        runtime.invoke(prepare,manualWork);
        HookContractTest.check(manualWork.originalCalls==0&&manualWork.started==1,"manually expanded file must continue");
        HookContractTest.check(Boolean.TRUE.equals(runtime.invoke(PtpPullFileToTransfer.class.getDeclaredMethod("canCopy"),manual)),"manual file selectable");
        HookContractTest.check(Boolean.FALSE.equals(runtime.invoke(PtpPullFileToTransfer.class.getDeclaredMethod("canCopy"),automatic)),"automatic file not selectable");
        // Do not let an application's equals()/hashCode() confuse object provenance.
        class EqualItem {public boolean equals(Object o){return o instanceof EqualItem;}public int hashCode(){return 1;}}
        WeakIdentitySet identities=new WeakIdentitySet();Object first=new EqualItem(),second=new EqualItem();identities.add(first);
        HookContractTest.check(identities.contains(first)&&!identities.contains(second),"provenance is by identity, never value equality");
        System.out.println("manual provenance / automatic expansion regression passed");
    }
}
