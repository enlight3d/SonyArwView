// WicFrameDecode.h
//
// IWICBitmapFrameDecode wrapper around the built-in WIC JPEG frame. Every call
// delegates to the inner frame: we expose the embedded JPEG preview exactly as
// Windows' own JPEG decoder sees it (size, pixel format, resolution, pixels).
#pragma once

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

class SonyArwFrameDecode : public IWICBitmapFrameDecode {
public:
    explicit SonyArwFrameDecode(Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> inner) noexcept;
    virtual ~SonyArwFrameDecode();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IWICBitmapSource
    IFACEMETHODIMP GetSize(UINT* puiWidth, UINT* puiHeight) override;
    IFACEMETHODIMP GetPixelFormat(WICPixelFormatGUID* pPixelFormat) override;
    IFACEMETHODIMP GetResolution(double* pDpiX, double* pDpiY) override;
    IFACEMETHODIMP CopyPalette(IWICPalette* pIPalette) override;
    IFACEMETHODIMP CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize,
                              BYTE* pbBuffer) override;

    // IWICBitmapFrameDecode
    IFACEMETHODIMP GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    IFACEMETHODIMP GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts,
                                    UINT* pcActualCount) override;
    IFACEMETHODIMP GetThumbnail(IWICBitmapSource** ppIThumbnail) override;

private:
    volatile LONG m_ref;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> m_inner;
};
