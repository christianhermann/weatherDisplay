#include <Arduino.h>
#include <epd_driver.h>
#include <network_management.h>
#define LILYGO_T5_47_S3
#define SLEEP_MINUTES_LONG 120
#define SLEEP_MINUTES_SHORT 60


#include "ui_layout.h"

// Force external declaration to debug linking
uint8_t *framebuffer = NULL;

// RTC memory variables
RTC_DATA_ATTR char last_update_ts[32] = {0};

// ---------------------------------------------------------
// HELPER: GET ADAPTIVE SLEEP DURATION (FALLBACK)
// ---------------------------------------------------------
uint32_t getSleepDurationMinutes(int hour)
{
    if (hour >= 0 && hour < 6) {
        return SLEEP_MINUTES_LONG;  // Midnight to 6 AM: 2 hours
    } else if (hour >= 6 && hour < 8) {
        return SLEEP_MINUTES_SHORT;   // 6 AM to 8 AM: 1 hour
    } else if (hour >= 8 && hour < 12) {
        return SLEEP_MINUTES_LONG;  // 8 AM to 12 PM: 2 hours
    } else {
        return SLEEP_MINUTES_SHORT;   // 12 PM to 24:00: 1 hour
    }
}

// ---------------------------------------------------------
// HELPER: CHRONO-SYNCED DEEP SLEEP CALCULATION
// ---------------------------------------------------------
// Sleep continuously from 00:00 to 05:02.
// During daytime, dynamically wake up at :02 past the next hour.
uint64_t calculateSleepDurationUs(const WeatherData &weather)
{
    // Default fallback: 1 hour (3600 seconds)
    const int DEFAULT_SLEEP_SEC = 3600;
    int sleep_seconds = DEFAULT_SLEEP_SEC;

    if (weather.valid && strlen(weather.current.timestamp) >= 16)
    {
        // Validate digit characters for HH and MM
        if (isdigit(weather.current.timestamp[11]) && isdigit(weather.current.timestamp[12]) &&
            isdigit(weather.current.timestamp[14]) && isdigit(weather.current.timestamp[15]))
        {
            // ISO format: "YYYY-MM-DDTHH:MM:SS" or "YYYY-MM-DDTHH:MM"
            char hBuf[3] = { weather.current.timestamp[11], weather.current.timestamp[12], '\0' };
            char mBuf[3] = { weather.current.timestamp[14], weather.current.timestamp[15], '\0' };
            int hour = atoi(hBuf);
            int minute = atoi(mBuf);
            int second = 0;
            if (strlen(weather.current.timestamp) >= 19 && weather.current.timestamp[16] == ':' &&
                isdigit(weather.current.timestamp[17]) && isdigit(weather.current.timestamp[18]))
            {
                char sBuf[3] = { weather.current.timestamp[17], weather.current.timestamp[18], '\0' };
                second = atoi(sBuf);
            }

            // Boundary validation for hour, minute, second
            if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59)
            {
                // Night Window: 00:00 to 05:02 (strictly hour >= 0 && hour < 5)
                // Wakes up continuously at 05:02:00
                if (hour >= 0 && hour < 5)
                {
                    int target_seconds = 5 * 3600 + 2 * 60; // 18120s (05:02:00)
                    int current_seconds = hour * 3600 + minute * 60 + second;
                    sleep_seconds = target_seconds - current_seconds;
                    if (sleep_seconds < 60) sleep_seconds += 3600;
                    Serial.printf("Night sleep scheduled until 05:02: %d seconds (%d min)\n", sleep_seconds, sleep_seconds / 60);
                }
                else
                {
                    // Daytime Window (05:00 to 23:59): chrono-sync to :02 past the NEXT hour
                    // For hourly observations (minute = 0), sleeps (62 - 0)*60 = 3720s (62 min)
                    // At 23:00, (62 - 0)*60 = 3720s targets 00:02:00, which then triggers the 5h night sleep.
                    sleep_seconds = (62 - minute) * 60 - second;
                    if (sleep_seconds < 60) sleep_seconds += 3600;
                    Serial.printf("Daytime chrono-sync scheduled to :02: %d seconds (%d min)\n", sleep_seconds, sleep_seconds / 60);
                }
            }
            else
            {
                Serial.println("Timestamp hour/minute out of range; using fallback sleep.");
                sleep_seconds = DEFAULT_SLEEP_SEC;
            }
        }
        else
        {
            Serial.println("Malformed timestamp characters; using fallback sleep.");
            sleep_seconds = DEFAULT_SLEEP_SEC;
        }
    }
    else
    {
        Serial.println("Invalid weather data timestamp; using fallback 3600s sleep.");
    }

    // Strict RTC hardware limit clamp: prevent underflow, zero, or multi-year sleep bricking
    if (sleep_seconds < 60) sleep_seconds = DEFAULT_SLEEP_SEC;
    if (sleep_seconds > 86400) sleep_seconds = DEFAULT_SLEEP_SEC;

    return (uint64_t)sleep_seconds * 1000000ULL;
}

