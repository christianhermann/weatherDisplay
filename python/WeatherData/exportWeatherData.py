import json
import os
import logging
import paho.mqtt.client as mqtt
from dataclasses import asdict
from typing import List
from dotenv import load_dotenv
from datetime import datetime
from importWeatherData import WeatherPoint

# --- Configuration ---
load_dotenv() 
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_USER = os.getenv("MQTT_USER")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")
MQTT_TOPIC = "home/weather/display"

if not MQTT_USER or not MQTT_PASSWORD:
    raise ValueError("Missing MQTT credentials in .env file")

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("WeatherExporter")

def _format_point(wp: WeatherPoint) -> dict:
    """
    Helper to convert a WeatherPoint into a minimal dictionary for the display.
    Includes temperature, icon, rain probability, wind, and humidity.
    """
    if wp is None:
        return {}
        
    return {
        # ISO format allows parsing on ESP32 (or string slicing for simple HH:MM)
        "ts": wp.timestamp.isoformat(), 
        
        # Rounded Temp (e.g., 22.4 -> 22)
        "temp": round(wp.temperature_c) if wp.temperature_c is not None else None,
        
        # Precipitation
        "rain": wp.precipitation if wp.precipitation is not None else None,
        
        # Precipitation Probability (e.g., 50)
        "rain_probability": round(wp.precipitation_probability) if wp.precipitation_probability is not None else None,

        # Cloud Cover Percentage(e. g. 35)
        "cloudco" : round (wp.cloud_cover_pct) if wp.cloud_cover_pct is not None else None,

        # Wind Speed (e.g., 15 km/h)
        "wind": round(wp.wind_speed * 3.6, 1) if wp.wind_speed is not None else None,

        "wind60": round(wp.wind_speed_60 * 3.6, 1) if wp.wind_speed_60 is not None else None,

        # Dew point (e.g., 12°C) - Great for "Comfort" metrics on display
        "dew_point":round(wp.dew_point) if wp.dew_point is not None else None,

                # Icon string (e.g., "partly-cloudy-day")
        "icon": wp.icon
    }

def publish_weather_data(current: WeatherPoint, forecast: List[WeatherPoint]):
    """
    Connects to MQTT and publishes the combined weather data.
    """
    # Construct the Payload
    # Combine current conditions and the forecast list into one JSON object.
    payload_dict = {
        "update_ts": datetime.now().isoformat(), # So you know when data was last fetched
        "current": _format_point(current),
        "forecast": [_format_point(fp) for fp in forecast]
    }
    
    payload_json = json.dumps(payload_dict)
    
    # MQTT Setup
    client = mqtt.Client(client_id="WeatherPythonScript", protocol=mqtt.MQTTv311)

    # Authentication (Required since we set allow_anonymous false)
    client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    
    try:
        logger.info(f"Connecting to MQTT Broker at {MQTT_BROKER}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)

        # Start the network loop in a background thread
        # This is required for wait_for_publish to work correctly!
        client.loop_start() 

        #  Publish with RETAIN=True
        # retain=True ensures the display gets this message immediately upon waking up
        logger.info(f"Publishing to {MQTT_TOPIC} (Retained)...")
        msg_info = client.publish(MQTT_TOPIC, payload_json, qos=1, retain=True)
        
# Wait max 5 seconds for acknowledgement
        msg_info.wait_for_publish(timeout=5)
        
        if msg_info.is_published():
            logger.info("Publish confirmed by broker.")
        else:
            logger.warning("Publish timed out (Broker didn't ACK), but message might still be sent.")
            
        client.loop_stop()
        client.disconnect()
        
        return payload_json
        
    except Exception as e:
        logger.error(f"Failed to publish to MQTT: {e}")
        raise e