// SonyArwOpen - the .arw "open" handler for the SonyArwView package.
//
// Built as a GUI-subsystem app (no console) so double-clicking an .arw does NOT
// flash a command window. It extracts the embedded JPEG preview, injects the
// correct EXIF orientation, writes it to %TEMP%, and opens it in the default
// image viewer (Windows Photos). On failure it shows a small message box rather
// than failing silently.
//
// (The console tool SonyArwPreviewExtract.exe shares the same logic but stays a
// console app for its --info / --scan-folder CLI; this binary exists purely so
// the file-open experience is windowless.)

#include "ArwPreviewExtractor.h"
#include "JpegValidation.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <filesystem>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

namespace {

bool WriteAllBytes(const wchar_t* path, const std::vector<uint8_t>& bytes) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    size_t total = 0;
    bool ok = true;
    while (total < bytes.size()) {
        const DWORD want = static_cast<DWORD>(
            (bytes.size() - total) > (16u * 1024 * 1024) ? (16u * 1024 * 1024)
                                                         : (bytes.size() - total));
        DWORD wrote = 0;
        if (!WriteFile(h, bytes.data() + total, want, &wrote, nullptr) || wrote == 0) {
            ok = false;
            break;
        }
        total += wrote;
    }
    CloseHandle(h);
    return ok && total == bytes.size();
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 1;
    const wchar_t* input = argv[1];

    // WIC validation inside the extractor needs COM on this thread.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    int rc = 0;
    std::vector<uint8_t> jpeg;
    arw::ExtractResult result;
    const HRESULT hr = arw::ExtractEmbeddedJpegFromFile(input, jpeg, &result);
    if (FAILED(hr) || !result.success || jpeg.empty()) {
        MessageBoxW(nullptr,
            L"SonyArwView couldn't find a usable preview inside this .ARW file.",
            L"SonyArwView", MB_OK | MB_ICONWARNING);
        rc = 2;
    } else {
        // Inject orientation (lossless) so Photos shows portrait shots upright.
        arw::jpeg::InjectExifOrientation(jpeg, result.orientation);

        wchar_t temp[MAX_PATH] = {};
        if (GetTempPathW(MAX_PATH, temp) != 0) {
            fs::path out = fs::path(temp) /
                           (fs::path(input).stem().wstring() + L"_preview.jpg");
            if (WriteAllBytes(out.c_str(), jpeg)) {
                ShellExecuteW(nullptr, L"open", out.c_str(), nullptr, nullptr,
                              SW_SHOWNORMAL);
            } else {
                rc = 3;
            }
        } else {
            rc = 3;
        }
    }

    CoUninitialize();
    return rc;
}
