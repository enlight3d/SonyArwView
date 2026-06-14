<#
.SYNOPSIS
    Install the Sony ARW preview solution (signed MSIX thumbnail provider +
    optional classic WIC decoder).

.DESCRIPTION
    1. Trusts the package's signing certificate (machine store; one UAC prompt).
    2. Installs the signed MSIX (App Installer) — gives Explorer thumbnails and
       the .arw "open in Photos" handler.
    3. Optionally registers the classic WIC decoder for desktop WIC apps.
    4. Refreshes the Explorer thumbnail cache.

    After install you must set our app as the default for .arw (one-time, in the
    UI) so Windows routes thumbnails to our handler — see the printed final step.

    Run build-package.ps1 first to produce the signed package.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\Install.ps1
#>
[CmdletBinding()]
param([switch]$SkipWicDecoder)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $repo 'build\installer'
$msix = Join-Path $out 'SonyArwThumbnail.msix'
$cer  = Join-Path $out 'SonyArwThumbnail.cer'
if (-not (Test-Path $msix)) { Write-Error "Package not found. Run build-package.ps1 first."; exit 1 }

# 1. Trust the signing certificate (LocalMachine\TrustedPeople) — needs admin.
$thumb = (Get-PfxCertificate $cer).Thumbprint
if (-not (Test-Path "Cert:\LocalMachine\TrustedPeople\$thumb")) {
    Write-Host "Trusting signing certificate (approve the UAC prompt)..." -ForegroundColor Cyan
    $cmd = "Import-Certificate -FilePath '$cer' -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null"
    Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', $cmd -Wait
}

# 2. Install the signed package (replace any prior dev registration first).
Get-AppxPackage -Name 'SonyArwView' | Remove-AppxPackage -ErrorAction SilentlyContinue
Write-Host "Installing signed package..." -ForegroundColor Cyan
Add-AppxPackage -Path $msix
Write-Host "Package installed (visible in Settings > Apps > Installed apps)." -ForegroundColor Green

# 3. Optional: classic WIC decoder for desktop WIC apps.
if (-not $SkipWicDecoder) {
    $wic = Join-Path $repo 'build\src\SonyArwWicDecoder\Release\SonyArwWicDecoder.dll'
    if (Test-Path $wic) {
        Start-Process regsvr32 -ArgumentList '/s', "`"$wic`"" -Wait
        Write-Host "WIC decoder registered (per-user)." -ForegroundColor Green
    }
}

# 4. Refresh thumbnail cache + Explorer.
& (Join-Path $PSScriptRoot 'clear-thumbnail-cache.ps1') | Out-Null

# 5. Open Settings to SonyArwView's default-apps page so the user can set it as
#    the default for .arw (required for thumbnails; Windows has no silent API).
Write-Host ""
Write-Host "==================================================================" -ForegroundColor Yellow
Write-Host " FINAL STEP (one time): set SonyArwView as the default for .arw." -ForegroundColor Yellow
Write-Host " Opening Settings to the right page now -- click the '.arw' entry" -ForegroundColor Yellow
Write-Host " and choose 'SonyArwView - Sony ARW Preview'." -ForegroundColor Yellow
Write-Host " (Required: Windows ties the thumbnail handler to the default app.)" -ForegroundColor Yellow
Write-Host "==================================================================" -ForegroundColor Yellow
& (Join-Path $PSScriptRoot 'set-default.ps1') | Out-Null
