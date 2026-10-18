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
    Input(std::vector<uint8_t> data,int mode):data_(std::move(data)),mode_(mode){}
    ~Input()override{++destroyed;}
    size_t read(void* dst,size_t n)override{
        n=std::min(n,data_.size()-pos_);if(dst&&n)std::memcpy(dst,data_.data()+pos_,n);pos_+=n;return n;
    }
    bool rewind()override{if(mode_!=1)return false;pos_=0;return true;}
    bool seek(size_t n)override{if(mode_!=2||n>data_.size())return false;pos_=n;return true;}
    bool hasLength()const override{return false;}
    size_t getLength()const override{return 0;}
private:std::vector<uint8_t>data_;size_t pos_=0;int mode_;
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
    std::puts("fallback identity, replay, rewind/seek and ownership tests passed");
}
