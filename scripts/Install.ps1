<#
.SYNOPSIS
    Install SonyArwView: trust the signing certificate, install the signed MSIX,
    (optionally) register the classic WIC decoder, refresh thumbnails, and open
    Settings so you can set SonyArwView as the default .arw app.

.DESCRIPTION
    Works two ways with no changes:
      * From a release: place SonyArwView.msix + SonyArwView.cer next to this
        script and run it.
      * From source: run scripts\build-package.ps1 first, then this.

    No Developer Mode required (the package is signed; the cert is trusted into
    LocalMachine\TrustedPeople via one UAC prompt).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\Install.ps1
#>
[CmdletBinding()]
param([switch]$SkipWicDecoder)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = Split-Path -Parent $here

function Find-First([string[]]$names) {
    foreach ($d in @($here, (Join-Path $repo 'build\installer'),
                     (Join-Path $repo 'build\src\SonyArwWicDecoder\Release'))) {
        foreach ($n in $names) { $p = Join-Path $d $n; if (Test-Path $p) { return (Resolve-Path $p).Path } }
    }
    return $null
}

$msix = Find-First @('SonyArwView.msix', 'SonyArwThumbnail.msix')
$cer  = Find-First @('SonyArwView.cer',  'SonyArwThumbnail.cer')
if (-not $msix) { Write-Error "SonyArwView.msix not found. Build it (scripts\build-package.ps1) or place it next to this script."; exit 1 }
if (-not $cer)  { Write-Error "SonyArwView.cer not found next to the package."; exit 1 }

# 1. Trust the signing certificate (LocalMachine\TrustedPeople) -- needs admin.
$thumb = (Get-PfxCertificate $cer).Thumbprint
if (-not (Test-Path "Cert:\LocalMachine\TrustedPeople\$thumb")) {
    Write-Host "Trusting the signing certificate (approve the UAC prompt)..." -ForegroundColor Cyan
    $cmd = "Import-Certificate -FilePath '$cer' -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null"
    Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', $cmd -Wait
}

# 2. Install the signed package (replace any prior version first).
Get-AppxPackage -Name 'SonyArwView' | Remove-AppxPackage -ErrorAction SilentlyContinue
Write-Host "Installing SonyArwView..." -ForegroundColor Cyan
Add-AppxPackage -Path $msix
Write-Host "Installed (see Settings > Apps > Installed apps)." -ForegroundColor Green

# 3. Optional: classic WIC decoder for desktop WIC apps (if present).
if (-not $SkipWicDecoder) {
    $wic = Find-First @('SonyArwWicDecoder.dll')
    if ($wic) { Start-Process regsvr32 -ArgumentList '/s', "`"$wic`"" -Wait; Write-Host "WIC decoder registered (per-user)." -ForegroundColor Green }
}

# 4. Refresh the Explorer thumbnail cache.
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Get-ChildItem "$env:LOCALAPPDATA\Microsoft\Windows\Explorer" -Filter 'thumbcache_*.db' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
Start-Process explorer.exe

# 5. Open Settings to SonyArwView's default-apps page for the one-time choice.
$pkg = Get-AppxPackage -Name 'SonyArwView'
if ($pkg) { Start-Process "ms-settings:defaultapps?registeredAUMID=$($pkg.PackageFamilyName)!App" }

Write-Host ""
Write-Host "==================================================================" -ForegroundColor Yellow
Write-Host " LAST STEP (one click): in the Settings page that just opened," -ForegroundColor Yellow
Write-Host " set  .arw  to  'SonyArwView - Sony ARW Preview'." -ForegroundColor Yellow
Write-Host " (Required: Windows resolves the thumbnail handler from the default" -ForegroundColor Yellow
Write-Host "  app, and protects that choice so it can't be set silently.)" -ForegroundColor Yellow
Write-Host "==================================================================" -ForegroundColor Yellow
