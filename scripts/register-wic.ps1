<#
.SYNOPSIS
    Registers the Sony ARW WIC decoder (per-user, no admin required).

.DESCRIPTION
    Calls DllRegisterServer (via regsvr32) on SonyArwWicDecoder.dll. The DLL
    writes only to HKCU\Software\Classes, so elevation is NOT required.

    By default it locates the Release DLL produced by the CMake build. Pass
    -DllPath to register a specific build (e.g. the Debug build).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\register-wic.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\register-wic.ps1 -DllPath C:\path\SonyArwWicDecoder.dll
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

if (-not $DllPath -or -not (Test-Path $DllPath)) {
    Write-Error "SonyArwWicDecoder.dll not found. Build first (cmake --build build --config Release) or pass -DllPath."
    exit 1
}

$DllPath = (Resolve-Path $DllPath).Path
Write-Host "Registering WIC decoder: $DllPath" -ForegroundColor Cyan

# regsvr32 /s = silent. The DLL's DllRegisterServer writes to HKCU only.
$proc = Start-Process -FilePath 'regsvr32.exe' -ArgumentList '/s', "`"$DllPath`"" -Wait -PassThru
if ($proc.ExitCode -ne 0) {
    Write-Error "regsvr32 failed with exit code $($proc.ExitCode)."
    exit $proc.ExitCode
}

Write-Host "Registered. Verify with:" -ForegroundColor Green
Write-Host "  .\build\src\WicDecodeTest\Release\WicDecodeTest.exe --list-decoders"
Write-Host "  .\build\src\WicDecodeTest\Release\WicDecodeTest.exe --force-clsid <input.ARW> out.png"
