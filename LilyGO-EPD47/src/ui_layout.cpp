#include "epd_driver.h" // Standard EPD47 driver
#include "weather_icons.h"
#include "icon_display.h"
#include "stdio.h"
#include "HardwareSerial.h"
#include "network_management.h"
#include "ui_layout.h"
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


// --- Accurate Nonlinear LiPo Curve ---
struct LiPoPoint {
    float voltage;
    int percentage;
};

static const LiPoPoint LIPO_CURVE[] = {
    {4.20f, 100},
    {4.15f, 95},
    {4.10f, 90},
    {4.03f, 80},
    {3.95f, 70},
    {3.88f, 60},
    {3.83f, 50},
    {3.80f, 40},
    {3.76f, 30},
    {3.73f, 20},
    {3.69f, 10},
    {3.60f, 5},
    {3.40f, 2},
    {3.30f, 0}
};

// --- HELPER FUNCTIONS ---
// Get battery percentage (returns 0-100) using 14-point nonlinear LiPo curve
int getBatteryPercentage()
{
    // 1. Read Voltage
    analogSetAttenuation(ADC_11db);
    pinMode(BATTERY_PIN, ANALOG);

    uint32_t raw = 0;
    for (int i = 0; i < 8; i++)
    {
        raw += analogRead(BATTERY_PIN);
    }
    raw /= 8;
    pinMode(BATTERY_PIN, INPUT); // Reset pin to high-impedance to avoid current leakage

    float voltage = (raw / 4095.0f) * 3.3f * 2.0f; // Voltage divider adjustment

    const size_t num_points = sizeof(LIPO_CURVE) / sizeof(LIPO_CURVE[0]);
    if (voltage >= LIPO_CURVE[0].voltage)
        return 100;
    if (voltage <= LIPO_CURVE[num_points - 1].voltage)
        return 0;

    for (size_t i = 0; i < num_points - 1; i++)
    {
        if (voltage <= LIPO_CURVE[i].voltage && voltage >= LIPO_CURVE[i + 1].voltage)
        {
            float v_high = LIPO_CURVE[i].voltage;
            float v_low = LIPO_CURVE[i + 1].voltage;
            int p_high = LIPO_CURVE[i].percentage;
            int p_low = LIPO_CURVE[i + 1].percentage;

            float fraction = (voltage - v_low) / (v_high - v_low);
            return p_low + (int)(fraction * (p_high - p_low) + 0.5f);
        }
    }
    return 0;
}

void drawBattery(int battery_percentage)
{
    // 2. Format Text
    char buf[8];
    sprintf(buf, "%d%%", battery_percentage);

    // 3. Calculate Position (Top Right with 15px margin)
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds((GFXfont *)&OpenSans8B, buf, &x0, &y0, &x1, &y1, &w, &h, NULL);

    int32_t cursor_x = SCREEN_W - 15 - w;
    int32_t cursor_y = 25; // Baseline of text

    // 4. Draw to Framebuffer
    writeln((GFXfont *)&OpenSans8B, buf, &cursor_x, &cursor_y, framebuffer);
}

// Draw text with default properties
void drawTextHelper(int x, int y, const char *text, const GFXfont *font)
{
    int cursor_x = x;
    int cursor_y = y;
    FontProperties props = {15, 0, 0}; // White BG, Black FG
    writeln((GFXfont *)font, (char *)text, &cursor_x, &cursor_y, framebuffer);
}

