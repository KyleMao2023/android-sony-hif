package jp.co.sony.ips.portalapp.transfer.ptp;
public class PtpSimpleDifferencePullItem extends PtpPullItem {
    @Override public java.util.List<jp.co.sony.ips.portalapp.transfer.ICameraItem> getFilesToTransfer(){return java.util.Arrays.asList(new PtpPullFileToTransfer());}
}
