// Exercises the real platform AImageDecoder and the platform's CFI checks.
// The only patched memory belongs to this short-lived probe process.
#include "router.h"
#include <android/imagedecoder.h>
#include <android/bitmap.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>

using CreateAdapter=HeifDecoder*(*)(HeifFactory);
static CreateAdapter adapter=nullptr;
static HeifFactory original=nullptr;
static HeifDecoder* factoryHook(){return adapter(original);}
struct HookSearch {bool found=false;};
static int patchHwui(dl_phdr_info* info,size_t,void* context){
    if(!info->dlpi_name || !std::strstr(info->dlpi_name,"/libhwui.so"))return 0;
    auto& search=*static_cast<HookSearch*>(context);
    uintptr_t base=info->dlpi_addr;
    const ElfW(Dyn)* dynamic=nullptr;
    for(int i=0;i<info->dlpi_phnum;++i)if(info->dlpi_phdr[i].p_type==PT_DYNAMIC)
        dynamic=reinterpret_cast<const ElfW(Dyn)*>(base+info->dlpi_phdr[i].p_vaddr);
    if(!dynamic)return 1;
    uintptr_t sym=0,str=0,rel=0;size_t bytes=0;
    auto ptr=[base](uintptr_t p){return p<base?base+p:p;};
    for(auto d=dynamic;d->d_tag!=DT_NULL;++d){
        if(d->d_tag==DT_SYMTAB)sym=ptr(d->d_un.d_ptr);
        if(d->d_tag==DT_STRTAB)str=ptr(d->d_un.d_ptr);
        if(d->d_tag==DT_JMPREL)rel=ptr(d->d_un.d_ptr);
        if(d->d_tag==DT_PLTRELSZ)bytes=d->d_un.d_val;
    }
    if(!sym||!str||!rel)return 1;
    auto symbols=reinterpret_cast<const ElfW(Sym)*>(sym);
    auto strings=reinterpret_cast<const char*>(str);
    auto relocations=reinterpret_cast<const ElfW(Rela)*>(rel);
    for(size_t i=0;i<bytes/sizeof(ElfW(Rela));++i){
        const auto& r=relocations[i];
        if(ELF64_R_TYPE(r.r_info)!=R_AARCH64_JUMP_SLOT)continue;
        const char* name=strings+symbols[ELF64_R_SYM(r.r_info)].st_name;
        if(std::strcmp(name,"_Z17createHeifDecoderv"))continue;
        auto slot=reinterpret_cast<void**>(base+r.r_offset);
        long pageSize=sysconf(_SC_PAGESIZE);
        void* page=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot)&~(uintptr_t(pageSize)-1));
        if(mprotect(page,size_t(pageSize),PROT_READ|PROT_WRITE)!=0){std::perror("mprotect");return 1;}
        original=reinterpret_cast<HeifFactory>(*slot);
        __atomic_store_n(slot,reinterpret_cast<void*>(factoryHook),__ATOMIC_RELEASE);
        if(mprotect(page,size_t(pageSize),PROT_READ)!=0){std::perror("restore RELRO");return 1;}
        search.found=true;return 1;
    }
    return 1;
}
int main(int argc,char**argv){
    if(argc<4){std::fprintf(stderr,"usage: system-probe MODULE_SO FILE OUTPUT_PPM [--stock]\n");return 2;}
    bool stock=argc>4&&!std::strcmp(argv[4],"--stock");
    void* graphics=dlopen("libjnigraphics.so",RTLD_NOW|RTLD_LOCAL);
    if(!graphics){std::fprintf(stderr,"libjnigraphics: %s\n",dlerror());return 2;}
#define LOAD(name) auto name##Fn=reinterpret_cast<decltype(&name)>(dlsym(graphics,#name)); if(!name##Fn){std::fprintf(stderr,"missing %s\n",#name);return 2;}
    LOAD(AImageDecoder_createFromFd)
    LOAD(AImageDecoder_getHeaderInfo)
    LOAD(AImageDecoderHeaderInfo_getWidth)
    LOAD(AImageDecoderHeaderInfo_getHeight)
    LOAD(AImageDecoder_setAndroidBitmapFormat)
    LOAD(AImageDecoder_setTargetSize)
    LOAD(AImageDecoder_getMinimumStride)
    LOAD(AImageDecoder_decodeImage)
    LOAD(AImageDecoder_delete)
#undef LOAD
    if(!stock){
        void* module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
        if(!module){std::fprintf(stderr,"adapter dlopen: %s\n",dlerror());return 2;}
        adapter=reinterpret_cast<CreateAdapter>(dlsym(module,"sony_heif_create_decoder"));
        if(!adapter)return 2;
        HookSearch search;dl_iterate_phdr(patchHwui,&search);
        if(!search.found){std::fprintf(stderr,"system HEIF factory relocation not found\n");return 2;}
        std::printf("hooked libhwui factory; original=%p\n",reinterpret_cast<void*>(original));
    }
    int fd=open(argv[2],O_RDONLY|O_CLOEXEC);
    if(fd<0){std::perror("open input");return 2;}
    AImageDecoder* decoder=nullptr;
    int status=AImageDecoder_createFromFdFn(fd,&decoder);
    std::printf("%s create status=%d file=%s\n",stock?"STOCK":"HOOK",status,argv[2]);
    if(status!=ANDROID_IMAGE_DECODER_SUCCESS){close(fd);return 3;}
    auto header=AImageDecoder_getHeaderInfoFn(decoder);
    int width=AImageDecoderHeaderInfo_getWidthFn(header),height=AImageDecoderHeaderInfo_getHeightFn(header);
    int outWidth=width/6,outHeight=height/6;
    status=AImageDecoder_setAndroidBitmapFormatFn(decoder,ANDROID_BITMAP_FORMAT_RGBA_8888);
    if(status==0)status=AImageDecoder_setTargetSizeFn(decoder,outWidth,outHeight);
    size_t stride=AImageDecoder_getMinimumStrideFn(decoder);
    std::vector<uint8_t> pixels(stride*size_t(outHeight));
    if(status==0)status=AImageDecoder_decodeImageFn(decoder,pixels.data(),stride,pixels.size());
    std::printf("platform primary=%dx%d output=%dx%d decode status=%d\n",width,height,outWidth,outHeight,status);
    AImageDecoder_deleteFn(decoder);close(fd);
    if(status!=0||width!=9504||height!=6336)return 1;
    FILE* output=std::fopen(argv[3],"wb");
    if(!output){std::perror("output");return 2;}
    std::fprintf(output,"P6\n%d %d\n255\n",outWidth,outHeight);
    for(int y=0;y<outHeight;++y)for(int x=0;x<outWidth;++x)std::fwrite(pixels.data()+size_t(y)*stride+size_t(x)*4,1,3,output);
    bool writeOk=!std::ferror(output);std::fclose(output);
    return writeOk?0:1;
}
