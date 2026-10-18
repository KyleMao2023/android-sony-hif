#pragma once
#include "HeifDecoderAPI.h"
#include <memory>

bool isSonyHeifHeader(const uint8_t* data,size_t length);
class ReplayStream final:public HeifStream {
public:
    explicit ReplayStream(HeifStream* source);
    const std::vector<uint8_t>& header()const{return prefix_;}
    // Platform consumers must receive their own original object/vtable.
    // Returns null without transferring ownership if the source cannot reset.
    std::unique_ptr<HeifStream> takeOriginalFromStart();
    size_t read(void*,size_t)override;
    bool rewind()override;
    bool seek(size_t)override;
    bool hasLength()const override{return source_->hasLength();}
    size_t getLength()const override{return source_->getLength();}
private:
    std::unique_ptr<HeifStream> source_;
    std::vector<uint8_t> prefix_;
    size_t position_=0,sourcePosition_=0;
};
