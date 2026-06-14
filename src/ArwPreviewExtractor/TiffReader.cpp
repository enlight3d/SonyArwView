// TiffReader.cpp
#include "TiffReader.h"

namespace arw {

size_t TiffTypeSize(uint16_t type) {
    switch (static_cast<TiffType>(type)) {
        case TiffType::Byte:      return 1;
        case TiffType::Ascii:     return 1;
        case TiffType::Short:     return 2;
        case TiffType::Long:      return 4;
        case TiffType::Rational:  return 8;
        case TiffType::SByte:     return 1;
        case TiffType::Undefined: return 1;
        case TiffType::SShort:    return 2;
        case TiffType::SLong:     return 4;
        case TiffType::SRational: return 8;
        case TiffType::Float:     return 4;
        case TiffType::Double:    return 8;
        default:                  return 0; // unknown / unsupported
    }
}

bool ByteReader::inRange(uint64_t offset, uint64_t length) const noexcept {
    // 64-bit math avoids overflow: offset and length are each <= 2^64-1, and
    // their sum is compared in 64-bit. size_ fits in 64-bit on all targets.
    if (length == 0) {
        return offset <= static_cast<uint64_t>(size_);
    }
    const uint64_t end = offset + length;
    if (end < offset) {
        return false; // wrapped => overflow
    }
    return end <= static_cast<uint64_t>(size_);
}

bool ByteReader::readU8(uint64_t offset, uint8_t& out) const noexcept {
    if (!inRange(offset, 1)) return false;
    out = data_[offset];
    return true;
}

bool ByteReader::readU16(uint64_t offset, uint16_t& out) const noexcept {
    if (!inRange(offset, 2)) return false;
    const uint8_t b0 = data_[offset];
    const uint8_t b1 = data_[offset + 1];
    if (order_ == ByteOrder::LittleEndian) {
        out = static_cast<uint16_t>(b0 | (b1 << 8));
    } else {
        out = static_cast<uint16_t>((b0 << 8) | b1);
    }
    return true;
}

bool ByteReader::readU32(uint64_t offset, uint32_t& out) const noexcept {
    if (!inRange(offset, 4)) return false;
    const uint8_t b0 = data_[offset];
    const uint8_t b1 = data_[offset + 1];
    const uint8_t b2 = data_[offset + 2];
    const uint8_t b3 = data_[offset + 3];
    if (order_ == ByteOrder::LittleEndian) {
        out = static_cast<uint32_t>(b0) |
              (static_cast<uint32_t>(b1) << 8) |
              (static_cast<uint32_t>(b2) << 16) |
              (static_cast<uint32_t>(b3) << 24);
    } else {
        out = (static_cast<uint32_t>(b0) << 24) |
              (static_cast<uint32_t>(b1) << 16) |
              (static_cast<uint32_t>(b2) << 8) |
              static_cast<uint32_t>(b3);
    }
    return true;
}

bool ByteReader::readBytes(uint64_t offset, size_t length,
                           std::vector<uint8_t>& out) const {
    if (!inRange(offset, length)) return false;
    out.assign(data_ + offset, data_ + offset + length);
    return true;
}

bool ParseTiffHeader(const uint8_t* data, size_t size,
                     ByteOrder& orderOut, uint32_t& firstIfdOffsetOut) noexcept {
    if (data == nullptr || size < 8) return false;

    ByteOrder order;
    if (data[0] == 'I' && data[1] == 'I') {
        order = ByteOrder::LittleEndian;
    } else if (data[0] == 'M' && data[1] == 'M') {
        order = ByteOrder::BigEndian;
    } else {
        return false; // not a TIFF byte-order mark
    }

    ByteReader reader(data, size, order);

    uint16_t magic = 0;
    if (!reader.readU16(2, magic)) return false;
    // Standard TIFF magic is 42. (BigTIFF uses 43 with a different layout; we do
    // not support BigTIFF here — Sony ARW is classic TIFF.)
    if (magic != 42) return false;

    uint32_t firstIfd = 0;
    if (!reader.readU32(4, firstIfd)) return false;
    // The IFD offset must be within the file and not point into the header.
    if (firstIfd < 8 || firstIfd >= size) return false;

    orderOut = order;
    firstIfdOffsetOut = firstIfd;
    return true;
}

bool ReadIfd(const ByteReader& reader, uint64_t ifdOffset,
             std::vector<TiffEntry>& entries, uint32_t& nextIfdOffset) noexcept {
    entries.clear();
    nextIfdOffset = 0;

    uint16_t entryCount = 0;
    if (!reader.readU16(ifdOffset, entryCount)) return false;

    // An IFD with zero entries is technically legal; still read the next ptr.
    // Sanity cap: each entry is 12 bytes; the whole table must be in range.
    const uint64_t tableStart = ifdOffset + 2;
    const uint64_t tableBytes = static_cast<uint64_t>(entryCount) * 12u;
    if (!reader.inRange(tableStart, tableBytes)) return false;

    // Next-IFD pointer follows the entry table.
    const uint64_t nextPtrOffset = tableStart + tableBytes;
    uint32_t next = 0;
    if (!reader.readU32(nextPtrOffset, next)) {
        // Some files omit the trailing pointer at EOF; tolerate by treating as 0.
        next = 0;
    }

    entries.reserve(entryCount);
    for (uint16_t i = 0; i < entryCount; ++i) {
        const uint64_t entryOff = tableStart + static_cast<uint64_t>(i) * 12u;
        TiffEntry e;
        uint16_t tag = 0, type = 0;
        uint32_t count = 0;
        if (!reader.readU16(entryOff + 0, tag))  return false;
        if (!reader.readU16(entryOff + 2, type)) return false;
        if (!reader.readU32(entryOff + 4, count)) return false;
        e.tag = tag;
        e.type = type;
        e.count = count;
        e.valueFieldOffset = entryOff + 8; // the 4-byte value/offset field
        entries.push_back(e);
    }

    nextIfdOffset = next;
    return true;
}

bool ReadEntryAsU32(const ByteReader& reader, const TiffEntry& entry,
                    std::vector<uint32_t>& valuesOut, uint32_t maxCount) noexcept {
    valuesOut.clear();

    const size_t elemSize = TiffTypeSize(entry.type);
    // We only resolve SHORT (2) and LONG (4) element types into uint32.
    const TiffType t = static_cast<TiffType>(entry.type);
    if (t != TiffType::Short && t != TiffType::Long) return false;
    if (elemSize == 0) return false;

    uint32_t count = entry.count;
    if (count == 0) return true; // legal but empty
    if (count > maxCount) count = maxCount; // clamp pathological counts

    const uint64_t totalBytes = static_cast<uint64_t>(elemSize) * entry.count;

    // Determine where the values live: inline (<=4 bytes) or at an offset.
    uint64_t valuesBase;
    if (totalBytes <= 4) {
        valuesBase = entry.valueFieldOffset; // inline in the entry
    } else {
        uint32_t off = 0;
        if (!reader.readU32(entry.valueFieldOffset, off)) return false;
        valuesBase = off;
    }

    valuesOut.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t pos = valuesBase + static_cast<uint64_t>(i) * elemSize;
        uint32_t v = 0;
        if (t == TiffType::Short) {
            uint16_t s = 0;
            if (!reader.readU16(pos, s)) return false;
            v = s;
        } else { // Long
            if (!reader.readU32(pos, v)) return false;
        }
        valuesOut.push_back(v);
    }
    return true;
}

} // namespace arw
