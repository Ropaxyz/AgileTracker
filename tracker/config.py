from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CONFIG_PATH = ROOT / "config.yaml"


@dataclass
class Settings:
    product_code: str = "AGILE-24-10-01"
    region: str = "N"
    postcode: str = "KA198AY"
    epd_driver: str = "V4"
    rate_poll_seconds: int = 900
    screen_rotate_seconds: int = 30
    full_refresh_every: int = 10
    screens: list[str] = field(default_factory=lambda: ["now", "today", "tomorrow"])

    @property
    def tariff_code(self) -> str:
        return f"E-1R-{self.product_code}-{self.region}"

    @property
    def rates_url(self) -> str:
        base = "https://api.octopus.energy/v1"
        return (
            f"{base}/products/{self.product_code}/electricity-tariffs/"
            f"{self.tariff_code}/standard-unit-rates/"
        )


def load_settings(path: Path | None = None) -> Settings:
    config_path = path or DEFAULT_CONFIG_PATH
    if not config_path.exists():
        return Settings()

    with config_path.open(encoding="utf-8") as handle:
        raw: dict[str, Any] = yaml.safe_load(handle) or {}

    return Settings(
        product_code=str(raw.get("product_code", Settings.product_code)),
        region=str(raw.get("region", Settings.region)),
        postcode=str(raw.get("postcode", Settings.postcode)),
        epd_driver=str(raw.get("epd_driver", Settings.epd_driver)).upper(),
        rate_poll_seconds=int(raw.get("rate_poll_seconds", Settings.rate_poll_seconds)),
        screen_rotate_seconds=int(
            raw.get("screen_rotate_seconds", Settings.screen_rotate_seconds)
        ),
        full_refresh_every=int(raw.get("full_refresh_every", Settings.full_refresh_every)),
        screens=list(raw.get("screens", Settings().screens)),
    )
