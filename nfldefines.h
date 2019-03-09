#ifndef nfldefines_h
#define nfldefines_h

#define LEDMATRIX
#define NEOMATRIX
#include "neomatrix_config.h"
#include <LEDSprites.h>

// On ESP32, I have a 64x64 direct matrix (not tiled) with 2 options of drivers.
#ifdef ESP32
    // 64x64 matrix with optional 16 pin parallel driver
    // 55fps without 16PINS, 110fps with 16PINS
    #define ESP32_16PINS

    #define LAST_MATRIX 87
    // Which demos are shown, and in which order
    // 24 buggy storm, 25 pacman, 57 BM gif
    uint8_t demo_mapping[] = { 57, LAST_MATRIX, 2, 23, 1, 18, 19, 20, 21, 22, 23, 3, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 7, 46, 47, 48, 49, 50, 8, 51, 52, 53, 54, 55, 9, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, };
    uint8_t best_mapping[] = { 9, 18, 25, 27, 55, 66, LAST_MATRIX };

    #define RECV_PIN 34
#else // not ESP32 (ESP8266)
    #define LAST_MATRIX 69
    // Which demos are shown, and in which order
    // skip 8 burn, 9 safety third
    uint8_t demo_mapping[] = { LAST_MATRIX, 2, 23, 1, 4, 18, 19, 20, 21, 22, 24, 25, 3, 26, 27, 28, 29, 30, 5, 31, 32, 33, 34, 35, 6, 36, 37, 38, 39, 7, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 0, };
    // On ESP8266, I drive 3x 32x8 panels plus neopixel strips
    // 55 hypno, 60 dance girl, 66 sonix
    uint8_t best_mapping[] = { 5, 18, 24, 25, 27, 55, 60, 66, LAST_MATRIX };
    #define M32B8X3
    #define STRIP_NUM_LEDS 48

    CRGB leds[STRIP_NUM_LEDS];
    #define NEOPIXEL_PIN D1 // GPIO5

    #define RECV_PIN D4
#endif // ESP32
#define demo_cnt (sizeof(demo_mapping)/sizeof(demo_mapping[0]))
#define best_cnt (sizeof(best_mapping)/sizeof(best_mapping[0]))

// Disable fonts in many sizes
// Sketch uses 283784 bytes (27%) of program storage space. Maximum is 1044464 bytes.
// Global variables use 32880 bytes (40%) of dynamic memory, leaving 49040 bytes for local variables. Maximum is 81920 bytes.
// Uploading 287936 bytes from /tmp/arduino_build_498793/NeoMatrix-FastLED-IR.ino.bin to flash at 0x00000000
//#define NOFONTS 1

// Separate Neopixel Strip, if used. Not the NeoMatrix
uint8_t led_brightness = 64;

// show all demos by default,
bool show_best_demos = false;           


bool matrix_reset_demo = 1;

// FIXME, show could be a callback function for FastLEDshowESP32
void matrix_show();
void aurora_setup();

#endif // nfldefines_h
