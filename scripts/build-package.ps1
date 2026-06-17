<#
.SYNOPSIS
    Build a SIGNED MSIX installer for the Sony ARW thumbnail provider.

.DESCRIPTION
    Packs build\msixstage into SonyArwThumbnail.msix (makeappx), creates a
    self-signed code-signing certificate whose subject matches the package
    Publisher (CN=SonyARWDev), signs the package (signtool), and exports the
    public certificate (.cer) that end users trust once to install.

    Output (build\installer\):
      SonyArwThumbnail.msix   - the installable package
      SonyArwThumbnail.cer    - public cert to trust (Install.ps1 does this)
      SonyArwSign.pfx         - private signing cert (keep local; not for users)

    Prereq: the package must be staged first. If build\msixstage is missing,
    run scripts\register-msix-thumbnail.ps1 once (it stages + dev-registers), or
    this script will stage it for you from the Release build.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\build-package.ps1
#>
[CmdletBinding()]
param([string]$Password = 'SonyArwDev!1')

$ErrorActionPreference = 'Stop'
$repo  = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $repo 'build\msixstage'
$out   = Join-Path $repo 'build\installer'
$relDir = Join-Path $repo 'build\src'

# --- locate SDK tools -------------------------------------------------------
$verBin = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Directory |
          Where-Object { $_.Name -match '^10\.' } | Sort-Object Name -Descending | Select-Object -First 1
$x64 = Join-Path $verBin.FullName 'x64'
$makeappx = Join-Path $x64 'makeappx.exe'
$signtool = Join-Path $x64 'signtool.exe'
foreach ($t in @($makeappx, $signtool)) { if (-not (Test-Path $t)) { Write-Error "Missing SDK tool: $t"; exit 1 } }

# --- stage (always refresh payload from the latest Release build) -----------
Write-Host "Staging package..." -ForegroundColor Cyan
$thumbDll = Join-Path $relDir 'SonyArwThumbnailProvider\Release\SonyArwThumbnailProvider.dll'
$openExe  = Join-Path $relDir 'SonyArwOpen\Release\SonyArwOpen.exe'
$manifest = Join-Path $repo  'src\SonyArwThumbnailPackage\AppxManifest.xml'
foreach ($f in @($thumbDll, $openExe, $manifest)) { if (-not (Test-Path $f)) { Write-Error "Missing $f (build Release first)"; exit 1 } }
if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage 'Assets') | Out-Null
Copy-Item $manifest (Join-Path $stage 'AppxManifest.xml') -Force
Copy-Item $thumbDll (Join-Path $stage 'SonyArwThumbnailProvider.dll') -Force
Copy-Item $openExe  (Join-Path $stage 'SonyArwOpen.exe') -Force
Add-Type -AssemblyName System.Drawing
function New-Png([string]$p,[int]$w,[int]$h){ $b=New-Object System.Drawing.Bitmap($w,$h); $g=[System.Drawing.Graphics]::FromImage($b); $g.Clear([System.Drawing.Color]::FromArgb(255,0,120,215)); $g.Dispose(); $b.Save($p,[System.Drawing.Imaging.ImageFormat]::Png); $b.Dispose() }
New-Png (Join-Path $stage 'Assets\Square150x150Logo.png') 150 150
New-Png (Join-Path $stage 'Assets\Square44x44Logo.png')   44  44
New-Png (Join-Path $stage 'Assets\StoreLogo.png')         50  50

New-Item -ItemType Directory -Force $out | Out-Null
$msix = Join-Path $out 'SonyArwView.msix'
$cer  = Join-Path $out 'SonyArwView.cer'
$pfx  = Join-Path $out 'SonyArwView.pfx'

# --- pack -------------------------------------------------------------------
Write-Host "Packing MSIX..." -ForegroundColor Cyan
& $makeappx pack /o /d $stage /p $msix | Write-Host
if ($LASTEXITCODE -ne 0) { throw "makeappx failed ($LASTEXITCODE)" }

# --- self-signed signing cert (subject MUST equal manifest Publisher) -------
# Reuse an existing CN=SonyArwView cert if present so the package keeps the same
# signer (already trusted by Install.ps1) across rebuilds -- avoids re-trusting.
$cert = Get-ChildItem 'Cert:\CurrentUser\My' |
        Where-Object { $_.Subject -eq 'CN=SonyArwView' -and $_.HasPrivateKey } |
        Select-Object -First 1
if ($cert) {
    Write-Host "Reusing existing signing certificate (CN=SonyArwView)." -ForegroundColor Cyan
} else {
    Write-Host "Creating self-signed signing certificate (CN=SonyArwView)..." -ForegroundColor Cyan
    $cert = New-SelfSignedCertificate -Type Custom -Subject 'CN=SonyArwView' `
        -KeyUsage DigitalSignature -FriendlyName 'SonyArwView Signing' `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
}
$sec = ConvertTo-SecureString -String $Password -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $sec | Out-Null
Export-Certificate   -Cert $cert -FilePath $cer | Out-Null

# --- sign -------------------------------------------------------------------
Write-Host "Signing package..." -ForegroundColor Cyan
& $signtool sign /fd SHA256 /f $pfx /p $Password $msix | Write-Host
if ($LASTEXITCODE -ne 0) { throw "signtool failed ($LASTEXITCODE)" }

Write-Host ""
Write-Host "Built installer:" -ForegroundColor Green
Write-Host "  Package : $msix"
Write-Host "  Cert    : $cer"
Write-Host "Install with: powershell -ExecutionPolicy Bypass -File .\scripts\Install.ps1"
