package local.sonyheif.creators;

import android.app.Application;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import java.lang.reflect.Method;
import java.util.List;
import de.robv.android.xposed.*;
import de.robv.android.xposed.callbacks.XC_LoadPackage.LoadPackageParam;

public final class Entry implements IXposedHookLoadPackage {
    private final InstallOnce bootstrap=new InstallOnce(),features=new InstallOnce();
    private List<AutoCloseable> installed;
    @Override public void handleLoadPackage(final LoadPackageParam loaded)throws Throwable {
        if(!loaded.isFirstApplication||!HookPolicy.targetProcess(loaded.packageName,loaded.processName))return;
        bootstrap.run(()->{
            Method attach=Application.class.getDeclaredMethod("attach",Context.class);
            attach.setAccessible(true);
            XposedBridge.hookMethod(attach,new XC_MethodHook(){
                @Override protected void afterHookedMethod(MethodHookParam p){
                    if(p.hasThrowable())return;
                    Context context=(Context)p.args[0];
                    if(context==null||!HookPolicy.PACKAGE.equals(context.getPackageName()))return;
                    try{
                        features.run(()->{
                            PackageInfo info=context.getPackageManager().getPackageInfo(HookPolicy.PACKAGE,PackageManager.PackageInfoFlags.of(0));
                            if(!HookPolicy.applies(loaded.packageName,loaded.processName,info.versionName,info.getLongVersionCode())){
                                XposedBridge.log("CreatorsHeif: disabled for unsupported app version");return;
                            }
                            installed=CreatorsHooks.install(loaded.classLoader);
                            XposedBridge.log("CreatorsHeif: installed "+installed.size()+" hooks for 3.5.0 manual Wi-Fi HEIF");
                        });
                    }catch(Throwable e){CreatorsHooks.log("disabled after initialization failure",e);}
                }
            });
        });
    }
}
