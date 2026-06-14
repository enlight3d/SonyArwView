// dllmain.cpp - COM in-proc server entry points for SonyArwThumbnailProvider.dll
#include "DllCommon.h"
#include "ClassFactory.h"
#include "Registration.h"
#include "Debug.h"
#include "guids.h"

#include <windows.h>
#include <new>

HMODULE       g_hModule     = nullptr;
volatile LONG g_dllRefCount = 0;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hInstance;
            DisableThreadLibraryCalls(hInstance);
            dbg::Logf(L"DLL_PROCESS_ATTACH SonyArwThumbnailProvider.dll");
            break;
        case DLL_PROCESS_DETACH:
            dbg::Logf(L"DLL_PROCESS_DETACH SonyArwThumbnailProvider.dll");
            break;
        default: break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_SonyArwThumbnailProvider)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto* factory = new (std::nothrow) SonyArwThumbClassFactory();
    if (factory == nullptr) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (InterlockedCompareExchange(&g_dllRefCount, 0, 0) == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    HRESULT hr;
    try { hr = RegisterThumbnailProvider(); }
    catch (...) { hr = E_UNEXPECTED; }
    dbg::LogHr(L"DllRegisterServer", hr);
    return hr;
}

STDAPI DllUnregisterServer() {
    HRESULT hr;
    try { hr = UnregisterThumbnailProvider(); }
    catch (...) { hr = E_UNEXPECTED; }
    dbg::LogHr(L"DllUnregisterServer", hr);
    return hr;
}
