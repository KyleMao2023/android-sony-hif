// Calls the real platform HEIF decoder in this process only. The optional
// anonymous vtable models an injected proxy absent from the linker CFI shadow.
#include "router.h"
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
namespace {
HeifFactory platformFactory=nullptr;
HeifStream* originalInput=nullptr;
bool simulate=true,exactOriginal=false;
class FileStream final:public HeifStream {
public:
    explicit FileStream(const char* path):file_(std::fopen(path,"rb")){
        if(file_){std::fseek(file_,0,SEEK_END);long n=std::ftell(file_);if(n>=0)length_=size_t(n);std::rewind(file_);}
    }
    ~FileStream()override{if(file_)std::fclose(file_);}
    bool good()const{return file_;}
    size_t read(void* dst,size_t n)override{
        if(dst)return std::fread(dst,1,n,file_);
        long at=std::ftell(file_);if(at<0)return 0;n=std::min(n,length_-std::min(size_t(at),length_));
        return std::fseek(file_,long(n),SEEK_CUR)==0?n:0;
    }
    bool rewind()override{return std::fseek(file_,0,SEEK_SET)==0;}
    bool seek(size_t at)override{return at<=length_&&std::fseek(file_,long(at),SEEK_SET)==0;}
    bool hasLength()const override{return true;}
    size_t getLength()const override{return length_;}
private:FILE*file_;size_t length_=0;
};
class PlatformGate final:public HeifDecoder {
public:
    PlatformGate():real_(platformFactory()){}
    ~PlatformGate()override{real_.reset();if(page_)munmap(page_,pageSize_);}
    bool getEncodedColor(HeifEncodedColor* c)const override{return real_->getEncodedColor(c);}
    bool setOutputColor(HeifColorFormat c)override{return real_->setOutputColor(c);}
    bool init(HeifStream* input,HeifFrameInfo* frame)override{
        exactOriginal=input==originalInput;
        std::printf("platform input=%s mode=%s\n",exactOriginal?"original":"proxy",simulate?"unregistered-proxy":"registered");
        if(simulate&&!exactOriginal){
            pageSize_=size_t(sysconf(_SC_PAGESIZE));
            void* requested=reinterpret_cast<void*>(uintptr_t(0x2f10000000ull));
            page_=mmap(requested,pageSize_,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
            if(page_==MAP_FAILED||page_!=requested){std::perror("isolated vtable mmap");page_=nullptr;delete input;return false;}
            auto location=reinterpret_cast<void***>(input);
            std::memcpy(page_,*location-2,9*sizeof(void*));
            if(mprotect(page_,pageSize_,PROT_READ)){std::perror("vtable mprotect");delete input;return false;}
            *location=static_cast<void**>(page_)+2;
        }
        return real_->init(input,frame);
    }
    bool getSequenceInfo(HeifFrameInfo* f,size_t* n)override{return real_->getSequenceInfo(f,n);}
    bool decode(HeifFrameInfo* f)override{return real_->decode(f);}
    bool decodeSequence(int i,HeifFrameInfo* f)override{return real_->decodeSequence(i,f);}
    bool getScanline(uint8_t* p)override{return real_->getScanline(p);}
    size_t skipScanlines(size_t n)override{return real_->skipScanlines(n);}
    uint32_t getColorDepth()override{return real_->getColorDepth();}
private:std::unique_ptr<HeifDecoder>real_;void*page_=nullptr;size_t pageSize_=0;
};
HeifDecoder* gate(){return new PlatformGate;}
}
int main(int argc,char**argv){
    if(argc<3){std::fprintf(stderr,"usage: fallback-probe MODULE_SO ORDINARY_HEIF [--registered] [--expect-original]\n");return 2;}
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    bool expectOriginal=false;
    for(int i=3;i<argc;++i){
        if(!std::strcmp(argv[i],"--registered"))simulate=false;
        else if(!std::strcmp(argv[i],"--expect-original"))expectOriginal=true;
        else{std::fprintf(stderr,"unknown option: %s\n",argv[i]);return 2;}
    }
    void* heif=dlopen("libheif.so",RTLD_NOW|RTLD_LOCAL);
    if(!heif){std::fprintf(stderr,"platform dlopen: %s\n",dlerror());return 2;}
    platformFactory=reinterpret_cast<HeifFactory>(dlsym(heif,"_Z17createHeifDecoderv"));
    void* module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!module||!platformFactory){std::fprintf(stderr,"factory/dlopen: %s\n",dlerror());return 2;}
    using Create=HeifDecoder*(*)(HeifFactory);
    auto create=reinterpret_cast<Create>(dlsym(module,"sony_heif_create_decoder"));
    if(!create)return 2;
    std::unique_ptr<HeifDecoder> decoder(create(gate));
    auto input=std::make_unique<FileStream>(argv[2]);
    if(!input->good()||!decoder)return 2;
    originalInput=input.get();HeifFrameInfo info{};
    if(!decoder->setOutputColor(kHeifColorFormat_RGBA_8888))return 2;
    bool ok=decoder->init(input.release(),&info);
    std::printf("%s real platform init=%d size=%ux%u exact_original=%d\n",ok?"PASS":"FAIL",ok,info.mWidth,info.mHeight,exactOriginal);
    return ok&&info.mWidth==9504&&info.mHeight==6336&&(!expectOriginal||exactOriginal)?0:1;
}
