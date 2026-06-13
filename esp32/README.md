# ESP32-C6 Agile Tracker

Firmware for **Waveshare ESP32-C6-Touch-AMOLED-2.16** (480×480 round AMOLED + touch).

Full setup, features, and troubleshooting: see the **[repo README](../README.md)**.

## Quick start

```powershell
# From repo root
powershell -ExecutionPolicy Bypass -File scripts\install-arduino-libraries.ps1
copy esp32\Agile_Tracker\secrets.h.example esp32\Agile_Tracker\secrets.h
# edit secrets.h with your WiFi
```

Open `Agile_Tracker/Agile_Tracker.ino` in Arduino IDE.

- Board: **ESP32C6 Dev Module**
- arduino-esp32: **v3.3.0+**
- Serial monitor: **115200**

Do **not** install generic LVGL or ArduinoJson from Library Manager — the install script provides everything needed.

## Tabs

**Now** | **Today** | **Next** | **Set**

Rates refresh every 15 minutes from the Octopus public API.
