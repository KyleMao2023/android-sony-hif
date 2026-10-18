// Multiple independent platform-style decoder objects in one isolated process.
#include "router.h"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/resource.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class Input final:public HeifStream {
public:
    explicit Input(const char* path):file_(std::fopen(path,"rb")){}
    ~Input()override{if(file_)std::fclose(file_);}
    bool good()const{return file_;}
    size_t read(void* dst,size_t n)override{
        if(dst)return std::fread(dst,1,n,file_);
        uint8_t buffer[4096];size_t total=0;
        while(n){size_t count=std::fread(buffer,1,std::min(n,sizeof(buffer)),file_);total+=count;n-=count;if(!count)break;}
        return total;
    }
    bool rewind()override{return false;}
    bool seek(size_t)override{return false;}
    bool hasLength()const override{return false;}
    size_t getLength()const override{return 0;}
private:FILE*file_;
};
struct Result {bool ok=false;uint64_t hash=0;long long milliseconds=0;};
int main(int argc,char**argv){
    if(argc<4){std::fprintf(stderr,"usage: concurrent-probe MODULE_SO --same FILE | MODULE_SO --mixed FILE420 FILE422 FILEHLG\n");return 2;}
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    bool same=!std::strcmp(argv[2],"--same");
    if((same&&argc!=4)||(!same&&(std::strcmp(argv[2],"--mixed")||argc!=6)))return 2;
    void* module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!module){std::fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
    using Create=HeifDecoder*(*)(HeifFactory);
    auto create=reinterpret_cast<Create>(dlsym(module,"sony_heif_create_decoder"));
    if(!create)return 2;
    int count=same?8:9;
    std::printf("pid=%d mode=%s requests=%d\n",getpid(),same?"same":"mixed",count);
    std::mutex mutex;std::condition_variable changed;int ready=0;
    std::vector<Result>results(static_cast<size_t>(count));
    std::vector<std::thread>threads;
    for(int index=0;index<count;++index){
        threads.emplace_back([&,index]{
            int fileIndex=same?0:index%3;const char* path=argv[3+fileIndex];
            auto started=std::chrono::steady_clock::now();
            std::unique_ptr<HeifDecoder>decoder(create(nullptr));
            auto input=std::make_unique<Input>(path);HeifFrameInfo info{};
            bool ok=decoder&&input->good();
            if(ok)ok=decoder->init(input.release(),&info);
            {
                std::unique_lock<std::mutex>lock(mutex);++ready;changed.notify_all();
                changed.wait(lock,[&]{return ready==count;});
            }
            if(ok)ok=decoder->setOutputColor(kHeifColorFormat_RGBA_8888)&&decoder->decode(&info);
            uint64_t hash=14695981039346656037ull;
            if(ok){
                std::vector<uint8_t>row(size_t(info.mWidth)*4);
                for(uint32_t y=0;y<info.mHeight;++y){
                    if(!decoder->getScanline(row.data())){ok=false;break;}
                    for(uint8_t byte:row)hash=(hash^byte)*1099511628211ull;
                }
            }
            const uint64_t expected=std::strstr(path,"422hlg")?0x444c433937951481ull:
                std::strstr(path,"422")?0x063810d9c54dbc1aull:0x1e746690d0587735ull;
            ok=ok&&info.mWidth==9504&&info.mHeight==6336&&hash==expected;
            results[size_t(index)]={ok,hash,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()};
        });
    }
    for(auto& thread:threads)thread.join();
    int passed=0;
    for(int i=0;i<count;++i){const auto& r=results[size_t(i)];passed+=r.ok?1:0;
        std::printf("%s request=%d file=%d hash=%016llx total_ms=%lld\n",r.ok?"PASS":"FAIL",i,same?0:i%3,
            static_cast<unsigned long long>(r.hash),r.milliseconds);}
    rusage usage{};getrusage(RUSAGE_SELF,&usage);
    std::printf("summary passed=%d failed=%d peak_rss_kib=%ld\n",passed,count-passed,usage.ru_maxrss);
    return passed==count?0:1;
}
