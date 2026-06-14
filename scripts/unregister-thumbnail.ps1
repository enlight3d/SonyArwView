<#
.SYNOPSIS
    Unregisters the Sony ARW shell thumbnail provider (per-user).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\unregister-thumbnail.ps1
#>
[CmdletBinding()]
param([string]$DllPath)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $DllPath) {
    $candidates = @(
        (Join-Path $repoRoot 'build\src\SonyArwThumbnailProvider\Release\SonyArwThumbnailProvider.dll'),
        (Join-Path $repoRoot 'build\src\SonyArwThumbnailProvider\Debug\SonyArwThumbnailProvider.dll')
    )
    $DllPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if ($DllPath -and (Test-Path $DllPath)) {
    $DllPath = (Resolve-Path $DllPath).Path
    Write-Host "Unregistering thumbnail provider: $DllPath" -ForegroundColor Cyan
    $proc = Start-Process -FilePath 'regsvr32.exe' -ArgumentList '/s', '/u', "`"$DllPath`"" -Wait -PassThru
    if ($proc.ExitCode -ne 0) { Write-Warning "regsvr32 /u returned $($proc.ExitCode); removing keys manually." }
}

# Belt-and-suspenders manual cleanup.
$clsid   = '{7A501F17-2299-40E4-ABC8-C1FECD9D10D2}'
$shellex = '{E357FCCD-A995-4576-B01F-234630154E96}'
$paths = @(
    "HKCU:\Software\Classes\CLSID\$clsid",
    "HKCU:\Software\Classes\.arw\ShellEx\$shellex",
    "HKCU:\Software\Classes\SystemFileAssociations\.arw\ShellEx\$shellex"
)
foreach ($p in $paths) {
    if (Test-Path $p) {
        Remove-Item -Path $p -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "Removed $p"
    }
}
Write-Host "Unregistered. Clear the thumbnail cache to drop any cached thumbnails." -ForegroundColor Green
