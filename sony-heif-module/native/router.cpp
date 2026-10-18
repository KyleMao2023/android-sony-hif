#include "router.h"
#include "stream.h"
#include "codec_ffi.h"
#include "decode_queue.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <new>
#ifdef __ANDROID__
#include <android/log.h>
#include <unistd.h>
#define HLOG(...) __android_log_print(ANDROID_LOG_INFO,"SonyHeif",__VA_ARGS__)
#else
#include <cstdio>
#define HLOG(...) std::fprintf(stderr,__VA_ARGS__)
#endif

namespace {
constexpr size_t kMaxInput=128u*1024u*1024u;
class Router final:public HeifDecoder {
public:
    explicit Router(HeifFactory original):original_(original?original():nullptr){}
    ~Router()override{releasePixels();}
    bool getEncodedColor(HeifEncodedColor* color)const override{
        return delegated_&&original_?original_->getEncodedColor(color):false;
    }
    bool setOutputColor(HeifColorFormat color)override{
        if(color<kHeifColorFormat_RGB565 || color>kHeifColorFormat_RGBA_1010102)return false;
        format_=color;
        return delegated_&&original_?original_->setOutputColor(color):true;
    }
    bool init(HeifStream* stream,HeifFrameInfo* frame)override{
        releasePixels();encoded_.reset();sony_=false;delegated_=false;initialized_=false;row_=0;decoded_=false;
        if(!stream)return false;
        try{
            auto input=std::make_unique<ReplayStream>(stream);
            if(!isSonyHeifHeader(input->header().data(),input->header().size())){
                delegated_=true;
                if(!original_)return false;
                auto platformInput=input->takeOriginalFromStart();
                if(!platformInput){HLOG("platform fallback failed: original stream cannot reset");return false;}
                original_->setOutputColor(format_);
                HLOG("platform fallback using original stream");
                return original_->init(platformInput.release(),frame);
            }
            sony_=true;
            if(input->hasLength()&&input->getLength()>kMaxInput){HLOG("input exceeds 128 MiB");return false;}
            std::vector<uint8_t> encoded;
            std::array<uint8_t,65536> chunk{};
            for(;;){
                size_t n=input->read(chunk.data(),chunk.size());
                if(!n)break;
                if(n>chunk.size() || n>kMaxInput-encoded.size())return false;
                encoded.insert(encoded.end(),chunk.begin(),chunk.begin()+n);
            }
            if(sony_hif_probe(encoded.data(),encoded.size(),&info_)!=0){HLOG("probe failed: %s",sony_hif_last_error());return false;}
            encoded_=std::make_shared<const std::vector<uint8_t>>(std::move(encoded));
            initialized_=true;
            fillFrame(frame);
            HLOG("Sony primary %ux%u depth=%u chroma=%u HLG=%u grid=%ux%u",info_.width,info_.height,
                info_.bit_depth,info_.chroma,info_.hlg,info_.grid_columns,info_.grid_rows);
            return true;
        }catch(...){HLOG("init allocation or stream exception");return false;}
    }
    bool getSequenceInfo(HeifFrameInfo* frame,size_t* count)override{
        return delegated_&&original_?original_->getSequenceInfo(frame,count):false;
    }
    bool decode(HeifFrameInfo* frame)override{
        if(delegated_)return original_&&original_->decode(frame);
        if(!sony_||!initialized_)return false;
        row_=0;
        if(!pixels_){
            try{
                auto timeout=std::chrono::milliseconds(15000);
#ifdef __ANDROID__
                // Queuing on the app's main thread must not create a long ANR wait.
                if(gettid()==getpid())timeout=std::chrono::milliseconds(250);
#endif
                auto result=processDecodeQueue().acquire(encoded_,timeout);
                if(!result.image){HLOG("decode failed: %s (elapsed=%llu ms)",result.error.data(),static_cast<unsigned long long>(result.elapsedMs));return false;}
                pixels_=std::move(result.image);
                HLOG("%s decode %ux%u elapsed=%llu ms",result.reused?"shared":"new",info_.width,info_.height,
                    static_cast<unsigned long long>(result.elapsedMs));
            }catch(...){HLOG("decode queue allocation exception");return false;}
        }
        decoded_=true;fillFrame(frame);return true;
    }
    bool decodeSequence(int index,HeifFrameInfo* frame)override{
        return delegated_&&original_?original_->decodeSequence(index,frame):false;
    }
    bool getScanline(uint8_t* dst)override{
        if(delegated_)return original_&&original_->getScanline(dst);
        if(!decoded_||!pixels_||!dst||row_>=info_.height)return false;
        if(sony_hif_row(pixels_->get(),row_,static_cast<uint32_t>(format_),dst,rowBytes())!=0)return false;
        ++row_;if(row_==info_.height)releasePixels();
        return true;
    }
    size_t skipScanlines(size_t count)override{
        if(delegated_)return original_?original_->skipScanlines(count):0;
        if(!decoded_||row_>=info_.height)return 0;
        size_t skipped=std::min(count,size_t(info_.height-row_));row_+=static_cast<uint32_t>(skipped);
        if(row_==info_.height)releasePixels();
        return skipped;
    }
    uint32_t getColorDepth()override{
        return delegated_?(original_?original_->getColorDepth():0):(initialized_?info_.bit_depth:0);
    }
private:
    size_t rowBytes()const{return size_t(info_.width)*(format_==kHeifColorFormat_RGB565?2:4);}
    void fillFrame(HeifFrameInfo* frame)const{
        if(!frame)return;
        frame->mWidth=info_.width;frame->mHeight=info_.height;frame->mRotationAngle=0;
        frame->mBytesPerPixel=format_==kHeifColorFormat_RGB565?2:4;frame->mDurationUs=0;
        frame->mBitDepth=info_.bit_depth;frame->mIccData.clear();
    }
    void releasePixels(){pixels_.reset();}
    std::unique_ptr<HeifDecoder> original_;
    EncodedImage encoded_;
    HifInfo info_{};
    std::shared_ptr<const SharedDecode>pixels_;
    HeifColorFormat format_=kHeifColorFormat_RGB565;
    uint32_t row_=0;
    bool sony_=false,delegated_=false,initialized_=false,decoded_=false;
};
}
HeifDecoder* sony_heif_create_decoder(HeifFactory original){
    try{return new(std::nothrow)Router(original);}catch(...){return nullptr;}
}
