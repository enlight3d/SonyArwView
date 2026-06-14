// guids.h - identifiers for the Sony ARW shell thumbnail provider.
#pragma once

#include <guiddef.h>

// CLSID of our IThumbnailProvider COM object (must match the decoder project's
// declared value and the registration scripts).
// {7A501F17-2299-40E4-ABC8-C1FECD9D10D2}
DEFINE_GUID(CLSID_SonyArwThumbnailProvider,
    0x7a501f17, 0x2299, 0x40e4, 0xab, 0xc8, 0xc1, 0xfe, 0xcd, 0x9d, 0x10, 0xd2);

#define SZ_CLSID_SONYARWTHUMBPROVIDER L"{7A501F17-2299-40E4-ABC8-C1FECD9D10D2}"

// Well-known shell IThumbnailProvider handler interface id (ShellEx subkey).
#define SZ_SHELLEX_THUMBNAILPROVIDER  L"{E357FCCD-A995-4576-B01F-234630154E96}"

#define SZ_THUMBPROVIDER_FRIENDLYNAME L"Sony ARW Embedded Preview Thumbnail Provider"
