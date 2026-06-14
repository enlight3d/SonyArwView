// dllmain.cpp - COM in-proc server entry points for SonyArwWicDecoder.dll.
//
// Exports (see SonyArwWicDecoder.def):
//   DllGetClassObject, DllCanUnloadNow, DllRegisterServer, DllUnregisterServer
//
// Safety: no exceptions cross these boundaries; all failures map to HRESULT.
#include "DllCommon.h"
#include "ClassFactory.h"
#include "Registration.h"
#include "Debug.h"
#include "guids.h"

#include <windows.h>
#include <new>

// ---- DLL-wide globals ------------------------------------------------------
HMODULE       g_hModule    = nullptr;
volatile LONG g_dllRefCount = 0;

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_hModule = hInstance;
            DisableThreadLibraryCalls(hInstance);
            dbg::Logf(L"DLL_PROCESS_ATTACH SonyArwWicDecoder.dll");
            break;
        case DLL_PROCESS_DETACH:
            dbg::Logf(L"DLL_PROCESS_DETACH SonyArwWicDecoder.dll");
            break;
        default:
            break;
    }
    return TRUE;
}

// Classic COM in-proc server: hand out a class factory for our CLSID.
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;

    if (!IsEqualCLSID(rclsid, CLSID_SonyArwDecoder)) {
        dbg::Logf(L"DllGetClassObject: unknown CLSID");
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) SonyArwClassFactory();
    if (factory == nullptr) return E_OUTOFMEMORY;

    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    dbg::Logf(L"DllGetClassObject -> hr=0x%08X", static_cast<unsigned>(hr));
    return hr;
}

// The host may unload us only when we have no live objects / locks.
STDAPI DllCanUnloadNow() {
    const bool canUnload = (InterlockedCompareExchange(&g_dllRefCount, 0, 0) == 0);
    return canUnload ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    HRESULT hr;
    try {
        hr = RegisterWicDecoder();
    } catch (...) {
        hr = E_UNEXPECTED;
    }
    dbg::LogHr(L"DllRegisterServer", hr);
    return hr;
}

STDAPI DllUnregisterServer() {
    HRESULT hr;
    try {
        hr = UnregisterWicDecoder();
    } catch (...) {
        hr = E_UNEXPECTED;
    }
    dbg::LogHr(L"DllUnregisterServer", hr);
    return hr;
}
