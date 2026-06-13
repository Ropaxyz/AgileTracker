from __future__ import annotations

import logging
from pathlib import Path

from PIL import Image

from tracker.config import Settings

logger = logging.getLogger(__name__)


class EpaperDisplay:
    """Thin wrapper around Waveshare 2.13\" drivers (V2 / V4)."""

    def __init__(self, settings: Settings) -> None:
        self.settings = settings
        self._epd = None
        self._driver_name = settings.epd_driver
        self._partial_count = 0

    def _load_driver(self):
        driver = self._driver_name
        errors: list[str] = []

        import importlib

        for candidate in (driver, "V4", "V2"):
            if candidate in errors:
                continue
            module_name = f"epd2in13_{candidate}"
            try:
                module = importlib.import_module(f"waveshare_epd.{module_name}")
                epd_class = getattr(module, "EPD")
                logger.info("Using Waveshare driver epd2in13_%s", candidate)
                self._driver_name = candidate
                return epd_class()
            except Exception as exc:  # noqa: BLE001 — try fallbacks
                errors.append(candidate)
                logger.debug("Driver %s unavailable: %s", candidate, exc)

        raise RuntimeError(
            "Waveshare e-Paper library not found. Run scripts/install-pi.sh on the Pi, "
            f"or set epd_driver in config.yaml. Tried: {', '.join(errors)}"
        )

    @property
    def epd(self):
        if self._epd is None:
            self._epd = self._load_driver()
        return self._epd

    def show(self, image: Image.Image, *, full_refresh: bool) -> None:
        epd = self.epd
        buffer = epd.getbuffer(image)

        if full_refresh:
            epd.init(epd.FULL_UPDATE)
            epd.display(buffer)
            self._partial_count = 0
            return

        epd.init(epd.PART_UPDATE)
        epd.displayPartial(buffer)
        self._partial_count += 1

    def sleep(self) -> None:
        try:
            self.epd.sleep()
        except Exception:  # noqa: BLE001
            pass


class PreviewDisplay:
    """Save PNG previews when developing off the Pi."""

    def __init__(self, output_dir: Path) -> None:
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self._partial_count = 0

    def show(self, image: Image.Image, *, full_refresh: bool) -> None:
        path = self.output_dir / "latest.png"
        rgb = image.convert("L")
        rgb.save(path)
        logger.info("Preview saved to %s (full=%s)", path, full_refresh)
        self._partial_count += 1

    def sleep(self) -> None:
        return
