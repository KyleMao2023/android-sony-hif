#include "stream.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <numeric>
#undef assert
#define assert(expr) do { if(!(expr)){ std::cerr<<"FAIL: " #expr <<" at "<<__LINE__<<"\n"; return 1; } } while(0)

class NonSeekable final:public HeifStream {
public:
    explicit NonSeekable(std::vector<uint8_t> data,int* destroyed=nullptr,size_t maxRead=SIZE_MAX):data_(std::move(data)),destroyed_(destroyed),maxRead_(maxRead){}
    ~NonSeekable()override{if(destroyed_)++*destroyed_;}
    size_t read(void* dst,size_t n)override{
        n=std::min({n,data_.size()-pos_,maxRead_});if(dst&&n)std::memcpy(dst,data_.data()+pos_,n);pos_+=n;return n;
    }
    bool rewind()override{return false;}
    bool seek(size_t)override{return false;}
    bool hasLength()const override{return false;}
    size_t getLength()const override{return 0;}
private:std::vector<uint8_t>data_;size_t pos_=0;int*destroyed_;size_t maxRead_;
};
int main(){
    std::vector<uint8_t> header={0,0,0,24,'f','t','y','p','h','e','i','x',0,0,0,0,'S','H','I','F','m','i','f','1'};
    assert(isSonyHeifHeader(header.data(),header.size()));
    auto ordinary=header;ordinary[8]='a';assert(!isSonyHeifHeader(ordinary.data(),ordinary.size()));
    assert(!isSonyHeifHeader(header.data(),20));
    auto invalid=header;invalid[3]=0;assert(!isSonyHeifHeader(invalid.data(),invalid.size()));
    std::vector<uint8_t> input(300);std::iota(input.begin(),input.end(),0);
    const std::vector<uint8_t> sony40={0,0,0,40,'f','t','y','p','h','e','i','x',0,0,0,0,'m','i','f','1','h','e','i','x','m','i','a','f','M','i','H','A','j','p','e','g','S','H','I','F'};
    std::copy(sony40.begin(),sony40.end(),input.begin());
    int destroyed=0;
    {
        ReplayStream s(new NonSeekable(input,&destroyed));
        uint8_t first[12];assert(s.read(first,sizeof(first))==sizeof(first));
        assert(std::equal(first,first+12,input.begin()));assert(s.rewind());
        std::vector<uint8_t> output(300);assert(s.read(output.data(),output.size())==300);
        assert(output==input);assert(s.read(first,1)==0);assert(!s.rewind());
    }
    assert(destroyed==1);
    {
        ReplayStream s(new NonSeekable(input));
        assert(s.read(nullptr,9)==9);uint8_t v=0;assert(s.read(&v,1)==1 && v==input[9]);
        assert(s.seek(2));assert(s.read(&v,1)==1 && v==input[2]);
    }
    for(size_t maxRead:{size_t(1),size_t(5),SIZE_MAX}){
        ReplayStream s(new NonSeekable(input,nullptr,maxRead));
        assert(s.header()==sony40); // No fixed 128-byte read beyond the ftyp.
        assert(isSonyHeifHeader(s.header().data(),s.header().size()));
        std::vector<uint8_t> output;uint8_t chunk[17];size_t n;
        while((n=s.read(chunk,sizeof(chunk))))output.insert(output.end(),chunk,chunk+n);
        assert(output==input);
    }
    // SHIF may be at the end of the supported 128-byte box.
    auto longest=sony40;longest.resize(128,'x');longest[3]=128;
    std::fill(longest.begin()+36,longest.end(),'x');std::memcpy(longest.data()+124,"SHIF",4);
    {
        ReplayStream s(new NonSeekable(longest));assert(s.header()==longest);
        assert(isSonyHeifHeader(s.header().data(),s.header().size()));
    }
    std::cout<<"stream tests passed\n";
}
