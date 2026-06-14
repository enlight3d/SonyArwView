// WicFrameDecode.cpp
#include "WicFrameDecode.h"
#include "DllCommon.h"
#include "Debug.h"

#include <utility>  // std::move

using Microsoft::WRL::ComPtr;

SonyArwFrameDecode::SonyArwFrameDecode(ComPtr<IWICBitmapFrameDecode> inner) noexcept
    : m_ref(1), m_inner(std::move(inner)) {
    DllAddRef();
}

SonyArwFrameDecode::~SonyArwFrameDecode() {
    DllRelease();
}

// ---- IUnknown --------------------------------------------------------------
IFACEMETHODIMP SonyArwFrameDecode::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, __uuidof(IWICBitmapSource)) ||
        IsEqualIID(riid, __uuidof(IWICBitmapFrameDecode))) {
        *ppv = static_cast<IWICBitmapFrameDecode*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) SonyArwFrameDecode::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

IFACEMETHODIMP_(ULONG) SonyArwFrameDecode::Release() {
    const LONG c = InterlockedDecrement(&m_ref);
    if (c == 0) delete this;
    return static_cast<ULONG>(c);
}

// ---- IWICBitmapSource (all delegated to the inner JPEG frame) ---------------
IFACEMETHODIMP SonyArwFrameDecode::GetSize(UINT* w, UINT* h) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    return m_inner->GetSize(w, h);
}

IFACEMETHODIMP SonyArwFrameDecode::GetPixelFormat(WICPixelFormatGUID* fmt) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    return m_inner->GetPixelFormat(fmt);
}

IFACEMETHODIMP SonyArwFrameDecode::GetResolution(double* dpiX, double* dpiY) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    return m_inner->GetResolution(dpiX, dpiY);
}

IFACEMETHODIMP SonyArwFrameDecode::CopyPalette(IWICPalette* pal) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    return m_inner->CopyPalette(pal);
}

IFACEMETHODIMP SonyArwFrameDecode::CopyPixels(const WICRect* prc, UINT stride,
                                              UINT bufSize, BYTE* buf) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    if (dbg::Enabled()) {
        if (prc) {
            dbg::Logf(L"CopyPixels rect=(%d,%d %dx%d) stride=%u size=%u",
                      prc->X, prc->Y, prc->Width, prc->Height, stride, bufSize);
        } else {
            dbg::Logf(L"CopyPixels full-frame stride=%u size=%u", stride, bufSize);
        }
    }
    return m_inner->CopyPixels(prc, stride, bufSize, buf);
}

// ---- IWICBitmapFrameDecode -------------------------------------------------
IFACEMETHODIMP SonyArwFrameDecode::GetMetadataQueryReader(
        IWICMetadataQueryReader** reader) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    // Delegate to the JPEG frame's EXIF/IFD metadata reader where available.
    return m_inner->GetMetadataQueryReader(reader);
}

IFACEMETHODIMP SonyArwFrameDecode::GetColorContexts(UINT cCount,
        IWICColorContext** contexts, UINT* actual) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    return m_inner->GetColorContexts(cCount, contexts, actual);
}

IFACEMETHODIMP SonyArwFrameDecode::GetThumbnail(IWICBitmapSource** thumb) {
    if (!m_inner) return WINCODEC_ERR_NOTINITIALIZED;
    // The embedded preview JPEG may itself carry an EXIF thumbnail; hand it back
    // if present, otherwise report none.
    return m_inner->GetThumbnail(thumb);
}