// Draw low battery warning screen
void drawBatteryWarningScreen(int battery_percentage)
{
    // 1. Clear Framebuffer to White
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    
    // 2. Draw warning border (thick lines around edge)
    for (int i = 0; i < 10; i++) {
        epd_draw_hline(0, i, SCREEN_W, COLOR_BLACK, framebuffer);
        epd_draw_hline(0, SCREEN_H - 1 - i, SCREEN_W, COLOR_BLACK, framebuffer);
        epd_draw_vline(i, 0, SCREEN_H, COLOR_BLACK, framebuffer);
        epd_draw_vline(SCREEN_W - 1 - i, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    }

    auto drawCentered = [](int y, const char *text, const GFXfont *font) {
        int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0, w = 0, h = 0;
        get_text_bounds(font, text, &x0, &y0, &x1, &y1, &w, &h, NULL);
        drawTextHelper((SCREEN_W - w) / 2, y, text, font);
    };

    // 3. German Title
    drawCentered(80, "! WARNUNG !", &FiraSans);
    
    // 4. Centered battery icon
    int batt_x = SCREEN_W / 2 - 60;
    int batt_y = 200;
    epd_draw_rect(batt_x, batt_y, 120, 80, COLOR_BLACK, framebuffer);
    epd_fill_rect(batt_x + 120, batt_y + 25, 8, 30, COLOR_BLACK, framebuffer);
    
    // 5. Centered percentage inside battery icon
    char pct_str[8];
    sprintf(pct_str, "%d%%", battery_percentage);
    int32_t px0 = 0, py0 = 0, px1 = 0, py1 = 0, pw = 0, ph = 0;
    get_text_bounds(&FiraSans, pct_str, &px0, &py0, &px1, &py1, &pw, &ph, NULL);
    int pct_x = batt_x + (120 - pw) / 2;
    int pct_y = batt_y + 55;
    drawTextHelper(pct_x, pct_y, pct_str, &FiraSans);
    
    // 6. German Warning Messages
    drawCentered(350, "Batteriestand kritisch!", &FiraSans);
    drawCentered(420, "Bitte das Gerät", &FiraSans);
    drawCentered(490, "sofort aufladen.", &FiraSans);
}

// Draw a large temperature using custom bitmaps
void drawBigTemp(int x, int y, int temperature)
{
    char buf[10];
    if (isSentinel(temperature))
    {
        strcpy(buf, "--");
    }
    else
    {
        sprintf(buf, "%d", temperature);
    }
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
    if (!src_data || !framebuffer)
        return;
    const int src_w = 256;
    const int dst_w = 128, dst_h = 128; // Downscale 256x256 to 128x128

    for (int dy = 0; dy < dst_h; dy++)
    {
        int sy0 = dy * 2;
        int sy1 = sy0 + 1;
        int py = y + dy;
        if (py < 0 || py >= SCREEN_H)
            continue;

        const uint8_t *src_row0 = &src_data[sy0 * (src_w / 2)];
        const uint8_t *src_row1 = &src_data[sy1 * (src_w / 2)];
        uint8_t *fb_row = &framebuffer[py * (SCREEN_W / 2)];

        for (int dx = 0; dx < dst_w; dx++)
        {
            int px = x + dx;
            if (px < 0 || px >= SCREEN_W)
                continue;

            uint8_t b0 = src_row0[dx];
            uint8_t b1 = src_row1[dx];

            uint8_t p00 = b0 & 0x0F;
            uint8_t p10 = (b0 >> 4) & 0x0F;
            uint8_t p01 = b1 & 0x0F;
            uint8_t p11 = (b1 >> 4) & 0x0F;

            // 2x2 box-filter downscale with rounding
            uint8_t avg = (uint8_t)((p00 + p10 + p01 + p11 + 2) >> 2);

            uint8_t *buf_ptr = &fb_row[px >> 1];
            if (px & 1)
            {
                *buf_ptr = (*buf_ptr & 0x0F) | (avg << 4);
            }
            else
            {
                *buf_ptr = (*buf_ptr & 0xF0) | avg;
            }
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

void updateUI(const WeatherData &data)
{
    // 1. Clear Framebuffer to White
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 2. Get Battery Percentage
    int battery_percentage = getBatteryPercentage();
    Serial.printf("Battery: %d%%\n", battery_percentage);

    // 3. Check if battery is critically low (< 3%)
    if (battery_percentage < 3)
    {
        // Show low battery warning screen instead of weather
        drawBatteryWarningScreen(battery_percentage);
        
        // 4. Flush to Screen
        Serial.println("Updating Display (Low Battery Warning)...");
        epd_poweron();
        epd_clear();
        epd_draw_grayscale_image(epd_full_screen(), framebuffer);
        epd_poweroff_all();
        Serial.println("Display Update Done.");
        return;  // Exit early, don't show weather data
    }

    // 4. Draw Static Lines and Battery (normal flow)
    drawForecastGridLines();
    drawBattery(battery_percentage);

    if (!data.valid)
    {
        drawTextHelper(50, 200, "Waiting for Data...", &FiraSans);
        // We still flush so the user sees the error/waiting message
    }
    else
    {
        // --- PREPARE LEFT PANEL STRINGS ---
        static char windStr[32], dewStr[32], cloudStr[32], rainStr[32], dateStr[64];

        if (isSentinel(data.current.windSpeed60))
            strcpy(windStr, "-- km/h Wind");
        else
            sprintf(windStr, "%.0f km/h Wind", data.current.windSpeed60);

        if (isSentinel(data.current.dew_point))
            strcpy(dewStr, "--°C Taupunkt");
        else
            sprintf(dewStr, "%.0f°C Taupunkt", data.current.dew_point);

        if (isSentinel(data.current.cloudCover))
            strcpy(cloudStr, "--% bewölkt");
        else
            sprintf(cloudStr, "%d%% bewölkt", data.current.cloudCover);

        if (isSentinel(data.current.rain))
            strcpy(rainStr, "-- mm/h Regen");
        else
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

            if (isSentinel(data.forecast[dataIndex].temp))
                strcpy(f_temp, "--°C");
            else
                sprintf(f_temp, "%.0f°C", data.forecast[dataIndex].temp);

            if (isSentinel(data.forecast[dataIndex].windSpeed))
                strcpy(f_wind, "-- km/h Wind");
            else
                sprintf(f_wind, "%.0f km/h Wind", data.forecast[dataIndex].windSpeed);

            if (isSentinel(data.forecast[dataIndex].dew_point))
                strcpy(f_dew, "--°C Taupunkt");
            else
                sprintf(f_dew, "%.0f°C Taupunkt", data.forecast[dataIndex].dew_point);

            if (isSentinel(data.forecast[dataIndex].cloudCover))
                strcpy(f_cloud, "--% bewölkt");
            else
                sprintf(f_cloud, "%d%% bewölkt", data.forecast[dataIndex].cloudCover);

            if (isSentinel(data.forecast[dataIndex].rain_probability))
                strcpy(f_rain, "--% Regen");
            else
                sprintf(f_rain, "%d%% Regen", data.forecast[dataIndex].rain_probability);

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
    Serial.println("Clearing screen to prevent ghosting / overlapping...");
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
    Serial.println("Display Update Done.");
}