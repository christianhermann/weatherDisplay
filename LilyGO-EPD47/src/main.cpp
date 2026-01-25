#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

const char* WIFI_SSID = "Pretty fly for a Wifi";
const char* WIFI_PASS = "giveittomebaby";

// CET/CEST rules for Europe/Berlin (POSIX TZ string)
static const char* TZ_INFO = "CET-1CEST,M3.5.0/02:00:00,M10.5.0/03:00:00";
static const char* NTP_SERVER = "pool.ntp.org";

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Sets timezone + starts SNTP time sync
  configTzTime(TZ_INFO, NTP_SERVER);  // common ESP32 Arduino approach [web:180]

  // Wait up to 10 seconds for time to be set
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("Failed to obtain time");
    return;
  }

 // Serial.println("Time synced!");
 // Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
}

void loop() {
 // struct tm timeinfo;
 // if (getLocalTime(&timeinfo)) {
   // Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
 // }
 // delay(1000);
}
