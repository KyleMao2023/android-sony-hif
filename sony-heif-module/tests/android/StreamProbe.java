package local.sonyheif.audit;
import android.graphics.*;
import java.io.*;
import java.nio.file.*;

// app_process only. No app installation or hooks in any other process.
public final class StreamProbe {
    private static native boolean install(String module);
    private static String baseline;
    private static int failures;
    private static BitmapFactory.Options options(){
        BitmapFactory.Options o=new BitmapFactory.Options();
        o.inSampleSize=4;o.inPreferredConfig=Bitmap.Config.ARGB_8888;return o;
    }
    private static void report(String input,Bitmap bitmap){
        if(bitmap==null){System.out.println(input+": FAIL null");failures++;return;}
        try{
            int width=bitmap.getWidth(),height=bitmap.getHeight();
            int[] row=new int[width];long hash=0xcbf29ce484222325L;
            for(int y=0;y<height;y++){
                bitmap.getPixels(row,0,width,0,y,width,1);
                for(int pixel:row)hash=(hash^(pixel&0xffffffffL))*0x100000001b3L;
            }
            String result=width+"x"+height+" argb_fnv="+Long.toHexString(hash);
            boolean same=baseline==null||baseline.equals(result);
            if(baseline==null)baseline=result;
            if(!same)failures++;
            System.out.println(input+": "+(same?"PASS ":"FAIL mismatch ")+result);
        }finally{bitmap.recycle();}
    }
    public static void main(String[] args)throws Exception {
        if(args.length!=3)throw new IllegalArgumentException("stock|BRIDGE_SO MODULE_SO INPUT_FILE");
        boolean hook=!"stock".equals(args[0]);
        if(hook){System.load(args[0]);if(!install(args[1]))throw new IllegalStateException("probe hook not installed");}
        String file=args[2];System.out.println("mode="+(hook?"hooked":"stock")+" file="+file);
        report("decodeFile",BitmapFactory.decodeFile(file,options()));
        byte[] data=Files.readAllBytes(Paths.get(file));
        report("decodeByteArray",BitmapFactory.decodeByteArray(data,0,data.length,options()));
        try(InputStream stream=new FileInputStream(file)){report("decodeStream(FileInputStream)",BitmapFactory.decodeStream(stream,null,options()));}
        try(InputStream stream=new ByteArrayInputStream(data)){report("decodeStream(ByteArrayInputStream)",BitmapFactory.decodeStream(stream,null,options()));}
        if(failures!=0){
            System.err.println("FAIL "+failures+" decode paths failed or differed");
            System.exit(1);return;
        }
        System.out.println("PASS all four decode paths, matching dimensions and pixels");
    }
}
