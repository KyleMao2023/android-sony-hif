// Android HEIF ABI declarations adapted from AOSP frameworks/av,
// media/libheif/include/HeifDecoderAPI.h (Apache-2.0, Copyright 2017 AOSP).
// Interface version includes getColorDepth(), matching this Android 16 device.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

enum HeifColorFormat { kHeifColorFormat_RGB565=0, kHeifColorFormat_RGBA_8888=1,
    kHeifColorFormat_BGRA_8888=2, kHeifColorFormat_RGBA_1010102=3 };
enum HeifEncodedColor { kHeifEncodedColor_RGB=0, kHeifEncodedColor_YUV=1, kHeifEncodedColor_CMYK=2 };
struct HeifFrameInfo {
    uint32_t mWidth=0,mHeight=0;
    int32_t mRotationAngle=0;
    uint32_t mBytesPerPixel=0;
    int64_t mDurationUs=0;
    uint32_t mBitDepth=0;
    std::vector<uint8_t> mIccData;
};
struct HeifStream {
    virtual ~HeifStream()=default;
    virtual size_t read(void* buffer,size_t size)=0;
    virtual bool rewind()=0;
    virtual bool seek(size_t position)=0;
    virtual bool hasLength()const=0;
    virtual size_t getLength()const=0;
};
struct HeifDecoder {
    virtual ~HeifDecoder()=default;
    virtual bool getEncodedColor(HeifEncodedColor*)const=0;
    virtual bool setOutputColor(HeifColorFormat)=0;
    virtual bool init(HeifStream*,HeifFrameInfo*)=0;
    virtual bool getSequenceInfo(HeifFrameInfo*,size_t*)=0;
    virtual bool decode(HeifFrameInfo*)=0;
    virtual bool decodeSequence(int,HeifFrameInfo*)=0;
    virtual bool getScanline(uint8_t*)=0;
    virtual size_t skipScanlines(size_t)=0;
    virtual uint32_t getColorDepth()=0;
};
using HeifFactory=HeifDecoder*(*)();
#if INTPTR_MAX == INT64_MAX
static_assert(sizeof(HeifFrameInfo)==56,"HeifFrameInfo ABI size");
static_assert(offsetof(HeifFrameInfo,mIccData)==32,"HeifFrameInfo ICC ABI offset");
#endif
