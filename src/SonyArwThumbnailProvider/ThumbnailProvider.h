// ThumbnailProvider.h
//
// IThumbnailProvider + IInitializeWithStream for Sony .ARW. The shell hands us
// the file stream; we extract the embedded JPEG preview, decode it with WIC,
// scale it to the requested size, and return an HBITMAP. No RAW decoding.
#pragma once

#include <windows.h>
#include <thumbcache.h>   // IThumbnailProvider, WTS_ALPHATYPE
#include <propsys.h>      // IInitializeWithStream
#include <wrl/client.h>

class SonyArwThumbnailProvider : public IInitializeWithStream,
                                 public IThumbnailProvider {
public:
    SonyArwThumbnailProvider() noexcept;
    virtual ~SonyArwThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStream, DWORD grfMode) override;

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override;

private:
    volatile LONG                   m_ref;
    Microsoft::WRL::ComPtr<IStream> m_stream;
};
