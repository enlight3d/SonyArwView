// ThumbnailProvider.cpp
#include "ThumbnailProvider.h"
#include "DllCommon.h"
#include "Debug.h"

#include "ArwPreviewExtractor.h"

#include <wincodec.h>
#include <shlwapi.h>     // SHCreateMemStream
#include <new>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

SonyArwThumbnailProvider::SonyArwThumbnailProvider() noexcept : m_ref(1) {
    DllAddRef();
    dbg::Logf(L"ThumbnailProvider constructed (%p)", this);
}

SonyArwThumbnailProvider::~SonyArwThumbnailProvider() {
    dbg::Logf(L"ThumbnailProvider destroyed (%p)", this);
    DllRelease();
}

// ---- IUnknown --------------------------------------------------------------
IFACEMETHODIMP SonyArwThumbnailProvider::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, __uuidof(IInitializeWithStream))) {
        *ppv = static_cast<IInitializeWithStream*>(this);
    } else if (IsEqualIID(riid, __uuidof(IThumbnailProvider))) {
        *ppv = static_cast<IThumbnailProvider*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) SonyArwThumbnailProvider::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

IFACEMETHODIMP_(ULONG) SonyArwThumbnailProvider::Release() {
    const LONG c = InterlockedDecrement(&m_ref);
    if (c == 0) delete this;
    return static_cast<ULONG>(c);
}

// ---- IInitializeWithStream -------------------------------------------------
IFACEMETHODIMP SonyArwThumbnailProvider::Initialize(IStream* pStream, DWORD /*grfMode*/) {
    if (pStream == nullptr) return E_INVALIDARG;
    if (m_stream) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    m_stream = pStream; // ComPtr AddRefs
    dbg::Logf(L"Initialize: stream stored");
    return S_OK;
}

