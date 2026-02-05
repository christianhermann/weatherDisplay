#ifndef NETWORK_MANAGEMENT_H
#define NETWORK_MANAGEMENT_H

#include <Arduino.h>

// Data structure to hold weather info
struct WeatherPoint {
    char timestamp[32];   // "2026-02-01T15:30:00+01:00"
    float temp;           // Temperature (C)
    int humidity;         // % (can be -1 if null)
    float windSpeed;      // km/h (can be -1.0 if null)
    int cloudCover;       // %
    int rain;          // % chance of rain (can be -1 if null)
    char icon[32];        // "clear-day", "cloudy", etc.
    bool valid;           // Is this point populated?
};

// The Main Container
struct WeatherData {
    char update_ts[32];        // When the file was generated
    WeatherPoint current;      // Current conditions
    
    // Fixed array for forecast items (from your JSON, looks like ~6 items)
    // We'll allocate space for up to 6 to be safe.
    WeatherPoint forecast[6]; 
    int forecastCount;         // How many forecast items we actually parsed (0-6)
    
    bool valid;                // Did the overall fetch succeed?
};

// Function prototypes
WeatherData fetchWeatherData(); 

#endif
