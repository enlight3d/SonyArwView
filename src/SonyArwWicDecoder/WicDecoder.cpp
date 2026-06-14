// WicDecoder.cpp
#include "WicDecoder.h"
#include "WicFrameDecode.h"
#include "DllCommon.h"
#include "Debug.h"
#include "guids.h"

#include "ArwPreviewExtractor.h"

#include <shlwapi.h>  // SHCreateMemStream
#include <new>

using Microsoft::WRL::ComPtr;

SonyArwDecoder::SonyArwDecoder() noexcept
    : m_ref(1), m_initialized(false) {
    DllAddRef();
    dbg::Logf(L"SonyArwDecoder constructed (%p)", this);
}

SonyArwDecoder::~SonyArwDecoder() {
    dbg::Logf(L"SonyArwDecoder destroyed (%p)", this);
    DllRelease();
}

HRESULT SonyArwDecoder::EnsureFactory() {
    if (m_factory) return S_OK;
    const HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                        CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&m_factory));
    if (FAILED(hr)) dbg::LogHr(L"CoCreateInstance(WICImagingFactory)", hr);
    return hr;
}

// ---- IUnknown --------------------------------------------------------------
IFACEMETHODIMP SonyArwDecoder::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, __uuidof(IWICBitmapDecoder))) {
        *ppv = static_cast<IWICBitmapDecoder*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) SonyArwDecoder::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

IFACEMETHODIMP_(ULONG) SonyArwDecoder::Release() {
    const LONG c = InterlockedDecrement(&m_ref);
    if (c == 0) delete this;
    return static_cast<ULONG>(c);
}

// ---- IWICBitmapDecoder -----------------------------------------------------

// Inspect a stream and report whether we can decode it. We claim the file only
// if we can actually locate and validate an embedded JPEG preview. We carefully
// restore the stream position so we don't disturb other decoders' probing.
IFACEMETHODIMP SonyArwDecoder::QueryCapability(IStream* pIStream, DWORD* pCapability) {
    if (pCapability == nullptr) return E_INVALIDARG;
    *pCapability = 0;
    if (pIStream == nullptr) return E_INVALIDARG;

    // Save the caller's current position.
    LARGE_INTEGER zero = {};
    ULARGE_INTEGER savedPos = {};
    pIStream->Seek(zero, STREAM_SEEK_CUR, &savedPos);

    std::vector<uint8_t> jpeg;
    arw::ExtractResult res;
    const HRESULT hr = arw::ExtractEmbeddedJpegFromStream(pIStream, jpeg, &res);

    // Restore position regardless of outcome.
    LARGE_INTEGER restore;
    restore.QuadPart = static_cast<LONGLONG>(savedPos.QuadPart);
    pIStream->Seek(restore, STREAM_SEEK_SET, nullptr);

    if (SUCCEEDED(hr) && res.success) {
        *pCapability = WICBitmapDecoderCapabilityCanDecodeAllImages |
                       WICBitmapDecoderCapabilityCanDecodeSomeImages;
        dbg::Logf(L"QueryCapability: CLAIM (%ux%u, %s)", res.width, res.height,
                  arw::ToString(res.method));
    } else {
        dbg::Logf(L"QueryCapability: decline (hr=0x%08X)", static_cast<unsigned>(hr));
    }
    return S_OK;
}

IFACEMETHODIMP SonyArwDecoder::Initialize(IStream* pIStream, WICDecodeOptions cacheOptions) {
    if (pIStream == nullptr) return E_INVALIDARG;

    HRESULT hr = EnsureFactory();
    if (FAILED(hr)) return hr;

    arw::ExtractResult res;
    hr = arw::ExtractEmbeddedJpegFromStream(pIStream, m_jpegBytes, &res);
    if (FAILED(hr) || !res.success || m_jpegBytes.empty()) {
        dbg::Logf(L"Initialize: extraction failed (hr=0x%08X)", static_cast<unsigned>(hr));
        return WINCODEC_ERR_BADIMAGE;
    }
    dbg::Logf(L"Initialize: extracted preview %ux%u, %zu bytes via %s",
              res.width, res.height, m_jpegBytes.size(), arw::ToString(res.method));

    // SHCreateMemStream copies the bytes into a self-owned stream, so the inner
    // JPEG decoder (and the frames it hands out) stay valid independent of our
    // object's lifetime / the original m_jpegBytes buffer.
    ComPtr<IStream> mem;
    mem.Attach(SHCreateMemStream(m_jpegBytes.data(),
                                 static_cast<UINT>(m_jpegBytes.size())));
    if (!mem) {
        return E_OUTOFMEMORY;
    }
    // The stream owns its own copy now; release our large buffer.
    m_jpegBytes.clear();
    m_jpegBytes.shrink_to_fit();
    m_jpegStream = mem;

    hr = m_factory->CreateDecoderFromStream(m_jpegStream.Get(), nullptr,
                                            cacheOptions, &m_inner);
    if (FAILED(hr)) {
        dbg::LogHr(L"CreateDecoderFromStream(JPEG)", hr);
        return hr;
    }

    m_initialized = true;
    return S_OK;
}

