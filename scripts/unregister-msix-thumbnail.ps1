<#
.SYNOPSIS
    Unregister the packaged (MSIX) Sony ARW thumbnail provider and restore the
    default .arw handler.

.DESCRIPTION
    Removes the SonyArwView package (which also removes its
    thumbnail handler and file association) and clears the per-user default
    choice for .arw so Windows reverts to its prior behaviour. Clearing the
    thumbnail cache afterwards is recommended.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\unregister-msix-thumbnail.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'SilentlyContinue'

$pkg = Get-AppxPackage -Name "SonyArwView"
if ($pkg) {
    Write-Host "Removing package $($pkg.PackageFullName) ..." -ForegroundColor Cyan
    Remove-AppxPackage -Package $pkg.PackageFullName
    Write-Host "Removed." -ForegroundColor Green
} else {
    Write-Host "Package not installed." -ForegroundColor Yellow
}

# Clear the per-user default choice for .arw (it pointed at our now-removed app).
$ucKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.arw\UserChoice"
if (Test-Path $ucKey) {
    Remove-Item -Path $ucKey -Recurse -Force
    Write-Host "Cleared .arw UserChoice (default app)." -ForegroundColor Green
}

Write-Host ""
Write-Host "Done. Recommended: clear the thumbnail cache and restart Explorer:" -ForegroundColor Green
Write-Host "  powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1"
Write-Host "You can re-pick a default app for .arw via right-click > Open with > Choose another app."