// ---------------------------------------------------------
// HELPER: DEBUG PRINT
// ---------------------------------------------------------
void printWeatherDebug(const WeatherData &data)
{
    Serial.println("\n--------------------------------------");
    Serial.println("       WEATHER DATA REPORT");
    Serial.println("--------------------------------------");

    if (!data.valid)
    {
        Serial.println("[ERROR] Data is INVALID/Empty.");
        return;
    }

    Serial.printf("Data Update TS: %s\n", data.update_ts);

    Serial.println("\n--- CURRENT CONDITIONS ---");
    Serial.printf("Time:     %s\n", data.current.timestamp);
    Serial.printf("Temp:     %.1f C\n", data.current.temp);
    Serial.printf("Rain:     %.0f mm/H\n", data.current.rain);
    Serial.printf("Rain Prob:     %d %%\n", data.current.rain_probability);
    Serial.printf("Dew Point: %.0f C\n", data.current.dew_point);
    Serial.printf("Wind60:     %.1f km/h\n", data.current.windSpeed60);
    Serial.printf("Wind:     %.1f km/h\n", data.current.windSpeed);
    Serial.printf("Cloud:    %d %%\n", data.current.cloudCover);
    Serial.printf("Icon:     %s\n", data.current.icon);

    Serial.println("\n--- FORECAST ---");
    Serial.printf("Items parsed: %d\n", data.forecastCount);

    for (int i = 0; i < data.forecastCount; i++)
    {
        WeatherPoint p = data.forecast[i];
        Serial.printf("[%d] %s | T: %.1fC | Rain:  %d %% | Wind: %.1f | Icon: %s | Cloud: %d%% Dew: %.0FC \n",
                      i,
                      p.timestamp,
                      p.temp,
                      p.rain_probability,
                      p.windSpeed,
                      p.icon,
                      p.cloudCover,
                      p.dew_point);
    }
    Serial.println("--------------------------------------\n");
}

