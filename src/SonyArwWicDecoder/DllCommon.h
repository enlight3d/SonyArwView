// DllCommon.h - shared DLL-wide state (module handle + lifetime ref count).
#pragma once

#include <windows.h>

// Set in DllMain. Used to resolve our own path for InprocServer32 registration.
extern HMODULE g_hModule;

// Number of live COM objects + server locks. DllCanUnloadNow consults this.
extern volatile LONG g_dllRefCount;

inline void DllAddRef() noexcept  { InterlockedIncrement(&g_dllRefCount); }
inline void DllRelease() noexcept { InterlockedDecrement(&g_dllRefCount); }
