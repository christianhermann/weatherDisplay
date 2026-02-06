#include "epd_driver.h" // Standard EPD47 driver
#include "weather_icons.h"
#include "icon_display.h"
#include "stdio.h"
#include "HardwareSerial.h"
#include "network_management.h"
#include "chrono"
#include "Arduino.h"
// FONTS
#include "FiraSans.h" // Assuming this is your main font (approx 12-16pt?)
#include "opensans8b.h"
#include "opensans10b.h"
#include "digits_48pt.h" // Custom large numbers

// --- CONFIGURATION ---
// Screen Dimensions
#define SCREEN_W 960
#define SCREEN_H 540

// Colors
#define COLOR_BLACK 0x0
#define COLOR_WHITE 0xF

// --- LAYOUT MEASUREMENTS ---
// Left Panel (Current Weather)
#define LEFT_PANEL_W 400
#define LEFT_PANEL_PAD 15 // Padding from left edge

// Right Grid (Forecast)
#define GRID_BOX_W ((SCREEN_W - LEFT_PANEL_W) / 2) // 280px
#define GRID_BOX_H (SCREEN_H / 2)                  // 270px

// Forecast Box Internal Offsets (Relative to Top-Left of Box)
#define BOX_DATE_X 20
#define BOX_DATE_Y 60 // Baseline

#define BOX_ICON_X 5
#define BOX_ICON_Y 75 // Top-left of icon (128x128)

#define BOX_TEMP_X 20
#define BOX_TEMP_Y 230 // Baseline (approx)

// List Items inside Box (Wind, Humidity, etc.)
#define BOX_LIST_X 125  // Right side of box
#define BOX_LIST_Y 120  // Start Y
#define BOX_LIST_GAP 37 // Gap between lines

#define BATTERY_PIN 14

// Reference the global variable from main.cpp
extern uint8_t *framebuffer;

// --- HELPER FUNCTIONS ---
void drawBattery()
{
    // 1. Read Voltage
    analogSetAttenuation(ADC_11db);
    pinMode(BATTERY_PIN, ANALOG);

    uint32_t raw = 0;
    for (int i = 0; i < 20; i++)
    {
        raw += analogRead(BATTERY_PIN);
    }
    raw /= 20;

    float voltage = (raw / 4095.0) * 3.3 * 2.0; // Voltage divider adjustment
    int percentage = 0;
    if (voltage > 4.2)
        percentage = 100;
    else if (voltage < 3.3)
        percentage = 0;
    else
        percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);

    // 2. Format Text
    char buf[8];
    sprintf(buf, "%d%%", percentage); // e.g., "85%"

    // 3. Calculate Position (Top Right)
    // Screen Width is 960.
    // "100%" in size 8 font is roughly 30-35 pixels wide.
    int cursor_x = SCREEN_W - 45; // Start 45px from right edge
    int cursor_y = 25;            // ~15px from top (baseline of text)

    // 4. Draw to Framebuffer
    // writeln(font, text, &x, &y, buffer)
    writeln((GFXfont *)&OpenSans8B, buf, &cursor_x, &cursor_y, framebuffer);

    Serial.printf("Battery drawn: %s (%.2fV)\n", buf, voltage);
}

// Draw text with default properties
void drawTextHelper(int x, int y, const char *text, const GFXfont *font)
{
    int cursor_x = x;
    int cursor_y = y;
    FontProperties props = {15, 0, 0}; // White BG, Black FG
    writeln((GFXfont *)font, (char *)text, &cursor_x, &cursor_y, framebuffer);
}

