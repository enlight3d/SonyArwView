// ArwPreviewExtractor.cpp
#include "ArwPreviewExtractor.h"

#include "TiffReader.h"
#include "JpegValidation.h"

#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <cstdarg>
#include <cwchar>
#include <set>

namespace arw {

// ---- Tunable safety limits -------------------------------------------------
namespace {
constexpr uint16_t TAG_JPEG_INTERCHANGE        = 0x0201; // JPEGInterchangeFormat (offset)
constexpr uint16_t TAG_JPEG_INTERCHANGE_LENGTH = 0x0202; // JPEGInterchangeFormatLength
constexpr uint16_t TAG_SUBIFDS                 = 0x014A; // SubIFDs (array of IFD offsets)
constexpr uint16_t TAG_EXIF_IFD                = 0x8769; // Exif IFD pointer

constexpr int      MAX_IFDS_VISITED = 128; // hard cap on total IFDs walked
constexpr int      MAX_IFD_DEPTH    = 6;   // SubIFD nesting depth limit
constexpr uint32_t MAX_SUBIFD_COUNT = 64;  // cap SubIFD array length
constexpr size_t   MAX_SCAN_SOI     = 4096;// cap fallback SOI candidates
constexpr uint64_t MIN_JPEG_BYTES   = 64;  // anything smaller is not a JPEG

// One located preview candidate.
struct Candidate {
    uint64_t      offset = 0;
    uint64_t      length = 0;          // authoritative length (segment-walked)
    ExtractMethod method = ExtractMethod::None;
    std::wstring  source;              // human label, e.g. "IFD0", "SubIFD@0x..."
    uint32_t      sofW = 0;            // dimensions from SOF marker (no COM)
    uint32_t      sofH = 0;
    bool          structuralOk = false;
};

void Logf(std::wstring& log, _In_z_ _Printf_format_string_ const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    log += buf;
    log += L"\r\n";
}

// Resolve a single-element SHORT/LONG entry to a uint32. Returns false if absent
// or unreadable.
bool GetScalarU32(const ByteReader& reader, const std::vector<TiffEntry>& entries,
                  uint16_t tag, uint32_t& out) {
    for (const auto& e : entries) {
        if (e.tag != tag) continue;
        std::vector<uint32_t> vals;
        if (ReadEntryAsU32(reader, e, vals, 1) && !vals.empty()) {
            out = vals[0];
            return true;
        }
        return false;
    }
    return false;
}

// Compute the structural validity + dimensions of a candidate whose start offset
// is known. Uses the segment-aware end finder to derive an authoritative length.
void FinalizeCandidate(const ByteReader& reader, Candidate& c) {
    const uint8_t* data = reader.data();
    const size_t   size = reader.size();

    if (!reader.inRange(c.offset, 3)) return;
    if (!jpeg::LooksLikeJpegStart(data + c.offset, size - static_cast<size_t>(c.offset)))
        return;

    // Derive the real end via marker walk (skips embedded thumbnails correctly).
    size_t endExclusive = 0;
    if (jpeg::FindJpegEnd(data, size, static_cast<size_t>(c.offset), endExclusive)) {
        const uint64_t walked = endExclusive - c.offset;
        // Trust the walked length; it is structurally exact.
        c.length = walked;
    }
    // If the marker walk failed but the IFD gave us a length, keep that length
    // (already set by the caller) as a best effort.

    if (c.length < MIN_JPEG_BYTES) return;
    if (!reader.inRange(c.offset, c.length)) return;

    uint32_t w = 0, h = 0;
    if (jpeg::GetSofDimensions(data + c.offset, static_cast<size_t>(c.length), w, h)) {
        c.sofW = w;
        c.sofH = h;
    }
    c.structuralOk = true;
}

// Walk the IFD tree (top-level chain + SubIFDs + Exif IFD) collecting every
// JPEGInterchangeFormat candidate. Breadth-first with an explicit work list so
// recursion depth is bounded and loops are impossible (visited set).
void CollectTiffCandidates(const ByteReader& reader, uint32_t firstIfd,
                           std::vector<Candidate>& out, std::wstring& log) {
    struct Job {
        uint64_t     offset;
        int          depth;
        ExtractMethod method; // method to tag candidates found in this IFD
        std::wstring label;
    };

    std::vector<Job> work;
    work.push_back({firstIfd, 0, ExtractMethod::TiffJpegInterchange, L"IFD0"});

    std::set<uint64_t> visited;
    int visitedCount = 0;
    int chainIndex = 0;

    while (!work.empty()) {
        Job job = work.back();
        work.pop_back();

        if (visitedCount >= MAX_IFDS_VISITED) {
            Logf(log, L"  [limit] reached MAX_IFDS_VISITED (%d), stopping walk",
                 MAX_IFDS_VISITED);
            break;
        }
        if (job.depth > MAX_IFD_DEPTH) continue;
        if (visited.count(job.offset)) continue;
        visited.insert(job.offset);
        ++visitedCount;

        std::vector<TiffEntry> entries;
        uint32_t nextIfd = 0;
        if (!ReadIfd(reader, job.offset, entries, nextIfd)) {
            Logf(log, L"  [skip] %s @ 0x%llX: unreadable/malformed IFD",
                 job.label.c_str(), (unsigned long long)job.offset);
            continue;
        }
        Logf(log, L"  %s @ 0x%llX: %zu entries (depth %d)",
             job.label.c_str(), (unsigned long long)job.offset,
             entries.size(), job.depth);

        // JPEGInterchangeFormat (+ length) — the primary preview pointer.
        uint32_t jpegOff = 0, jpegLen = 0;
        const bool haveOff = GetScalarU32(reader, entries, TAG_JPEG_INTERCHANGE, jpegOff);
        const bool haveLen = GetScalarU32(reader, entries, TAG_JPEG_INTERCHANGE_LENGTH, jpegLen);
        if (haveOff && jpegOff != 0) {
            Candidate c;
            c.offset = jpegOff;
            c.length = haveLen ? jpegLen : 0;
            c.method = job.method;
            c.source = job.label;
            Logf(log, L"    tag 0x0201=0x%X (off), 0x0202=%u (len) -> candidate",
                 jpegOff, jpegLen);
            FinalizeCandidate(reader, c);
            if (c.structuralOk) {
                Logf(log, L"      validated: off=0x%llX len=%llu  %ux%u",
                     (unsigned long long)c.offset, (unsigned long long)c.length,
                     c.sofW, c.sofH);
                out.push_back(std::move(c));
            } else {
                Logf(log, L"      rejected: not a structurally valid JPEG here");
            }
        }

        // SubIFDs (tag 0x014A): array of IFD offsets — enqueue each.
        for (const auto& e : entries) {
            if (e.tag != TAG_SUBIFDS) continue;
            std::vector<uint32_t> subs;
            if (ReadEntryAsU32(reader, e, subs, MAX_SUBIFD_COUNT)) {
                for (size_t i = 0; i < subs.size(); ++i) {
                    if (subs[i] == 0 || subs[i] >= reader.size()) continue;
                    wchar_t lbl[96];
                    _snwprintf_s(lbl, _countof(lbl), _TRUNCATE,
                                 L"%s/SubIFD%zu@0x%X", job.label.c_str(), i, subs[i]);
                    work.push_back({subs[i], job.depth + 1,
                                    ExtractMethod::TiffSubIfd, lbl});
                }
            }
            break;
        }

        // Exif IFD pointer (tag 0x8769): occasionally holds preview-related dirs.
        uint32_t exifOff = 0;
        if (GetScalarU32(reader, entries, TAG_EXIF_IFD, exifOff) &&
            exifOff != 0 && exifOff < reader.size()) {
            work.push_back({exifOff, job.depth + 1, job.method, L"ExifIFD"});
        }

        // Follow the top-level IFD chain (IFD0 -> IFD1 -> ...).
        if (nextIfd != 0 && nextIfd < reader.size() && !visited.count(nextIfd)) {
            ++chainIndex;
            wchar_t lbl[32];
            _snwprintf_s(lbl, _countof(lbl), _TRUNCATE, L"IFD%d", chainIndex);
            work.push_back({nextIfd, job.depth,
                            ExtractMethod::TiffJpegInterchange, lbl});
        }
    }
}

// Fallback: scan the whole buffer for JPEG SOI markers and build candidates.
void CollectScanCandidates(const ByteReader& reader,
                           std::vector<Candidate>& out, std::wstring& log) {
    const uint8_t* data = reader.data();
    const size_t   size = reader.size();
    size_t found = 0;

    Logf(log, L"  [fallback] scanning for FF D8 FF markers...");
    for (size_t i = 0; i + 2 < size && found < MAX_SCAN_SOI; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD8 && data[i + 2] == 0xFF) {
            ++found;
            Candidate c;
            c.offset = i;
            c.method = ExtractMethod::FallbackMarkerScan;
            wchar_t lbl[48];
            _snwprintf_s(lbl, _countof(lbl), _TRUNCATE, L"scan@0x%zX", i);
            c.source = lbl;
            FinalizeCandidate(reader, c);
            if (c.structuralOk) {
                out.push_back(std::move(c));
            }
            // Note: we intentionally keep scanning past nested SOIs; ranking by
            // dimensions will favour the largest (outer) preview.
        }
    }
    Logf(log, L"  [fallback] %zu SOI markers, %zu structurally valid candidates",
         found, out.size());
}

} // namespace

