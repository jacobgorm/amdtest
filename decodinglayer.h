#ifndef __DECODINGLAYER2D_H__
#define __DECODINGLAYER2D_H__

#include "buffer.h"
#include "condition.h"

class Device;
class ImageBuffer;

class DecodingLayer {
protected:
    uint8_t *buffer = nullptr;
    static const size_t max_buffer = 0x80000;

    Device *device;
    int width = 0, height = 0;
    bool is_hevc = false;

    ConditionVariable<ImageBuffer *> current_frame;

    DecodingLayer(Device *device);
#if 0
    void PutFrame(Buffer<uint8_t> *frame);
#endif

public:
    virtual ~DecodingLayer();
    virtual bool ReceiveBytes(const uint8_t *bytes, size_t compressed_size) = 0;
    ImageBuffer *GetFrame();

    static DecodingLayer *Create(Device *device);
};

#endif /* __DECODINGLAYER2D_H__ */
