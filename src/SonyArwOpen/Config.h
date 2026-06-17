// Config.h - shared pass-through-viewer config used by both the open handler and
// the settings window. The viewer path lives in a file (not the registry) because
// the packaged process gets a virtualized HKCU but a real file system; see
// ReadPreferredViewer in main.cpp / HOW-IT-WORKS.md.
#pragma once

#include <windows.h>
#include <string>

namespace cfg {

inline std::wstring ConfigDir() {
    wchar_t profile[MAX_PATH] = {};
    const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    return std::wstring(profile) + L"\\.sonyarwview";
}

inline std::wstring ConfigPath() {
    const std::wstring dir = ConfigDir();
    return dir.empty() ? std::wstring() : dir + L"\\viewer.txt";
}

// The stored viewer path verbatim (no env expansion). Empty if unset.
inline std::wstring ReadViewerRaw() {
    const std::wstring path = ConfigPath();
    if (path.empty()) return std::wstring();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::wstring();
    char buf[2048] = {};
    DWORD read = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
    CloseHandle(h);
    if (read == 0) return std::wstring();
    std::wstring s;
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, static_cast<int>(read), nullptr, 0);
    if (wlen <= 0) return std::wstring();
    s.resize(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, 0, buf, static_cast<int>(read), &s[0], wlen);
    if (!s.empty() && s.front() == 0xFEFF) s.erase(s.begin());  // strip BOM
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' ||
                          s.back() == L' ' || s.back() == L'\t')) {
        s.pop_back();
    }
    return s;
}

// Write the chosen viewer path (UTF-8, no BOM). Returns false on failure.
inline bool WriteViewer(const std::wstring& viewerPath) {
    const std::wstring dir = ConfigDir();
    if (dir.empty()) return false;
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring file = ConfigPath();
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const int n = WideCharToMultiByte(CP_UTF8, 0, viewerPath.c_str(),
                                      static_cast<int>(viewerPath.size()),
                                      nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<size_t>(n), '\0');
    if (n > 0) {
        WideCharToMultiByte(CP_UTF8, 0, viewerPath.c_str(), static_cast<int>(viewerPath.size()),
                            &u[0], n, nullptr, nullptr);
    }
    DWORD wrote = 0;
    const BOOL ok = WriteFile(h, u.data(), static_cast<DWORD>(u.size()), &wrote, nullptr);
    CloseHandle(h);
    return ok && wrote == static_cast<DWORD>(u.size());
}

// Revert to extract-and-open-in-Photos (delete the config file).
inline void ClearViewer() {
    const std::wstring file = ConfigPath();
    if (!file.empty()) DeleteFileW(file.c_str());
}

} // namespace cfg
