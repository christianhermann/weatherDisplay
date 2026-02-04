#include "epd_driver.h"  // Standard EPD47 driver
#include "weather_icons.h"
#include "icon_display.h"
#include "stdio.h"
#include <HardwareSerial.h>

// FONTS
#include "FiraSans.h" // Assuming this is your main font (approx 12-16pt?)
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
#define LEFT_PANEL_W    400
#define LEFT_PANEL_PAD  15  // Padding from left edge

// Right Grid (Forecast)
#define GRID_BOX_W      ((SCREEN_W - LEFT_PANEL_W) / 2) // 280px
#define GRID_BOX_H      (SCREEN_H / 2)                  // 270px

// Forecast Box Internal Offsets (Relative to Top-Left of Box)
#define BOX_DATE_X      20
#define BOX_DATE_Y      60   // Baseline

#define BOX_ICON_X      5
#define BOX_ICON_Y      75   // Top-left of icon (128x128)

#define BOX_TEMP_X      20
#define BOX_TEMP_Y      230  // Baseline (approx)

// List Items inside Box (Wind, Humidity, etc.)
#define BOX_LIST_X      125  // Right side of box
#define BOX_LIST_Y      120  // Start Y
#define BOX_LIST_GAP    37   // Gap between lines


// Reference the global variable from main.cpp
extern uint8_t *framebuffer; 

// --- HELPER FUNCTIONS ---

// Draw text with default properties
void drawTextHelper(int x, int y, const char *text, const GFXfont *font) {
    int cursor_x = x;
    int cursor_y = y;
    FontProperties props = {15, 0, 0}; // White BG, Black FG
    writeln((GFXfont *)font, (char *)text, &cursor_x, &cursor_y, framebuffer);
}

// Draw a large temperature using custom bitmaps
void drawBigTemp(int x, int y, int temperature) {
    char buf[10];
    sprintf(buf, "%d", temperature);
    int cursor_x = x;
    
    for (int i = 0; buf[i] != '\0'; i++) {
        char c = buf[i];
        const uint8_t *data = NULL;
        int w = 0, h = 0;
        
        switch(c) {
            case '0': data = DIGIT_48; w = DIGIT_48_W; h = DIGIT_48_H; break;
            case '1': data = DIGIT_49; w = DIGIT_49_W; h = DIGIT_49_H; break;
            case '2': data = DIGIT_50; w = DIGIT_50_W; h = DIGIT_50_H; break;
            case '3': data = DIGIT_51; w = DIGIT_51_W; h = DIGIT_51_H; break;
            case '4': data = DIGIT_52; w = DIGIT_52_W; h = DIGIT_52_H; break;
            case '5': data = DIGIT_53; w = DIGIT_53_W; h = DIGIT_53_H; break;
            case '6': data = DIGIT_54; w = DIGIT_54_W; h = DIGIT_54_H; break;
            case '7': data = DIGIT_55; w = DIGIT_55_W; h = DIGIT_55_H; break;
            case '8': data = DIGIT_56; w = DIGIT_56_W; h = DIGIT_56_H; break;
            case '9': data = DIGIT_57; w = DIGIT_57_W; h = DIGIT_57_H; break;
            case '-': data = DIGIT_MINUS; w = DIGIT_MINUS_W; h = DIGIT_MINUS_H; break;
        }

        if (data) {
            Rect_t area = { .x = cursor_x, .y = y, .width = w, .height = h };
            epd_copy_to_framebuffer(area, (uint8_t *)data, framebuffer);
            cursor_x += (w - 2); // Tight kerning
        }
    }
    
    // Symbol °C
    Rect_t degArea = { .x = cursor_x, .y = y, .width = DIGIT_DEG_W, .height = DIGIT_DEG_H };
    epd_copy_to_framebuffer(degArea, (uint8_t *)DIGIT_DEG, framebuffer);
    cursor_x += DIGIT_DEG_W;
    
    Rect_t C_Area = { .x = cursor_x, .y = y, .width = DIGIT_67_W, .height = DIGIT_67_H };
    epd_copy_to_framebuffer(C_Area, (uint8_t *)DIGIT_67, framebuffer);
}

// Icon Helpers
void drawIcon256(int x, int y, const uint8_t *icon_data) {
    if (!icon_data) return;
    Rect_t area = { .x = x, .y = y, .width = 256, .height = 256 };
    epd_copy_to_framebuffer(area, (uint8_t *)icon_data, framebuffer);
}

