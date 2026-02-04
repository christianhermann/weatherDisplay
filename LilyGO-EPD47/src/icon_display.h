#ifndef ICON_DISPLAY_H
#define ICON_DISPLAY_H

#include <Arduino.h>
#include <string.h>
#include "weather_icons.h"

// Lookup raw icon data by Bright Sky icon string (e.g. "clear-day").
static inline const uint8_t* getIconData(const char* icon_name) {
  if (!icon_name) return nullptr;
  for (int i = 0; i < WEATHER_ICONS_COUNT; i++) {
    if (strcmp(WEATHER_ICONS[i].name, icon_name) == 0) return WEATHER_ICONS[i].data;
  }
  return nullptr;
}

// Debug helper to list all available icon names
static inline void printAvailableIcons() {
  Serial.println("Available weather icons:");
  for (int i = 0; i < WEATHER_ICONS_COUNT; i++) Serial.println(WEATHER_ICONS[i].name);
}

#endif // ICON_DISPLAY_H
