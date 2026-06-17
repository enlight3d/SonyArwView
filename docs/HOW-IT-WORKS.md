# How SonyArwView works — architecture & the road to it

This is the "how we got here" document: the design, the phases it was built in,
the Windows realities that shaped it, and the dead-ends we hit and ruled out. If
you just want to install and use it, see the [README](../README.md). If you're
troubleshooting, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

## The core idea

A Sony `.ARW` is a TIFF-like container. Inside it, the camera already stores a
**full-resolution JPEG preview** (7008×4672 on the A7 V). We never touch the RAW
sensor data — no demosaicing. We locate that embedded JPEG, hand it to Windows'
**built-in** JPEG decoder, and expose it as the image. Everything else is plumbing
to get Windows to *use* that.

```
.ARW (TIFF) ──parse IFD chain──► embedded JPEG preview ──► built-in WIC JPEG decoder ──► pixels
```

---

## Built in phases

The project was developed in deliberate phases, each independently testable, so
that a failure at one layer could be diagnosed before moving on.

**Phase 1 — Shared extractor + console tool.**
`ArwPreviewExtractor` is an endian-aware, bounds-checked TIFF/IFD parser. It
walks IFD0 → IFD1 → IFD*n* and any SubIFDs, collects every
`JPEGInterchangeFormat` (tag `0x0201`/`0x0202`) candidate, validates each (SOF
dimensions + WIC decode), and picks the largest. A segment-aware marker scanner
(`FF D8 … FF D9`, skipping embedded EXIF thumbnails) is the fallback. The console
tool `SonyArwPreviewExtract.exe` proved this against real samples: **17/17 A7 V
files yielded a valid full-res preview via the clean TIFF-tag path** (the
fallback was never needed). This had to work before any COM/WIC code.

**Phase 2 — WIC decoder DLL.**
`SonyArwWicDecoder.dll` implements `IWICBitmapDecoder` / `IWICBitmapFrameDecode`.
On `Initialize` it extracts the preview and creates an *internal* built-in JPEG
decoder over those bytes; size/format/resolution/`CopyPixels` all delegate to it.
`WicDecodeTest.exe` is the source of truth: it enumerates decoders, decodes
through WIC arbitration, and can force our CLSID. **Our decoder wins WIC
arbitration for `.arw` and decodes correctly.**

**Phase 3 — Shell thumbnail provider.**
`SonyArwThumbnailProvider.dll` implements `IThumbnailProvider` +
`IInitializeWithStream`, rendering an HBITMAP from the preview. `ThumbTest.exe`
proved it produces a correct 256×171 thumbnail in isolation.

**Phase 4 — Packaging (the part that actually made Explorer work).** See below.

---

## The wall: Windows 11 gives `.arw` to *packaged* apps

Phase 2/3 worked perfectly in our test tools but **Explorer still showed generic
icons, and Photos refused the file**. Tracing module loads with logging revealed
the truth:

* During Explorer thumbnail generation, **neither of our DLLs is ever loaded.**
  Classic (regsvr32 / `HKCU` / `HKLM`) thumbnail handlers and WIC codecs are
  **never consulted** for an extension that a *packaged* app claims.
* `.arw` is owned by **packaged** image apps — **Windows Photos** and the
  **Microsoft Raw Image Extension**. Photos runs in an AppContainer and does not
  use third-party WIC codecs, so it can't decode A7 V RAW even with our decoder
  registered.

We verified this from every angle (extension-level `ShellEx`, our own default
ProgID, clearing `OpenWithProgids`): the packaged claim wins every time.

### Dead-end: package our *codec* like Microsoft does

Microsoft's Raw Image Extension is a packaged **`windows.mediaCodec`** (a packaged
WIC codec). Replicating that is the obvious move — except it declares the
**restricted `inProcessMediaExtension` capability**, which **third parties cannot
install, even via sideloading, even as admin.** It's reserved for Microsoft and
approved partners. That route is closed by design.

### The breakthrough: a packaged *thumbnail handler* + be the default app

A packaged `IThumbnailProvider` does **not** need that restricted capability. So
we ship `SonyArwThumbnailProvider.dll` inside an **MSIX package** that declares:

