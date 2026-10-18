#include "router.h"
#include <dlfcn.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

class FileStream final:public HeifStream {
public:
    explicit FileStream(const char* path,bool seekable=true):f_(std::fopen(path,"rb")),seekable_(seekable){
        if(f_){std::fseek(f_,0,SEEK_END);long n=std::ftell(f_);if(n>=0)size_=size_t(n);std::rewind(f_);}
    }
    ~FileStream()override{if(f_)std::fclose(f_);}
    bool good()const{return f_;}
    size_t read(void* dst,size_t n)override{
        if(!f_)return 0;
        if(dst)return std::fread(dst,1,n,f_);
        long p=std::ftell(f_);if(p<0)return 0;size_t skip=std::min(n,size_-std::min(size_,size_t(p)));
        return std::fseek(f_,long(skip),SEEK_CUR)==0?skip:0;
    }
    bool rewind()override{return seekable_&&f_&&std::fseek(f_,0,SEEK_SET)==0;}
    bool seek(size_t p)override{return seekable_&&f_&&p<=size_&&std::fseek(f_,long(p),SEEK_SET)==0;}
    bool hasLength()const override{return seekable_;}
    size_t getLength()const override{return seekable_?size_:0;}
private:FILE*f_=nullptr;size_t size_=0;bool seekable_=true;
};
int main(int argc,char**argv){
    if(argc<3){std::fprintf(stderr,"usage: heif-probe MODULE_SO [--nonseek] FILE...\n");return 2;}
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    bool nonseek=!std::strcmp(argv[2],"--nonseek");
    void* module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!module){std::fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
    using Create=HeifDecoder*(*)(HeifFactory);
    auto create=reinterpret_cast<Create>(dlsym(module,"sony_heif_create_decoder"));
    if(!create){std::fprintf(stderr,"adapter export missing\n");return 2;}
    int failures=0;
    for(int i=nonseek?3:2;i<argc;++i){
        auto begin=std::chrono::steady_clock::now();
        std::unique_ptr<HeifDecoder> decoder(create(nullptr));
        auto input=std::make_unique<FileStream>(argv[i],!nonseek);
        HeifFrameInfo info{};
        if(!decoder||!input->good()||!decoder->init(input.release(),&info)){
            std::printf("FAIL init %s\n",argv[i]);++failures;continue;
        }
        if(!decoder->setOutputColor(kHeifColorFormat_RGBA_8888)||!decoder->decode(&info)){
            std::printf("FAIL decode %s\n",argv[i]);++failures;continue;
        }
        std::vector<uint8_t> row(size_t(info.mWidth)*4);
        uint64_t hash=14695981039346656037ull,sum=0;bool ok=true;
        for(uint32_t y=0;y<info.mHeight;++y){
            if(!decoder->getScanline(row.data())){ok=false;break;}
            for(size_t x=0;x<row.size();++x){hash=(hash^row[x])*1099511628211ull;if(x%4!=3)sum+=row[x];}
        }
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
        double mean=double(sum)/(double(info.mWidth)*info.mHeight*3*255);
        ok=ok&&!decoder->getScanline(row.data())&&info.mWidth==9504&&info.mHeight==6336&&mean>0.01&&mean<0.99;
        std::printf("%s %s %ux%u depth=%u rgba8_fnv=%016llx mean=%.6f ms=%lld\n",ok?"PASS":"FAIL",argv[i],
            info.mWidth,info.mHeight,info.mBitDepth,(unsigned long long)hash,mean,(long long)ms);
        if(!ok)++failures;
    }
    return failures?1:0;
}
