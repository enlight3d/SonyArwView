// JpegValidation.h
//
// Low-level JPEG helpers used by the ARW preview extractor:
//   * Segment-aware "find the true end of a JPEG" (skips embedded EXIF
//     thumbnails so we capture the full preview, not a truncated blob).
//   * SOF-marker dimension parsing (no COM dependency).
//   * Authoritative validation via the built-in WIC JPEG decoder.
//
// The COM/WIC functions are optional: callers that cannot or do not want to
// initialize COM can rely on the structural + SOF checks alone.

#pragma once

#include <cstddef>
#include <cstdint>

// Forward declaration to avoid pulling <wincodec.h> into every translation unit.
struct IWICImagingFactory;

namespace arw {
namespace jpeg {

// True if the buffer begins with a JPEG SOI + marker: FF D8 FF.
bool LooksLikeJpegStart(const uint8_t* data, size_t size) noexcept;

// Walks JPEG marker segments starting at `soiOffset` (which must point at FF D8)
// and finds the offset just past the matching EOI (FF D9).
//
// Correctly skips:
//   * APPn / COM / DQT / DHT / SOF segments (by their 2-byte length),
//   * entropy-coded scan data after SOS (until the next real marker),
//   * embedded EXIF/MPF thumbnail JPEGs (they live inside APPn payloads).
//
// On success sets endExclusiveOut to soiOffset + jpegLength and returns true.
// Returns false on malformed / out-of-range input.
bool FindJpegEnd(const uint8_t* data, size_t size, size_t soiOffset,
                 size_t& endExclusiveOut) noexcept;

// Parses the SOFn marker of a JPEG blob (which must start at FF D8) to recover
// image dimensions, without decoding pixels and without COM. Returns false if no
// frame header is found or the blob is malformed.
bool GetSofDimensions(const uint8_t* data, size_t size,
                      uint32_t& widthOut, uint32_t& heightOut) noexcept;

// Validates a JPEG blob by handing it to the built-in WIC JPEG decoder and
// querying the frame size. This is the authoritative "can Windows actually
// decode this?" check required by the spec.
//
// `factory` must be a valid IWICImagingFactory (caller owns it; COM must be
// initialized on the calling thread). Returns false if decoding fails or the
// reported size is zero.
bool ValidateJpegWithWic(IWICImagingFactory* factory,
                         const uint8_t* data, size_t size,
                         uint32_t& widthOut, uint32_t& heightOut) noexcept;

} // namespace jpeg
} // namespace arw
