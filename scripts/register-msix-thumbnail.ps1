<#
.SYNOPSIS
    Stage and register the Sony ARW thumbnail provider as a packaged MSIX app
    (loose/unsigned, via Developer Mode). This is the ONLY way a third-party
    thumbnail handler can compete with the packaged image apps that own .arw.

.DESCRIPTION
    Requires Developer Mode (AllowDevelopmentWithoutDevLicense=1). Stages the
    manifest + our native thumbnail DLL + a stub host exe + generated logo
    assets into build\msixstage, then runs Add-AppxPackage -Register on the
    loose AppxManifest.xml (no signing needed in Developer Mode).

    The classic (regsvr32) thumbnail registration is removed first to avoid the
    same CLSID being registered two ways.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\register-msix-thumbnail.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$relDir   = Join-Path $repoRoot 'build\src'
$stage    = Join-Path $repoRoot 'build\msixstage'

$thumbDll = Join-Path $relDir 'SonyArwThumbnailProvider\Release\SonyArwThumbnailProvider.dll'
$hostExe  = Join-Path $relDir 'SonyArwPreviewExtract\Release\SonyArwPreviewExtract.exe'
$manifest = Join-Path $repoRoot 'src\SonyArwThumbnailPackage\AppxManifest.xml'

foreach ($f in @($thumbDll, $hostExe, $manifest)) {
    if (-not (Test-Path $f)) { Write-Error "Missing required file: $f"; exit 1 }
}

# Verify Developer Mode.
$dm = (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" -ErrorAction SilentlyContinue).AllowDevelopmentWithoutDevLicense
if ($dm -ne 1) {
    Write-Warning "Developer Mode is not enabled (AllowDevelopmentWithoutDevLicense != 1)."
    Write-Warning "Enable it in Settings > System > For developers, or set the registry value (admin)."
}

# Remove the classic regsvr32 thumbnail registration so the CLSID isn't double-registered.
& (Join-Path $PSScriptRoot 'unregister-thumbnail.ps1') | Out-Null

# Fresh staging dir.
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stage 'Assets') | Out-Null

Copy-Item $manifest (Join-Path $stage 'AppxManifest.xml') -Force
Copy-Item $thumbDll (Join-Path $stage 'SonyArwThumbnailProvider.dll') -Force
Copy-Item $hostExe  (Join-Path $stage 'SonyArwPreviewExtract.exe') -Force

# Generate simple solid-colour logo assets at the required sizes.
Add-Type -AssemblyName System.Drawing
function New-Png([string]$path, [int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(255, 0, 120, 215))
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
New-Png (Join-Path $stage 'Assets\Square150x150Logo.png') 150 150
New-Png (Join-Path $stage 'Assets\Square44x44Logo.png')   44  44
New-Png (Join-Path $stage 'Assets\StoreLogo.png')         50  50

Write-Host "Registering package from $stage ..." -ForegroundColor Cyan
Add-AppxPackage -Register (Join-Path $stage 'AppxManifest.xml')

Write-Host "Registered. Package:" -ForegroundColor Green
Get-AppxPackage -Name "SonyArwView" | Select-Object Name, Version, PackageFullName, Status | Format-List
