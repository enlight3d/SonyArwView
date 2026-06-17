// SonyArwPreviewExtract - console tool
//
// Usage:
//   SonyArwPreviewExtract.exe input.ARW output.jpg     Extract preview to a file
//   SonyArwPreviewExtract.exe --info input.ARW         Print diagnostics only
//   SonyArwPreviewExtract.exe --scan-folder "C:\dir"   Scan every .ARW in a folder
//
// Phase 1 of the project: this tool must work BEFORE any COM/WIC registration.
// It exercises the shared ArwPreviewExtractor library end-to-end.

#include "ArwPreviewExtractor.h"
#include "JpegValidation.h"  // arw::jpeg::InjectExifOrientation

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>  // ShellExecuteW (file-handler "open" mode)

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void PrintUsage() {
    std::fwprintf(stderr,
        L"SonyArwPreviewExtract - extract the embedded JPEG preview from Sony .ARW\n"
        L"\n"
        L"Usage:\n"
        L"  SonyArwPreviewExtract.exe <input.ARW> <output.jpg>   Extract preview to a file\n"
        L"  SonyArwPreviewExtract.exe --info <input.ARW>         Print diagnostics only\n"
        L"  SonyArwPreviewExtract.exe --scan-folder <folder>     Scan every .ARW in a folder\n"
        L"\n"
        L"Exit code 0 on success, non-zero on failure.\n");
}

// RAII guard for COM initialization on this thread.
struct ComInit {
    bool ok = false;
    explicit ComInit() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
    ~ComInit() { if (ok) CoUninitialize(); }
};

bool WriteAllBytes(const wchar_t* path, const std::vector<uint8_t>& bytes) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    size_t total = 0;
    bool ok = true;
    while (total < bytes.size()) {
        DWORD wrote = 0;
        const DWORD want = static_cast<DWORD>(
            (bytes.size() - total) > (16u * 1024 * 1024) ? (16u * 1024 * 1024)
                                                         : (bytes.size() - total));
        if (!WriteFile(h, bytes.data() + total, want, &wrote, nullptr) || wrote == 0) {
            ok = false;
            break;
        }
        total += wrote;
    }
    CloseHandle(h);
    return ok && total == bytes.size();
}

void PrintResultSummary(const arw::ExtractResult& r, bool verbose) {
    if (verbose) {
        std::fwprintf(stdout, L"---- diagnostics ----\n%s---------------------\n",
                      r.diagnostic.c_str());
    }
    if (r.success) {
        std::fwprintf(stdout,
            L"OK   method=%s  offset=0x%llX  length=%llu bytes  dims=%ux%u  orient=%u  wic=%s\n",
            arw::ToString(r.method),
            (unsigned long long)r.offset, (unsigned long long)r.length,
            r.width, r.height, r.orientation,
            r.wicValidated ? L"validated" : L"not-validated");
    } else {
        std::fwprintf(stdout, L"FAIL no embedded JPEG preview could be extracted.\n");
    }
}

int DoExtract(const wchar_t* input, const wchar_t* output) {
    std::vector<uint8_t> jpeg;
    arw::ExtractResult result;
    const HRESULT hr = arw::ExtractEmbeddedJpegFromFile(input, jpeg, &result);
    PrintResultSummary(result, /*verbose*/ true);
    if (FAILED(hr) || !result.success) {
        std::fwprintf(stderr, L"Extraction failed (hr=0x%08X).\n", (unsigned)hr);
        return 2;
    }
    if (!WriteAllBytes(output, jpeg)) {
        std::fwprintf(stderr, L"Failed to write output file: %s\n", output);
        return 3;
    }
    std::fwprintf(stdout, L"Wrote %zu bytes to %s\n", jpeg.size(), output);
    return 0;
}

int DoInfo(const wchar_t* input) {
    std::vector<uint8_t> jpeg;
    arw::ExtractResult result;
    const HRESULT hr = arw::ExtractEmbeddedJpegFromFile(input, jpeg, &result);
    PrintResultSummary(result, /*verbose*/ true);
    return (SUCCEEDED(hr) && result.success) ? 0 : 2;
}

bool IsArwExtension(const fs::path& p) {
    std::wstring ext = p.extension().wstring();
    for (auto& ch : ext) ch = static_cast<wchar_t>(towlower(ch));
    return ext == L".arw";
}

int DoScanFolder(const wchar_t* folder) {
    std::error_code ec;
    fs::path dir(folder);
    if (!fs::is_directory(dir, ec)) {
        std::fwprintf(stderr, L"Not a directory: %s\n", folder);
        return 4;
    }

    int total = 0, ok = 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (!IsArwExtension(entry.path())) continue;
        ++total;

        std::vector<uint8_t> jpeg;
        arw::ExtractResult result;
        const HRESULT hr = arw::ExtractEmbeddedJpegFromFile(
            entry.path().c_str(), jpeg, &result);
        const bool good = SUCCEEDED(hr) && result.success;
        if (good) ++ok;
        std::fwprintf(stdout, L"%-28s %s  %ux%u  %llu bytes  [%s]\n",
                      entry.path().filename().c_str(),
                      good ? L"OK  " : L"FAIL",
                      result.width, result.height,
                      (unsigned long long)result.length,
                      good ? arw::ToString(result.method) : L"-");
    }
    std::fwprintf(stdout, L"\nScanned %d .ARW file(s); %d succeeded, %d failed.\n",
                  total, ok, total - ok);
    return (total > 0 && ok == total) ? 0 : 5;
}

// "Open" mode: invoked when Windows launches us as the .arw file handler with a
// single file path (e.g. double-click, when our app is the default for .arw).
// We extract the embedded preview to %TEMP% and open it in the default image
// viewer, so opening an .arw shows the photo.
int DoOpen(const wchar_t* input) {
    std::vector<uint8_t> jpeg;
    arw::ExtractResult result;
    const HRESULT hr = arw::ExtractEmbeddedJpegFromFile(input, jpeg, &result);
    if (FAILED(hr) || !result.success) {
        std::fwprintf(stderr, L"Could not extract preview from %s\n", input);
        return 2;
    }
    // Make the temp JPEG carry the correct orientation so Photos shows it upright.
    arw::jpeg::InjectExifOrientation(jpeg, result.orientation);
    wchar_t temp[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, temp) == 0) return 3;
    fs::path out = fs::path(temp) / (fs::path(input).stem().wstring() + L"_preview.jpg");
    if (!WriteAllBytes(out.c_str(), jpeg)) {
        std::fwprintf(stderr, L"Could not write preview to %s\n", out.c_str());
        return 3;
    }
    ShellExecuteW(nullptr, L"open", out.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    ComInit com; // WIC validation needs COM; extraction still works without it.

    const std::wstring arg1 = argv[1];

    if (arg1 == L"--info") {
        if (argc < 3) { PrintUsage(); return 1; }
        return DoInfo(argv[2]);
    }
    if (arg1 == L"--scan-folder") {
        if (argc < 3) { PrintUsage(); return 1; }
        return DoScanFolder(argv[2]);
    }
    if (arg1 == L"--help" || arg1 == L"-h" || arg1 == L"/?") {
        PrintUsage();
        return 0;
    }

    // A single file argument => "open" mode (we are the .arw file handler).
    if (argc == 2) {
        return DoOpen(argv[1]);
    }

    // Default: extract <input> <output>
    if (argc < 3) { PrintUsage(); return 1; }
    return DoExtract(argv[1], argv[2]);
}
