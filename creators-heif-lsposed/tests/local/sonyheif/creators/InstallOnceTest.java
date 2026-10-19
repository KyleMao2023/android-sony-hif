package local.sonyheif.creators;
import java.util.concurrent.atomic.AtomicInteger;
public class InstallOnceTest {
    public static void main(String[] args)throws Throwable {
        InstallOnce once=new InstallOnce();AtomicInteger calls=new AtomicInteger();
        once.run(()->calls.incrementAndGet());once.run(()->calls.incrementAndGet());
        if(calls.get()!=1)throw new AssertionError("duplicate attach must not reinstall");
        InstallOnce failed=new InstallOnce();
        try{failed.run(()->{calls.incrementAndGet();throw new IllegalStateException("deliberate");});}catch(IllegalStateException expected){}
        failed.run(()->calls.incrementAndGet());
        if(calls.get()!=2)throw new AssertionError("failed initialization must stay disabled");
        System.out.println("one-time successful and failed initialization passed");
    }
}
