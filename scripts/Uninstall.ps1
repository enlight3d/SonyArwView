<#
.SYNOPSIS
    Uninstall SonyArwView and restore prior state.

.DESCRIPTION
    Removes the MSIX package, clears the per-user default app for .arw,
    unregisters the classic WIC decoder (if registered), removes the trusted
    signing certificate (one UAC prompt), and refreshes the thumbnail cache.

    (You can also simply remove "SonyArwView" from Settings > Apps; this script
    additionally cleans up the cert and the .arw default choice.)

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\Uninstall.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'SilentlyContinue'
$here = $PSScriptRoot
$repo = Split-Path -Parent $here

function Find-First([string[]]$names) {
    foreach ($d in @($here, (Join-Path $repo 'build\installer'),
                     (Join-Path $repo 'build\src\SonyArwWicDecoder\Release'))) {
        foreach ($n in $names) { $p = Join-Path $d $n; if (Test-Path $p) { return (Resolve-Path $p).Path } }
    }
    return $null
}

# 1. Remove the package.
$pkg = Get-AppxPackage -Name 'SonyArwView'
if ($pkg) { Remove-AppxPackage -Package $pkg.PackageFullName; Write-Host "Package removed." -ForegroundColor Green }

# 2. Clear the .arw default-app choice (it pointed at SonyArwView).
$uc = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.arw\UserChoice"
if (Test-Path $uc) { Remove-Item $uc -Recurse -Force; Write-Host "Cleared .arw default app." -ForegroundColor Green }

# 3. Unregister the classic WIC decoder if present.
$wic = Find-First @('SonyArwWicDecoder.dll')
if ($wic) { Start-Process regsvr32 -ArgumentList '/u', '/s', "`"$wic`"" -Wait }

# 4. Remove the trusted signing certificate (needs admin).
$cer = Find-First @('SonyArwView.cer', 'SonyArwThumbnail.cer')
if ($cer) {
    $thumb = (Get-PfxCertificate $cer).Thumbprint
    if (Test-Path "Cert:\LocalMachine\TrustedPeople\$thumb") {
        Write-Host "Removing trusted certificate (approve the UAC prompt)..." -ForegroundColor Cyan
        $cmd = "Remove-Item 'Cert:\LocalMachine\TrustedPeople\$thumb' -Force -ErrorAction SilentlyContinue"
        Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', $cmd -Wait
    }
}

# 5. Refresh the thumbnail cache + Explorer.
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Get-ChildItem "$env:LOCALAPPDATA\Microsoft\Windows\Explorer" -Filter 'thumbcache_*.db' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
Start-Process explorer.exe
Write-Host "Uninstalled." -ForegroundColor Green
