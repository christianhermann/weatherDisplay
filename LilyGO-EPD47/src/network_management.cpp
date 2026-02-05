#include "network_management.h"
#include "WiFi.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"
#include "secrets.h"
// ---------------------------------------------------------
// CONFIGURATION
// ---------------------------------------------------------


// Max JSON size? Your example is ~600-800 bytes. 
// 2KB (2048) buffer is safe for ESP32.
#define JSON_BUFFER_SIZE 4096 
#define DEBUG_NET Serial

WiFiClient espClient;
PubSubClient client(espClient);

// Temporary buffer for the incoming large message
// (Dynamic allocation is safer for stack size, or use global if RAM permits)
char* jsonBuffer = NULL; 
int jsonBufferIndex = 0;
bool messageReceived = false;

// ---------------------------------------------------------
// HELPERS
// ---------------------------------------------------------

void connectWiFi() {
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 15) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " Failed");
}

// Helper to safely extract nullable fields
float getFloat(JsonVariant v, float defaultVal = -1.0) {
    return v.isNull() ? defaultVal : v.as<float>();
}
int getInt(JsonVariant v, int defaultVal = -1) {
    return v.isNull() ? defaultVal : v.as<int>();
}

// ---------------------------------------------------------
// PARSING LOGIC
// ---------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    DEBUG_NET.printf(">> MQTT MSG RECEIVED: Topic '%s', Length %d\n", topic, length);

    if (length >= JSON_BUFFER_SIZE) {
        DEBUG_NET.println("!! ERROR: JSON too large for buffer !!");
        return;
    }

    if (jsonBuffer != NULL) {
        memcpy(jsonBuffer, payload, length);
        jsonBuffer[length] = '\0'; 
        messageReceived = true;
        // Print the first 50 chars to verify content
        char preview[51];
        strncpy(preview, jsonBuffer, 50);
        preview[50] = '\0';
        DEBUG_NET.printf(">> Payload Preview: %s...\n", preview);
    }
}

WeatherData parseJson(char* json) {
    DEBUG_NET.println(">> Parsing JSON...");
    
    // 1. Initialize with SAFE defaults (Zeros)
    WeatherData data;
    memset(&data, 0, sizeof(WeatherData)); // CRITICAL: Zero out the memory first!
    data.valid = false;
    data.forecastCount = 0;

    // 2. Parse
    JsonDocument doc; // Use Dynamic if stack is tight, but auto is usually fine for 4KB
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        DEBUG_NET.print("!! JSON PARSE ERROR: ");
        DEBUG_NET.println(error.f_str());
        return data; // Returns the zeroed-out struct
    }

    // 3. Extract Metadata
    // strlcpy is safer than strncpy (guarantees null termination)
    const char* ts = doc["update_ts"];
    if (ts) strlcpy(data.update_ts, ts, sizeof(data.update_ts));
    else strcpy(data.update_ts, "Unknown");

    // 4. Extract Current
    JsonObject cur = doc["current"];
    if (cur.isNull()) {
        DEBUG_NET.println("!! ERROR: 'current' object missing");
        return data;
    }

    const char* cur_ts = cur["ts"];
    if (cur_ts) strlcpy(data.current.timestamp, cur_ts, sizeof(data.current.timestamp));
    
    data.current.temp       = cur["temp"] | -99.0;
    data.current.humidity   = cur["humi"] | -1;
    data.current.windSpeed  = cur["wind"] | -1.0;
    data.current.cloudCover = cur["cloudco"] | -1;
    data.current.rainPct    = cur["rain_pct"] | -1;
    
    const char* cur_icon = cur["icon"];
    if (cur_icon) strlcpy(data.current.icon, cur_icon, sizeof(data.current.icon));
    else strcpy(data.current.icon, "unknown");

    data.current.valid = true;

    // 5. Extract Forecast
    JsonArray arr = doc["forecast"];
    if (arr.isNull()) {
         DEBUG_NET.println("!! WARNING: 'forecast' array missing");
    } else {
        int count = 0;
        for (JsonObject f : arr) {
            if (count >= 6) break;

            const char* f_ts = f["ts"];
            if (f_ts) strlcpy(data.forecast[count].timestamp, f_ts, sizeof(data.forecast[count].timestamp));
            
            data.forecast[count].temp       = f["temp"] | -99.0;
            data.forecast[count].humidity   = f["humi"] | -1;
            data.forecast[count].windSpeed  = f["wind"] | -1.0;
            data.forecast[count].cloudCover = f["cloudco"] | -1;
            data.forecast[count].rainPct    = f["rain_pct"] | -1;

            const char* f_icon = f["icon"];
            if (f_icon) strlcpy(data.forecast[count].icon, f_icon, sizeof(data.forecast[count].icon));
            else strcpy(data.forecast[count].icon, "unknown");
            
            data.forecast[count].valid = true;
            count++;
        }
        data.forecastCount = count;
    }

    DEBUG_NET.printf(">> Parse Success. Forecast items: %d\n", data.forecastCount);
    data.valid = true;
    return data;
}
// ---------------------------------------------------------
// MAIN FETCH FUNCTION
// ---------------------------------------------------------


WeatherData fetchWeatherData() {
    WeatherData finalData;
    finalData.valid = false;
    messageReceived = false;

    jsonBuffer = (char*)malloc(JSON_BUFFER_SIZE);
    if (!jsonBuffer) {
        DEBUG_NET.println("!! ERROR: Malloc failed");
        return finalData;
    }

    // 1. WiFi Check
    if (WiFi.status() != WL_CONNECTED) connectWiFi();
    
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_NET.println("!! ERROR: WiFi failed to connect");
        free(jsonBuffer);
        return finalData;
    }

    // 2. MQTT Connect
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqttCallback);
    client.setBufferSize(JSON_BUFFER_SIZE + 50);

    DEBUG_NET.print(">> Connecting to MQTT Broker: ");
    DEBUG_NET.println(MQTT_SERVER);

if (client.connect("LilyGo_Display_Client", MQTT_USER, MQTT_PASS)) { 
        DEBUG_NET.println(">> MQTT Connected");
        
        DEBUG_NET.printf(">> Subscribing to: %s\n", MQTT_TOPIC);
        client.subscribe(MQTT_TOPIC);
        
        // 3. Wait Loop
        DEBUG_NET.println(">> Waiting for message...");
        long start = millis();
        while (millis() - start < 3000) { // Increased to 3s
            client.loop();
            if (messageReceived) {
                DEBUG_NET.println(">> Message flag set!");
                break;
            }
            delay(10);
        }

        if (!messageReceived) {
             DEBUG_NET.println("!! ERROR: Timeout waiting for MQTT message");
             DEBUG_NET.println("   (Is the message RETAINED on the broker?)");
        }

        client.disconnect();
    } else {
        DEBUG_NET.printf("!! ERROR: MQTT Connect failed, rc=%d\n", client.state());
        // Common RC codes: 
        // -2: Connect failed (Network)
        // -4: Timeout
        // 5: Unauthorized (Wrong User/Pass)
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    if (messageReceived) {
        finalData = parseJson(jsonBuffer);
    }

    free(jsonBuffer);
    return finalData;
}