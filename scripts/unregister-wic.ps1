<#
.SYNOPSIS
    Unregisters the Sony ARW WIC decoder (per-user).

.DESCRIPTION
    Calls DllUnregisterServer (via regsvr32) which deletes every key the
    decoder created under HKCU\Software\Classes.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\unregister-wic.ps1
#>
[CmdletBinding()]
param(
    [string]$DllPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $DllPath) {
    $candidates = @(
        (Join-Path $repoRoot 'build\src\SonyArwWicDecoder\Release\SonyArwWicDecoder.dll'),
        (Join-Path $repoRoot 'build\src\SonyArwWicDecoder\Debug\SonyArwWicDecoder.dll'),
        (Join-Path $repoRoot 'build\Release\SonyArwWicDecoder.dll')
    )
    $DllPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if ($DllPath -and (Test-Path $DllPath)) {
    $DllPath = (Resolve-Path $DllPath).Path
    Write-Host "Unregistering WIC decoder: $DllPath" -ForegroundColor Cyan
    $proc = Start-Process -FilePath 'regsvr32.exe' -ArgumentList '/s', '/u', "`"$DllPath`"" -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        Write-Warning "regsvr32 /u returned exit code $($proc.ExitCode). Falling back to manual key removal."
    }
}

# Belt-and-suspenders: remove the keys directly in case the DLL is missing.
$decoderClsid = '{622A9BFD-B2BB-414C-A69C-A4E9144642B1}'
$catid        = '{7ED96837-96F0-4812-B211-F13C24117ED3}'
$paths = @(
    "HKCU:\Software\Classes\CLSID\$decoderClsid",
    "HKCU:\Software\Classes\CLSID\$catid\Instance\$decoderClsid"
)
foreach ($p in $paths) {
    if (Test-Path $p) {
        Remove-Item -Path $p -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "Removed $p"
    }
}

Write-Host "Unregistered." -ForegroundColor Green
