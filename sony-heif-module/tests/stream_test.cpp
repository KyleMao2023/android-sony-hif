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
    explicit NonSeekable(std::vector<uint8_t> data,int* destroyed=nullptr):data_(std::move(data)),destroyed_(destroyed){}
    ~NonSeekable()override{if(destroyed_)++*destroyed_;}
    size_t read(void* dst,size_t n)override{
        n=std::min(n,data_.size()-pos_);if(dst)std::memcpy(dst,data_.data()+pos_,n);pos_+=n;return n;
    }
    bool rewind()override{return false;}
    bool seek(size_t)override{return false;}
    bool hasLength()const override{return false;}
    size_t getLength()const override{return 0;}
private:std::vector<uint8_t>data_;size_t pos_=0;int*destroyed_;
};
int main(){
    std::vector<uint8_t> header={0,0,0,24,'f','t','y','p','h','e','i','x',0,0,0,0,'S','H','I','F','m','i','f','1'};
    assert(isSonyHeifHeader(header.data(),header.size()));
    auto ordinary=header;ordinary[8]='a';assert(!isSonyHeifHeader(ordinary.data(),ordinary.size()));
    assert(!isSonyHeifHeader(header.data(),20));
    auto invalid=header;invalid[3]=0;assert(!isSonyHeifHeader(invalid.data(),invalid.size()));
    std::vector<uint8_t> input(300);std::iota(input.begin(),input.end(),0);
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
        assert(s.read(nullptr,9)==9);uint8_t v=0;assert(s.read(&v,1)==1 && v==9);
        assert(s.seek(2));assert(s.read(&v,1)==1 && v==2);
    }
    std::cout<<"stream tests passed\n";
}
