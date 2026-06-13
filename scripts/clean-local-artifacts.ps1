# Removes large local-only folders so the repo is ready to push to GitHub.
# Your source code and scripts are kept. Libraries can be reinstalled anytime.
#
# Run:  powershell -ExecutionPolicy Bypass -File scripts\clean-local-artifacts.ps1

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

$toRemove = @(
    (Join-Path $RepoRoot ".arduino"),
    (Join-Path $RepoRoot "_waveshare_ref"),
    (Join-Path $RepoRoot "preview"),
    (Join-Path $RepoRoot "tracker\lvgl8"),
    (Join-Path $RepoRoot "tracker\lvgl9"),
    (Join-Path $RepoRoot "esp32\Agile_Tracker\libraries\lvgl"),
    (Join-Path $RepoRoot "esp32\Agile_Tracker\libraries\XPowersLib"),
    (Join-Path $RepoRoot "esp32\Agile_Tracker\libraries\ArduinoJson"),
    (Join-Path $RepoRoot "esp32\Agile_Tracker\libraries\lv_conf.h"),
    (Join-Path $RepoRoot ".venv"),
    (Join-Path $RepoRoot "__pycache__")
)

Write-Host "Cleaning local artifacts in $RepoRoot"
Write-Host ""

foreach ($path in $toRemove) {
    if (Test-Path $path) {
        Remove-Item $path -Recurse -Force
        Write-Host "  Removed  $path"
    }
}

# Remove __pycache__ under tracker/
Get-ChildItem -Path (Join-Path $RepoRoot "tracker") -Recurse -Directory -Filter "__pycache__" -ErrorAction SilentlyContinue |
    ForEach-Object { Remove-Item $_.FullName -Recurse -Force; Write-Host "  Removed  $($_.FullName)" }

Write-Host ""
Write-Host "Done. Safe to commit and push."
Write-Host "Before compiling again, run:"
Write-Host "  powershell -ExecutionPolicy Bypass -File scripts\install-arduino-libraries.ps1"
