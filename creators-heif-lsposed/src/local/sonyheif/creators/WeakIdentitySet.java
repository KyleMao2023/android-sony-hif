package local.sonyheif.creators;
import java.lang.ref.*;
import java.util.*;
// App objects may implement value equality; provenance belongs to one instance only.
final class WeakIdentitySet {
    private final ReferenceQueue<Object> queue=new ReferenceQueue<>();
    private final Set<Key> entries=new HashSet<>();
    private static final class Key extends WeakReference<Object> {
        private final int hash;
        Key(Object item,ReferenceQueue<Object> queue){super(item,queue);hash=System.identityHashCode(item);}
        @Override public int hashCode(){return hash;}
        @Override public boolean equals(Object other){return this==other||(other instanceof Key&&get()!=null&&get()==((Key)other).get());}
    }
    private void drain(){Reference<?> key;while((key=queue.poll())!=null)entries.remove(key);}
    synchronized void add(Object item){drain();entries.add(new Key(item,queue));}
    synchronized boolean contains(Object item){drain();return entries.contains(new Key(item,null));}
}
