#ifndef nfldefines_h
#define nfldefines_h

#define LEDMATRIX
//#define NEOMATRIX
#define DISABLE_MATRIX_TEST
#include "neomatrix_config.h"

#define HAS_FS
#ifdef ESP8266
    #define mheight 32
#elif defined(ESP32)
    #ifdef M64BY64
	#define mheight 64
    #else
	#define mheight 96
    #endif
#elif defined(ARDUINOONPC)
    #define mheight 192
#else
#error "Matrix config undefined, please set height"
#endif

#ifdef ESP32
// Use https://github.com/lbernstone/IR32.git instead of IRRemote
//#define ESP32RMTIR
#endif

// On ESP32, I have a 64x64 direct matrix (not tiled) with 2 options of drivers.
#if mheight == 96 || mheight == 192
    // Using RGBPanel via SmartMatrix

    // Memory After GIF init:
    // before SmartMatrix::GFX bufferless change
    // Heap/32-bit Memory Available: 114048 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  28300 bytes total,  15456 bytes largest free block

    // after SmartMatrix::GFX zero copy change.
    // Heap/32-bit Memory Available: 132476 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  46728 bytes total,  40976 bytes largest free block


    // Which demos are shown, and in which order
    // 0 unused
    // 2 BM essbr
    // 3 BM essbr fade
    // 4 TF tfsf zoom
    // 5 TF tfsf zoom2
    // 6 tfsf letters
    // 7 with every beat
    // 8 BM burn baby burn, 64x64 only
    // 9 BM safety third (24x32 shirt and 64x64 only)
    // 10 scroll code
    // 12 smiley face, not meant for high res
    // 13-16 unused
    // 17 fireworks
    // 19 pride, not good enough with thin dots
    // 21 color sweeping, not enough dots for 64x96
    // 25 pacman, not for anything but 24x32
    // 35 aurora spiro1
    // 36 aurora spiro2
    // 56 hypno
    // 78 MJ, 84  dancing people, 87 rubix cubes moving, 88 spin triangles, 91 plasma,
    // 94 colortoroid, 96 spinningpattern, 97 spacetime, 99 hearttunnel, 100 sonic
    const uint8_t demo_mapping[] = {
	//                 5                10                 15
    #ifndef BM
	10, 84, 78, 17, 2, 18, 5, 3, 20, 4,  22, 23, 24, 6,  26, 27, 7,
    #else
	10, 84, 78, 17, 2, 18, 9, 3, 20, 56, 22, 23, 24, 88, 26, 27, 7,
    #endif
	// aurora (removed 38 and 41, and then 31, 37, 39)
	// attract, (bounce), cube, flock, flowfield, incre1, incre2, (37/pendulumwave), (38/radar), (39/spiral), spiro, (swirl), wave
	//          20                  25
     // 30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 42,
	30, 87, 32, 33, 34, 35, 36, 88, 91, 40, 42,
	//      30                  35                  40
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
	// gifs
	//              45                  50                  55                  60
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 
	//              65                  70
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 
	101, 105, 106,
    #ifdef BM
	108, 109, 110, 111, 112,
    #endif
    };

    #ifndef BM
        const uint8_t best_mapping[] = { 2, 5, 3, 23, 27, 35,  56, 77, 78, 84, 87, 88, 91, 94, 96, 97, 99, 100, };
    #else
        const uint8_t best_mapping[] = {  2, 23, 27, 56, 78, 84, 3, 87, 88, 91, 94, 9, 96, 97, 99, 100, 8, 108, 109, 110, 111, 112, 35, 36 };
    #endif
    
    #ifndef ARDUINOONPC
        #define RECV_PIN 34
    
        #define STRIP_NUM_LEDS 48
        CRGB leds[STRIP_NUM_LEDS];
        #define NEOPIXEL_PIN 13
    #else
        #undef HAS_FS
    #endif

#elif mheight == 64
    // Which demos are shown, and in which order
    const uint8_t demo_mapping[] = {
	//                 5                10                 15
	108, 10, 8, 17, 2, 18, 9, 3, 20, 19, 22, 23, 24, 21, 26, 27, 7,
	// aurora (removed 38 and 41)
	//          20                  25
	30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 42,
	// table mark estes. 
	//      30                  35                  40
	45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 
	// gifs
	//              45                  50                  55                  60
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 
	//              65                  70
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 
	108, 109, 111, 112, // BM
    };
    const uint8_t best_mapping[] = { 17, 23, 24, 27, 56, 78, 84, 87, 88, 91, 94, 96, 97, 99, 100, };

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