const wchar_t* ToString(ExtractMethod m) noexcept {
    switch (m) {
        case ExtractMethod::TiffJpegInterchange: return L"TIFF JPEGInterchangeFormat";
        case ExtractMethod::TiffSubIfd:          return L"TIFF SubIFD JPEGInterchangeFormat";
        case ExtractMethod::FallbackMarkerScan:  return L"Fallback marker scan";
        case ExtractMethod::None:                return L"None";
        default:                                 return L"Unknown";
    }
}

bool ExtractEmbeddedJpegFromMemory(const uint8_t* data, size_t size,
                                   std::vector<uint8_t>& jpegOut,
                                   ExtractResult& result,
                                   IWICImagingFactory* wicFactory) noexcept {
    jpegOut.clear();
    result = ExtractResult{};

    try {
        if (data == nullptr || size < 16) {
            result.diagnostic = L"Input too small to be an ARW/TIFF file.\r\n";
            return false;
        }

        ByteOrder order;
        uint32_t firstIfd = 0;
        std::wstring& log = result.diagnostic;

        std::vector<Candidate> candidates;

        if (ParseTiffHeader(data, size, order, firstIfd)) {
            Logf(log, L"TIFF header OK: byte order %s, first IFD @ 0x%X",
                 order == ByteOrder::LittleEndian ? L"II (little-endian)"
                                                  : L"MM (big-endian)",
                 firstIfd);
            ByteReader reader(data, size, order);
            // EXIF Orientation (tag 0x0112) lives in the ARW's IFD0, not in the
            // embedded preview JPEG. Read it here so thumbnails can be drawn
            // upright (the preview pixels are always in sensor/landscape order).
            {
                std::vector<TiffEntry> ifd0;
                uint32_t nextIfd = 0;
                if (ReadIfd(reader, firstIfd, ifd0, nextIfd)) {
                    uint32_t o = 0;
                    if (GetScalarU32(reader, ifd0, 0x0112u, o) && o >= 1 && o <= 8) {
                        result.orientation = static_cast<uint16_t>(o);
                        Logf(log, L"IFD0 EXIF Orientation = %u", o);
                    }
                }
            }
            CollectTiffCandidates(reader, firstIfd, candidates, log);
        } else {
            Logf(log, L"Not a valid TIFF header; will rely on fallback scan.");
        }

        // If the TIFF walk yielded nothing usable, run the fallback scanner.
        if (candidates.empty()) {
            ByteReader reader(data, size,
                              ParseTiffHeader(data, size, order, firstIfd)
                                  ? order : ByteOrder::LittleEndian);
            CollectScanCandidates(reader, candidates, log);
        }

        result.candidateCount = static_cast<int>(candidates.size());
        if (candidates.empty()) {
            Logf(log, L"No embedded JPEG preview found.");
            return false;
        }

        // Rank: largest pixel area first, then largest byte length. This makes
        // the full-size preview win over tiny embedded thumbnails.
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) {
                      const uint64_t aa = static_cast<uint64_t>(a.sofW) * a.sofH;
                      const uint64_t ba = static_cast<uint64_t>(b.sofW) * b.sofH;
                      if (aa != ba) return aa > ba;
                      return a.length > b.length;
                  });

        Logf(log, L"Ranking %zu candidate(s):", candidates.size());
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto& c = candidates[i];
            Logf(log, L"  #%zu %-28s off=0x%llX len=%llu %ux%u",
                 i, ToString(c.method),
                 (unsigned long long)c.offset, (unsigned long long)c.length,
                 c.sofW, c.sofH);
        }

        // Accept the best candidate. If a WIC factory is available, confirm each
        // candidate actually decodes (authoritative) before accepting it.
        for (const auto& c : candidates) {
            const uint8_t* blob = data + c.offset;
            const size_t   blobLen = static_cast<size_t>(c.length);

            uint32_t w = c.sofW, h = c.sofH;
            bool accepted = false;
            bool wicOk = false;

            if (wicFactory != nullptr) {
                uint32_t ww = 0, hh = 0;
                if (jpeg::ValidateJpegWithWic(wicFactory, blob, blobLen, ww, hh)) {
                    w = ww; h = hh;
                    wicOk = true;
                    accepted = true;
                } else {
                    Logf(log, L"  WIC rejected candidate off=0x%llX; trying next",
                         (unsigned long long)c.offset);
                }
            } else {
                // No WIC available: accept the top structurally-valid candidate.
                accepted = true;
            }

            if (accepted) {
                jpegOut.assign(blob, blob + blobLen);
                result.success      = true;
                result.method       = c.method;
                result.offset       = c.offset;
                result.length       = c.length;
                result.width        = w;
                result.height       = h;
                result.wicValidated = wicOk;
                Logf(log, L"SELECTED: %s  off=0x%llX len=%llu  %ux%u  (WIC %s)",
                     ToString(c.method),
                     (unsigned long long)c.offset, (unsigned long long)c.length,
                     w, h, wicOk ? L"validated" : L"not used");
                return true;
            }
        }

        Logf(log, L"All candidates failed WIC validation.");
        return false;
    }
    catch (...) {
        // Never let an exception escape across the (future) COM boundary.
        result.success = false;
        result.diagnostic += L"Unexpected exception during extraction.\r\n";
        return false;
    }
}

