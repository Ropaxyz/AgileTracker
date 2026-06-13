from __future__ import annotations

from dataclasses import dataclass
from datetime import date, datetime, time, timedelta, timezone
from zoneinfo import ZoneInfo

import requests

from tracker.config import Settings

UK = ZoneInfo("Europe/London")


@dataclass(frozen=True)
class RateSlot:
    start: datetime
    end: datetime
    pence_inc_vat: float

    @property
    def label(self) -> str:
        return self.start.astimezone(UK).strftime("%H:%M")


class AgileRates:
    def __init__(self, slots: list[RateSlot], fetched_at: datetime) -> None:
        self.slots = sorted(slots, key=lambda slot: slot.start)
        self.fetched_at = fetched_at

    def slots_on(self, day: date) -> list[RateSlot]:
        start = datetime.combine(day, time.min, tzinfo=UK)
        end = start + timedelta(days=1)
        return [slot for slot in self.slots if start <= slot.start < end]

    def current_slot(self, moment: datetime | None = None) -> RateSlot | None:
        now = moment or datetime.now(tz=UK)
        for slot in self.slots:
            if slot.start <= now < slot.end:
                return slot
        return None

    def next_slot(self, moment: datetime | None = None) -> RateSlot | None:
        now = moment or datetime.now(tz=UK)
        for slot in self.slots:
            if slot.start > now:
                return slot
        return None

    def cheapest_in_hours(self, hours: float, moment: datetime | None = None) -> RateSlot | None:
        now = moment or datetime.now(tz=UK)
        horizon = now + timedelta(hours=hours)
        upcoming = [slot for slot in self.slots if now <= slot.start < horizon]
        if not upcoming:
            return None
        return min(upcoming, key=lambda slot: slot.pence_inc_vat)

    @staticmethod
    def min_max(slots: list[RateSlot]) -> tuple[float, float] | None:
        if not slots:
            return None
        values = [slot.pence_inc_vat for slot in slots]
        return min(values), max(values)

    @staticmethod
    def average(slots: list[RateSlot]) -> float | None:
        if not slots:
            return None
        values = [slot.pence_inc_vat for slot in slots]
        return sum(values) / len(values)


def _parse_slot(raw: dict) -> RateSlot:
    return RateSlot(
        start=datetime.fromisoformat(raw["valid_from"].replace("Z", "+00:00")),
        end=datetime.fromisoformat(raw["valid_to"].replace("Z", "+00:00")),
        pence_inc_vat=float(raw["value_inc_vat"]),
    )


def _day_bounds(day: date) -> tuple[datetime, datetime]:
    start = datetime.combine(day, time.min, tzinfo=UK)
    end = start + timedelta(days=1)
    return start, end


def fetch_agile_rates(settings: Settings, session: requests.Session | None = None) -> AgileRates:
    """Fetch Agile half-hour rates for today and tomorrow."""
    session = session or requests.Session()
    today = datetime.now(tz=UK).date()
    period_from, _ = _day_bounds(today)
    _, period_to = _day_bounds(today + timedelta(days=2))

    params = {
        "period_from": period_from.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "period_to": period_to.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "page_size": 1500,
    }

    slots: list[RateSlot] = []
    url: str | None = settings.rates_url

    while url:
        response = session.get(url, params=params if url == settings.rates_url else None, timeout=30)
        response.raise_for_status()
        payload = response.json()
        slots.extend(_parse_slot(item) for item in payload.get("results", []))
        url = payload.get("next")
        params = None

    return AgileRates(slots=slots, fetched_at=datetime.now(tz=UK))


def resolve_region(postcode: str, session: requests.Session | None = None) -> str:
    session = session or requests.Session()
    response = session.get(
        "https://api.octopus.energy/v1/industry/grid-supply-points/",
        params={"postcode": postcode.replace(" ", "").upper()},
        timeout=30,
    )
    response.raise_for_status()
    results = response.json().get("results", [])
    if not results:
        raise ValueError(f"No GSP region found for postcode {postcode}")
    group_id = results[0]["group_id"]
    return group_id.lstrip("_")
