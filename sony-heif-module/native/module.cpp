#include <sys/stat.h>
#include <unistd.h>
#include <android/log.h>
#include <cstring>
#include <dlfcn.h>
#include "vendor/zygisk.hpp"
#include "router.h"
#include "target_process.h"

namespace {
HeifFactory originalFactory=nullptr;
HeifDecoder* hookedFactory(){return sony_heif_create_decoder(originalFactory);}
class SonyHeifModule final:public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api,JNIEnv* env)override{api_=api;env_=env;}
    void preAppSpecialize(zygisk::AppSpecializeArgs* args)override{
        if(args->nice_name){
            const char* name=env_->GetStringUTFChars(args->nice_name,nullptr);
            if(name){selected_=isTargetProcess(name);
                env_->ReleaseStringUTFChars(args->nice_name,name);}
        }
        if(!selected_)api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const zygisk::AppSpecializeArgs*)override{
        if(!selected_)return;
        struct stat st{};
        if(stat("/system/lib64/libhwui.so",&st)!=0){log("libhwui stat failed");return;}
        originalFactory=reinterpret_cast<HeifFactory>(dlsym(RTLD_DEFAULT,"_Z17createHeifDecoderv"));
        if(!originalFactory){log("original HEIF factory unavailable; leaving platform unchanged");return;}
        void* original=nullptr;
        api_->pltHookRegister(st.st_dev,st.st_ino,"_Z17createHeifDecoderv",reinterpret_cast<void*>(hookedFactory),&original);
        bool ok=api_->pltHookCommit();
        if(original)originalFactory=reinterpret_cast<HeifFactory>(original);
        __android_log_print(ANDROID_LOG_INFO,"SonyHeif","PLT hook commit=%d original=%p pid=%d",ok,original,getpid());
        // Never unload this library after attempting a hook: installed pointers must remain valid.
    }
    void preServerSpecialize(zygisk::ServerSpecializeArgs*)override{api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);}
private:
    static void log(const char* msg){__android_log_print(ANDROID_LOG_WARN,"SonyHeif","%s",msg);}
    zygisk::Api* api_=nullptr;JNIEnv* env_=nullptr;bool selected_=false;
};
}
REGISTER_ZYGISK_MODULE(SonyHeifModule)
