// Reuse the real-platform probe's relocation patch in this isolated process.
// This helper is never packaged in the installed module.
#define main unusedSystemProbeMain
#include "../../native/system_probe.cpp"
#undef main
#include <jni.h>

extern "C" JNIEXPORT jboolean JNICALL Java_local_sonyheif_audit_StreamProbe_install(JNIEnv* env,jclass,jstring path){
    const char* name=env->GetStringUTFChars(path,nullptr);
    if(!name)return JNI_FALSE;
    void* graphics=dlopen("libjnigraphics.so",RTLD_NOW|RTLD_LOCAL);
    void* module=dlopen(name,RTLD_NOW|RTLD_LOCAL);
    env->ReleaseStringUTFChars(path,name);
    if(!graphics||!module){std::fprintf(stderr,"probe dlopen failure: %s\n",dlerror());return JNI_FALSE;}
    adapter=reinterpret_cast<CreateAdapter>(dlsym(module,"sony_heif_create_decoder"));
    if(!adapter)return JNI_FALSE;
    HookSearch search;dl_iterate_phdr(patchHwui,&search);
    return search.found?JNI_TRUE:JNI_FALSE;
}
