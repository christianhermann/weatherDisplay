# main.py
from importWeatherData import (
    fetch_current_weather,
    parse_current_to_point,
    fetch_forecast_window,
    pick_target_forecasts,
)

LAT = 48.17
LON = 11.53


def print_point(label, p):
    print("\n{}".format(label))
    print("  time: {}".format(p.timestamp.isoformat()))
    print("  temperature: {} °C".format(p.temperature_c))
    print("  cloud_cover: {} %".format(p.cloud_cover_pct))
    print("  precipitation_probability: {} %".format(p.precipitation_probability_pct))
    print("  sunshine_duration: {} min".format(p.sunshine_duration_min))
    print("  wind_speed: {} m/s".format(p.wind_speed_ms))
    print("  relative_humidity: {} %".format(p.relative_humidity_pct))
    print("  icon: {}".format(p.icon))


def main():
    current_payload = fetch_current_weather(LAT, LON)
    current = parse_current_to_point(current_payload)
    print_point("Today (right now) – current_weather", current)

    forecast_payload = fetch_forecast_window(LAT, LON)
    forecasts = pick_target_forecasts(forecast_payload)

    if not forecasts:
        print("\nForecast\n  Did not find the four target timestamps (06:00/14:00 for next two days).")
        return

    for p in forecasts:
        print_point("Forecast", p)


if __name__ == "__main__":
    main()
