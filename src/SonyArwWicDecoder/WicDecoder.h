// WicDecoder.h
//
// IWICBitmapDecoder for Sony .ARW. It does NOT decode RAW. On Initialize it
// extracts the embedded JPEG preview (via ArwPreviewExtractor) and creates an
// internal built-in WIC JPEG decoder over those bytes. Frame 0 is that preview.
#pragma once

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

class SonyArwDecoder : public IWICBitmapDecoder {
public:
    SonyArwDecoder() noexcept;
    virtual ~SonyArwDecoder();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IWICBitmapDecoder
    IFACEMETHODIMP QueryCapability(IStream* pIStream, DWORD* pCapability) override;
    IFACEMETHODIMP Initialize(IStream* pIStream, WICDecodeOptions cacheOptions) override;
    IFACEMETHODIMP GetContainerFormat(GUID* pguidContainerFormat) override;
    IFACEMETHODIMP GetDecoderInfo(IWICBitmapDecoderInfo** ppIDecoderInfo) override;
    IFACEMETHODIMP CopyPalette(IWICPalette* pIPalette) override;
    IFACEMETHODIMP GetMetadataQueryReader(IWICMetadataQueryReader** ppIMetadataQueryReader) override;
    IFACEMETHODIMP GetPreview(IWICBitmapSource** ppIBitmapSource) override;
    IFACEMETHODIMP GetColorContexts(UINT cCount, IWICColorContext** ppIColorContexts,
                                    UINT* pcActualCount) override;
    IFACEMETHODIMP GetThumbnail(IWICBitmapSource** ppIThumbnail) override;
    IFACEMETHODIMP GetFrameCount(UINT* pCount) override;
    IFACEMETHODIMP GetFrame(UINT index, IWICBitmapFrameDecode** ppIBitmapFrame) override;

private:
    HRESULT EnsureFactory();

    volatile LONG                            m_ref;
    Microsoft::WRL::ComPtr<IWICImagingFactory> m_factory;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder>  m_inner;     // built-in JPEG decoder
    Microsoft::WRL::ComPtr<IStream>            m_jpegStream; // memory stream of preview
    std::vector<uint8_t>                       m_jpegBytes;  // keeps the stream alive
    bool                                       m_initialized;
};