// Draw a large temperature using custom bitmaps
void drawBigTemp(int x, int y, int temperature)
{
    char buf[10];
    sprintf(buf, "%d", temperature);
    int cursor_x = x;

    for (int i = 0; buf[i] != '\0'; i++)
    {
        char c = buf[i];
        const uint8_t *data = NULL;
        int w = 0, h = 0;

        switch (c)
        {
        case '0':
            data = DIGIT_48;
            w = DIGIT_48_W;
            h = DIGIT_48_H;
            break;
        case '1':
            data = DIGIT_49;
            w = DIGIT_49_W;
            h = DIGIT_49_H;
            break;
        case '2':
            data = DIGIT_50;
            w = DIGIT_50_W;
            h = DIGIT_50_H;
            break;
        case '3':
            data = DIGIT_51;
            w = DIGIT_51_W;
            h = DIGIT_51_H;
            break;
        case '4':
            data = DIGIT_52;
            w = DIGIT_52_W;
            h = DIGIT_52_H;
            break;
        case '5':
            data = DIGIT_53;
            w = DIGIT_53_W;
            h = DIGIT_53_H;
            break;
        case '6':
            data = DIGIT_54;
            w = DIGIT_54_W;
            h = DIGIT_54_H;
            break;
        case '7':
            data = DIGIT_55;
            w = DIGIT_55_W;
            h = DIGIT_55_H;
            break;
        case '8':
            data = DIGIT_56;
            w = DIGIT_56_W;
            h = DIGIT_56_H;
            break;
        case '9':
            data = DIGIT_57;
            w = DIGIT_57_W;
            h = DIGIT_57_H;
            break;
        case '-':
            data = DIGIT_MINUS;
            w = DIGIT_MINUS_W;
            h = DIGIT_MINUS_H;
            break;
        }

        if (data)
        {
            Rect_t area = {.x = cursor_x, .y = y, .width = w, .height = h};
            epd_copy_to_framebuffer(area, (uint8_t *)data, framebuffer);
            cursor_x += (w - 2); // Tight kerning
        }
    }

    // Symbol °C
    Rect_t degArea = {.x = cursor_x, .y = y, .width = DIGIT_DEG_W, .height = DIGIT_DEG_H};
    epd_copy_to_framebuffer(degArea, (uint8_t *)DIGIT_DEG, framebuffer);
    cursor_x += DIGIT_DEG_W;

    Rect_t C_Area = {.x = cursor_x, .y = y, .width = DIGIT_67_W, .height = DIGIT_67_H};
    epd_copy_to_framebuffer(C_Area, (uint8_t *)DIGIT_67, framebuffer);
}

// Icon Helpers
void drawIcon256(int x, int y, const uint8_t *icon_data)
{
    if (!icon_data)
        return;
    Rect_t area = {.x = x, .y = y, .width = 256, .height = 256};
    epd_copy_to_framebuffer(area, (uint8_t *)icon_data, framebuffer);
}

void drawIcon128(int x, int y, const uint8_t *src_data)
{
    if (!src_data)
        return;
    int src_w = 256, src_h = 256;
    int dst_w = 128, dst_h = 128; // Downscale by 2

    for (int dy = 0; dy < dst_h; dy++)
    {
        for (int dx = 0; dx < dst_w; dx++)
        {
            // Nearest Neighbor: Sample every 2nd pixel
            int sx = dx * 2;
            int sy = dy * 2;
            int src_idx = (sy * src_w + sx) / 2;
            uint8_t src_byte = src_data[src_idx];

            uint8_t color = (sx % 2 == 0) ? (src_byte & 0xF0) >> 4 : (src_byte & 0x0F);
            epd_draw_pixel(x + dx, y + dy, color * 17, framebuffer);
        }
    }
}

//  Parse Hour from ISO String "YYYY-MM-DDTHH:MM..."
int getHourFromISO(const char *iso)
{
    if (strlen(iso) < 13)
        return 0;
    char hourStr[3];
    hourStr[0] = iso[11];
    hourStr[1] = iso[12];
    hourStr[2] = '\0';
    return atoi(hourStr);
}

