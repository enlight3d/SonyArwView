// DllCommon.h - shared DLL-wide state for the thumbnail provider.
#pragma once

#include <windows.h>

extern HMODULE       g_hModule;
extern volatile LONG g_dllRefCount;

inline void DllAddRef() noexcept  { InterlockedIncrement(&g_dllRefCount); }
inline void DllRelease() noexcept { InterlockedDecrement(&g_dllRefCount); }
