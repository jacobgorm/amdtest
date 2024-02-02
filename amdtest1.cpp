#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


class DecoderImpl;
class Device;
class ImageBuffer;

#include "win32decodinglayer.h"

#include <d3dx12.h>
#include <dxgi1_4.h>
#include <err.h>

static IDXGIFactory4 *s_factory;
static ID3D12Device *s_device;
static IDXGIAdapter1 *s_dxgi_adapter;

IDXGIAdapter1 *GetD3DAdapter() {
    assert(s_dxgi_adapter);
    return s_dxgi_adapter;
}

IDXGIFactory4 *GetD3DFactory() {
    assert(s_factory);
    return s_factory;
}

ID3D12Device *GetD3DDevice() {
    assert(s_device);
    return s_device;
}

#define CHECK(_hr) \
    if (FAILED(_hr)) { \
        const char *msg = nullptr; \
        switch (_hr) { \
            case E_FAIL: \
                msg = "E_FAIL"; \
                break; \
            case E_OUTOFMEMORY: \
                msg = "E_OUTOFMEMORY"; \
                break; \
            case E_INVALIDARG: \
                msg = "E_INVALIDARG"; \
                break; \
            case DXGI_ERROR_DEVICE_REMOVED: \
                msg = "DXGI_ERROR_DEVICE_REMOVED"; \
                warnx("device removed %s line %d, hr=%x : %s\n", __FUNCTION__, __LINE__, (uint32_t) hr, msg); \
                break; \
            case DXGI_ERROR_INVALID_CALL: \
                msg = "DXGI_ERROR_INVALID_CALL"; \
                break; \
            default: \
                msg = "unknown error"; \
                break; \
        } \
        errx(1, "failed %s line %d, hr=%x : %s\n", __FUNCTION__, __LINE__, (uint32_t) hr, msg); \
    }


static void InitD3D() {

    HRESULT hr;

#if 0
    // In debug mode
    ID3D12Debug *debugController;
    if (D3D12GetDebugInterface(IID_ID3D12Debug, (void**)&debugController) >= 0) {
        debugController->EnableDebugLayer();
    }
#endif

    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&s_factory));
    CHECK(hr);

    IDXGIAdapter *dxgiAdapter = nullptr;
    for (int i = 0; ; ++i) {
        hr = s_factory->EnumAdapters(i, &dxgiAdapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        CHECK(hr);

        ID3D12Device *device = nullptr;
        hr = D3D12CreateDevice((IUnknown *) dxgiAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));

        if (hr == S_OK) {
            IDXGIAdapter1 *dxgi_adapter;
            hr = dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter));
            CHECK(hr);

            DXGI_ADAPTER_DESC desc = {};
            hr = dxgi_adapter->GetDesc(&desc);
            CHECK(hr);
            printf("%s found d3d adapter #%d \"%ws\"\n", __PRETTY_FUNCTION__, i, desc.Description);

            if (wcsstr(desc.Description, L"Basic Render Driver")) {
                dxgi_adapter->Release();
                device->Release();
                continue;
            } else {
                s_dxgi_adapter = dxgi_adapter;
                s_device = device;
                break;
            }
        }
        if (hr == DXGI_ERROR_UNSUPPORTED) continue;
        CHECK(hr);
    }

    LARGE_INTEGER version;
    hr = s_dxgi_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version);
    if (SUCCEEDED(hr)) {
        printf("%s driver version %u.%u\n", __FUNCTION__, (uint32_t) version.HighPart, (uint32_t) version.LowPart);
    }

}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc < 2) {
        errx(1, "usage: %s input-video", argv[0]);
    }

    InitD3D();

    char *video = argv[1];
    FILE *f = fopen(video, "rb");
    if (!f) {
        err(1, "unable to open %s", video);
    }

    auto dl = new Win32DecodingLayer(nullptr);

    size_t max_buffer = 0x200000;
    auto buffer = new uint8_t[max_buffer];
    for (;;) {
        int r = fread(buffer, 1, max_buffer, f);
        if (r == 0) {
            break;
        }
        dl->ReceiveBytes(buffer, r);
    }
    fclose(f);

    return 0;
}
