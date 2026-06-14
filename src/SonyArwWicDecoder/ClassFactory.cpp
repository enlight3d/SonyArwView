// ClassFactory.cpp
#include "ClassFactory.h"
#include "WicDecoder.h"
#include "DllCommon.h"
#include "Debug.h"

#include <new>

SonyArwClassFactory::SonyArwClassFactory() noexcept : m_ref(1) {
    DllAddRef();
}

SonyArwClassFactory::~SonyArwClassFactory() {
    DllRelease();
}

IFACEMETHODIMP SonyArwClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) SonyArwClassFactory::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

IFACEMETHODIMP_(ULONG) SonyArwClassFactory::Release() {
    const LONG c = InterlockedDecrement(&m_ref);
    if (c == 0) delete this;
    return static_cast<ULONG>(c);
}

IFACEMETHODIMP SonyArwClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                                   void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    // We do not support aggregation.
    if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

    auto* decoder = new (std::nothrow) SonyArwDecoder();
    if (decoder == nullptr) return E_OUTOFMEMORY;

    const HRESULT hr = decoder->QueryInterface(riid, ppv);
    decoder->Release(); // QI took its own ref (or failed and we destroy it)
    return hr;
}

IFACEMETHODIMP SonyArwClassFactory::LockServer(BOOL fLock) {
    if (fLock) DllAddRef();
    else       DllRelease();
    return S_OK;
}
