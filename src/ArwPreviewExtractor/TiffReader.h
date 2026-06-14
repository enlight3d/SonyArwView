// TiffReader.h
//
// Minimal, bounds-checked, endian-aware reader for TIFF/ARW container parsing.
//
// Design goals (see project spec):
//   * Never read out of bounds. Every accessor validates against the buffer size.
//   * Endian-aware: supports "II" (little-endian) and "MM" (big-endian) TIFF.
//   * Guard against integer overflow when computing offsets/lengths.
//   * No ownership of the underlying bytes: ByteReader is a non-owning view.
//
// This header is intentionally free of Windows/COM dependencies so the TIFF
// parsing logic can be reasoned about and unit-tested in isolation.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace arw {

// TIFF byte order, decoded from the 2-byte header tag.
enum class ByteOrder {
    LittleEndian, // "II"
    BigEndian     // "MM"
};

// TIFF field (entry value) types. Sizes per TIFF6 spec.
enum class TiffType : uint16_t {
    Byte      = 1,  // 1 byte
    Ascii     = 2,  // 1 byte
    Short     = 3,  // 2 bytes
    Long      = 4,  // 4 bytes
    Rational  = 5,  // 8 bytes (2 x LONG)
    SByte     = 6,  // 1 byte
    Undefined = 7,  // 1 byte
    SShort    = 8,  // 2 bytes
    SLong     = 9,  // 4 bytes
    SRational = 10, // 8 bytes
    Float     = 11, // 4 bytes
    Double    = 12, // 8 bytes
    // TIFF/BigTIFF extensions occasionally seen; treated as unknown otherwise.
};

// Returns the size in bytes of a single element of the given TIFF type.
// Returns 0 for unknown/unsupported types.
size_t TiffTypeSize(uint16_t type);

// A non-owning, bounds-checked, endian-aware view over a byte buffer.
// All read accessors return false (and leave the out-param untouched) if the
// requested range is not fully contained within the buffer.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size, ByteOrder order) noexcept
        : data_(data), size_(size), order_(order) {}

    const uint8_t* data() const noexcept { return data_; }
    size_t         size() const noexcept { return size_; }
    ByteOrder      order() const noexcept { return order_; }
    void setOrder(ByteOrder o) noexcept { order_ = o; }

    // True if [offset, offset+length) is fully within the buffer.
    // Overflow-safe (uses 64-bit math; offset/length are size_t-promoted).
    bool inRange(uint64_t offset, uint64_t length) const noexcept;

    // Endian-aware fixed-width reads. Return false if out of range.
    bool readU8 (uint64_t offset, uint8_t&  out) const noexcept;
    bool readU16(uint64_t offset, uint16_t& out) const noexcept;
    bool readU32(uint64_t offset, uint32_t& out) const noexcept;

    // Raw little/big chosen by current order; convenience for callers that
    // already know they want the container order.
    bool readBytes(uint64_t offset, size_t length, std::vector<uint8_t>& out) const;

private:
    const uint8_t* data_;
    size_t         size_;
    ByteOrder      order_;
};

// A single 12-byte TIFF IFD entry, decoded.
struct TiffEntry {
    uint16_t tag   = 0;
    uint16_t type  = 0;
    uint32_t count = 0;
    // Offset (within the buffer) of the 4-byte value/offset field of this entry.
    // The caller resolves inline-vs-offset using type/count.
    uint64_t valueFieldOffset = 0;
};

// Parses the 8-byte TIFF header at the start of the buffer.
//   bytes 0..1 : "II" or "MM"
//   bytes 2..3 : magic 0x002A (read in the detected byte order)
//   bytes 4..7 : offset of the first IFD
// Returns false if the header is not valid TIFF.
bool ParseTiffHeader(const uint8_t* data, size_t size,
                     ByteOrder& orderOut, uint32_t& firstIfdOffsetOut) noexcept;

// Reads the IFD located at ifdOffset.
//   * Reads the 2-byte entry count.
//   * Validates the whole IFD block is in range.
//   * Fills `entries` with decoded entries.
//   * Sets nextIfdOffset to the trailing 4-byte "next IFD" pointer (0 if none).
// Returns false on any out-of-range / malformed condition.
bool ReadIfd(const ByteReader& reader, uint64_t ifdOffset,
             std::vector<TiffEntry>& entries, uint32_t& nextIfdOffset) noexcept;

// Resolves a LONG/SHORT-typed entry into a list of uint32 values, correctly
// handling the "inline vs. offset" rule (value fits in 4 bytes => stored inline).
// Only SHORT and LONG element types are supported (the ones we need: SubIFDs,
// JPEGInterchangeFormat, lengths). Returns false on out-of-range or unsupported
// type. `maxCount` caps how many values we will read to avoid pathological input.
bool ReadEntryAsU32(const ByteReader& reader, const TiffEntry& entry,
                    std::vector<uint32_t>& valuesOut, uint32_t maxCount) noexcept;

} // namespace arw
