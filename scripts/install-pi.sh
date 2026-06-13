#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
INSTALL_DIR="${1:-$HOME/agile-tracker}"
REPO="https://github.com/waveshare/e-Paper.git"

echo "==> Installing system packages"
sudo apt-get update
sudo apt-get install -y python3-pip python3-pil python3-venv git fonts-dejavu-core

echo "==> Enabling SPI (if not already)"
if command -v raspi-config >/dev/null 2>&1; then
  sudo raspi-config nonint do_spi 0 || true
fi

echo "==> Cloning Waveshare e-Paper library"
TMP="$(mktemp -d)"
git clone --depth 1 "$REPO" "$TMP/e-Paper"
sudo pip3 install --break-system-packages spidev RPi.GPIO 2>/dev/null \
  || sudo pip3 install spidev RPi.GPIO
sudo python3 "$TMP/e-Paper/RaspberryPi_JetsonNano/python/setup.py" install
rm -rf "$TMP"

echo "==> Setting up tracker in $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
rsync -a --exclude preview --exclude .venv --exclude __pycache__ \
  --exclude .arduino --exclude _waveshare_ref \
  --exclude tracker/lvgl8 --exclude tracker/lvgl9 \
  --exclude 'esp32/Agile_Tracker/libraries/lvgl' \
  --exclude 'esp32/Agile_Tracker/libraries/XPowersLib' \
  --exclude 'esp32/Agile_Tracker/libraries/ArduinoJson' \
  "$PROJECT_DIR/" "$INSTALL_DIR/" 2>/dev/null \
  || cp -r "$PROJECT_DIR"/* "$INSTALL_DIR/"

cd "$INSTALL_DIR"
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

echo "==> Testing Waveshare driver (optional — cancel if no display attached)"
echo "    Try: python3 $TMP/e-Paper/.../epd_2in13_V4_test.py"
echo "    If blank/wrong, try epd_2in13_V2_test.py and set epd_driver: V2 in config.yaml"

echo "==> Preview render (no hardware)"
.venv/bin/python main.py --preview --once

echo ""
echo "Done. Next steps:"
echo "  cd $INSTALL_DIR"
echo "  source .venv/bin/activate"
echo "  python main.py              # run on e-ink"
echo "  sudo cp systemd/agile-tracker.service /etc/systemd/system/"
echo "  sudo systemctl enable --now agile-tracker"
