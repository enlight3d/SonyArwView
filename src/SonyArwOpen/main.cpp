// SonyArwOpen - the .arw "open" handler for the SonyArwView package.
//
// WINDOWS-subsystem app (no console window -> no command-window flash). The
// package manifest declares the .arw association with Parameters="%1", so for
// this full-trust packaged app Windows passes the opened file PATH on the command
// line (argv[1]) -- NOT through the WinRT activation contract. That keeps this a
// plain, dependency-free Win32 program.
//
// It either hands the ORIGINAL file to the user's configured pass-through viewer
// (ReadPreferredViewer) or extracts the embedded JPEG preview, injects EXIF
// orientation, writes it to %TEMP% and opens it in the default image viewer
// (Photos). On failure it shows a small message box.
//
// (SonyArwPreviewExtract.exe shares the extraction logic but stays a console CLI
// for its --info / --scan-folder modes.)

#include "ArwPreviewExtractor.h"
#include "JpegValidation.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
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

// Optional "pass-through" viewer the user configured (full path to an app that
// opens .arw natively, e.g. FastStone/Lightroom). Empty if unset.
//
// Stored in a FILE at %USERPROFILE%\.sonyarwview\viewer.txt rather than the
// registry: a packaged (MSIX) process gets a virtualized HKCU and cannot see a
// Software\SonyArwView key written by the (non-packaged) Set-Viewer script, but
// its file system is NOT virtualized, and %USERPROFILE% resolves to the same real
// profile for both. The file is UTF-8 (optionally BOM); env vars are expanded.
std::wstring ReadPreferredViewer() {
    wchar_t profile[MAX_PATH] = {};
    const DWORD pn = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    if (pn == 0 || pn >= MAX_PATH) return std::wstring();
    const std::wstring path = std::wstring(profile) + L"\\.sonyarwview\\viewer.txt";

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::wstring();

    char buf[2048] = {};
    DWORD read = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
    CloseHandle(h);
    if (read == 0) return std::wstring();

    std::wstring raw;
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, static_cast<int>(read), nullptr, 0);
    if (wlen <= 0) return std::wstring();
    raw.resize(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, 0, buf, static_cast<int>(read), &raw[0], wlen);
    if (!raw.empty() && raw.front() == 0xFEFF) raw.erase(raw.begin());  // strip BOM
    while (!raw.empty() && (raw.back() == L'\r' || raw.back() == L'\n' ||
                            raw.back() == L' ' || raw.back() == L'\t')) {
        raw.pop_back();
    }
    if (raw.empty()) return std::wstring();

    wchar_t expanded[1024] = {};
    const DWORD n = ExpandEnvironmentStringsW(raw.c_str(), expanded, 1024);
    return (n > 0 && n <= 1024) ? std::wstring(expanded) : raw;
}

bool FileExists(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// The opened file path. Windows passes it as argv[1] (manifest Parameters="%1").
std::wstring GetInputPath() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring path;
    if (argv) {
        if (argc >= 2 && argv[1] && argv[1][0] != L'\0') path = argv[1];
        LocalFree(argv);
    }
    return path;
}

int Run(const std::wstring& inputStr) {
    if (inputStr.empty()) return 1;
    const wchar_t* input = inputStr.c_str();

    // Pass-through mode: hand the ORIGINAL file to the user's native .arw viewer
    // (full album browsing). Falls through to extract-and-open when unset.
    {
        const std::wstring viewer = ReadPreferredViewer();
        if (!viewer.empty() && FileExists(viewer)) {
            std::wstring params = L"\"" + inputStr + L"\"";
            ShellExecuteW(nullptr, L"open", viewer.c_str(), params.c_str(),
                          nullptr, SW_SHOWNORMAL);
            return 0;
        }
    }

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

} // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return Run(GetInputPath());
}
