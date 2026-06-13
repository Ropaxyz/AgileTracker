# Enables Montserrat 18 and 22 in lv_conf.h (needed by agile_ui.cpp).
# Run automatically from install-arduino-libraries.ps1.

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

$paths = @(
    (Join-Path $RepoRoot "esp32\Agile_Tracker\libraries\lv_conf.h"),
    (Join-Path $RepoRoot ".arduino\libraries\lv_conf.h")
)

foreach ($path in $paths) {
    if (-not (Test-Path $path)) { continue }
    $text = Get-Content $path -Raw
    $text = $text -replace '#define LV_FONT_MONTSERRAT_18\s+0', '#define LV_FONT_MONTSERRAT_18 1'
    $text = $text -replace '#define LV_FONT_MONTSERRAT_22\s+0', '#define LV_FONT_MONTSERRAT_22 1'
    Set-Content -Path $path -Value $text -NoNewline
    Write-Host "Patched fonts in $path"
}
