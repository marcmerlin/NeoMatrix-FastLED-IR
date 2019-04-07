#ifndef nfldefines_h
#define nfldefines_h

#define LEDMATRIX
//#define NEOMATRIX
#define DISABLE_MATRIX_TEST
#include "neomatrix_config.h"

#ifdef ESP8266
    #define mheight 32
#elif defined(ESP32)
    #ifdef NEOMATRIX
	#define mheight 64
    #else
	#define mheight 96
    #endif
#else
#error "Matrix config undefined, please set height"
#endif

// On ESP32, I have a 64x64 direct matrix (not tiled) with 2 options of drivers.
#if mheight == 96
    // Using RGBPanel via SmartMatrix

    // Which demos are shown, and in which order
    // 8 burn baby burn,
    // 9 safety third
    // 12 smiley face, not meant for high res
    // 21 not enough dots
    // 25 pacman, 
    // fix 28, 31, 
    // 32 looks bad on mismatched size
    // 35 wave looks bad, 
    // 37 also looks bad, 
    // 39 not big enough
    // 86 Bman
    #define LAST_MATRIX 87
    const uint8_t demo_mapping[] = { 5, LAST_MATRIX, 2, 23, 18, 19, 3, 22, 20, 24, 4, 26, 27, 29, 5, 30, 33, 34, 36, 6, 40, 41, 42, 7, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 
     56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, };
    const uint8_t best_mapping[] = { 5, 23, 24, 27, 63, 64, 70, 74, 83, LAST_MATRIX };

    #define RECV_PIN 34

    #define STRIP_NUM_LEDS 48
    CRGB leds[STRIP_NUM_LEDS];
    #define NEOPIXEL_PIN 13
#elif mheight == 64
    // 64x64 matrix with optional 16 pin parallel driver
    // 55fps without 16PINS, 110fps with 16PINS
    #define ESP32_16PINS

    #define LAST_MATRIX 87
    // Which demos are shown, and in which order
    // 25 pacman, 57 BM gif
    const uint8_t demo_mapping[] = { 57, LAST_MATRIX, 2, 23, 1, 12, 18, 19, 20, 21, 22, 24, 3, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 7, 46, 47, 48, 49, 50, 8, 51, 52, 53, 54, 55, 9, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 0, };
    const uint8_t best_mapping[] = { 9, 25, 27, 55, 66, LAST_MATRIX };

    #define RECV_PIN 34
#elif mheight == 32
    #define LAST_MATRIX 69
    // Which demos are shown, and in which order
    // skip 8 burn, 9 safety third
    const uint8_t demo_mapping[] = { LAST_MATRIX, 2, 23, 1, 4, 12, 18, 19, 20, 21, 22, 24, 25, 3, 26, 27, 28, 29, 30, 5, 31, 32, 33, 34, 35, 6, 36, 37, 38, 39, 7, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 0, };
    // On ESP8266, I drive 3x 32x8 panels plus neopixel strips
    // 55 hypno, 60 dance girl, 66 sonix
    const uint8_t best_mapping[] = { 5, 12, 24, 25, 27, 55, 60, 66, LAST_MATRIX };

    #define M32B8X3
    #define STRIP_NUM_LEDS 48

    CRGB leds[STRIP_NUM_LEDS];
    #define NEOPIXEL_PIN D1 // GPIO5

    #define RECV_PIN D4
#else
#error "unknown matrix height, no idea what demos to run"
#endif 

#define demo_cnt ARRAY_SIZE(demo_mapping)
#define best_cnt ARRAY_SIZE(best_mapping)

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
bool sav_newgif(const char *pathname);
void sav_setup();
bool sav_loop();

#endif // nfldefines_h
