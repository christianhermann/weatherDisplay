# importWeatherData.py
from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timedelta, timezone, time
from zoneinfo import ZoneInfo

import requests

TZ_NAME = "Europe/Berlin"
BASE = "https://api.brightsky.dev"  # public Bright Sky instance [web:4]


@dataclass(frozen=True)
class WeatherPoint:
    timestamp: datetime
    temperature_c: float | None
    cloud_cover_pct: float | None
    precipitation_probability_pct: float | None
    sunshine_duration_min: float | None
    wind_speed_ms: float | None
    relative_humidity_pct: float | None
    icon: str | None


def _parse_ts(ts: str) -> datetime:
    return datetime.fromisoformat(ts)


def fetch_current_weather(lat: float, lon: float) -> dict:
    """
    Calls /current_weather for the latest 'right now' conditions. [web:43]
    """
    url = f"{BASE}/current_weather"
    params = {"lat": lat, "lon": lon, "tz": TZ_NAME}
    r = requests.get(url, params=params, timeout=20)
    r.raise_for_status()
    return r.json()


def fetch_forecast_window(lat: float, lon: float) -> dict:
    """
    Calls /weather to retrieve hourly records (observations + forecasts) for a time window. [web:43][web:52]
    We query from 'today 00:00 local' through 'day-after-tomorrow 15:00 local' to safely include
    tomorrow/day-after at 06:00 and 14:00.
    """
    tz = ZoneInfo(TZ_NAME)
    now_local = datetime.now(timezone.utc).astimezone(tz)

    start_local = datetime.combine(now_local.date(), time(0, 0), tzinfo=tz)
    day_after = now_local.date() + timedelta(days=2)
    end_local = datetime.combine(day_after, time(15, 0), tzinfo=tz)

    url = f"{BASE}/weather"
    params = {
        "lat": lat,
        "lon": lon,
        "date": start_local.isoformat(timespec="seconds"),
        "last_date": end_local.isoformat(timespec="seconds"),
        "tz": TZ_NAME,
    }
    r = requests.get(url, params=params, timeout=20)
    r.raise_for_status()
    return r.json()


def parse_current_to_point(payload: dict) -> WeatherPoint:
    """
    /current_weather returns a single weather object in the response. [web:43]
    """
    w = payload["weather"]
    ts = _parse_ts(w["timestamp"])
    if ts.tzinfo is None:
        ts = ts.replace(tzinfo=ZoneInfo(TZ_NAME))

    return WeatherPoint(
        timestamp=ts,
        temperature_c=w.get("temperature"),
        cloud_cover_pct=w.get("cloud_cover"),
        precipitation_probability_pct=w.get("precipitation_probability"),
        sunshine_duration_min=w.get("sunshine"),
        wind_speed_ms=w.get("wind_speed"),
        relative_humidity_pct=w.get("relative_humidity"),
        icon=w.get("icon"),
    )


def pick_target_forecasts(payload: dict) -> list[WeatherPoint]:
    """
    From /weather response, pick exactly:
    - tomorrow 06:00, tomorrow 14:00
    - day after tomorrow 06:00, day after tomorrow 14:00
    using Europe/Berlin time (tz parameter requested). [web:43]
    """
    tz = ZoneInfo(TZ_NAME)
    now_local = datetime.now(timezone.utc).astimezone(tz)
    tomorrow = now_local.date() + timedelta(days=1)
    day_after = now_local.date() + timedelta(days=2)

    targets = {
        datetime.combine(tomorrow, time(6, 0), tzinfo=tz),
        datetime.combine(tomorrow, time(14, 0), tzinfo=tz),
        datetime.combine(day_after, time(6, 0), tzinfo=tz),
        datetime.combine(day_after, time(14, 0), tzinfo=tz),
    }

    out: list[WeatherPoint] = []
    for w in payload.get("weather", []):

        ts = _parse_ts(w["timestamp"])
        if ts.tzinfo is None:
            ts = ts.replace(tzinfo=tz)

        if ts in targets:
            out.append(
                WeatherPoint(
                    timestamp=ts,
                    temperature_c=w.get("temperature"),
                    cloud_cover_pct=w.get("cloud_cover"),
                    precipitation_probability_pct=w.get("precipitation_probability"),
                    sunshine_duration_min=w.get("sunshine"),
                    wind_speed_ms=w.get("wind_speed"),
                    relative_humidity_pct=w.get("relative_humidity"),
                    icon=w.get("icon"),
                )
            )

    out.sort(key=lambda p: p.timestamp)
    return out