// ---- helpers ---------------------------------------------------------------
namespace {

// Map an EXIF orientation (1..8) to a WIC flip/rotate transform.
WICBitmapTransformOptions OrientationToTransform(UINT orientation) {
    switch (orientation) {
        case 2: return WICBitmapTransformFlipHorizontal;
        case 3: return WICBitmapTransformRotate180;
        case 4: return WICBitmapTransformFlipVertical;
        case 5: return static_cast<WICBitmapTransformOptions>(
                    WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
        case 6: return WICBitmapTransformRotate90;
        case 7: return static_cast<WICBitmapTransformOptions>(
                    WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
        case 8: return WICBitmapTransformRotate270;
        case 1:
        default: return WICBitmapTransformRotate0;
    }
}

// Decode a JPEG blob, apply orientation, and scale into a top-down 32bpp BGRA
// HBITMAP that fits within a cx-by-cx box (aspect-preserving). `arwOrientation`
// is the EXIF orientation read from the ARW container (the preview JPEG itself
// usually carries none); if it is 1 we fall back to any JPEG-embedded EXIF.
HRESULT JpegToHBITMAP(IWICImagingFactory* factory, const uint8_t* data, size_t size,
                      UINT cx, UINT arwOrientation, HBITMAP* phbmp) {
    *phbmp = nullptr;

    ComPtr<IStream> mem;
    mem.Attach(SHCreateMemStream(data, static_cast<UINT>(size)));
    if (!mem) return E_OUTOFMEMORY;

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromStream(mem.Get(), nullptr,
                    WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return hr;

    // Honour orientation (from the ARW container) so portrait shots aren't shown
    // sideways. We SCALE first (on the JPEG frame, which streams efficiently),
    // then materialise the small result and flip/rotate THAT. Chaining a scaler
    // directly onto a flip-rotator is unstable, so we deliberately avoid it.
    UINT orientation = arwOrientation;
    if (orientation < 1 || orientation > 8) orientation = 1;
    const bool swapWH = (orientation >= 5); // orientations 5..8 are 90/270 rotations

    UINT sw = 0, sh = 0;
    hr = frame->GetSize(&sw, &sh);
    if (FAILED(hr) || sw == 0 || sh == 0) return E_FAIL;

    // Display (post-rotation) dimensions, used to compute the target box fit.
    const UINT dw = swapWH ? sh : sw;
    const UINT dh = swapWH ? sw : sh;
    UINT tw = cx, th = cx;
    if (dw >= dh) {
        th = static_cast<UINT>((static_cast<unsigned long long>(cx) * dh + dw / 2) / dw);
    } else {
        tw = static_cast<UINT>((static_cast<unsigned long long>(cx) * dw + dh / 2) / dh);
    }
    if (tw == 0) tw = 1;
    if (th == 0) th = 1;

    // Pre-rotation scaled size (in the frame's native orientation).
    const UINT pw = swapWH ? th : tw;
    const UINT ph = swapWH ? tw : th;

    ComPtr<IWICBitmapScaler> scaler;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) return hr;
    hr = scaler->Initialize(frame.Get(), pw, ph, WICBitmapInterpolationModeFant);
    if (FAILED(hr)) return hr;

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return hr;
    hr = converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return hr;

    // Final source: the scaled BGRA image, flip/rotated if needed. Materialise
    // the scaled image first so the flip-rotator works on a real in-memory
    // bitmap (robust) instead of a streaming pipeline.
    ComPtr<IWICBitmapSource> finalSrc;
    if (orientation == 1) {
        finalSrc = converter;
    } else {
        ComPtr<IWICBitmap> materialised;
        hr = factory->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad,
                                             &materialised);
        if (FAILED(hr)) return hr;
        ComPtr<IWICBitmapFlipRotator> rotator;
        hr = factory->CreateBitmapFlipRotator(&rotator);
        if (FAILED(hr)) return hr;
        hr = rotator->Initialize(materialised.Get(), OrientationToTransform(orientation));
        if (FAILED(hr)) return hr;
        finalSrc = rotator;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = static_cast<LONG>(tw);
    bmi.bmiHeader.biHeight      = -static_cast<LONG>(th); // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hbmp == nullptr || bits == nullptr) return E_OUTOFMEMORY;

    const UINT stride = tw * 4;
    const UINT bufSize = stride * th;
    hr = finalSrc->CopyPixels(nullptr, stride, bufSize, static_cast<BYTE*>(bits));
    if (FAILED(hr)) {
        DeleteObject(hbmp);
        return hr;
    }

    *phbmp = hbmp;
    dbg::Logf(L"JpegToHBITMAP: src %ux%u orient=%u -> %ux%u (cx=%u)",
              sw, sh, orientation, tw, th, cx);
    return S_OK;
}

} // namespace

// ---- IThumbnailProvider ----------------------------------------------------
IFACEMETHODIMP SonyArwThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp,
                                                      WTS_ALPHATYPE* pdwAlpha) {
    if (phbmp == nullptr || pdwAlpha == nullptr) return E_INVALIDARG;
    *phbmp = nullptr;
    *pdwAlpha = WTSAT_UNKNOWN;
    if (!m_stream) return E_UNEXPECTED;
    if (cx == 0) return E_INVALIDARG;

    try {
        // 1. Extract the embedded JPEG preview from the ARW stream.
        std::vector<uint8_t> jpeg;
        arw::ExtractResult res;
        HRESULT hr = arw::ExtractEmbeddedJpegFromStream(m_stream.Get(), jpeg, &res);
        if (FAILED(hr) || !res.success || jpeg.empty()) {
            dbg::Logf(L"GetThumbnail: extraction failed (hr=0x%08X)", (unsigned)hr);
            return WINCODEC_ERR_COMPONENTNOTFOUND;
        }

        // 2. Decode + scale to an HBITMAP using WIC.
        ComPtr<IWICImagingFactory> factory;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory));
        if (FAILED(hr)) { dbg::LogHr(L"WIC factory", hr); return hr; }

        hr = JpegToHBITMAP(factory.Get(), jpeg.data(), jpeg.size(), cx,
                           res.orientation, phbmp);
        if (FAILED(hr)) { dbg::LogHr(L"JpegToHBITMAP", hr); return hr; }

        // The preview is opaque; tell the shell to treat it as RGB (no alpha).
        *pdwAlpha = WTSAT_RGB;
        dbg::Logf(L"GetThumbnail OK (cx=%u, %ux%u source)", cx, res.width, res.height);
        return S_OK;
    }
    catch (...) {
        dbg::Logf(L"GetThumbnail: exception");
        if (*phbmp) { DeleteObject(*phbmp); *phbmp = nullptr; }
        return E_FAIL;
    }
}
