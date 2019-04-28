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

    // Memory After GIF init:
    // before SmartMatrix::GFX bufferless change
    // Heap/32-bit Memory Available: 114048 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  28300 bytes total,  15456 bytes largest free block

    // after SmartMatrix::GFX zero copy change.
    // Heap/32-bit Memory Available: 132476 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  46728 bytes total,  40976 bytes largest free block


    //TODO: enable swirl on 24 diplay but not big display
    //TODO2: make sure all other except radar are turned on


    // Which demos are shown, and in which order
    // 0 unused
    // 2 essr_bbb
    // 8 burn baby burn,
    // 9 safety third (24x32 shirt only)
    // 10 scroll code
    // 12 smiley face, not meant for high res
    // 13-17 unused
    // 19 pride, not good enough with thin dots
    // 21 not enough dots
    // 25 pacman,
    // 27 last pattern
    // 56 hypno
    // 70 dancing people, 73 rubix cubes moving, 47 spin triangles, 77 plasma, 80 colortoroid
    // 82 spinningpattern, 83 spacetime, 85 hearttunnel, 86 sonic, 87 BM, 96 MJ
    const uint8_t demo_mapping[] = {
	10, 70, 96, 17, 2, 18, 5, 3, 20, 4, 22, 23, 24, 6, 26, 27, 7,
	// aurora (removed 38 and 41)
	30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 42,
	// table mark estes. 
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 
	// gifs. removed 87 BM
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 88, 89, 
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 
    };

    const uint8_t best_mapping[] = { 5, 17, 23, 24, 27, 56, 70, 73, 77, 80, 82, 83, 87, 91, 100 };

    #define RECV_PIN 34

    #define STRIP_NUM_LEDS 48
    CRGB leds[STRIP_NUM_LEDS];
    #define NEOPIXEL_PIN 13
#elif mheight == 64
    // 64x64 matrix with optional 16 pin parallel driver
    // 55fps without 16PINS, 110fps with 16PINS
    #define ESP32_16PINS

    // Which demos are shown, and in which order
    // 25 pacman, 57 BM gif
    const uint8_t demo_mapping[] = {
	87, 10, 17, 2, 21, 18, 5, 19, 3, 20, 21, 4, 22, 8, 23, 24, 6, 26, 27, 7,
	// aurora (removed 38)
	30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 41, 42,
	// table mark estes	
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 
	// gifs
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 
    };
    const uint8_t best_mapping[] = { 9, 17, 25, 27, 56, 70, 73, 77, 80, 82, 83, 87, 91, 100 };

    #define RECV_PIN 34
#elif mheight == 32
    // Which demos are shown, and in which order
    // skip 8 burn, 9 safety third
    const uint8_t demo_mapping[] = {
	1, 17, 2, 12, 18, 5, 19, 3, 20, 21, 4, 22, 23, 24, 25, 6, 26, 27, 7,
	// aurora
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	// table mark estes	
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 
	// gifs
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
    };
    // On ESP8266, I drive 3x 32x8 panels plus neopixel strips
    // 70 hypno, 75 dance girl, 81 sonix???
    const uint8_t best_mapping[] = { 5, 12, 17, 24, 25, 27, 70, 75, 81 };

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
