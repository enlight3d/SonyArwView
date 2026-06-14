// WicDecodeTest - exercise the WIC decode path without Photos/Explorer.
//
// Usage:
//   WicDecodeTest.exe input.ARW output.png        Decode via WIC arbitration
//   WicDecodeTest.exe --force-clsid input.ARW out.png   Force OUR decoder CLSID
//   WicDecodeTest.exe --list-decoders             Enumerate registered WIC decoders
//
// This is the source of truth for whether the WIC decoder is correctly
// registered and functional. It prints which decoder CLSID was selected.

#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>

// Pull in our decoder's CLSID. Include <initguid.h> right before guids.h so
// only our GUIDs get storage in this TU (wincodec's GUIDs stay in uuid.lib).
#include <initguid.h>
#include "guids.h"

using Microsoft::WRL::ComPtr;

namespace {

struct ComInit {
    bool ok = false;
    ComInit() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
    ~ComInit() { if (ok) CoUninitialize(); }
};

std::wstring GuidToString(const GUID& g) {
    wchar_t buf[64] = {};
    StringFromGUID2(g, buf, 64);
    return buf;
}

// Map a known pixel-format GUID to a readable name (best effort).
const wchar_t* PixelFormatName(const GUID& f) {
    if (f == GUID_WICPixelFormat24bppBGR)  return L"24bppBGR";
    if (f == GUID_WICPixelFormat32bppBGRA) return L"32bppBGRA";
    if (f == GUID_WICPixelFormat32bppBGR)  return L"32bppBGR";
    if (f == GUID_WICPixelFormat8bppGray)  return L"8bppGray";
    if (f == GUID_WICPixelFormat48bppRGB)  return L"48bppRGB";
    return L"(other)";
}

void PrintDecoderInfo(IWICBitmapDecoder* decoder) {
    ComPtr<IWICBitmapDecoderInfo> info;
    if (FAILED(decoder->GetDecoderInfo(&info)) || !info) {
        std::wprintf(L"  (decoder info unavailable)\n");
        return;
    }
    CLSID clsid = {};
    info->GetCLSID(&clsid);
    UINT len = 0;
    wchar_t name[256] = {};
    info->GetFriendlyName(256, name, &len);
    std::wprintf(L"  selected decoder: %s  CLSID=%s\n", name, GuidToString(clsid).c_str());
}

// Encode an IWICBitmapSource to PNG or JPEG (chosen by output extension).
HRESULT SaveSource(IWICImagingFactory* factory, IWICBitmapSource* source,
                   const wchar_t* outPath) {
    std::wstring path(outPath);
    GUID container = GUID_ContainerFormatPng;
    const size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        std::wstring ext = path.substr(dot);
        for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
        if (ext == L".jpg" || ext == L".jpeg") container = GUID_ContainerFormatJpeg;
    }

    // Convert to 24bppBGR so both PNG and JPEG encoders accept it reliably.
    ComPtr<IWICBitmapSource> converted;
    HRESULT hr = WICConvertBitmapSource(GUID_WICPixelFormat24bppBGR, source,
                                        &converted);
    if (FAILED(hr)) return hr;

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) return hr;
    hr = stream->InitializeFromFilename(outPath, GENERIC_WRITE);
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(container, nullptr, &encoder);
    if (FAILED(hr)) return hr;
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) return hr;
    hr = frame->Initialize(props.Get());
    if (FAILED(hr)) return hr;

    hr = frame->WriteSource(converted.Get(), nullptr);
    if (FAILED(hr)) return hr;
    hr = frame->Commit();
    if (FAILED(hr)) return hr;
    return encoder->Commit();
}

int DecodeAndSave(ComPtr<IWICBitmapDecoder> decoder, IWICImagingFactory* factory,
                  const wchar_t* output) {
    PrintDecoderInfo(decoder.Get());

    UINT frameCount = 0;
    decoder->GetFrameCount(&frameCount);
    std::wprintf(L"  frame count: %u\n", frameCount);

    ComPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { std::wprintf(L"GetFrame(0) failed: 0x%08X\n", (unsigned)hr); return 2; }

    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    WICPixelFormatGUID fmt = {};
    frame->GetPixelFormat(&fmt);
    double dpiX = 0, dpiY = 0;
    frame->GetResolution(&dpiX, &dpiY);
    std::wprintf(L"  image: %ux%u  pixelFormat=%s %s  dpi=%.0fx%.0f\n",
                 w, h, PixelFormatName(fmt), GuidToString(fmt).c_str(), dpiX, dpiY);

    hr = SaveSource(factory, frame.Get(), output);
    if (FAILED(hr)) { std::wprintf(L"Encode failed: 0x%08X\n", (unsigned)hr); return 3; }
    std::wprintf(L"  wrote: %s\n", output);
    return 0;
}

