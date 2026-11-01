#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <Arduino.h>
#include "network_management.h"

// Public interface for UI Layout
void updateUI(const WeatherData &data);
int getBatteryPercentage();
void drawBatteryWarningScreen(int battery_percentage);

#endif // UI_LAYOUT_H
