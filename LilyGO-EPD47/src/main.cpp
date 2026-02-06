#include <Arduino.h>
#include <epd_driver.h>
#include "network_management.h"
#define LILYGO_T5_47_S3
#define SLEEP_MINUTES 1

// Declare the external UI function (defined in ui_layout.cpp)
extern void updateUI(WeatherData data);

// Force external declaration to debug linking
uint8_t *framebuffer = NULL;

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
{
    Serial.begin(115200);
    delay(2000);

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
    printWeatherDebug(weather);

    // 5. Run UI
    updateUI(weather);

    // --- DEEP SLEEP SEQUENCE ---
    Serial.println(" shutting down...");

    // A. Ensure EPD is off (Double check)
    epd_poweroff();

    // B. Delay slightly to let serial finish (optional)
    delay(100);

    // C. Configure Timer Wakeup
    // Time in Microseconds = Minutes * 60 * 1,000,000
    uint64_t sleep_us = (uint64_t)SLEEP_MINUTES * 60 * 1000000;
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