// ---- COM / WIC factory helper ---------------------------------------------
namespace {
IWICImagingFactory* CreateWicFactoryOrNull() noexcept {
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    return SUCCEEDED(hr) ? factory : nullptr;
}
} // namespace

HRESULT ExtractEmbeddedJpegFromStream(IStream* stream,
                                      std::vector<uint8_t>& jpegOut,
                                      ExtractResult* result) noexcept {
    if (stream == nullptr) return E_POINTER;

    ExtractResult local;
    ExtractResult& res = result ? *result : local;

    try {
        // Determine size via Stat, then read the whole stream into memory.
        STATSTG stat = {};
        HRESULT hr = stream->Stat(&stat, STATFLAG_NONAME);
        if (FAILED(hr)) return hr;

        const ULONGLONG cb = stat.cbSize.QuadPart;
        if (cb == 0 || cb > (256ull * 1024 * 1024)) {
            res.diagnostic = L"Stream empty or unreasonably large.\r\n";
            return E_FAIL;
        }

        // Seek to start.
        LARGE_INTEGER zero = {};
        hr = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        if (FAILED(hr)) return hr;

        std::vector<uint8_t> buffer(static_cast<size_t>(cb));
        ULONG totalRead = 0;
        while (totalRead < buffer.size()) {
            ULONG chunk = 0;
            const ULONG want = static_cast<ULONG>(
                std::min<size_t>(buffer.size() - totalRead, 16u * 1024 * 1024));
            hr = stream->Read(buffer.data() + totalRead, want, &chunk);
            if (FAILED(hr)) return hr;
            if (chunk == 0) break; // EOF
            totalRead += chunk;
        }
        buffer.resize(totalRead);

        IWICImagingFactory* factory = CreateWicFactoryOrNull();
        const bool ok = ExtractEmbeddedJpegFromMemory(
            buffer.data(), buffer.size(), jpegOut, res, factory);
        if (factory) factory->Release();
        return ok ? S_OK : WINCODEC_ERR_COMPONENTNOTFOUND;
    }
    catch (...) {
        if (result) result->diagnostic += L"Exception in stream extraction.\r\n";
        return E_FAIL;
    }
}