void setup()
{   // 0. Set CPU Frequency to 80MHz (Default is 240MHz on ESP32-S3) to reduce power consumption. EPD updates are slow, so we don't need the extra speed.
    // Set CPU Frequency to 160MHz for "race-to-sleep": finishes Wi-Fi & rendering faster
    setCpuFrequencyMhz(160); 
    // Disable Bluetooth to free up memory and reduce power consumption
    btStop();

#ifndef DISABLE_SERIAL
    Serial.begin(115200);
#endif

    // 1. Battery voltage check BEFORE Wi-Fi or EPD initialization
    int battery = getBatteryPercentage();
    if (battery < 5)
    {
        Serial.printf("Battery critical (%d%%). Drawing warning and sleeping permanently.\n", battery);
        if (psramInit())
        {
            framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
            if (framebuffer)
            {
                epd_init();
                drawBatteryWarningScreen(battery);
                epd_poweron();
                epd_clear();
                epd_draw_grayscale_image(epd_full_screen(), framebuffer);
                epd_poweroff_all();
            }
        }
        // Permanent deep sleep without timer wakeup
        esp_deep_sleep_start();
    }

    // 2. Fetch Weather Data
    // This will connect WiFi, get MQTT, parse JSON, and disconnect WiFi
    Serial.println("Fetching weather data...");
    WeatherData weather = fetchWeatherData();

    // 3. Print Results to Serial (Verification Step)
#ifndef DISABLE_SERIAL
    printWeatherDebug(weather);
#endif

    // Handle failed data fetch gracefully without freezing or corrupting existing display
    if (!weather.valid)
    {
        Serial.println("!! Weather data fetch failed !!");
        if (strlen(last_update_ts) > 0)
        {
            // We already have valid weather data on the screen from a previous wake.
            // Do NOT overwrite or corrupt the screen with "Waiting for Data...".
            // Go to sleep for 2 minutes and retry promptly so the display doesn't stay frozen.
            Serial.println("Preserving existing display on screen. Retrying in 2 minutes...");
            esp_sleep_enable_timer_wakeup(120ULL * 1000000ULL);
            esp_deep_sleep_start();
        }
        else
        {
            // First boot with no existing screen. Try initializing display to show status,
            // but sleep only 60 seconds before retrying instead of 1 hour.
            Serial.println("First boot fetch failed. Will retry in 60 seconds...");
        }
    }

    // 4. Skip UI update if weather data is unchanged
    if (weather.valid && strlen(last_update_ts) > 0 && strcmp(weather.update_ts, last_update_ts) == 0)
    {
        // Unchanged data is unlikely with regular backend publishing schedules,
        // but better to have and not need than otherwise: skip expensive EPD update
        // and return immediately to deep sleep to maximize battery life.
        Serial.println("Weather data unchanged since last update. Skipping UI render.");
        uint64_t sleep_us = calculateSleepDurationUs(weather);
        esp_sleep_enable_timer_wakeup(sleep_us);
        esp_deep_sleep_start();
    }

    if (weather.valid)
    {
        strlcpy(last_update_ts, weather.update_ts, sizeof(last_update_ts));
    }

    // 5. Init PSRAM
    if (!psramInit())
    {
        Serial.println("PSRAM Init Failed");
        esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
        esp_deep_sleep_start();
    }

    // 6. Allocate Framebuffer
    // EPD_WIDTH (960) * EPD_HEIGHT (540) / 2 = 259,200 bytes
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        Serial.println("Alloc memory failed !!!");
        esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
        esp_deep_sleep_start();
    }
    // Initialize to White (0xFF = White in this 4bpp mode usually)
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 7. Init EPD
    epd_init();

    // 8. Run UI
    updateUI(weather);

    // --- DEEP SLEEP SEQUENCE ---
    Serial.println(" shutting down...");

    // A. Ensure EPD is off (Double check)
    epd_poweroff_all();

    // B. Delay slightly to let serial finish (optional)
    delay(50);

    // C. Configure Timer Wakeup with chrono-synced interval
    uint64_t sleep_us = 0;
    if (weather.valid)
    {
        sleep_us = calculateSleepDurationUs(weather);
    }
    else
    {
        // First boot failed fetch fallback: retry in 60s
        sleep_us = 60ULL * 1000000ULL;
    }
    esp_sleep_enable_timer_wakeup(sleep_us);

    // D. Enter Deep Sleep
    // The ESP32 will largely power off. RAM is lost (except RTC RAM).
    // When it wakes, it resets and runs setup() from the start.
    esp_deep_sleep_start();
}

void loop()
{
    // Nothing here!
    // E-Ink displays should update once and then sleep.
}
