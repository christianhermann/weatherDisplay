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


if __name__ == "__main__":
      job()
