<#
.SYNOPSIS
    Choose which app SonyArwView hands an .arw to when you open one ("pass-through").

.DESCRIPTION
    If you have a viewer that opens Sony .arw natively (FastStone, Lightroom, …),
    point SonyArwView at it. Double-clicking an .arw then launches THAT viewer
    with the ORIGINAL file, so you keep full-folder browsing and side-by-side
    compare — while SonyArwView still provides the Explorer thumbnails.

    Without this, opening an .arw extracts the embedded preview to %TEMP% and
    opens it in your default image app (the only option for apps like Photos that
    can't decode .arw themselves).

    Stored in %USERPROFILE%\.sonyarwview\viewer.txt (per-user, no admin). A file
    is used rather than the registry because the packaged (MSIX) open handler runs
    with a virtualized registry and cannot see an HKCU key written here, but it can
    read this file (its %USERPROFILE% resolves to the same real profile).

.PARAMETER Path
    Full path to the viewer executable.

.PARAMETER Clear
    Remove the setting and revert to extract-to-default-viewer behavior.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\Set-Viewer.ps1 -Path "C:\Program Files\FastStone Image Viewer\FSViewer.exe"

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\Set-Viewer.ps1 -Clear
#>
[CmdletBinding(DefaultParameterSetName = 'Set')]
param(
    [Parameter(ParameterSetName = 'Set', Mandatory, Position = 0)][string]$Path,
    [Parameter(ParameterSetName = 'Clear')][switch]$Clear
)

$dir  = Join-Path $env:USERPROFILE '.sonyarwview'
$file = Join-Path $dir 'viewer.txt'

if ($Clear) {
    if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force -ErrorAction SilentlyContinue }
    Write-Host "Cleared. Opening an .arw will extract the preview and use your default image app." -ForegroundColor Green
    return
}

if (-not (Test-Path -LiteralPath $Path)) { Write-Error "Viewer not found: $Path"; exit 1 }
$full = (Resolve-Path -LiteralPath $Path).Path

New-Item -ItemType Directory -Path $dir -Force | Out-Null
# UTF-8 without BOM, no trailing newline (the handler trims, but keep it clean).
[System.IO.File]::WriteAllText($file, $full, (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Done. Opening an .arw will now launch:" -ForegroundColor Green
Write-Host "  $full"
Write-Host "with the original file (so you can browse the whole folder). Explorer thumbnails are unaffected."
