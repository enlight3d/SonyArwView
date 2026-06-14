// ClassFactory.cpp
#include "ClassFactory.h"
#include "ThumbnailProvider.h"
#include "DllCommon.h"

#include <new>

SonyArwThumbClassFactory::SonyArwThumbClassFactory() noexcept : m_ref(1) { DllAddRef(); }
SonyArwThumbClassFactory::~SonyArwThumbClassFactory() { DllRelease(); }

IFACEMETHODIMP SonyArwThumbClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) SonyArwThumbClassFactory::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&m_ref));
}

IFACEMETHODIMP_(ULONG) SonyArwThumbClassFactory::Release() {
    const LONG c = InterlockedDecrement(&m_ref);
    if (c == 0) delete this;
    return static_cast<ULONG>(c);
}

IFACEMETHODIMP SonyArwThumbClassFactory::CreateInstance(IUnknown* pUnkOuter,
                                                        REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_POINTER;
    *ppv = nullptr;
    if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

    auto* obj = new (std::nothrow) SonyArwThumbnailProvider();
    if (obj == nullptr) return E_OUTOFMEMORY;

    const HRESULT hr = obj->QueryInterface(riid, ppv);
    obj->Release();
    return hr;
}

IFACEMETHODIMP SonyArwThumbClassFactory::LockServer(BOOL fLock) {
    if (fLock) DllAddRef(); else DllRelease();
    return S_OK;
}
