#include "router.h"
#include "codec_ffi.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <numeric>
#include <vector>
#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr,"FAIL %s at line %d\n",#expr,__LINE__);return 1;}}while(0)

namespace {
int destroyed=0,initCalls=0;
HeifStream* expected=nullptr;
bool identity=false,acceptInit=true;
std::vector<uint8_t> observed;
class Input final:public HeifStream {
public:
    Input(std::vector<uint8_t> data,int mode,size_t maxRead=SIZE_MAX):data_(std::move(data)),mode_(mode),maxRead_(maxRead){}
    ~Input()override{++destroyed;}
    size_t read(void* dst,size_t n)override{
        n=std::min({n,data_.size()-pos_,maxRead_});if(dst&&n)std::memcpy(dst,data_.data()+pos_,n);pos_+=n;
        furthest_=std::max(furthest_,pos_);return n;
    }
    bool rewind()override{if(mode_!=1&&(mode_!=3||furthest_>32))return false;pos_=0;return true;}
    bool seek(size_t n)override{if((mode_!=2&&(mode_!=4||furthest_>32))||n>data_.size())return false;pos_=n;return true;}
    bool hasLength()const override{return false;}
    size_t getLength()const override{return 0;}
private:std::vector<uint8_t>data_;size_t pos_=0,furthest_=0;int mode_;size_t maxRead_;
};
class Original final:public HeifDecoder {
public:
    bool getEncodedColor(HeifEncodedColor*)const override{return false;}
    bool setOutputColor(HeifColorFormat)override{return true;}
    bool init(HeifStream* input,HeifFrameInfo*)override{
        input_.reset(input);++initCalls;identity=input==expected;observed.clear();
        uint8_t chunk[64];size_t n;
        while((n=input->read(chunk,sizeof(chunk))))observed.insert(observed.end(),chunk,chunk+n);
        return acceptInit;
    }
    bool getSequenceInfo(HeifFrameInfo*,size_t*)override{return false;}
    bool decode(HeifFrameInfo*)override{return false;}
    bool decodeSequence(int,HeifFrameInfo*)override{return false;}
    bool getScanline(uint8_t*)override{return false;}
    size_t skipScanlines(size_t)override{return 0;}
    uint32_t getColorDepth()override{return 8;}
private:std::unique_ptr<HeifStream>input_;
};
HeifDecoder* factory(){return new Original;}
}
// The fallback contract must never reach the Sony codec.
extern "C" int sony_hif_probe(const uint8_t*,size_t,HifInfo*){return -1;}
extern "C" void* sony_hif_decode(const uint8_t*,size_t){return nullptr;}
extern "C" int sony_hif_row(const void*,uint32_t,uint32_t,uint8_t*,size_t){return -1;}
extern "C" void sony_hif_free(void*){}
extern "C" const char* sony_hif_last_error(){return "unexpected Sony path";}

int main(){
    std::vector<uint8_t>bytes(300);std::iota(bytes.begin(),bytes.end(),0);
    for(int mode:{1,2}){
        destroyed=initCalls=0;identity=false;acceptInit=true;
        {
            std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
            expected=new Input(bytes,mode);
            CHECK(decoder->init(expected,nullptr));
            CHECK(initCalls==1&&identity); // Exact original object, no module-owned vtable.
            CHECK(observed==bytes&&destroyed==0);
        }
        CHECK(destroyed==1);
    }
    destroyed=initCalls=0;acceptInit=true;
    {
        std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
        CHECK(!decoder->init(new Input(bytes,0),nullptr));
        CHECK(initCalls==0&&destroyed==1);
    }
    destroyed=initCalls=0;identity=false;acceptInit=false;
    {
        std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
        expected=new Input(bytes,1);
        CHECK(!decoder->init(expected,nullptr));
        CHECK(initCalls==1&&identity&&observed==bytes);
    }
    CHECK(destroyed==1);
    destroyed=initCalls=0;identity=false;acceptInit=true;
    {
        std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
        expected=new Input({},0);CHECK(decoder->init(expected,nullptr));
        CHECK(initCalls==1&&identity&&observed.empty());
    }
    CHECK(destroyed==1);
    // A stream can rewind during type detection, then lose that capability
    // after reading beyond Skia's 32-byte front buffer (the actual regression).
    const std::vector<uint8_t> phone={0,0,0,24,'f','t','y','p','h','e','i','c',0,0,0,0,'m','i','f','1','h','e','i','c'};
    std::vector<std::vector<uint8_t>> ordinaryHeaders={phone};
    auto atBoundary=phone;atBoundary[3]=32;atBoundary.insert(atBoundary.end(),{'t','e','s','t','t','e','s','t'});ordinaryHeaders.push_back(atBoundary);
    auto heix=phone;heix[11]='x';ordinaryHeaders.push_back(heix); // heix alone is not Sony.
    for(uint32_t size:{0u,1u,12u,18u,129u,132u,0x7fffffffu}){
        auto bad=phone;bad[0]=uint8_t(size>>24);bad[1]=uint8_t(size>>16);bad[2]=uint8_t(size>>8);bad[3]=uint8_t(size);
        ordinaryHeaders.push_back(bad);
    }
    auto notFtyp=phone;notFtyp[4]='x';ordinaryHeaders.push_back(notFtyp);
    for(const auto& header:ordinaryHeaders){
        auto payload=header;payload.resize(300,0x5a);
        for(int mode:{3,4})for(size_t maxRead:{size_t(1),size_t(5),SIZE_MAX}){
            destroyed=initCalls=0;identity=false;acceptInit=true;
            {
                std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
                expected=new Input(payload,mode,maxRead);
                CHECK(decoder->init(expected,nullptr));
                CHECK(initCalls==1&&identity&&observed==payload&&destroyed==0);
            }
            CHECK(destroyed==1);
        }
    }
    // Incomplete ftyp/short streams must also preserve the bytes and ownership.
    auto truncated=phone;truncated[3]=40;
    for(size_t length:{size_t(0),size_t(5),size_t(8),size_t(24)}){
        auto payload=truncated;payload.resize(length);
        destroyed=initCalls=0;identity=false;acceptInit=true;
        {
            std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
            expected=new Input(payload,3,1);CHECK(decoder->init(expected,nullptr));
            CHECK(initCalls==1&&identity&&observed==payload);
        }
        CHECK(destroyed==1);
    }
    // Longer ordinary boxes still retain the existing seekable fallback.
    for(uint8_t size:{uint8_t(40),uint8_t(128)}){
        auto payload=phone;payload.resize(300,0x5a);payload[3]=size;
        destroyed=initCalls=0;identity=false;
        {
            std::unique_ptr<HeifDecoder>decoder(sony_heif_create_decoder(factory));
            expected=new Input(payload,1);CHECK(decoder->init(expected,nullptr));
            CHECK(initCalls==1&&identity&&observed==payload);
        }
        CHECK(destroyed==1);
    }
    std::puts("fallback identity, replay, rewind/seek and ownership tests passed");
    std::puts("ordinary HEIF with a 32-byte rewind window and short reads passed");
}
