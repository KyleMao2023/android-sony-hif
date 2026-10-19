package jp.co.sony.ips.portalapp.transfer.ptp;
import jp.co.sony.ips.portalapp.transfer.AbstractCameraItem;
import java.util.*;
public class PtpPullItem extends AbstractCameraItem {
    public final boolean canCopy(){return originalCanCopy();}
    public List<jp.co.sony.ips.portalapp.transfer.ICameraItem> getFilesToTransfer(){return Arrays.asList(new PtpPullFileToTransfer());}
    public final boolean shouldDownloadScn(){return false;}
}
