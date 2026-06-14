// Registration.cpp
//
// WIC decoder registration. We write per-user (HKCU\Software\Classes) so no
// elevation is required. The layout mirrors what a machine-level WIC codec
// writes under HKLM, but lives in the per-user classes hive which WIC merges
// into HKEY_CLASSES_ROOT at lookup time.
//
// Keys created (all under HKCU\Software\Classes):
//
//   CLSID\{decoder}\                      (default) = friendly name
//        \InprocServer32\                 (default) = <dll path>, ThreadingModel=Both
//        + values: FriendlyName, Vendor, ContainerFormat, MimeTypes, FileExtensions
//        \Formats\{pixelformat}\          (one empty key per supported output format)
//        \Patterns\0\                     Position/Length/Pattern/Mask (II*\0)
//        \Patterns\1\                     (MM\0* big-endian variant)
//
//   CLSID\{CATID_WICBitmapDecoders}\Instance\{decoder}\
//        CLSID = {decoder}, FriendlyName = friendly name
//
#include "Registration.h"
#include "DllCommon.h"
#include "Debug.h"
#include "guids.h"

#include <wincodec.h>
#include <string>

namespace {

constexpr const wchar_t* kClassesRoot = L"Software\\Classes";

// Well-known WIC pixel-format GUIDs (strings) the JPEG path can produce.
constexpr const wchar_t* kFmt24bppBGR  = L"{6FDDC324-4E03-4BFE-B185-3D77768DC90C}";
constexpr const wchar_t* kFmt32bppBGRA = L"{6FDDC324-4E03-4BFE-B185-3D77768DC90F}";
constexpr const wchar_t* kFmt8bppGray  = L"{6FDDC324-4E03-4BFE-B185-3D77768DC908}";

std::wstring Join(const wchar_t* a, const wchar_t* b) {
    std::wstring s(a);
    s += L"\\";
    s += b;
    return s;
}

LONG CreateKey(HKEY root, const std::wstring& sub, HKEY& out) {
    return RegCreateKeyExW(root, sub.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                           KEY_WRITE, nullptr, &out, nullptr);
}

LONG SetSz(HKEY key, const wchar_t* name, const wchar_t* value) {
    const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value), bytes);
}

