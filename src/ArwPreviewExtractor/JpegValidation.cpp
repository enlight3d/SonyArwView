// JpegValidation.cpp
#include "JpegValidation.h"

#include <windows.h>
#include <wincodec.h>

namespace arw {
namespace jpeg {

bool LooksLikeJpegStart(const uint8_t* data, size_t size) noexcept {
    return data != nullptr && size >= 3 &&
           data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

// Helper: read a big-endian 16-bit value (JPEG is always big-endian for the
// segment length fields, regardless of the enclosing TIFF byte order).
static bool ReadBE16(const uint8_t* data, size_t size, size_t off, uint16_t& out) noexcept {
    if (off + 2 > size) return false;
    out = static_cast<uint16_t>((data[off] << 8) | data[off + 1]);
    return true;
}

bool FindJpegEnd(const uint8_t* data, size_t size, size_t soiOffset,
                 size_t& endExclusiveOut) noexcept {
    if (data == nullptr) return false;
    if (soiOffset + 2 > size) return false;
    if (data[soiOffset] != 0xFF || data[soiOffset + 1] != 0xD8) return false;

    size_t pos = soiOffset + 2; // just past SOI

    while (pos + 1 < size) {
        // Find the marker prefix 0xFF. Tolerate fill bytes (0xFF 0xFF...).
        if (data[pos] != 0xFF) {
            // Resync: scan forward to the next 0xFF. Malformed, but be lenient.
            ++pos;
            continue;
        }
        // Skip any run of fill bytes 0xFF.
        size_t markerPos = pos;
        while (markerPos < size && data[markerPos] == 0xFF) ++markerPos;
        if (markerPos >= size) return false;
        const uint8_t marker = data[markerPos];
        pos = markerPos + 1; // position just past the marker byte

        if (marker == 0xD9) {
            // EOI — the end of this JPEG.
            endExclusiveOut = pos;
            return true;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            // TEM / RSTn: standalone markers, no length, no payload.
            continue;
        }
        if (marker == 0xDA) {
            // SOS: read the 2-byte header length, skip the header, then scan the
            // entropy-coded data until the next real marker (FF xx where xx is
            // not 0x00 and not an RSTn fill marker).
            uint16_t segLen = 0;
            if (!ReadBE16(data, size, pos, segLen)) return false;
            if (segLen < 2) return false;
            pos += segLen; // skip SOS header
            // Scan compressed data.
            while (pos + 1 < size) {
                if (data[pos] == 0xFF) {
                    const uint8_t next = data[pos + 1];
                    if (next == 0x00) {
                        pos += 2; // stuffed byte, part of entropy data
                        continue;
                    }
                    if (next >= 0xD0 && next <= 0xD7) {
                        pos += 2; // restart marker within scan, keep scanning
                        continue;
                    }
                    if (next == 0xFF) {
                        ++pos; // fill byte
                        continue;
                    }
                    // Real marker boundary — let the outer loop handle it.
                    break;
                }
                ++pos;
            }
            continue;
        }
        // All other markers (APPn, DQT, DHT, SOFn, COM, DRI, ...) carry a
        // 2-byte length that includes the length field itself.
        uint16_t segLen = 0;
        if (!ReadBE16(data, size, pos, segLen)) return false;
        if (segLen < 2) return false;
        pos += segLen;
    }
    return false; // ran out of data without finding EOI
}

bool GetSofDimensions(const uint8_t* data, size_t size,
                      uint32_t& widthOut, uint32_t& heightOut) noexcept {
    if (!LooksLikeJpegStart(data, size)) return false;

    size_t pos = 2; // past SOI
    while (pos + 1 < size) {
        if (data[pos] != 0xFF) { ++pos; continue; }
        size_t markerPos = pos;
        while (markerPos < size && data[markerPos] == 0xFF) ++markerPos;
        if (markerPos >= size) return false;
        const uint8_t marker = data[markerPos];
        pos = markerPos + 1;

        if (marker == 0xD9) return false; // EOI before any SOF
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;

        uint16_t segLen = 0;
        if (!ReadBE16(data, size, pos, segLen)) return false;
        if (segLen < 2) return false;

        // SOFn frame headers: C0-CF except C4 (DHT), C8 (JPG), CC (DAC).
        const bool isSof = (marker >= 0xC0 && marker <= 0xCF) &&
                           marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (isSof) {
            // Layout after marker: [len:2][precision:1][height:2][width:2]...
            if (pos + 5 + 2 > size) return false;
            uint16_t h = 0, w = 0;
            if (!ReadBE16(data, size, pos + 3, h)) return false;
            if (!ReadBE16(data, size, pos + 5, w)) return false;
            if (w == 0 || h == 0) return false;
            widthOut = w;
            heightOut = h;
            return true;
        }
        if (marker == 0xDA) return false; // reached scan without a SOF
        pos += segLen;
    }
    return false;
}

bool ValidateJpegWithWic(IWICImagingFactory* factory,
                         const uint8_t* data, size_t size,
                         uint32_t& widthOut, uint32_t& heightOut) noexcept {
    if (factory == nullptr || data == nullptr || size == 0) return false;
    if (size > 0xFFFFFFFFull) return false; // SHCreateMemStream cap / sanity

    bool ok = false;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;

    // Wrap the in-memory bytes in a WIC stream. CreateStream + InitializeFromMemory
    // copies/references the buffer; we keep `data` alive for the duration.
    HRESULT hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromMemory(const_cast<BYTE*>(data),
                                          static_cast<DWORD>(size));
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromStream(
            stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    }
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(hr)) {
        UINT w = 0, h = 0;
        hr = frame->GetSize(&w, &h);
        if (SUCCEEDED(hr) && w > 0 && h > 0) {
            widthOut = w;
            heightOut = h;
            ok = true;
        }
    }

    if (frame)   frame->Release();
    if (decoder) decoder->Release();
    if (stream)  stream->Release();
    return ok;
}

void InjectExifOrientation(std::vector<uint8_t>& jpeg, uint16_t orientation) noexcept {
    if (orientation <= 1 || orientation > 8) return;
    if (jpeg.size() < 2 || jpeg[0] != 0xFF || jpeg[1] != 0xD8) return; // need SOI
    const uint8_t app1[] = {
        0xFF, 0xE1, 0x00, 0x22,              // APP1, length = 34
        'E','x','i','f', 0x00, 0x00,         // "Exif\0\0"
        'I','I', 0x2A, 0x00,                 // TIFF little-endian, magic 42
        0x08, 0x00, 0x00, 0x00,              // IFD0 offset = 8
        0x01, 0x00,                          // IFD0 entry count = 1
        0x12, 0x01,                          // tag 0x0112 (Orientation)
        0x03, 0x00,                          // type 3 (SHORT)
        0x01, 0x00, 0x00, 0x00,              // count 1
        static_cast<uint8_t>(orientation & 0xFF),
        static_cast<uint8_t>((orientation >> 8) & 0xFF), 0x00, 0x00, // value (inline)
        0x00, 0x00, 0x00, 0x00               // next IFD offset = 0
    };
    jpeg.insert(jpeg.begin() + 2, app1, app1 + sizeof(app1));
}

} // namespace jpeg
} // namespace arw
