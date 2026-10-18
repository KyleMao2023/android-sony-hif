#include "stream.h"
#include <algorithm>
#include <cstring>

bool isSonyHeifHeader(const uint8_t* data,size_t length){
    if(!data || length<16 || std::memcmp(data+4,"ftyp",4))return false;
    uint32_t size=(uint32_t(data[0])<<24)|(uint32_t(data[1])<<16)|(uint32_t(data[2])<<8)|data[3];
    if(size<16 || size>length || (size-16)%4)return false;
    bool extended=false,sony=false;
    for(size_t p=8;p+4<=size;p+=4){
        if(p==12)continue;
        extended|=!std::memcmp(data+p,"heix",4);
        sony|=!std::memcmp(data+p,"SHIF",4);
    }
    return extended&&sony;
}
ReplayStream::ReplayStream(HeifStream* source):source_(source){
    if(!source_)return;
    prefix_.resize(128);
    size_t got=0;
    while(got<prefix_.size()){
        size_t n=source_->read(prefix_.data()+got,prefix_.size()-got);
        if(!n || n>prefix_.size()-got)break;
        got+=n;
    }
    prefix_.resize(got);sourcePosition_=got;
}
std::unique_ptr<HeifStream> ReplayStream::takeOriginalFromStart(){
    if(!source_)return {};
    if(sourcePosition_!=0 && !source_->rewind() && !source_->seek(0))return {};
    position_=sourcePosition_=0;
    return std::move(source_);
}
size_t ReplayStream::read(void* buffer,size_t size){
    if(!source_)return 0;
    size_t copied=0;
    if(position_<prefix_.size()){
        copied=std::min(size,prefix_.size()-position_);
        if(buffer)std::memcpy(buffer,prefix_.data()+position_,copied);
        position_+=copied;
    }
    if(copied==size)return copied;
    if(sourcePosition_>position_){
        if(!source_->seek(position_))return copied;
        sourcePosition_=position_;
    }
    while(sourcePosition_<position_){
        size_t need=position_-sourcePosition_;
        size_t skipped=source_->read(nullptr,need);
        if(!skipped || skipped>need)return copied;
        sourcePosition_+=skipped;
    }
    size_t n=source_->read(buffer?static_cast<uint8_t*>(buffer)+copied:nullptr,size-copied);
    if(n>size-copied)return copied;
    sourcePosition_+=n;position_+=n;
    return copied+n;
}
bool ReplayStream::rewind(){return seek(0);}
bool ReplayStream::seek(size_t offset){
    if(!source_)return false;
    if(offset<=prefix_.size() && sourcePosition_<=prefix_.size()){position_=offset;return true;}
    if(source_->seek(offset) || (offset==0&&source_->rewind())){
        position_=sourcePosition_=offset;return true;
    }
    return false;
}
