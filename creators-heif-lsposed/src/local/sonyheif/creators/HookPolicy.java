package local.sonyheif.creators;

public final class HookPolicy {
    public static final String PACKAGE="jp.co.sony.ips.portalapp";
    static final String PTP="jp.co.sony.ips.portalapp.transfer.ptp.";
    private HookPolicy(){}
    public static boolean targetProcess(String pkg,String process){return PACKAGE.equals(pkg)&&PACKAGE.equals(process);}
    public static boolean applies(String pkg,String process,String version,long code){
        return targetProcess(pkg,process)&&"3.5.0".equals(version)&&code==350607302L;
    }
    public static boolean allowHeif(String concreteClass,String format){
        return "HEIF".equals(format)&&((PTP+"PtpPullItem").equals(concreteClass)||(PTP+"PtpPullFileToTransfer").equals(concreteClass));
    }
}
