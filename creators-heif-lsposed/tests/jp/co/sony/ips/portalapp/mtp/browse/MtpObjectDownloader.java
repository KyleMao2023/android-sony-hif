package jp.co.sony.ips.portalapp.mtp.browse;
import java.util.concurrent.atomic.AtomicBoolean;
import jp.co.sony.ips.portalapp.transfer.ICameraItem;
import jp.co.sony.ips.portalapp.common.setting.EnumTransferImageSize;
import jp.co.sony.ips.portalapp.mtp.Copy;
public class MtpObjectDownloader {
    public final EnumTransferImageSize transferSize;
    public MtpObjectDownloader(ICameraItem item,String path,AtomicBoolean cancelled,EnumTransferImageSize size,boolean pull,Copy.MtpObjectDownloaderCallback callback){transferSize=size;}
}
