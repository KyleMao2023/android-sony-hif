// Isolated Zygisk API-v4 boundary regression. Never touches a live framework.
// Anonymous executable callback stubs model callbacks absent from linker CFI metadata.
#include <sys/types.h>
#include "vendor/zygisk.hpp"
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

static zygisk::internal::module_abi* registered=nullptr;
static unsigned registrationCount=0,optionCount=0;
static bool registration(zygisk::internal::api_table*,zygisk::internal::module_abi* abi){
    registered=abi;++registrationCount;return true;
}
static void option(void*,zygisk::Option value){
    if(value==zygisk::DLCLOSE_MODULE_LIBRARY)++optionCount;
}
static void* stub(uint8_t* page,size_t offset,void* target){
    // ldr x16, .+8; br x16; .quad target
    const uint32_t code[]={0x58000050,0xd61f0200};
    std::memcpy(page+offset,code,sizeof(code));
    std::memcpy(page+offset+8,&target,sizeof(target));
    return page+offset;
}
int main(int argc,char**argv){
    if(argc<2){std::fprintf(stderr,"usage: entry-probe MODULE_SO [--registered]\n");return 2;}
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    bool anonymous=!(argc>2&&!std::strcmp(argv[2],"--registered"));
    void* module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!module){std::fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
    auto entry=reinterpret_cast<void(*)(zygisk::internal::api_table*,JNIEnv*)>(dlsym(module,"zygisk_module_entry"));
    if(!entry){std::fprintf(stderr,"entry missing\n");return 2;}
    zygisk::internal::api_table table{};
    table.registerModule=registration;table.setOption=option;
    if(anonymous){
        size_t pageSize=size_t(sysconf(_SC_PAGESIZE));
        auto page=static_cast<uint8_t*>(mmap(nullptr,pageSize,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
        if(page==MAP_FAILED){std::perror("mmap");return 2;}
        table.registerModule=reinterpret_cast<decltype(table.registerModule)>(stub(page,0,reinterpret_cast<void*>(registration)));
        table.setOption=reinterpret_cast<decltype(table.setOption)>(stub(page,32,reinterpret_cast<void*>(option)));
        __builtin___clear_cache(reinterpret_cast<char*>(page),reinterpret_cast<char*>(page+64));
        if(mprotect(page,pageSize,PROT_READ|PROT_EXEC)){std::perror("mprotect");return 2;}
    }
    std::printf("boundary=%s calling Zygisk entry in isolated pid=%d\n",anonymous?"anonymous":"linker-registered",getpid());
    entry(&table,nullptr);
    if(registrationCount!=1||!registered||registered->api_version!=4||!registered->impl||!registered->preServerSpecialize){
        std::fprintf(stderr,"FAIL module registration\n");return 1;
    }
    registered->preServerSpecialize(registered->impl,nullptr);
    if(optionCount!=1){std::fprintf(stderr,"FAIL server unload option\n");return 1;}
    std::printf("PASS registerModule + onLoad + preServerSpecialize + setOption\n");
    return 0;
}
