# SonyARW — Embedded-Preview Codec & Thumbnails for Sony A7 V (.ARW)

Make Sony **A7 V / ILCE-7M5** `.ARW` RAW files **viewable** and **show
thumbnails** on Windows 11 — **without decoding RAW sensor data**. Every image
shown is the **JPEG preview already embedded** inside the `.ARW` container,
extracted and handed to Windows' own JPEG decoder.

> Status: **working end-to-end on Windows 11 x64.** Explorer shows real
> thumbnails for A7 V `.ARW`, and double-clicking an `.ARW` opens the embedded
> preview in Windows Photos. The Microsoft Raw Image Extension can stay
> installed — our solution coexists with it.

![Explorer showing ARW thumbnails](docs/explorer-thumbnails.png)

---

## What this is (and is not)

* **Is:** a TIFF/ARW container parser that locates the embedded JPEG preview, a
  WIC decoder that exposes that preview to WIC apps, and a shell thumbnail
  provider (shipped as an MSIX package) that draws Explorer thumbnails.
* **Is NOT:** a RAW decoder. There is **no demosaicing**, no sensor-data
  processing. We only surface the camera's own embedded preview JPEG
  (which on the A7 V is **full-resolution, 7008×4672**).

No LibRaw, no ExifTool, no ImageMagick, no .NET. Native C++17 + MSVC, COM DLLs.

---

## Components

| Component | Kind | Purpose |
|---|---|---|
| `ArwPreviewExtractor` | static lib | Endian-aware TIFF/IFD parser; finds & validates the embedded JPEG preview; segment-aware JPEG scanner fallback. |
| `SonyArwPreviewExtract.exe` | console | Extract preview to a file, `--info`, `--scan-folder`. Also acts as the `.arw` "open" handler (extract → open in default viewer). |
| `SonyArwWicDecoder.dll` | COM / WIC | Exposes `.ARW` to **classic WIC apps** by delegating to the built-in JPEG decoder. |
| `WicDecodeTest.exe` | console | Source-of-truth tester for the WIC decoder (`--list-decoders`, `--force-clsid`). |
| `SonyArwThumbnailProvider.dll` | COM | `IThumbnailProvider` + `IInitializeWithStream`; renders an **orientation-corrected** thumbnail from the preview (the rotation flag lives in the ARW's IFD0, not the preview JPEG, so portrait shots come out upright). |
| `ThumbTest.exe` | console | Direct + shell-pipeline tester for the thumbnail provider. |
| `SonyArwThumbnailPackage` | MSIX manifest | Packages the thumbnail provider so it can compete with Windows' packaged image apps for `.arw`. **This is what makes Explorer thumbnails work.** |

---

## Why an MSIX package? (the important part)

On Windows 11, `.arw` thumbnails are owned by **packaged** image apps (Photos /
the Microsoft Raw Image Extension). **Classic (regsvr32 / HKCU / HKLM) thumbnail
handlers are never consulted** for an extension a packaged app claims — verified
by tracing that our classic DLLs are never even loaded by Explorer's thumbnail
host. A third party can only compete at that tier by **also** being a packaged
app. So the thumbnail provider is shipped as an MSIX package, and our app is set
as the **default `.arw` handler** (the default app owns the thumbnail). See
[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for the full diagnosis.

The classic `SonyArwWicDecoder.dll` still works for **desktop WIC apps**; it just
can't drive Explorer/Photos because those are packaged. Both are provided.

---

## Build (Windows 11 x64, Visual Studio 2022 / Build Tools)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Requires: VS 2022 (or Build Tools) with the **VC++ x64 toolset** and **Windows
11 SDK (10.0.26100)**. All binaries link the **static CRT** (no VC++
redistributable needed). CMake ≥ 3.20.

Outputs land under `build/src/<project>/Release/`.

---

## Phase 1 — Extract a preview (no install needed)

```powershell
.\build\src\SonyArwPreviewExtract\Release\SonyArwPreviewExtract.exe samples\DSC04046.ARW preview.jpg
.\build\src\SonyArwPreviewExtract\Release\SonyArwPreviewExtract.exe --info samples\DSC04046.ARW
.\build\src\SonyArwPreviewExtract\Release\SonyArwPreviewExtract.exe --scan-folder samples
```

`preview.jpg` opens in any JPEG viewer. On the A7 V samples this is a
full-resolution 7008×4672 image.

---

## Phase 2 — WIC decoder (for classic WIC apps)

```powershell
# Register (per-user, no admin):
powershell -ExecutionPolicy Bypass -File .\scripts\register-wic.ps1

# Test through WIC (the source of truth):
.\build\src\WicDecodeTest\Release\WicDecodeTest.exe samples\DSC04046.ARW decoded.png
.\build\src\WicDecodeTest\Release\WicDecodeTest.exe --list-decoders

# Unregister:
powershell -ExecutionPolicy Bypass -File .\scripts\unregister-wic.ps1
```

---

## Phase 3/4 — Explorer thumbnails + open-in-Photos (the full experience)

This needs **Developer Mode** (to register an unsigned local MSIX). Enable it in
*Settings ▸ System ▸ For developers ▸ Developer Mode*, or:

```powershell
# (admin) enable Developer Mode
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" AllowDevelopmentWithoutDevLicense 1
```

Then:

```powershell
# 1. Register the packaged thumbnail provider (stages + Add-AppxPackage -Register)
powershell -ExecutionPolicy Bypass -File .\scripts\register-msix-thumbnail.ps1

# 2. Make our app the default handler for .arw  (REQUIRED for thumbnails):
#    Right-click any .arw > Open with > Choose another app >
#    "Sony ARW Embedded Preview Thumbnail" > Always.
#    (Windows protects this choice; it must be set in the UI, once.)

# 3. Refresh Explorer:
powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1
```

Open the folder with **Extra large icons** — thumbnails appear. Double-click an
`.arw` — the embedded preview opens in Photos.

> **Why set the default app?** Windows resolves the thumbnail handler from the
> file's *default* app. Our app must be the default for `.arw` so the thumbnail
> resolves to our handler. This also gives you double-click-to-view. If you set
> Photos back as the default, thumbnails revert (Photos can't decode A7 V RAW).

### Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\unregister-msix-thumbnail.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\unregister-wic.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1
```

Restart Explorer if needed: `taskkill /f /im explorer.exe & start explorer.exe`.

---

## Repo layout

```
src/
  ArwPreviewExtractor/      shared TIFF/JPEG extraction library
  SonyArwPreviewExtract/    Phase 1 console tool (+ .arw open handler)
  SonyArwWicDecoder/        Phase 2 WIC decoder DLL
  WicDecodeTest/            WIC decoder tester
  SonyArwThumbnailProvider/ Phase 3 shell thumbnail provider DLL
  ThumbTest/                thumbnail provider tester
  SonyArwThumbnailPackage/  Phase 4 MSIX manifest (packages the provider)
scripts/                    register/unregister/cache PowerShell scripts
docs/                       README / TROUBLESHOOTING / REGISTRY
samples/                    real Sony A7 V .ARW files
CMakeLists.txt
```

## Debugging

Set `SONY_ARW_WIC_DEBUG=1` (decoder) or `SONY_ARW_THUMB_DEBUG=1` (thumbnail) to
log to `%TEMP%\SonyArwWicDecoder.log` / `%TEMP%\SonyArwThumbnailProvider.log`.
For the packaged provider the log is under its package container's temp folder
(`%LOCALAPPDATA%\Packages\SonyARW.EmbeddedPreviewThumbnail_*\...`).

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) and
[docs/REGISTRY.md](docs/REGISTRY.md).
