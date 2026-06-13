# Installs all libraries needed to compile Agile_Tracker (no Library Manager required).
# Libraries go to:
#   - esp32/Agile_Tracker/libraries/   (beside the sketch)
#   - .arduino/libraries/              (local Arduino sketchbook, not OneDrive)
# Run:  powershell -ExecutionPolicy Bypass -File scripts/install-arduino-libraries.ps1

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$SketchLib = Join-Path $RepoRoot "esp32\Agile_Tracker\libraries"
$LocalSketchbook = Join-Path $RepoRoot ".arduino"
$LocalLib = Join-Path $LocalSketchbook "libraries"
$WaveshareLibSrc = Join-Path $RepoRoot "_waveshare_ref\01_Arduino_Libraries\lvgl9"
$IdeConfig = Join-Path $env:USERPROFILE ".arduinoIDE\arduino-cli.yaml"

New-Item -ItemType Directory -Force -Path $SketchLib | Out-Null
New-Item -ItemType Directory -Force -Path $LocalLib | Out-Null

if (-not (Test-Path $WaveshareLibSrc)) {
    Write-Host "Cloning Waveshare repo..."
    git clone --depth 1 https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16.git (Join-Path $RepoRoot "_waveshare_ref")
    $WaveshareLibSrc = Join-Path $RepoRoot "_waveshare_ref\01_Arduino_Libraries\lvgl9"
}

if (-not (Test-Path "$WaveshareLibSrc\lvgl\lvgl.h")) {
    throw "Missing Waveshare LVGL at $WaveshareLibSrc\lvgl"
}

Write-Host "Removing conflicting global library copies..."
& (Join-Path $RepoRoot "scripts\fix-arduino-conflicts.ps1")

function Install-LvglBundle {
    param([string]$DestLib)
    New-Item -ItemType Directory -Force -Path $DestLib | Out-Null
    if (Test-Path "$DestLib\lvgl") { Remove-Item "$DestLib\lvgl" -Recurse -Force }
    Copy-Item "$WaveshareLibSrc\lvgl" "$DestLib\lvgl" -Recurse
    Copy-Item "$WaveshareLibSrc\lv_conf.h" "$DestLib\lv_conf.h" -Force
}

function Install-XPowersLib {
    param([string]$DestLib)
    New-Item -ItemType Directory -Force -Path $DestLib | Out-Null
    $dest = Join-Path $DestLib "XPowersLib"
    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    git clone --depth 1 https://github.com/lewisxhe/XPowersLib.git $dest
}

Write-Host "Installing LVGL (Waveshare v9)..."
Install-LvglBundle $SketchLib
Install-LvglBundle $LocalLib

Write-Host "Installing XPowersLib..."
Install-XPowersLib $SketchLib
Install-XPowersLib $LocalLib

Write-Host "Patching lv_conf.h (Montserrat 18/22 fonts)..."
& (Join-Path $RepoRoot "scripts\patch-lv-conf.ps1")

Write-Host "Pointing Arduino IDE at local sketchbook: $LocalSketchbook"
$yaml = @"
board_manager:
    additional_urls:
        - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
    builtin:
        libraries: $env:LOCALAPPDATA\Arduino15\libraries
    data: $env:LOCALAPPDATA\Arduino15
    user: $LocalSketchbook
locale: en
"@
New-Item -ItemType Directory -Force -Path (Split-Path $IdeConfig -Parent) | Out-Null
Set-Content -Path $IdeConfig -Value $yaml -Encoding utf8

Write-Host ""
Write-Host "Done."
Write-Host "  Sketch libraries: esp32\Agile_Tracker\libraries\"
Write-Host "  IDE libraries:    .arduino\libraries\"
Write-Host ""
Write-Host "Restart Arduino IDE completely, then compile Agile_Tracker.ino"
Write-Host "Open the sketch from: $RepoRoot\esp32\Agile_Tracker\Agile_Tracker.ino"
