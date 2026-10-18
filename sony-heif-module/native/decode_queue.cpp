#include "decode_queue.h"
#include "codec_ffi.h"
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#ifdef __ANDROID__
#include <android/log.h>
#define QLOG(...) __android_log_print(ANDROID_LOG_INFO,"SonyHeif",__VA_ARGS__)
#else
#define QLOG(...) do{}while(0)
#endif

struct DecodeQueue::State {
    enum class Phase {Queued,Decoding,Ready,Failed};
    struct Job {
        uint64_t id=0,hash=0;EncodedImage input;Phase phase=Phase::Queued;size_t clients=0;
        std::weak_ptr<const SharedDecode> image;
        // Keep a result alive until all callers already waiting have received it.
        // This bridge is cleared when clients reaches zero, never used as a cache.
        std::shared_ptr<const SharedDecode> bridge;
        std::array<char,192> error{};
    };
    explicit State(Limits value):limits(value){}
    Limits limits;std::mutex mutex;std::condition_variable changed;
    std::shared_ptr<Job> active;std::deque<std::shared_ptr<Job>>pending;
    uint64_t nextId=1;size_t requests=0,encodedBytes=0;
};
SharedDecode::~SharedDecode(){if(raw_){sony_hif_free(raw_);if(released_)released_();}}
DecodeQueue::DecodeQueue():DecodeQueue(Limits{}){}
DecodeQueue::DecodeQueue(Limits limits):state_(std::make_shared<State>(limits)){}
DecodeQueue::~DecodeQueue()=default;
DecodeQueue::Result DecodeQueue::acquire(EncodedImage& input,std::chrono::milliseconds timeout){
    const auto begin=std::chrono::steady_clock::now(),deadline=begin+timeout;
    Result result;
    auto fail=[&](const char* text){std::snprintf(result.error.data(),result.error.size(),"%s",text);};
    if(!input||input->empty()){fail("empty encoded image");return result;}
    uint64_t hash=14695981039346656037ull;
    for(uint8_t byte:*input)hash=(hash^byte)*1099511628211ull;
    auto state=state_;
    // If clearing the bridge releases the final image, its destructor locks
    // State again. Declare this before lock so it is destroyed AFTER unlock.
    std::shared_ptr<const SharedDecode> dropAfterUnlock;
    std::unique_lock<std::mutex>lock(state->mutex);
    auto matches=[&](const std::shared_ptr<State::Job>& job){
        return job&&job->hash==hash&&job->input->size()==input->size()&&*job->input==*input;
    };
    auto elapsed=[&]{return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count());};
    std::shared_ptr<State::Job>job;
    if(matches(state->active)){
        input=state->active->input;
        if(state->active->phase==State::Phase::Ready){
            result.image=state->active->image.lock();
            if(result.image){result.reused=true;result.elapsedMs=elapsed();return result;}
            // Weak expiration can precede the final destructor's completion.
            // Leave active occupied until its post-free notification arrives.
        }else if(state->active->phase==State::Phase::Decoding)job=state->active;
    }
    if(!job){for(const auto& queued:state->pending)if(matches(queued)){job=queued;input=job->input;break;}}
    if(state->requests>=state->limits.requests){fail("decode request queue full");return result;}
    if(!job){
        if(state->pending.size()+(state->active?1u:0u)>=state->limits.jobs){fail("decode image queue full");return result;}
        if(input->size()>state->limits.encodedBytes-state->encodedBytes){fail("encoded queue byte budget exhausted");return result;}
        job=std::make_shared<State::Job>();job->id=state->nextId++;job->hash=hash;job->input=input;
        state->pending.push_back(job);state->encodedBytes+=input->size();
    }else result.reused=true;
    ++job->clients;++state->requests;
    auto finish=[&]{
        --job->clients;--state->requests;
        if(job->clients==0){
            dropAfterUnlock=std::move(job->bridge);
            if(job->phase==State::Phase::Queued){
                auto position=std::find(state->pending.begin(),state->pending.end(),job);
                if(position!=state->pending.end()){state->pending.erase(position);state->encodedBytes-=job->input->size();}
            }
        }
        result.elapsedMs=elapsed();state->changed.notify_all();
    };
    bool waited=false;
    try{
        for(;;){
            if(job->phase==State::Phase::Ready){
                result.image=job->image.lock();
                if(!result.image){fail("shared decode result expired unexpectedly");}
                finish();return result;
            }
            if(job->phase==State::Phase::Failed){result.error=job->error;finish();return result;}
            if(waited&&std::chrono::steady_clock::now()>=deadline){fail("decode queue wait timed out");finish();return result;}
            if(job->phase==State::Phase::Queued&&!state->active&&!state->pending.empty()&&state->pending.front()==job){
                state->pending.pop_front();state->active=job;job->phase=State::Phase::Decoding;
                lock.unlock();
                std::shared_ptr<SharedDecode>decoded;
                std::array<char,192>error{};
                try{
                    QLOG("backend decode job=%llu bytes=%zu",static_cast<unsigned long long>(job->id),job->input->size());
                    std::unique_ptr<void,decltype(&sony_hif_free)>raw(sony_hif_decode(job->input->data(),job->input->size()),sony_hif_free);
                    if(!raw){std::snprintf(error.data(),error.size(),"%s",sony_hif_last_error());}
                    else{
                        decoded=std::make_shared<SharedDecode>();
                        const uint64_t id=job->id;
                        decoded->released_=[state,id]{
                            std::lock_guard<std::mutex>guard(state->mutex);
                            if(state->active&&state->active->id==id){
                                state->encodedBytes-=state->active->input->size();state->active.reset();
                            }
                            state->changed.notify_all();
                        };
                        decoded->raw_=raw.release();
                    }
                }catch(...){std::snprintf(error.data(),error.size(),"decode result allocation failed");}
                lock.lock();
                if(decoded&&decoded->raw_){
                    job->image=decoded;job->bridge=decoded;job->phase=State::Phase::Ready;
                }else{
                    job->error=error;job->phase=State::Phase::Failed;
                    state->encodedBytes-=job->input->size();state->active.reset();
                }
                state->changed.notify_all();continue;
            }
            if(std::chrono::steady_clock::now()>=deadline){fail("decode queue wait timed out");finish();return result;}
            waited=true;
            state->changed.wait_until(lock,deadline);
        }
    }catch(...){
        if(!lock.owns_lock())lock.lock();
        fail("decode queue synchronization failed");finish();return result;
    }
}
DecodeQueue& processDecodeQueue(){static DecodeQueue queue;return queue;}
