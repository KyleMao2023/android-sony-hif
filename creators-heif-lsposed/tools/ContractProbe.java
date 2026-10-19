package local.sonyheif.creators;
import dalvik.system.PathClassLoader;
import java.lang.reflect.*;
// Isolated app_process probe: resolves real APK contracts, never installs hooks.
public final class ContractProbe {
    public static void main(String[] args)throws Exception {
        if(args.length!=2)throw new IllegalArgumentException("MODULE_APK ORIGINAL_SONY_APK");
        ClassLoader module=new PathClassLoader(args[0],ContractProbe.class.getClassLoader());
        ClassLoader sony=new PathClassLoader(args[1],module);
        Class<?> bindings=Class.forName("local.sonyheif.creators.CreatorsHooks$Bindings",true,module);
        Constructor<?> ctor=bindings.getDeclaredConstructor(ClassLoader.class);ctor.setAccessible(true);
        try{ctor.newInstance(sony);}catch(InvocationTargetException e){throw new IllegalStateException("real APK contract mismatch",e.getCause());}
        System.out.println("PASS real APK classes, methods, fields, callback constructor and Original enum; no hooks installed");
    }
}