IFACEMETHODIMP SonyArwDecoder::GetContainerFormat(GUID* pguid) {
    if (pguid == nullptr) return E_INVALIDARG;
    *pguid = GUID_ContainerFormatSonyArw;
    return S_OK;
}

IFACEMETHODIMP SonyArwDecoder::GetDecoderInfo(IWICBitmapDecoderInfo** ppInfo) {
    if (ppInfo == nullptr) return E_INVALIDARG;
    *ppInfo = nullptr;
    HRESULT hr = EnsureFactory();
    if (FAILED(hr)) return hr;

    ComPtr<IWICComponentInfo> ci;
    hr = m_factory->CreateComponentInfo(CLSID_SonyArwDecoder, &ci);
    if (FAILED(hr)) return hr;
    return ci->QueryInterface(IID_PPV_ARGS(ppInfo));
}

IFACEMETHODIMP SonyArwDecoder::CopyPalette(IWICPalette*) {
    return WINCODEC_ERR_PALETTEUNAVAILABLE;
}

IFACEMETHODIMP SonyArwDecoder::GetMetadataQueryReader(IWICMetadataQueryReader** reader) {
    if (reader == nullptr) return E_INVALIDARG;
    *reader = nullptr;
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    return m_inner->GetMetadataQueryReader(reader);
}

IFACEMETHODIMP SonyArwDecoder::GetPreview(IWICBitmapSource** ppSrc) {
    if (ppSrc) *ppSrc = nullptr;
    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

IFACEMETHODIMP SonyArwDecoder::GetColorContexts(UINT cCount, IWICColorContext** ctx,
                                                UINT* actual) {
    if (!m_inner) {
        if (actual) *actual = 0;
        return WINCODEC_ERR_NOTINITIALIZED;
    }
    return m_inner->GetColorContexts(cCount, ctx, actual);
}

IFACEMETHODIMP SonyArwDecoder::GetThumbnail(IWICBitmapSource** ppThumb) {
    if (ppThumb == nullptr) return E_INVALIDARG;
    *ppThumb = nullptr;
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    // Surface the preview JPEG's embedded EXIF thumbnail, if it has one.
    return m_inner->GetThumbnail(ppThumb);
}

IFACEMETHODIMP SonyArwDecoder::GetFrameCount(UINT* pCount) {
    if (pCount == nullptr) return E_INVALIDARG;
    *pCount = 1; // we expose exactly one frame: the embedded preview
    return S_OK;
}

IFACEMETHODIMP SonyArwDecoder::GetFrame(UINT index, IWICBitmapFrameDecode** ppFrame) {
    if (ppFrame == nullptr) return E_INVALIDARG;
    *ppFrame = nullptr;
    if (!m_initialized || !m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    if (index != 0) return WINCODEC_ERR_FRAMEMISSING;

    ComPtr<IWICBitmapFrameDecode> innerFrame;
    HRESULT hr = m_inner->GetFrame(0, &innerFrame);
    if (FAILED(hr)) {
        dbg::LogHr(L"inner GetFrame(0)", hr);
        return hr;
    }

    auto* wrapper = new (std::nothrow) SonyArwFrameDecode(innerFrame);
    if (wrapper == nullptr) return E_OUTOFMEMORY;
    *ppFrame = wrapper; // ctor set ref count to 1
    dbg::Logf(L"GetFrame(0): returned wrapper frame");
    return S_OK;
}
