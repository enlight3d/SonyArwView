// ArwPreviewExtractor.h
//
// Public API for extracting the embedded JPEG preview from a Sony .ARW file.
//
// IMPORTANT: This component does NOT decode RAW sensor data. It only locates the
// JPEG preview already embedded in the TIFF/ARW container and returns those JPEG
// bytes verbatim, so they can be handed to the built-in WIC JPEG decoder.
//
// Three entry points are provided:
//   * ExtractEmbeddedJpegFromMemory  - pure, no file/stream I/O (core logic).
//   * ExtractEmbeddedJpegFromStream  - IStream input (used by the WIC decoder).
//   * ExtractEmbeddedJpegFromFile    - file-path input (used by the console tool).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// We need HRESULT for the stream/file entry points. <windows.h> provides it;
// WIN32_LEAN_AND_MEAN / NOMINMAX are set globally by the build to keep it light.
#include <windows.h>

// Forward declarations for the COM interfaces used only as pointer parameters
// (avoids forcing <wincodec.h>/<objidl.h> on all callers). These are completed
// by the implementation .cpp and by any consumer that includes those SDK headers.
struct IWICImagingFactory;
struct IStream;

namespace arw {

// How the preview JPEG was located. Reported for diagnostics.
enum class ExtractMethod {
    None,                 // nothing found
    TiffJpegInterchange,  // tag 0x0201/0x0202 found in IFD0 or the IFD0->IFD1 chain
    TiffSubIfd,           // tag 0x0201/0x0202 found inside a SubIFD (tag 0x014A)
    FallbackMarkerScan,   // located by scanning for FF D8 ... FF D9
};

const wchar_t* ToString(ExtractMethod m) noexcept;

// Detailed result / diagnostics for one extraction attempt.
struct ExtractResult {
    bool         success      = false;
    ExtractMethod method      = ExtractMethod::None;
    uint64_t     offset       = 0;      // byte offset of the JPEG within the ARW
    uint64_t     length       = 0;      // byte length of the extracted JPEG
    uint32_t     width        = 0;      // approx dimensions (SOF or WIC)
    uint32_t     height       = 0;
    uint16_t     orientation  = 1;      // EXIF orientation (1..8) from ARW IFD0
    bool         wicValidated = false;  // true if WIC successfully decoded it
    std::wstring diagnostic;            // human-readable, multi-line log

    // Number of distinct preview candidates considered (for diagnostics).
    int candidateCount = 0;
};

// Core extraction over an in-memory buffer. Pure (no file/stream I/O).
//
//   data/size   : the full ARW file bytes.
//   jpegOut     : receives the extracted JPEG bytes on success (cleared first).
//   result      : receives method/offset/length/dimensions and a diagnostic log.
//   wicFactory  : OPTIONAL. If non-null, candidates are validated with the
//                 built-in WIC JPEG decoder (authoritative). If null, selection
//                 falls back to structural + SOF-dimension checks only.
//
// Returns true on success. Never throws; never reads out of bounds.
bool ExtractEmbeddedJpegFromMemory(const uint8_t* data, size_t size,
                                   std::vector<uint8_t>& jpegOut,
                                   ExtractResult& result,
                                   IWICImagingFactory* wicFactory) noexcept;

// IStream variant. Reads the whole stream into memory, creates a WIC factory
// internally (COM must be initialized on the calling thread), then delegates to
// ExtractEmbeddedJpegFromMemory. Returns an HRESULT (S_OK on success).
HRESULT ExtractEmbeddedJpegFromStream(IStream* stream,
                                      std::vector<uint8_t>& jpegOut,
                                      ExtractResult* result) noexcept;

// File-path variant (UTF-16 path). Reads the file into memory and delegates.
// Creates a WIC factory internally if COM is available. Returns an HRESULT.
HRESULT ExtractEmbeddedJpegFromFile(const wchar_t* path,
                                    std::vector<uint8_t>& jpegOut,
                                    ExtractResult* result) noexcept;

} // namespace arw
