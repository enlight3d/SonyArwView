<#
.SYNOPSIS
    Registers the Sony ARW shell thumbnail provider (per-user, no admin).

.DESCRIPTION
    Calls DllRegisterServer (via regsvr32) on SonyArwThumbnailProvider.dll, which
    writes only to HKCU\Software\Classes. After registering you should clear the
    thumbnail cache (scripts\clear-thumbnail-cache.ps1) and restart Explorer.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\register-thumbnail.ps1
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
if (-not $DllPath -or -not (Test-Path $DllPath)) {
    Write-Error "SonyArwThumbnailProvider.dll not found. Build first or pass -DllPath."
    exit 1
}

$DllPath = (Resolve-Path $DllPath).Path
Write-Host "Registering thumbnail provider: $DllPath" -ForegroundColor Cyan
$proc = Start-Process -FilePath 'regsvr32.exe' -ArgumentList '/s', "`"$DllPath`"" -Wait -PassThru
if ($proc.ExitCode -ne 0) { Write-Error "regsvr32 failed: $($proc.ExitCode)"; exit $proc.ExitCode }

Write-Host "Registered. Now clear the thumbnail cache and restart Explorer:" -ForegroundColor Green
Write-Host "  powershell -ExecutionPolicy Bypass -File .\scripts\clear-thumbnail-cache.ps1"
