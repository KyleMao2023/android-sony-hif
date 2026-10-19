package jp.co.sony.ips.portalapp.mtp;
import jp.co.sony.ips.portalapp.transfer.*;
import android.graphics.drawable.BitmapDrawable;
// Authored behavioral fixtures based on the observed public contracts, not Sony source.
public class Copy {
    public static class MtpObjectDownloaderCallback {}
    public static class CopyContentRunnable {
        public final ICameraItem content;
        public int originalCalls,started,skipped;
        public CopyContentRunnable(ICameraItem item){content=item;}
        public void prepareDownloadContent(){
            originalCalls++;
            if(content.getFileFormat()==EnumFileFormat.HEIF){skipped++;return;}
            content.getThumbnail(new prepareDownloadContent$1(this));
        }
        public static class prepareDownloadContent$1 implements IGetBitmapImageCallback {
            final CopyContentRunnable owner;
            public prepareDownloadContent$1(CopyContentRunnable owner){this.owner=owner;}
            public void onBitmapImageAcquired(long id,BitmapDrawable image){owner.started++;}
            public void onBitmapImageAcquisitionFailed(long id){owner.started++;}
        }
    }
}