// Returns pointer to "Mo.", "Di.", etc. based on YYYY, MM, DD
const char *getDayOfWeek(int year, int month, int day)
{
    // Zeller's Congruence adjustment for Jan/Feb
    if (month < 3)
    {
        month += 12;
        year -= 1;
    }

    int k = year % 100;
    int j = year / 100;

    // Formula for ISO Day (1=Mon, ... 7=Sun) is slightly different,
    // but standard Zeller gives: 0=Sat, 1=Sun, 2=Mon, ... 6=Fri
    int h = (day + 13 * (month + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;

    // Map Zeller output (0=Sat) to German Short strings
    switch (h)
    {
    case 0:
        return "Sa.";
    case 1:
        return "So.";
    case 2:
        return "Mo.";
    case 3:
        return "Di.";
    case 4:
        return "Mi.";
    case 5:
        return "Do.";
    case 6:
        return "Fr.";
    }
    return "??";
}

// Wrapper to parse "YYYY-MM-DD" string
const char *getDayFromString(const char *isoDate)
{
    // Expected format: "2026-02-01..."
    // We can use sscanf or atoi with offsets

    // Simple parsing (Ascii to Int)
    // "2026"
    int y = (isoDate[0] - '0') * 1000 + (isoDate[1] - '0') * 100 + (isoDate[2] - '0') * 10 + (isoDate[3] - '0');
    // "02"
    int m = (isoDate[5] - '0') * 10 + (isoDate[6] - '0');
    // "01"
    int d = (isoDate[8] - '0') * 10 + (isoDate[9] - '0');

    return getDayOfWeek(y, m, d);
}

// --- CORE DRAWING FUNCTIONS ---

void drawForecastGridLines()
{
    // Horizontal
    epd_draw_hline(LEFT_PANEL_W, SCREEN_H / 2, SCREEN_W - LEFT_PANEL_W, COLOR_BLACK, framebuffer);
    epd_draw_hline(LEFT_PANEL_W, SCREEN_H / 2 + 1, SCREEN_W - LEFT_PANEL_W, COLOR_BLACK, framebuffer);

    // Vertical
    int splitX = LEFT_PANEL_W + GRID_BOX_W;
    epd_draw_vline(LEFT_PANEL_W, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    epd_draw_vline(LEFT_PANEL_W + 1, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    epd_draw_vline(splitX, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    epd_draw_vline(splitX + 1, 0, SCREEN_H, COLOR_BLACK, framebuffer);
}

// Draws a single Forecast Box (Icon + Text)
void drawForecastBox(int boxX, int boxY, const char *iconName,
                     const char *date, const char *temp,
                     const char *wind, const char *dew,
                     const char *clouds, const char *rain)
{

    // 1. Icon (128x128)
    const uint8_t *icon = getIconData(iconName);
    drawIcon128(boxX + BOX_ICON_X, boxY + BOX_ICON_Y, icon);

    // 2. Date
    drawTextHelper(boxX + BOX_DATE_X, boxY + BOX_DATE_Y, date, &FiraSans);

    // 3. Temp
    drawTextHelper(boxX + BOX_TEMP_X, boxY + BOX_TEMP_Y, temp, &FiraSans); // Or use drawBigTemp if desired

    // 4. Details List
    int listX = boxX + BOX_LIST_X;
    int listY = boxY + BOX_LIST_Y;

    drawTextHelper(listX, listY + (0 * BOX_LIST_GAP), wind, &OpenSans10B);
    drawTextHelper(listX, listY + (1 * BOX_LIST_GAP), dew, &OpenSans10B);
    drawTextHelper(listX, listY + (2 * BOX_LIST_GAP), clouds, &OpenSans10B);
    drawTextHelper(listX, listY + (3 * BOX_LIST_GAP), rain, &OpenSans10B);
}

void drawLeftPanel(const char *iconName, int temp, const char *wind, const char *dew, const char *clouds, const char *rain, const char *timeStr)
{
    // 1. Large Icon (256x256)
    const uint8_t *icon = getIconData(iconName);
    drawIcon256(5, 5, icon); // Keeping your coordinates

    // 2. Large Temp
    drawBigTemp(245, 90, temp);

    // 3. Details List
    int listX = LEFT_PANEL_PAD;
    int listY = 300;
    int gap = 50;

    drawTextHelper(listX, listY + (0 * gap), wind, &FiraSans);
    drawTextHelper(listX, listY + (1 * gap), dew, &FiraSans);
    drawTextHelper(listX, listY + (2 * gap), clouds, &FiraSans);
    drawTextHelper(listX, listY + (3 * gap), rain, &FiraSans);

    // 4. Footer Time
    drawTextHelper(listX, 500, timeStr, &FiraSans);
}

void updateUI(WeatherData data)
{
    // 1. Clear Framebuffer to White
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 2. Draw Static Lines
    drawForecastGridLines();

    // 3. Draw Battery (Helper from ui_layout.cpp)
    drawBattery();

    if (!data.valid)
    {
        drawTextHelper(50, 200, "Waiting for Data...", &FiraSans);
        // We still flush so the user sees the error/waiting message
    }
    else
    {
        // --- PREPARE LEFT PANEL STRINGS ---
        char windStr[32], dewStr[32], cloudStr[32], rainStr[32], dateStr[64];

        sprintf(windStr, "%.0f km/h Wind", data.current.windSpeed60);
        sprintf(dewStr, "%.0f°C Taupunkt", data.current.dew_point);
        sprintf(cloudStr, "%d%% bewölkt", data.current.cloudCover);
        sprintf(rainStr, "%.0f mm/h Regen", data.current.rain);

        // Format Date: "HH:MM      DD.MM.YYYY"
        // Input: "2026-02-01T15:30:00+01:00"
        // We can cheat slightly and just parse pointers if format is strict,
        // or use strptime if available. For now, manual mapping:
        if (strlen(data.current.timestamp) >= 16)
        {
            // timestamp: 0123-56-89T11:34
            const char *t = data.current.timestamp;
            sprintf(dateStr, "%.5s      %.2s.%.2s.%.4s",
                    t + 11, // HH:MM
                    t + 8,  // DD
                    t + 5,  // MM
                    t + 0   // YYYY
            );
        }
        else
        {
            strcpy(dateStr, "Unknown Date");
        }

        // --- CALL LEFT PANEL HELPER ---
        drawLeftPanel(
            data.current.icon,
            (int)data.current.temp,
            windStr,
            dewStr,
            cloudStr,
            rainStr,
            dateStr);

        // --- CALL RIGHT GRID LOOP ---
        // --- FORECAST GRID LOGIC (SMART WINDOW) ---
        int currentHour = getHourFromISO(data.current.timestamp);
        int startIndex = 0;

        if (currentHour < 6) {
            startIndex = 0; // Show Today 6:00, 14:00, Tmrw 6:00, 14:00
        } else if (currentHour >= 6 && currentHour < 14) {
            startIndex = 1; // Show Today 14:00, Tmrw 6:00, 14:00, DayAfter 6:00
        } else {
            startIndex = 2; // Show Tmrw 6:00, 14:00, DayAfter 6:00, 14:00
        }

        // Draw up to 4 boxes
        for (int i = 0; i < 4; i++) {
            int dataIndex = startIndex + i;
            if (dataIndex >= data.forecastCount) break;

            int gridCol = i % 2;
            int gridRow = i / 2;
            int boxX = LEFT_PANEL_W + (gridCol * GRID_BOX_W);
            int boxY = (gridRow * GRID_BOX_H);

            char f_date[32], f_temp[16], f_wind[32], f_dew[32], f_cloud[32], f_rain[32];

            if (strlen(data.forecast[dataIndex].timestamp) >= 16) {
                const char *dayStr = getDayFromString(data.forecast[dataIndex].timestamp);
                sprintf(f_date, "%s %.5s", dayStr, data.forecast[dataIndex].timestamp + 11);
            } else {
                strcpy(f_date, "??:??");
            }

            sprintf(f_temp, "%.0f°C", data.forecast[dataIndex].temp);
            sprintf(f_wind, "%.0f km/h Wind", data.forecast[dataIndex].windSpeed); // Shortened "Wind" to fit box?
            sprintf(f_dew, "%.0f°C Taupunkt", data.forecast[dataIndex].dew_point);
            sprintf(f_cloud, "%d%% bewölkt", data.forecast[dataIndex].cloudCover);
            sprintf(f_rain, "%d %% Regen", data.forecast[dataIndex].rain_probability);

            drawForecastBox(
                boxX, boxY,
                data.forecast[dataIndex].icon,
                f_date,
                f_temp,
                f_wind,
                f_dew,
                f_cloud,
                f_rain);
        }
    }

    // 4. Flush to Screen
    Serial.println("Updating Display...");
    epd_poweron();
    epd_clear(); // Optional: Clear ghosting
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    Serial.println("Display Update Done.");
}