# Removes conflicting global LVGL copies (generic Library Manager / lvgl8 / OneDrive).
# Run: powershell -ExecutionPolicy Bypass -File scripts/fix-arduino-conflicts.ps1

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$SketchLib = Join-Path $RepoRoot "esp32\Agile_Tracker\libraries"

$GlobalLibDirs = @(
    (Join-Path $env:USERPROFILE "Documents\Arduino\libraries"),
    (Join-Path $env:USERPROFILE "OneDrive\Documents\Arduino\libraries"),
    (Join-Path $env:USERPROFILE "OneDrive\Desktop\Arduino\libraries"),
    (Join-Path $env:LOCALAPPDATA "Arduino15\libraries")
)

Write-Host "Removing conflicting global Agile_Tracker library copies..."
foreach ($dir in $GlobalLibDirs) {
    if (-not (Test-Path $dir)) { continue }
    foreach ($name in @("lvgl8", "lvgl", "XPowersLib", "ArduinoJson")) {
        $path = Join-Path $dir $name
        if (Test-Path $path) {
            Remove-Item $path -Recurse -Force
            Write-Host "  Deleted $path"
        }
    }
    $conf = Join-Path $dir "lv_conf.h"
    if (Test-Path $conf) {
        Remove-Item $conf -Force
        Write-Host "  Deleted $conf"
    }
}

if (-not (Test-Path "$SketchLib\lvgl\lvgl.h")) {
    Write-Host ""
    Write-Host "Waveshare LVGL missing from sketch. Run:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File scripts\install-arduino-libraries.ps1"
}

if (-not (Test-Path "$SketchLib\XPowersLib\src\XPowersLib.h")) {
    Write-Host ""
    Write-Host "XPowersLib missing from sketch. Run:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File scripts\install-arduino-libraries.ps1"
}

Write-Host ""
Write-Host "OK. Use Waveshare libraries from .arduino\libraries (local disk)."
Write-Host "Do not install generic LVGL or ArduinoJson from Library Manager."
Write-Host "Restart Arduino IDE completely, then compile Agile_Tracker.ino"
