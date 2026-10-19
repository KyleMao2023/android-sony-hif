package jp.co.sony.ips.portalapp.transfer.ptp;
import jp.co.sony.ips.portalapp.transfer.AbstractCameraItem;
public class PtpPullFileToTransfer extends AbstractCameraItem {
    public final boolean canCopy(){return originalCanCopy();}
    public final boolean shouldDownloadScn(){return false;}
}
