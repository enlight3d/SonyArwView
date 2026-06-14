<#
.SYNOPSIS
    Uninstall the Sony ARW preview solution and restore prior state.

.DESCRIPTION
    Removes the MSIX package, clears the per-user default app for .arw,
    unregisters the classic WIC decoder, removes the trusted signing
    certificate (one UAC prompt), and refreshes the thumbnail cache.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\Uninstall.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'SilentlyContinue'
$repo = Split-Path -Parent $PSScriptRoot

# 1. Remove the MSIX package.
$pkg = Get-AppxPackage -Name 'SonyArwView'
if ($pkg) { Remove-AppxPackage -Package $pkg.PackageFullName; Write-Host "Package removed." -ForegroundColor Green }

# 2. Clear the .arw default-app choice (it pointed at our app).
$uc = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.arw\UserChoice"
if (Test-Path $uc) { Remove-Item $uc -Recurse -Force; Write-Host "Cleared .arw default app." -ForegroundColor Green }

# 3. Unregister the classic WIC decoder.
$wic = Join-Path $repo 'build\src\SonyArwWicDecoder\Release\SonyArwWicDecoder.dll'
if (Test-Path $wic) { Start-Process regsvr32 -ArgumentList '/u', '/s', "`"$wic`"" -Wait }

# 4. Remove the trusted signing certificate (needs admin).
$cer = Join-Path $repo 'build\installer\SonyArwThumbnail.cer'
if (Test-Path $cer) {
    $thumb = (Get-PfxCertificate $cer).Thumbprint
    if (Test-Path "Cert:\LocalMachine\TrustedPeople\$thumb") {
        Write-Host "Removing trusted certificate (approve the UAC prompt)..." -ForegroundColor Cyan
        $cmd = "Remove-Item 'Cert:\LocalMachine\TrustedPeople\$thumb' -Force -ErrorAction SilentlyContinue"
        Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', $cmd -Wait
    }
}

# 5. Refresh thumbnail cache + Explorer.
& (Join-Path $PSScriptRoot 'clear-thumbnail-cache.ps1') | Out-Null
Write-Host "Uninstalled." -ForegroundColor Green