HRESULT ExtractEmbeddedJpegFromFile(const wchar_t* path,
                                    std::vector<uint8_t>& jpegOut,
                                    ExtractResult* result) noexcept {
    if (path == nullptr) return E_POINTER;

    ExtractResult local;
    ExtractResult& res = result ? *result : local;

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        res.diagnostic = L"Could not open input file.\r\n";
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT hr = S_OK;
    try {
        LARGE_INTEGER sz = {};
        if (!GetFileSizeEx(h, &sz)) {
            hr = HRESULT_FROM_WIN32(GetLastError());
        } else if (sz.QuadPart == 0 || sz.QuadPart > (256ll * 1024 * 1024)) {
            res.diagnostic = L"File empty or unreasonably large.\r\n";
            hr = E_FAIL;
        } else {
            std::vector<uint8_t> buffer(static_cast<size_t>(sz.QuadPart));
            size_t total = 0;
            while (total < buffer.size()) {
                DWORD got = 0;
                const DWORD want = static_cast<DWORD>(
                    std::min<size_t>(buffer.size() - total, 16u * 1024 * 1024));
                if (!ReadFile(h, buffer.data() + total, want, &got, nullptr)) {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    break;
                }
                if (got == 0) break;
                total += got;
            }
            if (SUCCEEDED(hr)) {
                buffer.resize(total);
                IWICImagingFactory* factory = CreateWicFactoryOrNull();
                const bool ok = ExtractEmbeddedJpegFromMemory(
                    buffer.data(), buffer.size(), jpegOut, res, factory);
                if (factory) factory->Release();
                hr = ok ? S_OK : WINCODEC_ERR_COMPONENTNOTFOUND;
            }
        }
    }
    catch (...) {
        res.diagnostic += L"Exception in file extraction.\r\n";
        hr = E_FAIL;
    }

    CloseHandle(h);
    return hr;
}

} // namespace arw
