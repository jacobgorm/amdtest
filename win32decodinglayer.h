#ifndef __WIN32DECODINGLAYER_H__
#define __WIN32DECODINGLAYER_H__

#include "decodinglayer.h"
#include "lock.h"

class Win32DecoderImpl;

class Win32DecodingLayer : public DecodingLayer {
    friend class Win32DecoderImpl;
    Win32DecoderImpl *impl = nullptr;
    Lock lock;
public:
    Win32DecodingLayer(Device *device);
    ~Win32DecodingLayer();
    bool ReceiveBytes(const uint8_t *bytes, size_t compressed_size);
};


#endif /* __WIN32DECODINGLAYER_H__ */
