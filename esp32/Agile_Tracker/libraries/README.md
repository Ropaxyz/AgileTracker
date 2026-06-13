# Sketch libraries (not in git)

This folder is populated by the install script. Do not commit the large
third-party trees to GitHub.

From the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\install-arduino-libraries.ps1
```

That installs:

- **LVGL v9** (Waveshare preconfigured copy)
- **XPowersLib** (AXP2101 PMIC)

into both `esp32/Agile_Tracker/libraries/` and `.arduino/libraries/`.

After install, restart Arduino IDE completely, then open
`esp32/Agile_Tracker/Agile_Tracker.ino`.
