#include <Arduino.h>
#include <epd_driver.h>
#define LILYGO_T5_47_S3 
 // The EPD driver

// Declare the external UI function (defined in ui_layout.cpp)
extern void updateUI();




#include <Arduino.h>
#include <epd_driver.h>

// Force external declaration to debug linking
uint8_t *framebuffer = NULL;

void setup() {
    Serial.begin(115200);
    delay(2000);

    // 1. Init PSRAM
    if (!psramInit()) {
        Serial.println("PSRAM Init Failed");
        while(1);
    }

    // 2. Allocate Framebuffer
    // EPD_WIDTH (960) * EPD_HEIGHT (540) / 2 = 259,200 bytes
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer) {
        Serial.println("Alloc memory failed !!!");
        while (1);
    }
    // Initialize to White (0xFF = White in this 4bpp mode usually, or 0x00? Check demo)
    // Usually 0xFF is White for EPD.
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // 2. Init EPD
    epd_init();

    // 3. CRITICAL CHECK: Is the global framebuffer actually set?
    if (framebuffer == NULL) {
        Serial.println("!! FRAMEBUFFER IS NULL !!");
        // This confirms the linker is using a different variable instance
    } else {
        Serial.printf("Framebuffer Address: %p\n", framebuffer);
    }

    // 4. Power on and Clear
    epd_poweron();
    epd_clear();
    epd_poweroff();

    // 5. Run UI
    updateUI();
}



void loop() {
    // Nothing here! 
    // E-Ink displays should update once and then sleep.
}
