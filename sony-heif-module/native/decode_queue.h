#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

using EncodedImage=std::shared_ptr<const std::vector<uint8_t>>;
class SharedDecode final {
public:
    SharedDecode()=default;
    ~SharedDecode();
    void* get()const{return raw_;}
    SharedDecode(const SharedDecode&)=delete;
    SharedDecode& operator=(const SharedDecode&)=delete;
private:
    friend class DecodeQueue;
    void* raw_=nullptr;
    std::function<void()> released_;
};
class DecodeQueue final {
public:
    struct Limits {size_t jobs=8,requests=64,encodedBytes=256u*1024u*1024u;};
    struct Result {
        std::shared_ptr<const SharedDecode> image;
        bool reused=false;
        uint64_t elapsedMs=0;
        std::array<char,192> error{};
    };
    DecodeQueue();
    explicit DecodeQueue(Limits limits);
    ~DecodeQueue();
    DecodeQueue(const DecodeQueue&)=delete;
    DecodeQueue& operator=(const DecodeQueue&)=delete;
    Result acquire(EncodedImage& input,std::chrono::milliseconds timeout);
private:
    struct State;
    std::shared_ptr<State>state_;
};
DecodeQueue& processDecodeQueue();
