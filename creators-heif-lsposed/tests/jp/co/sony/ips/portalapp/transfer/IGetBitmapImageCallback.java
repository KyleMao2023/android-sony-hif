package jp.co.sony.ips.portalapp.transfer;
import android.graphics.drawable.BitmapDrawable;
public interface IGetBitmapImageCallback {
    void onBitmapImageAcquired(long id,BitmapDrawable image);
    void onBitmapImageAcquisitionFailed(long id);
}
