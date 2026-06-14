# SonyArwView — Troubleshooting & Design Notes

This documents the *why* behind the design, the Windows arbitration realities we
hit, and how to diagnose problems. Read this if thumbnails or previews don't
appear, or if you want to understand why the solution is shaped the way it is.

---

## The core problem: who owns `.arw` on Windows 11

On Windows 11, image-file integration (thumbnails, "open") is owned by
**packaged** apps (MSIX), primarily **Windows Photos** and the **Microsoft Raw
Image Extension**. We verified, by tracing module loads, that:

* **Classic (regsvr32 / HKCU / HKLM) thumbnail handlers and WIC codecs are never
  even loaded** by Explorer's thumbnail host when a packaged app claims the
  extension. Our classic DLLs got *zero* calls.
* The Microsoft Raw Image Extension is a **`windows.mediaCodec`** package (a
  packaged WIC codec). It declares the restricted **`inProcessMediaExtension`**
  capability, which **third parties cannot sideload** (it fails to install even
  as admin / in Developer Mode). So we cannot replicate its mechanism.
* Photos runs in an **AppContainer** and does not consult third-party classic
  WIC codecs, so it can't decode A7 V RAW even with our WIC decoder registered.

### What actually works

A third party can only compete at the packaged tier by **also being a packaged
app**. So `SonyArwView` ships the thumbnail provider as an **MSIX package**
(`desktop2:ThumbnailHandler` + `com:SurrogateServer`), and:

* **The thumbnail handler is resolved from the file's _default app_.** Therefore
  `SonyArwView` must be the **default app for `.arw`**. Once it is, Explorer
  routes thumbnail requests to our handler and thumbnails appear.
* Opening an `.arw` runs our app, which extracts the embedded preview and opens
  it in the default image viewer (Photos shows the JPEG fine).

This is why the install has a one-time "set as default" step.

---

## "Thumbnails don't appear"

1. **Did you set `SonyArwView` as the default app for `.arw`?** This is
   mandatory. Right-click any `.arw` → *Open with* → *Choose another app* →
   `SonyArwView - Sony ARW Preview` → **Always**. (Windows protects this choice
   with a hash; it can only be set through the UI, once.)
2. **Clear the thumbnail cache** and restart Explorer:
   `powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1`
3. **Use a local folder** (not OneDrive/network) for the first test.
4. **View mode**: use *Extra large icons* / *Large icons*.
5. **Confirm the package is installed**: `Get-AppxPackage -Name SonyArwView`.
6. **Confirm the default**: the `ProgId` under
   `HKCU\...\Explorer\FileExts\.arw\UserChoice` should be your package's `AppX…`
   id (it changes if you reinstall).
7. **Verify the provider works in isolation** (bypasses the shell):
   `.\build\src\ThumbTest\Release\ThumbTest.exe <abs path>\DSC00001.ARW out.png`
   (direct-COM mode works only while the *classic* registration is present; the
   packaged install uses `--shell`/Explorer instead).
8. **Enable logging** (see below) and look for `GetThumbnail` calls.

### One file shows the icon but others have thumbnails

Usually a transient thumbnail-host hiccup or a cached placeholder. Clear the
thumbnail cache and refresh (F5). Confirm the file extracts with
`SonyArwPreviewExtract.exe --info that.ARW`.

---

## "Photos can't open the `.arw` directly"

Expected — the **Store Photos app does not use third-party RAW codecs**. That's
why our app's *open* handler extracts the preview to a JPEG and opens *that* in
Photos. If double-click doesn't show the photo, confirm `SonyArwView` is the
default app for `.arw` (so our open handler runs).

For **desktop WIC apps** (not packaged), the classic `SonyArwWicDecoder.dll`
works directly — verify with:
`.\build\src\WicDecodeTest\Release\WicDecodeTest.exe DSC00001.ARW out.png`

---

## Coexistence with the Microsoft Raw Image Extension

**You do not need to uninstall it.** It is a codec; our solution is a
thumbnail-handler + default-app and is orthogonal. We verified both installed
together: A7 V thumbnails render via `SonyArwView`, and the Microsoft extension
still serves your other RAW formats.

To test with it disabled anyway (conflict isolation): `Get-AppxPackage
*RawImageExtension* | Remove-AppxPackage`, then reinstall from the Store
(`winget install 9NCTDW2W1BH8 --source msstore`).

---

## Checking which WIC decoder Windows uses

```powershell
.\build\src\WicDecodeTest\Release\WicDecodeTest.exe --list-decoders
.\build\src\WicDecodeTest\Release\WicDecodeTest.exe DSC00001.ARW out.png          # arbitration
.\build\src\WicDecodeTest\Release\WicDecodeTest.exe --force-clsid DSC00001.ARW out.png
```

`--list-decoders` shows every registered WIC decoder and its CLSID;
`--force-clsid` instantiates ours directly to confirm it decodes.

---

## Diagnostic logging

| Component | Enable | Log file |
|---|---|---|
| WIC decoder | `setx SONY_ARW_WIC_DEBUG 1` | `%TEMP%\SonyArwWicDecoder.log` |
| Thumbnail provider | `setx SONY_ARW_THUMB_DEBUG 1` | `%TEMP%\SonyArwThumbnailProvider.log` |

For the **packaged** thumbnail provider, the process is full-trust but its
`%TEMP%` may resolve under the package container; search:
`%LOCALAPPDATA%\Packages\SonyArwView_*\` for the log. Logging is off by default;
both DLLs catch all exceptions and never throw across COM boundaries.

---

## Developer Mode / signing

* The signed installer (`build-package.ps1` → `Install.ps1`) trusts a
  self-signed cert in `LocalMachine\TrustedPeople` (one UAC) and needs **no**
  Developer Mode.
* The dev workflow (`register-msix-thumbnail.ps1`) registers an **unsigned**
  loose package and **requires Developer Mode**
  (`AllowDevelopmentWithoutDevLicense=1`).

---

## Why not just a classic shell thumbnail provider?

We built one (`SonyArwThumbnailProvider.dll`, registered via
`register-thumbnail.ps1`) and proved it works *in isolation* (`ThumbTest`). But
Explorer never calls it for `.arw` because a packaged app owns the extension.
The same DLL, hosted by the **MSIX package**, is what Explorer actually loads.
The classic registration is kept for reference / non-packaged scenarios.
