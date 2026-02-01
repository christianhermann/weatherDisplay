# main.py
import logging
import sys

from importWeatherData import (
    fetch_current_weather,
    parse_current_to_point,
    fetch_forecast_window,
    pick_target_forecasts,
)

from exportWeatherData import publish_weather_data 

LAT = 48.17
LON = 11.53

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger("WeatherMain")

""" 
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
 """

def job():
    try:
        logger.info("Starting Weather Update Job...")

        # 1. Fetch Data
        logger.info(f"Fetching data from Bright Sky for Lat:{LAT}, Lon:{LON}")
        current_json = fetch_current_weather(LAT, LON)
        forecast_json = fetch_forecast_window(LAT, LON)
        
        # 2. Parse Data
        logger.info("Parsing weather data...")
        current_wp = parse_current_to_point(current_json)
        forecast_wps = pick_target_forecasts(forecast_json)
        
        # 3. Publish Data
        logger.info(f"Publishing data (Current TS: {current_wp.timestamp})...")
        sent_payload = publish_weather_data(current_wp, forecast_wps)
        
        # 4. Success Verification (Optional print for log file)
        logger.info("Successfully published weather data to MQTT.")
        # Uncomment the next line if you want to see the full JSON in your logs
        # logger.info(f"Payload: {sent_payload}")
        
    except Exception as e:
        logger.error(f"Job Failed: {e}", exc_info=True)
        # Exit with error code 1 so Cron/System knows it failed
        sys.exit(1)


""" def main():
    current_payload = fetch_current_weather(LAT, LON)
    current = parse_current_to_point(current_payload)
    print_point("Today (right now) – current_weather", current)

    forecast_payload = fetch_forecast_window(LAT, LON)
    forecasts = pick_target_forecasts(forecast_payload)

    if not forecasts:
        print("\nForecast\n  Did not find the six target timestamps (06:00/14:00 for next two days).")
        return

    for p in forecasts:
        print_point("Forecast", p)

 """
if __name__ == "__main__":
      job()
