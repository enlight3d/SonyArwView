<#
.SYNOPSIS
    Open Windows Settings to help the user set SonyArwView as the default app
    for .arw (required for Explorer thumbnails).

.DESCRIPTION
    Windows protects the default-app choice with a per-user hash and offers no
    supported silent API. The smoothest path is to deep-link Settings straight
    to SonyArwView's "default apps" page, where the user clicks the .arw entry
    and picks SonyArwView. This script opens that page and prints guidance.

    If the deep link isn't honored on a given build, it falls back to opening the
    "Open with" chooser for a sample .arw (when one is supplied / found).

.PARAMETER SampleArw
    Optional path to an .arw file. If given, also offers the "Open with" chooser
    as an alternative one-click path.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\set-default.ps1
#>
[CmdletBinding()]
param([string]$SampleArw)

$ErrorActionPreference = 'SilentlyContinue'

$pkg = Get-AppxPackage -Name 'SonyArwView'
if (-not $pkg) {
    Write-Warning "SonyArwView is not installed. Run Install.ps1 first."
    return
}
$aumid = "$($pkg.PackageFamilyName)!App"

Write-Host "Opening Settings to set SonyArwView as the default for .arw..." -ForegroundColor Cyan
# Deep-link to this app's "Default apps" page (Windows 11).
Start-Process "ms-settings:defaultapps?registeredAUMID=$aumid"

Write-Host ""
Write-Host "In the Settings page that opens:" -ForegroundColor Yellow
Write-Host "  1. Find the  .arw  file type in the list." -ForegroundColor Yellow
Write-Host "  2. Click it and choose  'SonyArwView - Sony ARW Preview'." -ForegroundColor Yellow
Write-Host ""
Write-Host "Alternatively (often quicker): double-click any .arw file, then pick" -ForegroundColor Gray
Write-Host "'SonyArwView - Sony ARW Preview' and click 'Always'." -ForegroundColor Gray

# Optional: also pop the classic Open-With chooser for a concrete file.
if ($SampleArw -and (Test-Path $SampleArw)) {
    $full = (Resolve-Path $SampleArw).Path
    Write-Host ""
    Write-Host "Also opening the 'Open with' chooser for: $full" -ForegroundColor Cyan
    Start-Process rundll32.exe -ArgumentList "shell32.dll,OpenAs_RunDLL", "`"$full`""
}
