package jp.co.sony.ips.portalapp.transfer;
import android.graphics.drawable.BitmapDrawable;
public abstract class AbstractCameraItem implements ICameraItem {
    public EnumFileFormat format=EnumFileFormat.HEIF;
    public boolean throwThumbnail,duplicateThumbnail,throwBeforeThumbnail;
    public EnumFileFormat getFileFormat(){return format;}
    public long getUniqueId(){return 42;}
    public void getThumbnail(IGetBitmapImageCallback callback){
        if(throwBeforeThumbnail)throw new IllegalStateException("fixture failure before delivery");
        callback.onBitmapImageAcquired(42,new BitmapDrawable());
        if(duplicateThumbnail)callback.onBitmapImageAcquisitionFailed(42);
        if(throwThumbnail)throw new IllegalStateException("fixture thumbnail exception after completion");
    }
    protected boolean originalCanCopy(){return format==EnumFileFormat.JPEG||format==EnumFileFormat.RAW||format==EnumFileFormat.MP4;}
}
