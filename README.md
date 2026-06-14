# SonyArwView — Embedded-Preview Viewer & Thumbnails for Sony A7 V (.ARW)

View Sony **A7 V / ILCE-7M5** `.ARW` RAW files and see them as **thumbnails in
File Explorer** on Windows 11 — **without decoding RAW sensor data**. SonyArwView
surfaces the **full-resolution JPEG preview** (7008×4672) that the camera already
embeds in the `.ARW`, and hands it to Windows' own JPEG decoder.

> **Why this exists:** Windows 11's Photos app and the Microsoft Raw Image
> Extension don't (yet) handle A7 V `.ARW` — no thumbnails, won't open. This is a
> tiny, native, clean-room fix that does.

What you get:

* ✅ **Explorer thumbnails** for `.arw` — real, upright (orientation-correct) photos
* ✅ **Double-click → opens the photo in Windows Photos** (the embedded preview)
* ✅ **Batch extraction** to `.jpg` via a small console tool
* ✅ Coexists with the Microsoft Raw Image Extension — no need to remove it
* 🚫 **No RAW decode.** No demosaicing, no LibRaw/ExifTool/ImageMagick/.NET, no
  network, no background services. Native C++17 + MSVC; static-CRT binaries.

---

## Install (recommended: signed package, no Developer Mode)

Build, then run the installer. It trusts a local self-signed certificate (one
UAC prompt), installs the package, and opens Settings so you can set SonyArwView
as the default for `.arw`.

```powershell
# Build everything (Visual Studio 2022 / Build Tools + Windows 11 SDK 10.0.26100)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Build the signed package, then install it
powershell -ExecutionPolicy Bypass -File .\scripts\build-package.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\Install.ps1
```

**One manual step (required):** in the Settings page the installer opens, set
`.arw` to **SonyArwView - Sony ARW Preview**. Windows protects the default-app
choice, so it can't be set silently — and the thumbnail handler is resolved from
the default app, which is why this step is necessary.

Open a folder of `.arw` files with **Extra large icons** — thumbnails appear, and
double-clicking opens the photo in Photos.

**Uninstall:**

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Uninstall.ps1
```

(Removes the package, the `.arw` default choice, the WIC decoder, and the trusted
cert, then refreshes the thumbnail cache.)

---

## Use the console extractor (no install needed)

```powershell
$exe = ".\build\src\SonyArwPreviewExtract\Release\SonyArwPreviewExtract.exe"
& $exe DSC00001.ARW preview.jpg     # extract the embedded preview to a JPEG
& $exe --info DSC00001.ARW          # show what was found (offset, size, dims, orientation)
& $exe --scan-folder C:\path\to\arw # report every .ARW in a folder
```

`preview.jpg` opens in any viewer. Returns non-zero on failure.

---

## What's in the box

| Component | Kind | Purpose |
|---|---|---|
| `ArwPreviewExtractor` | static lib | Endian-aware TIFF/IFD parser; finds & validates the embedded JPEG preview; reads orientation. |
| `SonyArwPreviewExtract.exe` | console | Extractor (`--info` / `--scan-folder`) **and** the `.arw` "open" handler. |
| `SonyArwWicDecoder.dll` | COM / WIC | Exposes `.ARW` to classic/desktop **WIC** apps. |
| `SonyArwThumbnailProvider.dll` | COM | Orientation-correct Explorer thumbnails. |
| MSIX package + `Install.ps1` | installer | Packages the thumbnail provider so it can win `.arw` on Win11; signed, no Dev Mode. |
| `WicDecodeTest.exe` / `ThumbTest.exe` | console | Source-of-truth testers (`--list-decoders`, `--force-clsid`, …). |

```
src/   ArwPreviewExtractor · SonyArwPreviewExtract · SonyArwWicDecoder
       SonyArwThumbnailProvider · SonyArwThumbnailPackage · WicDecodeTest · ThumbTest
scripts/   build-package · Install · Uninstall · set-default · register/unregister · clear-thumbnail-cache
docs/      HOW-IT-WORKS · TROUBLESHOOTING · REGISTRY
```

---

## Notes & docs

* **How it works + why it's built this way** (Windows arbitration, why an MSIX
  package, the dead-ends): **[docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md)**
* **Troubleshooting** (no thumbnails, Photos, conflicts, debug logging):
  **[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)**
* **Registry / system footprint & clean removal:**
  **[docs/REGISTRY.md](docs/REGISTRY.md)**

**Diagnostic logging** (off by default): set `SONY_ARW_WIC_DEBUG=1` or
`SONY_ARW_THUMB_DEBUG=1` to log to `%TEMP%`.

**Without signing:** if you'd rather not sign, enable Developer Mode and use the
loose dev install instead — `scripts\register-msix-thumbnail.ps1`.

---

## Status

Verified end-to-end on Windows 11 x64 against real Sony A7 V samples: 17/17
extract a full-res preview; Explorer shows upright thumbnails; double-click opens
the preview in Photos; works alongside the Microsoft Raw Image Extension.
