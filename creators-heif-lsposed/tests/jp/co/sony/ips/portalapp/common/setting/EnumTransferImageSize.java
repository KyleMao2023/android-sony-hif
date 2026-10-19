package jp.co.sony.ips.portalapp.common.setting;
public enum EnumTransferImageSize {
    Original,TwoMegaPixels;
    public static EnumTransferImageSize preference=TwoMegaPixels;
    public static EnumTransferImageSize readImageSize(){return preference;}
    public static EnumTransferImageSize readImageSizeSimpleDifference(){return preference;}
}
