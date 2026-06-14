# SonyArwView — Registry & System Footprint

Everything this project writes, and how to remove it. All registration is
**per-user** except the signing certificate (machine cert store, for the signed
installer). No HKLM class registration is required.

---

## Stable identifiers

| Name | GUID |
|---|---|
| WIC decoder CLSID | `{622A9BFD-B2BB-414C-A69C-A4E9144642B1}` |
| WIC container format | `{D545ECCD-A3B4-440A-B62A-EBD4BE5EDA27}` |
| Vendor | `{C52660B5-95E5-4F8A-A907-A9D7280A1DB8}` |
| Thumbnail provider CLSID | `{7A501F17-2299-40E4-ABC8-C1FECD9D10D2}` |
| WIC decoder category (well-known) | `{7ED96837-96F0-4812-B211-F13C24117ED3}` |
| `IThumbnailProvider` ShellEx (well-known) | `{E357FCCD-A995-4576-B01F-234630154E96}` |

---

## 1. WIC decoder (classic, `register-wic.ps1`)

Under `HKCU\Software\Classes`:

```
CLSID\{622A9BFD-…}\                      (default) = "Sony ARW Embedded Preview WIC Decoder"
      \InprocServer32\                   (default) = <path to SonyArwWicDecoder.dll>
                                         ThreadingModel = Both
      values: FriendlyName, Vendor, ContainerFormat, MimeTypes(image/x-sony-arw),
              FileExtensions(.arw,.ARW), Description, Author
      \Formats\{pixelformat}\            (24bppBGR, 32bppBGRA, 8bppGray)
      \Patterns\0\                       Position=0 Length=4 Pattern=II*\0  Mask=FFFFFFFF
      \Patterns\1\                       Position=0 Length=4 Pattern=MM\0*  Mask=FFFFFFFF

CLSID\{7ED96837-…}\Instance\{622A9BFD-…}\  CLSID + FriendlyName   (WIC decoder category)
```

Remove: `unregister-wic.ps1` (calls `DllUnregisterServer`, then deletes the
keys above as a fallback).

---

## 2. Thumbnail provider — classic (reference only, `register-thumbnail.ps1`)

> Not used by the shipping (packaged) solution; kept for non-packaged scenarios.

Under `HKCU\Software\Classes`:

```
CLSID\{7A501F17-…}\InprocServer32\       (default)=<dll>, ThreadingModel=Apartment
.arw\ShellEx\{E357FCCD-…}\               (default) = {7A501F17-…}
SystemFileAssociations\.arw\ShellEx\{E357FCCD-…}\  (default) = {7A501F17-…}
```

Remove: `unregister-thumbnail.ps1`.

---

## 3. Thumbnail provider — packaged (the shipping solution)

Installed as an **MSIX package** (no direct registry writes by us — the AppX
runtime owns it):

* Package identity: `SonyArwView_…` (family name `SonyArwView_<hash>`).
* Declares: a `windows.comServer` (`com:SurrogateServer` → our CLSID + DLL),
  and a `windows.fileTypeAssociation` for `.arw` with a
  `desktop2:ThumbnailHandler` pointing at `{7A501F17-…}`.
* Adds an `AppX…` ProgId to `HKCU\Software\Classes\.arw\OpenWithProgids`.

**Default-app choice** (set once via the UI), at:

```
HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.arw\UserChoice
    ProgId = AppX…   (your package's id)   Hash = <Windows-computed, protected>
```

Remove: `unregister-msix-thumbnail.ps1` (or `Uninstall.ps1`) →
`Remove-AppxPackage` + clears the `UserChoice` key.

---

## 4. Signed-installer extras (`Install.ps1`)

* Self-signed code-signing cert imported to
  `Cert:\LocalMachine\TrustedPeople` (subject `CN=SonyArwView`). Needed so the
  signed `.msix` is trusted without Developer Mode. `Uninstall.ps1` removes it.

---

## 5. Developer-mode (dev workflow only)

`register-msix-thumbnail.ps1` requires:

```
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock
    AllowDevelopmentWithoutDevLicense = 1   (DWORD)
    AllowAllTrustedApps               = 1   (DWORD)
```

(Set with admin. Not needed for the signed installer.)

---

## Full clean removal

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Uninstall.ps1   # package + cert + WIC + default
powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1
```

Nothing is written to user photo folders. No services, no background threads, no
network access.
