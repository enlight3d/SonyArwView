# SonyArwView — Sony A7 V (.ARW) Thumbnails & Preview Viewer for Windows 11

Get **File Explorer thumbnails** for **Sony A7 V** (ILCE-7M5) `.ARW` RAW files —
and other recent Sony bodies — and **open them in Windows Photos**, on Windows
11, **without decoding RAW sensor data**. SonyArwView surfaces the
**full-resolution JPEG preview** the camera already embeds in every `.ARW` and
hands it to Windows' own JPEG decoder.

It targets **recent Sony cameras whose RAW Windows doesn't handle yet** — the
newer bodies shooting **Compressed HQ RAW** (A7 V / ILCE-7M5, A1 II, A9 III, …).
The extraction is format-general, so it works with any Sony `.ARW` that embeds a
preview. *(Built and verified on the Sony A7 V.)*

What you get:

* ✅ **Explorer thumbnails** for `.arw` — real, upright (orientation-correct) photos
* ✅ **Double-click → opens the photo in Windows Photos** (the embedded preview),
  with no command-window flash
* ✅ **Or pass it through to your own viewer** (FastStone, Lightroom, …) with the
  *original* `.arw`, so you keep full-folder browsing — see
  [Open in your preferred viewer](#open-in-your-preferred-viewer-optional)
* ✅ **Batch extraction** to `.jpg` via a small console tool
* ✅ Coexists with the Microsoft Raw Image Extension — no need to remove it
* 🚫 **No RAW decode.** No demosaicing, no LibRaw/ExifTool/ImageMagick/.NET, no
  network, no services. Native C++17 + MSVC; static-CRT binaries.

---

## Install

SonyArwView is a small **signed app package** (MSIX). It is signed with a
self-signed certificate, so you trust that certificate once, then install the
package. No Developer Mode needed.

Download `SonyArwView.msix` and `SonyArwView.cer` from the latest
[release](../../releases), then **three clicks**:

1. **Trust the certificate (once):** right-click **`SonyArwView.cer`** →
   *Install Certificate* → **Local Machine** → *Place all certificates in the
   following store* → **Trusted People** → Finish. (Approve the admin prompt.)
2. **Install the app:** double-click **`SonyArwView.msix`** → **Install**.
3. **Make it the default for `.arw`:** open any `.arw`, choose
   **SonyArwView — Sony ARW Preview**, and tick **Always**.

That's it. Open a folder with **Extra large icons** — thumbnails appear — and
double-click any `.arw` to view it in Photos.

> *(Step 3 is unavoidable: Windows resolves the thumbnail handler from the file's
> default app and protects that choice, so it can't be set silently.)*

**Prefer one command?** The release also bundles `Install.ps1`, which does steps
1–2 and opens the Settings page for step 3:
`powershell -ExecutionPolicy Bypass -File .\Install.ps1`

### Build from source

**Prerequisites** (Windows 11 x64):

* **Visual Studio 2022** *or* **Build Tools for Visual Studio 2022**, with:
  * the **MSVC v143 — VS 2022 C++ x64/x86 build tools**, and
  * the **Windows 11 SDK (10.0.26100)** — also provides `makeappx`/`signtool`
    used to build and sign the package.
* **CMake ≥ 3.20** (the VS *C++ CMake tools* component includes it).

Install the Build Tools with exactly those components in one shot:

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools --override `
  "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
```

Then build, and (optionally) build + install the package:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

powershell -ExecutionPolicy Bypass -File .\scripts\build-package.ps1  # -> build\installer\SonyArwView.msix
powershell -ExecutionPolicy Bypass -File .\scripts\Install.ps1        # trust cert + install
```

### Uninstall

Remove **SonyArwView** from *Settings ▸ Apps ▸ Installed apps*, or run
`scripts\Uninstall.ps1` (also removes the trusted cert and resets the `.arw`
default).

---

## Open in your preferred viewer (optional)

By default, double-clicking an `.arw` extracts the embedded preview and opens it
in Windows Photos. If you already have a viewer that reads Sony `.arw` natively
(FastStone, Lightroom, …), you can point SonyArwView at it — double-clicking then
launches **that** viewer with the **original** file, so you keep full-folder
browsing and side-by-side compare. Explorer thumbnails are unaffected either way.

Open **SonyArwView** from the Start menu to get a small settings window
(in English or French, following your Windows display language):

* it shows whether SonyArwView is your `.arw` default (with a button to fix it), and
* lets you choose **Windows Photos** or **your own viewer** (Browse to its `.exe`).

> *Power users:* the same setting can be scripted with
> `scripts\Set-Viewer.ps1 -Path "...\FSViewer.exe"` (or `-Clear`). It's stored
> per-user in `%USERPROFILE%\.sonyarwview\viewer.txt` — a file, not the registry,
> because the packaged handler runs with a virtualized registry and wouldn't see an
> `HKCU` value written from outside the package.

> **Note:** this only helps if your viewer can actually read the camera's RAW. For
> the newest Compressed HQ RAW that nothing decodes yet, leave it on Windows Photos
> (the extracted preview).

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
| `SonyArwPreviewExtract.exe` | console | Batch extractor (`--info` / `--scan-folder`). |
| `SonyArwOpen.exe` | windowless | The `.arw` "open" handler bundled in the package — extract→Photos, or pass-through to your viewer (no console window). Launched with no file (from Start) it shows the settings window. |
| `SonyArwWicDecoder.dll` | COM / WIC | Exposes `.ARW` to classic/desktop **WIC** apps (optional). |
| `SonyArwThumbnailProvider.dll` | COM | Orientation-correct Explorer thumbnails. |
| **SonyArwView** (MSIX) | app package | Packages the thumbnail provider so it can win `.arw` on Win11, and bundles the open handler. |
| `WicDecodeTest` / `ThumbTest` | console | Source-of-truth testers. |

```
src/     ArwPreviewExtractor · SonyArwPreviewExtract · SonyArwOpen · SonyArwWicDecoder
         SonyArwThumbnailProvider · SonyArwThumbnailPackage · WicDecodeTest · ThumbTest
scripts/ build-package · Install · Uninstall · Set-Viewer  (+ advanced register/unregister helpers)
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
