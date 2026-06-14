<#
.SYNOPSIS
    Clears the Windows Explorer thumbnail cache and restarts Explorer.

.DESCRIPTION
    Explorer aggressively caches thumbnails (including "this file has no
    thumbnail" results). After installing/updating a thumbnail source you must
    clear the cache to see new thumbnails. This stops explorer.exe, deletes the
    thumbcache_*.db files, and restarts Explorer.

    No admin rights required (operates on the current user's cache only).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1
#>
[CmdletBinding()]
param(
    [switch]$NoRestart
)

$ErrorActionPreference = 'SilentlyContinue'

Write-Host "Stopping Explorer..." -ForegroundColor Cyan
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 800

$cacheDir = Join-Path $env:LOCALAPPDATA 'Microsoft\Windows\Explorer'
Write-Host "Deleting thumbnail cache in $cacheDir ..." -ForegroundColor Cyan
$deleted = 0
Get-ChildItem -Path $cacheDir -Filter 'thumbcache_*.db' -ErrorAction SilentlyContinue | ForEach-Object {
    try { Remove-Item $_.FullName -Force -ErrorAction Stop; $deleted++ } catch {}
}
# Icon cache too (sometimes ARW shows a stale generic icon).
Get-ChildItem -Path $cacheDir -Filter 'iconcache_*.db' -ErrorAction SilentlyContinue | ForEach-Object {
    try { Remove-Item $_.FullName -Force -ErrorAction Stop; $deleted++ } catch {}
}
Write-Host "Removed $deleted cache file(s)." -ForegroundColor Green

if (-not $NoRestart) {
    Write-Host "Restarting Explorer..." -ForegroundColor Cyan
    Start-Process explorer.exe
}
Write-Host "Done. Open the samples folder with 'Extra large icons' to check thumbnails." -ForegroundColor Green
