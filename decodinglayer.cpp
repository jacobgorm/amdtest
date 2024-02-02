#include "imagebuffer.h"

#ifdef _WIN32
#include "win32decodinglayer.h"
#elif defined(__APPLE__)
#include "vtdecodinglayer.h"
#endif

DecodingLayer::DecodingLayer(Device *device)
    : device(device) {
}

DecodingLayer::~DecodingLayer() {
}

#if 0
void DecodingLayer::PutFrame(Buffer<uint8_t> *frame) {
    //current_frame = new ImageBuffer(ColorSpace::BGRA, width, height, frame, true);
}
#endif

ImageBuffer *DecodingLayer::GetFrame() {
    return current_frame;
}

DecodingLayer *DecodingLayer::Create(Device *device) {
#ifndef IS_CODEGEN
#ifdef _WIN32
    return new Win32DecodingLayer(device);
#elif defined(__APPLE__)
    return new VTDecodingLayer(device);
#endif
#else
    return nullptr;
#endif
}
