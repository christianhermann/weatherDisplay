#include <Arduino.h>
#include <epd_driver.h>
#include <network_management.h>
#define LILYGO_T5_47_S3
#define SLEEP_MINUTES_LONG 120
#define SLEEP_MINUTES_SHORT 60


// Declare the external UI function (defined in ui_layout.cpp)
extern void updateUI(WeatherData data);

// Force external declaration to debug linking
uint8_t *framebuffer = NULL;

// ---------------------------------------------------------
// HELPER: GET ADAPTIVE SLEEP DURATION
// ---------------------------------------------------------
// Returns sleep duration in minutes based on current hour
// Schedule:
//   00:00 - 06:00: 120 minutes (sleep period)
//   06:00 - 08:00: 60 minutes (wake-up)
//   08:00 - 12:00: 120 minutes (mid-morning)
//   12:00 - 24:00: 60 minutes (active)
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
// HELPER: DEBUG PRINT
// ---------------------------------------------------------
void printWeatherDebug(WeatherData data)
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
    setCpuFrequencyMhz(80); 
    // Disable Bluetooth to free up memory and reduce power consumption
    btStop();

#ifndef DISABLE_SERIAL
    Serial.begin(115200);
#endif

    // 1. Init PSRAM
    if (!psramInit())
    {
        Serial.println("PSRAM Init Failed");
        while (1)
            ;
    }

    // 2. Allocate Framebuffer
    // EPD_WIDTH (960) * EPD_HEIGHT (540) / 2 = 259,200 bytes
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        Serial.println("Alloc memory failed !!!");
        while (1)
            ;
    }
    // Initialize to White (0xFF = White in this 4bpp mode usually, or 0x00? Check demo)
    // Usually 0xFF is White for EPD.
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 2. Init EPD
    epd_init();

    // 3. CRITICAL CHECK: Is the global framebuffer actually set?
    if (framebuffer == NULL)
    {
        Serial.println("!! FRAMEBUFFER IS NULL !!");
        // This confirms the linker is using a different variable instance
    }
    else
    {
        Serial.printf("Framebuffer Address: %p\n", framebuffer);
    }
    // 2. Fetch Weather Data
    // This will connect WiFi, get MQTT, parse JSON, and disconnect WiFi
    Serial.println("Fetching weather data...");
    WeatherData weather = fetchWeatherData();

    // 3. Print Results to Serial (Verification Step)
#ifndef DISABLE_SERIAL
    printWeatherDebug(weather);
#endif

    // 5. Run UI
    updateUI(weather);

    // --- DEEP SLEEP SEQUENCE ---
    Serial.println(" shutting down...");

    // A. Ensure EPD is off (Double check)
    epd_poweroff();

    // B. Delay slightly to let serial finish (optional)
    delay(50);

    // C. Configure Timer Wakeup with adaptive interval
    uint32_t sleep_minutes = SLEEP_MINUTES_SHORT;  // Default fallback
    
    // If we have valid weather data, calculate dynamic sleep interval based on current hour
    if (weather.valid && strlen(weather.current.timestamp) >= 13)
    {
        // Extract hour from ISO timestamp (format: "2026-02-01T15:30:00+01:00")
        char hourStr[3];
        hourStr[0] = weather.current.timestamp[11];
        hourStr[1] = weather.current.timestamp[12];
        hourStr[2] = '\0';
        int current_hour = atoi(hourStr);
        
        sleep_minutes = getSleepDurationMinutes(current_hour);
        Serial.printf("Adaptive sleep: Hour %d -> Sleep %d minutes\n", current_hour, sleep_minutes);
    }
    else
    {
        Serial.printf("Using default sleep interval: %d minutes\n", SLEEP_MINUTES_SHORT);
    }
    
    // Time in Microseconds = Minutes * 60 * 1,000,000
    uint64_t sleep_us = (uint64_t)sleep_minutes * 60 * 1000000;
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
