// Registration.h - self-registration of the thumbnail provider under HKCU.
#pragma once

#include <windows.h>

HRESULT RegisterThumbnailProvider();
HRESULT UnregisterThumbnailProvider();
