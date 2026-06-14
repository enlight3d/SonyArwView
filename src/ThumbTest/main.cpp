// ThumbTest - directly exercise the registered IThumbnailProvider COM object,
// exactly like Explorer's thumbnail host does, and save the result to a PNG.
//
// Usage:
//   ThumbTest.exe input.ARW output.png [cx]
//
// This is the source of truth for the thumbnail provider (analogous to
// WicDecodeTest --force-clsid for the decoder).

#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>      // SHCreateItemFromParsingName, IShellItemImageFactory
#include <thumbcache.h>
#include <propsys.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>

#include <initguid.h>
#include "guids.h"   // CLSID_SonyArwThumbnailProvider

using Microsoft::WRL::ComPtr;

static HRESULT SaveHbitmapToPng(HBITMAP hbmp, const wchar_t* outPath) {
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmap> bmp;
    hr = factory->CreateBitmapFromHBITMAP(hbmp, nullptr, WICBitmapIgnoreAlpha, &bmp);
    if (FAILED(hr)) return hr;

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) return hr;
    hr = stream->InitializeFromFilename(outPath, GENERIC_WRITE);
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (FAILED(hr)) return hr;
    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&frame, &props);
    if (FAILED(hr)) return hr;
    hr = frame->Initialize(props.Get());
    if (FAILED(hr)) return hr;
    hr = frame->WriteSource(bmp.Get(), nullptr);
    if (FAILED(hr)) return hr;
    hr = frame->Commit();
    if (FAILED(hr)) return hr;
    return encoder->Commit();
}

// Test the REAL shell thumbnail pipeline (what Explorer uses internally), via
// IShellItemImageFactory::GetImage with SIIGBF_THUMBNAILONLY. This resolves and
// invokes whatever thumbnail handler the shell associates with .arw.
static int RunShellMode(const wchar_t* input, const wchar_t* output, UINT cx) {
    ComPtr<IShellItem> item;
    HRESULT hr = SHCreateItemFromParsingName(input, nullptr, IID_PPV_ARGS(&item));
    if (FAILED(hr)) { wprintf(L"SHCreateItemFromParsingName failed: 0x%08X\n", (unsigned)hr); return 2; }

    ComPtr<IShellItemImageFactory> imgFactory;
    hr = item.As(&imgFactory);
    if (FAILED(hr)) { wprintf(L"QI IShellItemImageFactory failed: 0x%08X\n", (unsigned)hr); return 2; }

    SIZE sz = { static_cast<LONG>(cx), static_cast<LONG>(cx) };
    HBITMAP hbmp = nullptr;
    // THUMBNAILONLY => fail rather than returning a generic file-type icon.
    hr = imgFactory->GetImage(sz, SIIGBF_THUMBNAILONLY, &hbmp);
    if (FAILED(hr) || !hbmp) {
        wprintf(L"shell GetImage(THUMBNAILONLY) failed: 0x%08X\n"
                L"(the shell did NOT produce a thumbnail for this file)\n", (unsigned)hr);
        return 3;
    }
    BITMAP bm = {};
    GetObject(hbmp, sizeof(bm), &bm);
    wprintf(L"shell thumbnail OK: %ldx%ld\n", bm.bmWidth, bm.bmHeight);
    hr = SaveHbitmapToPng(hbmp, output);
    DeleteObject(hbmp);
    if (FAILED(hr)) { wprintf(L"save PNG failed: 0x%08X\n", (unsigned)hr); return 4; }
    wprintf(L"wrote %s\n", output);
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        wprintf(L"Usage:\n"
                L"  ThumbTest.exe input.ARW output.png [cx]            (direct COM call)\n"
                L"  ThumbTest.exe --shell input.ARW output.png [cx]    (real shell pipeline)\n");
        return 1;
    }

    if (wcscmp(argv[1], L"--shell") == 0) {
        if (argc < 4) { wprintf(L"need input and output\n"); return 1; }
        HRESULT hrc = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hrc)) { wprintf(L"CoInitializeEx failed\n"); return 1; }
        UINT scx = (argc >= 5) ? static_cast<UINT>(_wtoi(argv[4])) : 256;
        int rc = RunShellMode(argv[2], argv[3], scx);
        CoUninitialize();
        return rc;
    }

    const wchar_t* input = argv[1];
    const wchar_t* output = argv[2];
    UINT cx = (argc >= 4) ? static_cast<UINT>(_wtoi(argv[3])) : 256;

    // Thumbnail providers expect a single-threaded apartment.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) { wprintf(L"CoInitializeEx failed\n"); return 1; }

    ComPtr<IInitializeWithStream> init;
    hr = CoCreateInstance(CLSID_SonyArwThumbnailProvider, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&init));
    if (FAILED(hr)) {
        wprintf(L"CoCreateInstance(thumbnail provider) failed: 0x%08X\n"
                     L"(is it registered? run scripts\\register-thumbnail.ps1)\n",
                     (unsigned)hr);
        return 2;
    }

    ComPtr<IStream> stream;
    hr = SHCreateStreamOnFileEx(input, STGM_READ | STGM_SHARE_DENY_WRITE,
                                FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
    if (FAILED(hr)) { wprintf(L"open input failed: 0x%08X\n", (unsigned)hr); return 2; }

    hr = init->Initialize(stream.Get(), 0);
    if (FAILED(hr)) { wprintf(L"Initialize failed: 0x%08X\n", (unsigned)hr); return 2; }

    ComPtr<IThumbnailProvider> tp;
    hr = init.As(&tp);
    if (FAILED(hr)) { wprintf(L"QI IThumbnailProvider failed: 0x%08X\n", (unsigned)hr); return 2; }

    HBITMAP hbmp = nullptr;
    WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
    hr = tp->GetThumbnail(cx, &hbmp, &alpha);
    if (FAILED(hr) || !hbmp) {
        wprintf(L"GetThumbnail failed: 0x%08X\n", (unsigned)hr);
        return 3;
    }

    BITMAP bm = {};
    GetObject(hbmp, sizeof(bm), &bm);
    wprintf(L"GetThumbnail OK: %ldx%ld, alpha=%d\n", bm.bmWidth, bm.bmHeight, (int)alpha);

    hr = SaveHbitmapToPng(hbmp, output);
    DeleteObject(hbmp);
    if (FAILED(hr)) { wprintf(L"save PNG failed: 0x%08X\n", (unsigned)hr); return 4; }
    wprintf(L"wrote %s\n", output);
    return 0;
}
