package local.sonyheif.creators;
import java.util.concurrent.atomic.AtomicBoolean;
final class InstallOnce {
    interface Action {void run() throws Throwable;}
    private final AtomicBoolean attempted=new AtomicBoolean();
    void run(Action action)throws Throwable {if(attempted.compareAndSet(false,true))action.run();}
}
