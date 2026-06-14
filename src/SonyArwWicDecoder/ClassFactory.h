// ClassFactory.h - IClassFactory that manufactures SonyArwDecoder instances.
#pragma once

#include <windows.h>
#include <unknwn.h>

class SonyArwClassFactory : public IClassFactory {
public:
    SonyArwClassFactory() noexcept;
    virtual ~SonyArwClassFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    IFACEMETHODIMP LockServer(BOOL fLock) override;

private:
    volatile LONG m_ref;
};