LONG SetDword(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

LONG SetBinary(HKEY key, const wchar_t* name, const BYTE* data, DWORD len) {
    return RegSetValueExW(key, name, 0, REG_BINARY, data, len);
}

// Create a key under HKCU\Software\Classes\<sub> and set its (default) value.
LONG CreateKeyWithDefault(const std::wstring& sub, const wchar_t* defaultValue) {
    HKEY k = nullptr;
    LONG r = CreateKey(HKEY_CURRENT_USER, Join(kClassesRoot, sub.c_str()), k);
    if (r != ERROR_SUCCESS) return r;
    if (defaultValue) r = SetSz(k, nullptr, defaultValue);
    RegCloseKey(k);
    return r;
}

// Helper to open (creating) a Classes subkey for value writes.
LONG OpenClassesKey(const std::wstring& sub, HKEY& out) {
    return CreateKey(HKEY_CURRENT_USER, Join(kClassesRoot, sub.c_str()), out);
}

bool GetModulePath(std::wstring& out) {
    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(g_hModule, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    out.assign(path, n);
    return true;
}

void WritePattern(const std::wstring& clsidBase, const wchar_t* index,
                  const BYTE pattern[4]) {
    static const BYTE mask[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    HKEY k = nullptr;
    std::wstring sub = clsidBase + L"\\Patterns\\" + index;
    if (OpenClassesKey(sub, k) == ERROR_SUCCESS) {
        SetDword(k, L"Position", 0);
        SetDword(k, L"Length", 4);
        SetBinary(k, L"Pattern", pattern, 4);
        SetBinary(k, L"Mask", mask, 4);
        RegCloseKey(k);
    }
}

} // namespace

HRESULT RegisterWicDecoder() {
    std::wstring dllPath;
    if (!GetModulePath(dllPath)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Base subkeys (relative to HKCU\Software\Classes).
    const std::wstring clsidBase = std::wstring(L"CLSID\\") + SZ_CLSID_SONYARWDECODER;

    LONG r = ERROR_SUCCESS;

    // 1. CLSID\{decoder}  (default) = friendly name
    r = CreateKeyWithDefault(clsidBase, SZ_DECODER_FRIENDLYNAME);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);

    // 2. InprocServer32 -> dll path, ThreadingModel = Both
    {
        HKEY k = nullptr;
        r = OpenClassesKey(clsidBase + L"\\InprocServer32", k);
        if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
        SetSz(k, nullptr, dllPath.c_str());
        SetSz(k, L"ThreadingModel", L"Both");
        RegCloseKey(k);
    }

    // 3. Decoder descriptive values.
    {
        HKEY k = nullptr;
        r = OpenClassesKey(clsidBase, k);
        if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
        SetSz(k, L"FriendlyName",   SZ_DECODER_FRIENDLYNAME);
        SetSz(k, L"Vendor",         SZ_VENDOR_SONYARW);
        SetSz(k, L"ContainerFormat", SZ_CONTAINERFORMAT_SONYARW);
        SetSz(k, L"MimeTypes",      L"image/x-sony-arw");
        SetSz(k, L"FileExtensions", L".arw,.ARW");
        SetSz(k, L"Description",    L"Extracts the embedded JPEG preview from Sony ARW files.");
        SetSz(k, L"Author",        L"SonyARW clean-room project");
        SetDword(k, L"SupportsAnimation", 0);
        SetDword(k, L"SupportMultiframe", 0);
        RegCloseKey(k);
    }

    // 4. Supported output pixel formats (informational hints for WIC).
    CreateKeyWithDefault(clsidBase + L"\\Formats", nullptr);
    CreateKeyWithDefault(clsidBase + L"\\Formats\\" + kFmt24bppBGR, nullptr);
    CreateKeyWithDefault(clsidBase + L"\\Formats\\" + kFmt32bppBGRA, nullptr);
    CreateKeyWithDefault(clsidBase + L"\\Formats\\" + kFmt8bppGray, nullptr);

    // 5. Byte-pattern signatures so WIC can match the container by content.
    //    TIFF little-endian "II*\0" and big-endian "MM\0*".
    {
        const BYTE iiPattern[4] = {0x49, 0x49, 0x2A, 0x00};
        const BYTE mmPattern[4] = {0x4D, 0x4D, 0x00, 0x2A};
        WritePattern(clsidBase, L"0", iiPattern);
        WritePattern(clsidBase, L"1", mmPattern);
    }

    // 6. Register under the WIC decoder category so the codec is enumerable.
    {
        const std::wstring instBase = std::wstring(L"CLSID\\") +
            SZ_CATID_WICBITMAPDECODERS + L"\\Instance\\" + SZ_CLSID_SONYARWDECODER;
        HKEY k = nullptr;
        r = OpenClassesKey(instBase, k);
        if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
        SetSz(k, L"CLSID",        SZ_CLSID_SONYARWDECODER);
        SetSz(k, L"FriendlyName", SZ_DECODER_FRIENDLYNAME);
        RegCloseKey(k);
    }

    dbg::Logf(L"RegisterWicDecoder OK: %s", dllPath.c_str());
    return S_OK;
}

HRESULT UnregisterWicDecoder() {
    const std::wstring classes(kClassesRoot);

    // Delete the category instance key.
    {
        std::wstring inst = classes + L"\\CLSID\\" + SZ_CATID_WICBITMAPDECODERS +
                            L"\\Instance\\" + SZ_CLSID_SONYARWDECODER;
        RegDeleteTreeW(HKEY_CURRENT_USER, inst.c_str());
    }
    // Delete the decoder CLSID tree.
    {
        std::wstring base = classes + L"\\CLSID\\" + SZ_CLSID_SONYARWDECODER;
        RegDeleteTreeW(HKEY_CURRENT_USER, base.c_str());
    }

    dbg::Logf(L"UnregisterWicDecoder done");
    return S_OK;
}
