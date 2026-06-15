# SonyArwView — Embedded-Preview Viewer & Thumbnails for Sony RAW (.ARW)

See your Sony `.ARW` RAW files as **thumbnails in File Explorer** and **open them
in Windows Photos** on Windows 11 — **without decoding RAW sensor data**.
SonyArwView surfaces the **full-resolution JPEG preview** the camera already
embeds in every `.ARW` and hands it to Windows' own JPEG decoder.

It targets **recent Sony cameras whose RAW Windows doesn't handle yet** — the
newer bodies shooting **Compressed HQ RAW** (A7 V / ILCE-7M5, A1 II, A9 III, …).
The extraction is format-general, so it works with any Sony `.ARW` that embeds a
preview. *(Built and verified on the Sony A7 V.)*

What you get:

* ✅ **Explorer thumbnails** for `.arw` — real, upright (orientation-correct) photos
* ✅ **Double-click → opens the photo in Windows Photos** (the embedded preview)
* ✅ **Batch extraction** to `.jpg` via a small console tool
* ✅ Coexists with the Microsoft Raw Image Extension — no need to remove it
* 🚫 **No RAW decode.** No demosaicing, no LibRaw/ExifTool/ImageMagick/.NET, no
  network, no services. Native C++17 + MSVC; static-CRT binaries.

---

## Install

SonyArwView is a small **signed app package** (MSIX). It is signed with a
self-signed certificate, so you trust that certificate once, then install the
package. No Developer Mode needed.

### From a release (recommended)

Download the latest [release](../../releases) — it bundles `SonyArwView.msix`,
`SonyArwView.cer`, and `Install.ps1`. Then **let the installer do it for you**
(trusts the cert via one UAC prompt, installs the app, and opens the right
Settings page):

```powershell
powershell -ExecutionPolicy Bypass -File .\Install.ps1
```

<details><summary>…or install by hand (no scripts)</summary>

1. **Trust the certificate (once):** right-click **`SonyArwView.cer`** →
   *Install Certificate* → **Local Machine** → *Place all certificates in the
   following store* → **Trusted People** → Finish. (Needs admin.)
2. **Install the app:** double-click **`SonyArwView.msix`** → *Install*.
</details>

**Finish with the one-time default-app step:** open any `.arw`, choose
**SonyArwView — Sony ARW Preview**, tick *Always*. *(Windows protects the
default-app choice, so this single click can't be automated — and the thumbnail
handler is resolved from the default app, which is why it's required.)*

Open a folder with **Extra large icons** — thumbnails appear, and double-clicking
an `.arw` shows the photo in Photos.

### Build from source

Requires Visual Studio 2022 / Build Tools (VC++ x64) + Windows 11 SDK 10.0.26100.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Build the signed .msix, then install it (one installer command that trusts the
# cert, installs the package, and opens Settings to set the default app):
powershell -ExecutionPolicy Bypass -File .\scripts\build-package.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\Install.ps1
```

### Uninstall

Remove **SonyArwView** from *Settings ▸ Apps ▸ Installed apps*, or run
`scripts\Uninstall.ps1` (also removes the trusted cert and resets the `.arw`
default).

---

## Batch-extract previews (no install needed)

```powershell
$exe = ".\build\src\SonyArwPreviewExtract\Release\SonyArwPreviewExtract.exe"
& $exe DSC00001.ARW preview.jpg     # extract the embedded preview to a JPEG
& $exe --info DSC00001.ARW          # offset, size, dimensions, orientation
& $exe --scan-folder C:\path\to\arw # report every .ARW in a folder
```

---

## What's in the box

| Component | Kind | Purpose |
|---|---|---|
| `ArwPreviewExtractor` | static lib | Endian-aware TIFF/IFD parser; finds & validates the embedded preview; reads orientation. |
| `SonyArwPreviewExtract.exe` | console | Extractor (`--info` / `--scan-folder`) **and** the `.arw` "open" handler. |
| `SonyArwWicDecoder.dll` | COM / WIC | Exposes `.ARW` to classic/desktop **WIC** apps (optional). |
| `SonyArwThumbnailProvider.dll` | COM | Orientation-correct Explorer thumbnails. |
| **SonyArwView** (MSIX) | app package | Packages the thumbnail provider so it can win `.arw` on Win11, and is the `.arw` open handler. |
| `WicDecodeTest` / `ThumbTest` | console | Source-of-truth testers. |

```
src/     ArwPreviewExtractor · SonyArwPreviewExtract · SonyArwWicDecoder
         SonyArwThumbnailProvider · SonyArwThumbnailPackage · WicDecodeTest · ThumbTest
scripts/ build-package · Install · Uninstall  (+ advanced register/unregister helpers)
docs/    HOW-IT-WORKS · TROUBLESHOOTING · REGISTRY
```

---

## Docs

* **How it works & why it's built this way** (Windows arbitration, why an MSIX
  package, the dead-ends) — **[docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md)**
* **Troubleshooting** (no thumbnails, Photos, conflicts, debug logging) —
  **[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)**
* **Registry / system footprint & clean removal** —
  **[docs/REGISTRY.md](docs/REGISTRY.md)**

Diagnostic logging is off by default (`SONY_ARW_WIC_DEBUG=1` /
`SONY_ARW_THUMB_DEBUG=1` → `%TEMP%`).

---

## Status

Verified end-to-end on Windows 11 x64 against real Sony A7 V samples: 17/17
extract a full-res preview; Explorer shows upright thumbnails; double-click opens
the preview in Photos; works alongside the Microsoft Raw Image Extension.