int ListDecoders(IWICImagingFactory* factory) {
    ComPtr<IEnumUnknown> en;
    HRESULT hr = factory->CreateComponentEnumerator(WICDecoder,
        WICComponentEnumerateDefault, &en);
    if (FAILED(hr)) { std::wprintf(L"enumerator failed: 0x%08X\n", (unsigned)hr); return 2; }

    std::wprintf(L"Registered WIC decoders:\n");
    int count = 0;
    ComPtr<IUnknown> unk;
    ULONG fetched = 0;
    while (en->Next(1, &unk, &fetched) == S_OK && fetched == 1) {
        ComPtr<IWICBitmapDecoderInfo> info;
        if (SUCCEEDED(unk.As(&info)) && info) {
            CLSID clsid = {};
            info->GetCLSID(&clsid);
            wchar_t name[256] = {}; UINT len = 0;
            info->GetFriendlyName(256, name, &len);
            wchar_t exts[256] = {}; UINT elen = 0;
            info->GetFileExtensions(256, exts, &elen);
            std::wprintf(L"  [%2d] %-45s %s  ext=%s\n",
                         ++count, name, GuidToString(clsid).c_str(), exts);
        }
        unk.Reset();
    }
    std::wprintf(L"Total: %d decoder(s).\n", count);
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wprintf(L"Usage:\n"
                     L"  WicDecodeTest.exe <input.ARW> <output.png|.jpg>\n"
                     L"  WicDecodeTest.exe --force-clsid <input.ARW> <output.png|.jpg>\n"
                     L"  WicDecodeTest.exe --list-decoders\n");
        return 1;
    }

    ComInit com;
    if (!com.ok) { std::wprintf(L"CoInitializeEx failed.\n"); return 1; }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { std::wprintf(L"WIC factory failed: 0x%08X\n", (unsigned)hr); return 1; }

    const std::wstring mode = argv[1];

    if (mode == L"--list-decoders") {
        return ListDecoders(factory.Get());
    }

    if (mode == L"--force-clsid") {
        if (argc < 4) { std::wprintf(L"need input and output\n"); return 1; }
        const wchar_t* input = argv[2];
        const wchar_t* output = argv[3];

        // Instantiate OUR decoder directly, bypassing WIC arbitration.
        ComPtr<IWICBitmapDecoder> decoder;
        hr = CoCreateInstance(CLSID_SonyArwDecoder, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&decoder));
        if (FAILED(hr)) {
            std::wprintf(L"CoCreateInstance(CLSID_SonyArwDecoder) failed: 0x%08X\n"
                         L"(is the decoder registered? run scripts\\register-wic.ps1)\n",
                         (unsigned)hr);
            return 2;
        }
        ComPtr<IStream> stream;
        hr = SHCreateStreamOnFileEx(input, STGM_READ | STGM_SHARE_DENY_WRITE,
                                    FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
        if (FAILED(hr)) { std::wprintf(L"open input failed: 0x%08X\n", (unsigned)hr); return 2; }
        hr = decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand);
        if (FAILED(hr)) { std::wprintf(L"decoder Initialize failed: 0x%08X\n", (unsigned)hr); return 2; }
        std::wprintf(L"Forced CLSID_SonyArwDecoder.\n");
        return DecodeAndSave(decoder, factory.Get(), output);
    }

    // Default: decode through WIC arbitration (this is what Photos/Explorer do).
    if (argc < 3) { std::wprintf(L"need input and output\n"); return 1; }
    const wchar_t* input = argv[1];
    const wchar_t* output = argv[2];

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(input, nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        std::wprintf(L"CreateDecoderFromFilename failed: 0x%08X\n"
                     L"(no registered WIC decoder accepted this file)\n", (unsigned)hr);
        return 2;
    }
    std::wprintf(L"Decoded via WIC arbitration.\n");
    return DecodeAndSave(decoder, factory.Get(), output);
}
