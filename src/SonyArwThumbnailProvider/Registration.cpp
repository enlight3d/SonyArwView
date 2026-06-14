// Registration.cpp
//
// Registers the IThumbnailProvider COM server (per-user, HKCU) and associates it
// with the .arw extension via the well-known thumbnail-handler ShellEx GUID
// {E357FCCD-A995-4576-B01F-234630154E96}.
//
// Keys created (all under HKCU\Software\Classes):
//   CLSID\{thumb}\                       (default) = friendly name
//        \InprocServer32\                (default) = <dll>, ThreadingModel=Apartment
//   .arw\ShellEx\{E357FCCD-...}\         (default) = {thumb}
//   SystemFileAssociations\.arw\ShellEx\{E357FCCD-...}\  (default) = {thumb}
//
#include "Registration.h"
#include "DllCommon.h"
#include "Debug.h"
#include "guids.h"

#include <shlobj.h>   // SHChangeNotify
#include <string>

namespace {

constexpr const wchar_t* kClasses = L"Software\\Classes";

std::wstring Join(const wchar_t* a, const std::wstring& b) {
    return std::wstring(a) + L"\\" + b;
}

LONG CreateKey(const std::wstring& sub, HKEY& out) {
    return RegCreateKeyExW(HKEY_CURRENT_USER, Join(kClasses, sub).c_str(), 0, nullptr,
                           REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &out, nullptr);
}

LONG SetSz(HKEY key, const wchar_t* name, const wchar_t* value) {
    const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value), bytes);
}

LONG SetKeyDefault(const std::wstring& sub, const wchar_t* value) {
    HKEY k = nullptr;
    LONG r = CreateKey(sub, k);
    if (r != ERROR_SUCCESS) return r;
    if (value) r = SetSz(k, nullptr, value);
    RegCloseKey(k);
    return r;
}

bool GetModulePath(std::wstring& out) {
    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(g_hModule, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    out.assign(path, n);
    return true;
}

} // namespace

HRESULT RegisterThumbnailProvider() {
    std::wstring dllPath;
    if (!GetModulePath(dllPath)) return HRESULT_FROM_WIN32(GetLastError());

    const std::wstring clsidBase = std::wstring(L"CLSID\\") + SZ_CLSID_SONYARWTHUMBPROVIDER;

    LONG r = SetKeyDefault(clsidBase, SZ_THUMBPROVIDER_FRIENDLYNAME);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);

    {
        HKEY k = nullptr;
        r = CreateKey(clsidBase + L"\\InprocServer32", k);
        if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
        SetSz(k, nullptr, dllPath.c_str());
        // Thumbnail handlers are loaded into the shell's STA host -> Apartment.
        SetSz(k, L"ThreadingModel", L"Apartment");
        RegCloseKey(k);
    }

    // Associate with .arw via the thumbnail-handler ShellEx GUID. We register
    // both on the extension directly and under SystemFileAssociations so the
    // handler is found regardless of any ProgID the extension is mapped to.
    const std::wstring shellexLeaf = std::wstring(L"\\ShellEx\\") + SZ_SHELLEX_THUMBNAILPROVIDER;
    r = SetKeyDefault(std::wstring(L".arw") + shellexLeaf, SZ_CLSID_SONYARWTHUMBPROVIDER);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
    r = SetKeyDefault(std::wstring(L"SystemFileAssociations\\.arw") + shellexLeaf,
                      SZ_CLSID_SONYARWTHUMBPROVIDER);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);

    // Tell the shell associations changed so it drops cached handler resolution
    // and re-queries the thumbnail handler for .arw.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, nullptr, nullptr);

    dbg::Logf(L"RegisterThumbnailProvider OK: %s", dllPath.c_str());
    return S_OK;
}

HRESULT UnregisterThumbnailProvider() {
    const std::wstring shellexLeaf = std::wstring(L"\\ShellEx\\") + SZ_SHELLEX_THUMBNAILPROVIDER;

    // Remove only OUR ShellEx handler subkeys (not the whole .arw association).
    std::wstring a = std::wstring(kClasses) + L"\\.arw" + shellexLeaf;
    std::wstring b = std::wstring(kClasses) + L"\\SystemFileAssociations\\.arw" + shellexLeaf;
    RegDeleteTreeW(HKEY_CURRENT_USER, a.c_str());
    RegDeleteTreeW(HKEY_CURRENT_USER, b.c_str());

    // Remove the COM server.
    std::wstring clsid = std::wstring(kClasses) + L"\\CLSID\\" + SZ_CLSID_SONYARWTHUMBPROVIDER;
    RegDeleteTreeW(HKEY_CURRENT_USER, clsid.c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, nullptr, nullptr);
    dbg::Logf(L"UnregisterThumbnailProvider done");
    return S_OK;
}
