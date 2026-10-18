#include "decode_queue.h"
#include "codec_ffi.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s at %d\n",#x,__LINE__);return 1;}}while(0)
namespace {
std::atomic<int>calls{0},live{0},peak{0},freeCount{0};
std::mutex backendMutex;std::condition_variable backendCv;bool pauseBackend=false,started=false;
struct Fake {uint8_t value;};
EncodedImage bytes(uint8_t value){return std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{value,2,3});}
bool retained(const std::weak_ptr<const std::vector<uint8_t>>& input){
    // The async caller owns one reference; admission retains a second in its job.
    // Observe ownership so ordering tests do not rely on thread-start sleeps.
    const auto deadline=std::chrono::steady_clock::now()+1s;
    while(input.use_count()<2&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    return input.use_count()>=2;
}
void reset(){calls=0;live=0;peak=0;freeCount=0;started=false;pauseBackend=false;}
}
extern "C" void* sony_hif_decode(const uint8_t* input,size_t){
    ++calls;
    if(live.fetch_add(1)!=0){--live;return nullptr;}
    peak=1;
    {std::unique_lock<std::mutex>lock(backendMutex);started=true;backendCv.notify_all();backendCv.wait(lock,[]{return !pauseBackend;});}
    if(input[0]==255){--live;return nullptr;}
    return new Fake{input[0]};
}
extern "C" void sony_hif_free(void* raw){delete static_cast<Fake*>(raw);--live;++freeCount;}
extern "C" const char* sony_hif_last_error(){return "backend refused or deliberate failure";}
int main(){
    reset();
    {
        DecodeQueue queue;auto a=bytes(1),same=bytes(1);
        auto first=queue.acquire(a,1s);CHECK(first.image);
        auto second=queue.acquire(same,1s);CHECK(second.image); // Old fail-fast behavior fails here.
        CHECK(second.reused&&calls==1&&first.image->get()==second.image->get());
        CHECK(a.get()==same.get());
        first.image.reset();CHECK(live==1);second.image.reset();CHECK(live==0&&freeCount==1);
    }
    reset();
    {
        DecodeQueue queue;auto a=bytes(1),b=bytes(2);auto held=queue.acquire(a,1s);CHECK(held.image);
        auto waiting=std::async(std::launch::async,[&]{return queue.acquire(b,2s);});
        CHECK(waiting.wait_for(40ms)==std::future_status::timeout);CHECK(calls==1);
        held.image.reset();auto next=waiting.get();CHECK(next.image&&calls==2&&live==1);
        CHECK(static_cast<Fake*>(next.image->get())->value==2);next.image.reset();CHECK(live==0);
    }
    reset();
    {
        DecodeQueue queue;auto a=bytes(1);auto held=queue.acquire(a,1s);
        auto b=bytes(2),c=bytes(3);
        std::weak_ptr<const std::vector<uint8_t>> observedB=b,observedC=c;
        auto second=std::async(std::launch::async,[&,input=std::move(b)]()mutable{return queue.acquire(input,2s);});
        CHECK(retained(observedB));
        auto third=std::async(std::launch::async,[&,input=std::move(c)]()mutable{return queue.acquire(input,2s);});
        CHECK(retained(observedC));held.image.reset();
        auto next=second.get();CHECK(next.image&&static_cast<Fake*>(next.image->get())->value==2);
        CHECK(third.wait_for(20ms)==std::future_status::timeout&&calls==2);
        next.image.reset();auto last=third.get();
        CHECK(last.image&&static_cast<Fake*>(last.image->get())->value==3&&calls==3);
        last.image.reset();CHECK(live==0);
    }
    reset();
    {
        DecodeQueue queue;auto a=bytes(1);auto held=queue.acquire(a,1s);
        auto b=bytes(2),c=bytes(3);
        std::weak_ptr<const std::vector<uint8_t>> observedB=b,observedC=c;
        auto expired=std::async(std::launch::async,[&,input=std::move(b)]()mutable{return queue.acquire(input,100ms);});
        CHECK(retained(observedB));
        auto third=std::async(std::launch::async,[&,input=std::move(c)]()mutable{return queue.acquire(input,2s);});
        CHECK(retained(observedC));auto timed=expired.get();CHECK(!timed.image&&calls==1);
        held.image.reset();auto next=third.get();
        CHECK(next.image&&static_cast<Fake*>(next.image->get())->value==3&&calls==2);
        next.image.reset();CHECK(live==0);
    }
    reset();
    {
        DecodeQueue queue;auto a=bytes(1),b=bytes(2);auto held=queue.acquire(a,1s);
        auto timed=queue.acquire(b,35ms);CHECK(!timed.image&&timed.error[0]);CHECK(calls==1);
        held.image.reset();auto retry=queue.acquire(b,1s);CHECK(retry.image&&calls==2);
    }
    reset();
    {
        DecodeQueue queue({1,8,1024});auto a=bytes(1),b=bytes(2);auto held=queue.acquire(a,1s);
        auto full=queue.acquire(b,1s);CHECK(!full.image&&calls==1);
        auto same=bytes(1);auto reused=queue.acquire(same,1s);CHECK(reused.image&&reused.reused&&calls==1);
    }
    reset();
    {
        DecodeQueue queue;auto bad=bytes(255);auto failed=queue.acquire(bad,1s);CHECK(!failed.image&&live==0);
        auto good=bytes(3);auto retry=queue.acquire(good,1s);CHECK(retry.image&&live==1&&calls==2);
    }
    reset();
    {
        DecodeQueue queue;
        {std::lock_guard<std::mutex>lock(backendMutex);pauseBackend=true;}
        std::vector<std::future<DecodeQueue::Result>>readers;
        for(int i=0;i<8;++i)readers.push_back(std::async(std::launch::async,[&]{auto input=bytes(7);return queue.acquire(input,2s);}));
        {std::unique_lock<std::mutex>lock(backendMutex);CHECK(backendCv.wait_for(lock,1s,[]{return started;}));}
        std::this_thread::sleep_for(60ms);
        {std::lock_guard<std::mutex>lock(backendMutex);pauseBackend=false;}backendCv.notify_all();
        std::vector<DecodeQueue::Result>results;
        for(auto& reader:readers){auto r=reader.get();CHECK(r.image);results.push_back(std::move(r));}
        CHECK(calls==1&&live==1&&peak==1);results.clear();CHECK(live==0&&freeCount==1);
    }
    reset();
    {
        DecodeQueue queue;auto a=bytes(1);auto held=queue.acquire(a,1s);
        auto waiting=std::async(std::launch::async,[&]{auto b=bytes(2);return queue.acquire(b,2s);});
        CHECK(waiting.wait_for(30ms)==std::future_status::timeout);
        auto sameQueued=bytes(2);auto cancelled=queue.acquire(sameQueued,35ms);
        CHECK(!cancelled.image&&calls==1);held.image.reset();
        auto remaining=waiting.get();CHECK(remaining.image&&calls==2);
        remaining.image.reset();CHECK(live==0);
    }
    reset();
    {
        DecodeQueue queue({4,8,4});auto a=bytes(1);auto held=queue.acquire(a,1s);
        auto b=bytes(2);auto denied=queue.acquire(b,1s);CHECK(!denied.image&&calls==1);
        CHECK(std::strstr(denied.error.data(),"byte budget"));
        held.image.reset();auto after=queue.acquire(b,1s);CHECK(after.image&&calls==2);
    }
    reset();
    {
        DecodeQueue queue({4,1,1024});
        {std::lock_guard<std::mutex>lock(backendMutex);pauseBackend=true;}
        auto running=std::async(std::launch::async,[&]{auto a=bytes(1);return queue.acquire(a,1s);});
        {std::unique_lock<std::mutex>lock(backendMutex);CHECK(backendCv.wait_for(lock,1s,[]{return started;}));}
        auto b=bytes(2);auto denied=queue.acquire(b,1s);CHECK(!denied.image&&calls==1);
        CHECK(std::strstr(denied.error.data(),"request queue full"));
        {std::lock_guard<std::mutex>lock(backendMutex);pauseBackend=false;}backendCv.notify_all();
        auto result=running.get();CHECK(result.image);result.image.reset();CHECK(live==0);
    }
    reset();
    {
        std::shared_ptr<const SharedDecode>held;
        {DecodeQueue queue;auto a=bytes(1);held=queue.acquire(a,1s).image;CHECK(held);}
        CHECK(live==1);held.reset();CHECK(live==0);
    }
    std::puts("decode queue sharing, serialization, timeout, limits, failure and lifetime tests passed");
}