* a `com:SurrogateServer` registering our DLL/CLSID, and
* a `windows.fileTypeAssociation` for `.arw` with a `desktop2:ThumbnailHandler`.

The last piece: **Windows resolves the thumbnail handler from the file's default
app.** So our app must be the **default for `.arw`**. Once it is, Explorer routes
thumbnail requests to our handler — and thumbnails appear. The same package makes
our app the `.arw` "open" handler too, so double-click extracts the preview and
opens it in Photos (as a plain JPEG Photos *can* show).

```
.arw  ──default app──►  SonyArwView (packaged)
                          ├─ thumbnail:  desktop2:ThumbnailHandler ► our DLL ► HBITMAP
                          └─ open:       our exe ► extract preview ► open JPEG in Photos
```

The one unavoidable manual step is making SonyArwView the default for `.arw`:
Windows protects that choice with a per-user hash specifically to stop apps
setting it silently (and recent builds harden it further with "UserChoiceLatest").
So the installer opens Settings to the right page for a single click.

### Getting the opened file to the handler — `Parameters="%1"`

Being the default app is also what makes "open" tricky. A packaged default app is
normally launched through the WinRT **activation contract**: the shell starts it
as an out-of-process COM server (command line `… -ServerName:App.AppX….mca`, *not*
the file path) and delivers the file via an inbound COM call. A WinUI/XAML app or
the .NET runtime host services that handshake automatically; a plain Win32/C++
process does not, so `AppInstance.GetActivatedEventArgs()` only ever reports a bare
*Launch* and the shell gives up with *"the application didn't start."* (Even the
Windows App SDK's AppLifecycle API doesn't fix this for a non-XAML app — the
servicing lives in the application framework, not the API.)

The fix is one manifest attribute. Declaring the association as a
`uap3:FileTypeAssociation` with **`Parameters="%1"`** switches a **full-trust**
packaged app off the activation contract: Windows passes the opened file as a
normal command-line argument instead. So `SonyArwOpen.exe` just reads `argv[1]` —
no WinRT, no COM server, no extra dependency — and as a `/SUBSYSTEM:WINDOWS` app it
shows no console window (no flash). One catch: `Parameters` is only honoured when
the app's `EntryPoint` is exactly `Windows.FullTrustApplication` (the manifest
validator is case-sensitive here).

The pass-through viewer setting can't live in `HKCU` for the same packaging
reason: an MSIX process gets a **virtualized registry** and can't see a
`Software\SonyArwView` key written by the (non-packaged) `Set-Viewer` script. Its
**file system isn't** virtualized, though, and `%USERPROFILE%` resolves to the same
real profile for both — so the setting is a file at
`%USERPROFILE%\.sonyarwview\viewer.txt`.

---

## Orientation

The A7 V's embedded preview JPEG carries **no EXIF orientation** — the rotation
flag lives in the **ARW container's IFD0** (tag `0x0112`). Portrait shots are
stored as 7008×4672 landscape with `orientation = 6/8`. We handle this in both
paths:

* **Thumbnail:** the extractor reads IFD0 orientation; the provider scales the
  frame, materialises it, then flip/rotates (scaling first and materialising
  avoids an unstable scaler-on-flip-rotator chain) → upright tiles.
* **Open:** the open handler injects a minimal EXIF APP1 Orientation segment into
  the temp JPEG (lossless, no re-encode) so Photos auto-rotates it.

---

## Coexistence with the Microsoft Raw Image Extension

You do **not** have to remove it. It's a codec; SonyArwView is a thumbnail
handler + default-app, and the two are orthogonal. Verified with both installed:
A7 V `.arw` thumbnails render via SonyArwView while the Microsoft extension still
serves your other RAW formats.

---

## Safety properties

The COM/WIC/shell DLLs never throw across COM boundaries (all paths return
`HRESULT`), validate pointers, ref-count correctly, spawn no threads, touch no
network, write to no photo folders, link the **static CRT** (no VC++ redist), and
fail fast on malformed input. Diagnostic logging is opt-in via environment
variables and off by default.
