#!/usr/bin/env python3
from __future__ import annotations

import argparse
import logging
import sys
import time
from pathlib import Path

from tracker.config import load_settings
from tracker.display_epaper import EpaperDisplay, PreviewDisplay
from tracker.octopus import fetch_agile_rates
from tracker.render import render_screen

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
)
logger = logging.getLogger("agile-tracker")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Octopus Agile e-ink rate tracker")
    parser.add_argument(
        "--config",
        type=Path,
        default=None,
        help="Path to config.yaml",
    )
    parser.add_argument(
        "--preview",
        action="store_true",
        help="Save PNG previews instead of driving the e-ink panel",
    )
    parser.add_argument(
        "--once",
        action="store_true",
        help="Render all screens once and exit",
    )
    parser.add_argument(
        "--screen",
        choices=["now", "today", "tomorrow"],
        default=None,
        help="Show a single screen (with --once)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    settings = load_settings(args.config)

    if args.preview:
        display: EpaperDisplay | PreviewDisplay = PreviewDisplay(Path("preview"))
    else:
        display = EpaperDisplay(settings)

    rates = None
    screen_index = 0
    refresh_count = 0
    last_fetch = 0.0

    screens = [args.screen] if args.screen else settings.screens

    try:
        while True:
            now = time.time()
            if rates is None or (now - last_fetch) >= settings.rate_poll_seconds:
                logger.info("Fetching Agile rates for %s", settings.tariff_code)
                rates = fetch_agile_rates(settings)
                last_fetch = now

            screen_name = screens[screen_index % len(screens)]
            image = render_screen(screen_name, rates)
            full_refresh = refresh_count == 0 or (
                refresh_count % settings.full_refresh_every == 0
            )
            display.show(image, full_refresh=full_refresh)
            logger.info("Showing screen %s (full_refresh=%s)", screen_name, full_refresh)
            refresh_count += 1

            if args.once:
                if args.screen:
                    break
                screen_index += 1
                if screen_index >= len(screens):
                    break
                continue

            time.sleep(settings.screen_rotate_seconds)
            screen_index += 1

    except KeyboardInterrupt:
        logger.info("Stopping")
    finally:
        display.sleep()

    return 0


if __name__ == "__main__":
    sys.exit(main())
