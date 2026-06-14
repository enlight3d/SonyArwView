// Registration.h - self-registration of the WIC decoder under HKCU.
#pragma once

#include <windows.h>

// Writes all registry keys needed for WIC to discover and load this decoder.
// Registers per-user under HKCU\Software\Classes (no admin required).
HRESULT RegisterWicDecoder();

// Removes everything RegisterWicDecoder created.
HRESULT UnregisterWicDecoder();
