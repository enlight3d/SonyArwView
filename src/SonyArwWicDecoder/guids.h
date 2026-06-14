// guids.h - stable identifiers for the Sony ARW WIC decoder component.
//
// These GUIDs are generated once and MUST remain stable: they are written into
// the registry by register-wic.ps1 and referenced by WIC and test tooling.
#pragma once

#include <guiddef.h>

// COM class id of our decoder (the InprocServer32 the registry points at).
// {622A9BFD-B2BB-414C-A69C-A4E9144642B1}
DEFINE_GUID(CLSID_SonyArwDecoder,
    0x622a9bfd, 0xb2bb, 0x414c, 0xa6, 0x9c, 0xa4, 0xe9, 0x14, 0x46, 0x42, 0xb1);

// Our WIC container format id (logical "this is a Sony ARW container").
// {D545ECCD-A3B4-440A-B62A-EBD4BE5EDA27}
DEFINE_GUID(GUID_ContainerFormatSonyArw,
    0xd545eccd, 0xa3b4, 0x440a, 0xb6, 0x2a, 0xeb, 0xd4, 0xbe, 0x5e, 0xda, 0x27);

// Vendor id for this clean-room project.
// {C52660B5-95E5-4F8A-A907-A9D7280A1DB8}
DEFINE_GUID(GUID_VendorSonyArwClean,
    0xc52660b5, 0x95e5, 0x4f8a, 0xa9, 0x07, 0xa9, 0xd7, 0x28, 0x0a, 0x1d, 0xb8);

// Thumbnail provider CLSID (Phase 3, declared here so both components agree).
// {7A501F17-2299-40E4-ABC8-C1FECD9D10D2}
DEFINE_GUID(CLSID_SonyArwThumbnailProvider,
    0x7a501f17, 0x2299, 0x40e4, 0xab, 0xc8, 0xc1, 0xfe, 0xcd, 0x9d, 0x10, 0xd2);

// String forms, used when writing the registry.
#define SZ_CLSID_SONYARWDECODER       L"{622A9BFD-B2BB-414C-A69C-A4E9144642B1}"
#define SZ_CONTAINERFORMAT_SONYARW    L"{D545ECCD-A3B4-440A-B62A-EBD4BE5EDA27}"
#define SZ_VENDOR_SONYARW             L"{C52660B5-95E5-4F8A-A907-A9D7280A1DB8}"
#define SZ_CLSID_SONYARWTHUMBPROVIDER L"{7A501F17-2299-40E4-ABC8-C1FECD9D10D2}"

// WIC decoder category (well-known): CATID_WICBitmapDecoders.
#define SZ_CATID_WICBITMAPDECODERS    L"{7ED96837-96F0-4812-B211-F13C24117ED3}"

// Friendly name shown to users / tooling.
#define SZ_DECODER_FRIENDLYNAME       L"Sony ARW Embedded Preview WIC Decoder"
