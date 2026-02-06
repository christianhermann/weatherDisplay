#ifndef ICON_DISPLAY_H
#define ICON_DISPLAY_H

#include <Arduino.h>
#include "weather_icons.h" // Your new file

// Helper struct to map string -> data
struct IconMap
{
    const char *name;
    const uint8_t *data;
};

// Map the API strings to your NEW variable names (from weather_icons.h)
const IconMap ICONS[] = {
    {"clear-day", clear_day_map},
    {"clear-night", clear_night_map},
    {"cloudy", cloudy_map},
    {"fog", fog_map},
    {"partly-cloudy-day", partly_cloudy_day_map},
    {"partly-cloudy-night", partly_cloudy_night_map},
    {"rain", rain_map},
    {"sleet", sleet_map},
    {"snow", snow_map},
    {"sleet", sleet_map},
    {"thunderstorm", thunderstorm_map},
    {"wind", wind_map},
    // ... Add all other icons here ...
    {NULL, NULL}};

// Helper function to find icon data by name
const uint8_t *getIconData(const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; ICONS[i].name != NULL; i++)
    {
        if (strcmp(ICONS[i].name, name) == 0)
        {
            return ICONS[i].data;
        }
    }
    return NULL; // Not found
}

#endif
