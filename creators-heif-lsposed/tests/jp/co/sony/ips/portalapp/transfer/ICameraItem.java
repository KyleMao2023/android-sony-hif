package jp.co.sony.ips.portalapp.transfer;
public interface ICameraItem {
    EnumFileFormat getFileFormat();
    long getUniqueId();
    void getThumbnail(IGetBitmapImageCallback callback);
}