void drawIcon128(int x, int y, const uint8_t *src_data) {
    if (!src_data) return;
    int src_w = 256, src_h = 256;
    int dst_w = 128, dst_h = 128; // Downscale by 2

    for (int dy = 0; dy < dst_h; dy++) {
        for (int dx = 0; dx < dst_w; dx++) {
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

// --- CORE DRAWING FUNCTIONS ---

void drawForecastGridLines() {
    // Horizontal
    epd_draw_hline(LEFT_PANEL_W, SCREEN_H/2, SCREEN_W - LEFT_PANEL_W, COLOR_BLACK, framebuffer);
    epd_draw_hline(LEFT_PANEL_W, SCREEN_H/2 + 1, SCREEN_W - LEFT_PANEL_W, COLOR_BLACK, framebuffer);

    // Vertical
    int splitX = LEFT_PANEL_W + GRID_BOX_W;
    epd_draw_vline(LEFT_PANEL_W, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    epd_draw_vline(LEFT_PANEL_W + 1, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    epd_draw_vline(splitX, 0, SCREEN_H, COLOR_BLACK, framebuffer);
    epd_draw_vline(splitX + 1, 0, SCREEN_H, COLOR_BLACK, framebuffer);
}


// Draws a single Forecast Box (Icon + Text)
void drawForecastBox(int boxX, int boxY, const char* iconName, 
                     const char* date, const char* temp, 
                     const char* wind, const char* hum, const char* clouds, const char* rain) {
    
    // 1. Icon (128x128)
    const uint8_t* icon = getIconData(iconName);
    drawIcon128(boxX + BOX_ICON_X, boxY + BOX_ICON_Y, icon);

    // 2. Date
    drawTextHelper(boxX + BOX_DATE_X, boxY + BOX_DATE_Y, date, &FiraSans);

    // 3. Temp
    drawTextHelper(boxX + BOX_TEMP_X, boxY + BOX_TEMP_Y, temp, &FiraSans); // Or use drawBigTemp if desired

    // 4. Details List
    int listX = boxX + BOX_LIST_X;
    int listY = boxY + BOX_LIST_Y;
    
    drawTextHelper(listX, listY + (0 * BOX_LIST_GAP), wind,   &OpenSans10B);
    drawTextHelper(listX, listY + (1 * BOX_LIST_GAP), hum,    &OpenSans10B);
    drawTextHelper(listX, listY + (2 * BOX_LIST_GAP), clouds, &OpenSans10B);
    drawTextHelper(listX, listY + (3 * BOX_LIST_GAP), rain,   &OpenSans10B);
}

void drawLeftPanel(const char* iconName, int temp, const char* wind, const char* hum, const char* clouds, const char* rain, const char* timeStr) {
    // 1. Large Icon (256x256)
    const uint8_t* icon = getIconData(iconName);
    drawIcon256(5, 5, icon); // Keeping your coordinates

    // 2. Large Temp
    drawBigTemp(245, 90, temp);

    // 3. Details List
    int listX = LEFT_PANEL_PAD;
    int listY = 300;
    int gap = 50;

    drawTextHelper(listX, listY + (0 * gap), wind,   &FiraSans);
    drawTextHelper(listX, listY + (1 * gap), hum,    &FiraSans);
    drawTextHelper(listX, listY + (2 * gap), clouds, &FiraSans);
    drawTextHelper(listX, listY + (3 * gap), rain,   &FiraSans);

    // 4. Footer Time
    drawTextHelper(listX, 500, timeStr, &FiraSans);
}


void updateUI() {
    // 1. Clear/Lines
    drawForecastGridLines();

    // 2. Draw Left Panel (Current)
    drawLeftPanel("clear-day", 37, 
                  "17 km/h Wind", "17% r.F.", "25% bewölkt", "17% Regen", 
                  "13:00      17.01.2026");

    // 3. Draw Right Grid (Forecast)
    int col1 = LEFT_PANEL_W;
    int col2 = LEFT_PANEL_W + GRID_BOX_W;
    int row1 = 0;
    int row2 = GRID_BOX_H;

    // Box 1 (Top Left)
    drawForecastBox(col1, row1, "clear-day", "Mo. 6:00", "12°C",
                    "17 km/h Wind", "17% r.F.", "25% bewölkt", "17% Regen");

    // Box 2 (Top Right)
    drawForecastBox(col2, row1, "clear-day", "Mo. 14:00", "12°C",
                    "17 km/h Wind", "17% r.F.", "25% bewölkt", "17% Regen");

    // Box 3 (Bottom Left)
    drawForecastBox(col1, row2, "clear-day", "Di. 6:00", "12°C",
                    "17 km/h Wind", "17% r.F.", "25% bewölkt", "17% Regen");

    // Box 4 (Bottom Right)
    drawForecastBox(col2, row2, "clear-day", "Di. 14:00", "12°C",
                    "17 km/h Wind", "17% r.F.", "25% bewölkt", "17% Regen");

    // 4. Flush to Screen
    Serial.println("Updating Display...");
    epd_poweron();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    Serial.println("Display Update Done.");
}