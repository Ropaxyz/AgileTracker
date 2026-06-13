# Git / GitHub guide

## Before your first push

### 1. Clean local bulk (recommended)

Removes libraries, reference clones, and caches — keeps only source:

```powershell
cd C:\Master_Code\tracker
powershell -ExecutionPolicy Bypass -File scripts\clean-local-artifacts.ps1
```

### 2. Check secrets are not staged

`secrets.h` contains your WiFi password and **must never** be committed.

```powershell
git status
```

You should **not** see `esp32/Agile_Tracker/secrets.h` in the list. If you do:

```powershell
git rm --cached esp32/Agile_Tracker/secrets.h
```

New clones use `secrets.h.example`:

```powershell
copy esp32\Agile_Tracker\secrets.h.example esp32\Agile_Tracker\secrets.h
# then edit secrets.h with your WiFi details
```

### 3. Initialise and push

```powershell
cd C:\Master_Code\tracker

git init
git add .
git status
# confirm: no secrets.h, no .arduino/, no libraries/lvgl/

git commit -m "Initial commit: Octopus Agile tracker (ESP32 + Pi e-ink)"

# Create an empty repo on GitHub, then:
git branch -M main
git remote add origin https://github.com/YOUR_USER/YOUR_REPO.git
git push -u origin main
```

Replace `YOUR_USER` / `YOUR_REPO` with your GitHub details.

---

## What git tracks vs ignores

| Tracked (in repo) | Ignored (local only) |
|-------------------|----------------------|
| `esp32/Agile_Tracker/*.ino`, `*.cpp`, `*.h` | `secrets.h` |
| `esp32/Agile_Tracker/src/` | `.arduino/` |
| `esp32/Agile_Tracker/libraries/README.md` | `libraries/lvgl/`, `XPowersLib/`, `ArduinoJson/` |
| `esp32/Agile_Tracker/secrets.h.example` | `_waveshare_ref/` |
| `tracker/*.py`, `main.py`, `config.yaml` | `tracker/lvgl8/`, `tracker/lvgl9/` |
| `docs/device.jpeg` (README photo) | `preview/`, `.venv/`, `__pycache__/` |
| `scripts/`, `systemd/`, `requirements.txt` | |

Rules live in `.gitignore`.

---

## Clone on a new PC

```powershell
git clone https://github.com/YOUR_USER/YOUR_REPO.git
cd YOUR_REPO

# ESP32
copy esp32\Agile_Tracker\secrets.h.example esp32\Agile_Tracker\secrets.h
# edit secrets.h
powershell -ExecutionPolicy Bypass -File scripts\install-arduino-libraries.ps1
# open Agile_Tracker.ino in Arduino IDE

# Pi e-ink (optional)
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python main.py --preview --once
```

---

## Day-to-day workflow

```powershell
git status
git add esp32/Agile_Tracker/agile_ui.cpp   # or specific files
git commit -m "Describe what changed and why"
git push
```

Prefer small, focused commits (e.g. “add auto night dim”, “fix Today chart legend”).

---

## If you already committed secrets by mistake

1. Remove from git history (rotate your WiFi password if it was pushed).
2. Ensure `secrets.h` is in `.gitignore`.
3. Use `git filter-repo` or GitHub’s secret scanning guidance — do not leave credentials in public history.

---

## Repo size tip

A clean push after `clean-local-artifacts.ps1` is typically **under 1 MB** of source. Without cleaning, duplicated LVGL trees can push the repo to **hundreds of MB**.
