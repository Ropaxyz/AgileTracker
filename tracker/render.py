from __future__ import annotations

from datetime import datetime, timedelta
from zoneinfo import ZoneInfo

from PIL import Image, ImageDraw, ImageFont

from tracker.octopus import AgileRates, RateSlot

UK = ZoneInfo("Europe/London")
WIDTH = 250
HEIGHT = 122


def _fonts() -> tuple[ImageFont.FreeTypeFont | ImageFont.ImageFont, ...]:
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    ]
    for path in candidates:
        try:
            return (
                ImageFont.truetype(path, 18),
                ImageFont.truetype(path, 11),
                ImageFont.truetype(path, 9),
            )
        except OSError:
            continue
    default = ImageFont.load_default()
    return default, default, default


def _blank() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("1", (WIDTH, HEIGHT), 255)
    return image, ImageDraw.Draw(image)


def _header(draw: ImageDraw.ImageDraw, title: str, font_small) -> None:
    draw.text((4, 2), title, font=font_small, fill=0)
    draw.line((0, 14, WIDTH, 14), fill=0, width=1)


def _footer(draw: ImageDraw.ImageDraw, text: str, font_tiny) -> None:
    draw.text((4, HEIGHT - 11), text, font=font_tiny, fill=0)


def _format_pence(value: float) -> str:
    return f"{value:.1f}p"


def render_now(rates: AgileRates) -> Image.Image:
    font_large, font_small, font_tiny = _fonts()
    image, draw = _blank()
    now = datetime.now(tz=UK)
    current = rates.current_slot(now)
    nxt = rates.next_slot(now)
    today = rates.slots_on(now.date())
    bounds = AgileRates.min_max(today)
    cheap = rates.cheapest_in_hours(6, now)

    _header(draw, "Agile · Southern Scotland", font_small)

    if current:
        draw.text((4, 22), _format_pence(current.pence_inc_vat), font=font_large, fill=0)
        draw.text((4, 44), f"until {current.end.astimezone(UK).strftime('%H:%M')}", font=font_tiny, fill=0)
    else:
        draw.text((4, 26), "No current slot", font=font_small, fill=0)

    if nxt:
        draw.text(
            (4, 58),
            f"Next {_format_pence(nxt.pence_inc_vat)} @ {nxt.label}",
            font=font_tiny,
            fill=0,
        )

    if bounds:
        draw.text(
            (4, 72),
            f"Today { _format_pence(bounds[0]) }–{ _format_pence(bounds[1]) }",
            font=font_tiny,
            fill=0,
        )

    if cheap:
        draw.text(
            (4, 84),
            f"Cheapest 6h: {_format_pence(cheap.pence_inc_vat)} @ {cheap.label}",
            font=font_tiny,
            fill=0,
        )

    _footer(draw, now.strftime("Updated %H:%M"), font_tiny)
    return image


def _draw_bar_chart(
    draw: ImageDraw.ImageDraw,
    slots: list[RateSlot],
    x: int,
    y: int,
    width: int,
    height: int,
    highlight: RateSlot | None,
) -> None:
    if not slots:
        draw.text((x, y + 20), "No data yet", fill=0)
        return

    values = [slot.pence_inc_vat for slot in slots]
    low, high = min(values), max(values)
    span = max(high - low, 0.1)
    bar_w = max(1, width // len(slots))

    for index, slot in enumerate(slots):
        normalised = (slot.pence_inc_vat - low) / span
        bar_h = max(2, int(normalised * (height - 4)))
        left = x + index * bar_w
        top = y + height - bar_h
        box = (left, top, left + bar_w - 1, y + height)
        if highlight and slot.start == highlight.start:
            draw.rectangle(box, fill=0)
        else:
            draw.rectangle(box, outline=0)


def render_today(rates: AgileRates) -> Image.Image:
    _, font_small, font_tiny = _fonts()
    image, draw = _blank()
    now = datetime.now(tz=UK)
    today = rates.slots_on(now.date())
    current = rates.current_slot(now)
    bounds = AgileRates.min_max(today)

    _header(draw, f"Today {now.strftime('%a %d %b')}", font_small)

    if bounds:
        draw.text(
            (4, 18),
            f"{_format_pence(bounds[0])} – {_format_pence(bounds[1])}",
            font=font_tiny,
            fill=0,
        )

    _draw_bar_chart(draw, today, 4, 32, WIDTH - 8, 68, current)

    if current:
        _footer(draw, f"Now {_format_pence(current.pence_inc_vat)} @ {current.label}", font_tiny)
    else:
        _footer(draw, "Half-hourly Agile rates", font_tiny)

    return image


def render_tomorrow(rates: AgileRates) -> Image.Image:
    _, font_small, font_tiny = _fonts()
    image, draw = _blank()
    now = datetime.now(tz=UK)
    tomorrow = now.date() + timedelta(days=1)
    slots = rates.slots_on(tomorrow)
    bounds = AgileRates.min_max(slots)
    average = AgileRates.average(slots)

    _header(draw, f"Tomorrow {tomorrow.strftime('%a %d %b')}", font_small)

    if len(slots) < 40:
        draw.text((4, 28), "Prices from ~4pm today", font=font_small, fill=0)
        draw.text((4, 48), "Octopus publishes day-ahead", font=font_tiny, fill=0)
        draw.text((4, 60), "Agile slots after 4pm UK", font=font_tiny, fill=0)
        _footer(draw, f"{len(slots)} slots loaded", font_tiny)
        return image

    if bounds and average is not None:
        draw.text(
            (4, 20),
            f"{_format_pence(bounds[0])} – {_format_pence(bounds[1])}  avg {_format_pence(average)}",
            font=font_tiny,
            fill=0,
        )

    cheapest = sorted(slots, key=lambda slot: slot.pence_inc_vat)[:3]
    y = 34
    draw.text((4, y), "Cheapest:", font=font_tiny, fill=0)
    y += 12
    for slot in cheapest:
        draw.text(
            (8, y),
            f"{slot.label}  {_format_pence(slot.pence_inc_vat)}",
            font=font_tiny,
            fill=0,
        )
        y += 11

    expensive = max(slots, key=lambda slot: slot.pence_inc_vat)
    draw.text(
        (4, 92),
        f"Peak {expensive.label} {_format_pence(expensive.pence_inc_vat)}",
        font=font_tiny,
        fill=0,
    )
    _footer(draw, f"{len(slots)} half-hour slots", font_tiny)
    return image


RENDERERS = {
    "now": render_now,
    "today": render_today,
    "tomorrow": render_tomorrow,
}


def render_screen(name: str, rates: AgileRates) -> Image.Image:
    renderer = RENDERERS.get(name)
    if renderer is None:
        raise ValueError(f"Unknown screen: {name}")
    return renderer(rates)
