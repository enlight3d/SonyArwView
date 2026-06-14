// ClassFactory.h - IClassFactory for SonyArwThumbnailProvider.
#pragma once

#include <windows.h>
#include <unknwn.h>

class SonyArwThumbClassFactory : public IClassFactory {
public:
    SonyArwThumbClassFactory() noexcept;
    virtual ~SonyArwThumbClassFactory();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override;
    IFACEMETHODIMP LockServer(BOOL fLock) override;

private:
    volatile LONG m_ref;
};
