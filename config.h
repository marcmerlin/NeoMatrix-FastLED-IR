#ifndef config_h
#define config_h


// Index of the last demo in matrix_change
// demo_mapping below controls which ones are displayed
#define LAST_MATRIX 59

// On ESP32, I have a 64x64 direct matrix (not tiled) with 2 options of drivers.
#ifdef ESP32
    // 64x64 matrix with optional 16 pin parallel driver
    // 55fps without 16PINS, 110fps with 16PINS
    //#define ESP32_16PINS
    // Which demos are shown, and in which order
    // 13, 14 buggy
    uint8_t demo_mapping[] = { LAST_MATRIX, 8, 9, 1, 10, 11, 12, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 0, };
#else // ESP32
    // Which demos are shown, and in which order
    uint8_t demo_mapping[] = { LAST_MATRIX, 2, 13, 8, 9, 1, 3, 10, 11, 12, 14, 15, 16, 17, 18, 19, 20, 4, 21, 22, 23, 24, 25, 5, 26, 27, 28, 29, 30, 6, 31, 32, 33, 34, 35, 7, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 0, };
    // On ESP8266, I drive 3x 32x8 panels plus neopixel strips
    #define M32B8X3
    #define STRIP_NUM_LEDS 48
#endif
#define demo_cnt (sizeof(demo_mapping)/sizeof(demo_mapping[0]))

#ifdef ESP32
    #ifdef ESP32_16PINS
    // This uses https://github.com/hpwit/fastled-esp32-16PINS.git
    // instead of https://github.com/samguyer/FastLED.git
    #define FASTLED_ALLOW_INTERRUPTS 0
    #define FASTLED_SHOW_CORE 0
    #else
    // Allow infrared
    #define FASTLED_ALLOW_INTERRUPTS 1
    #endif // ESP32_16PINS
#endif

#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>

#include <LEDMatrix.h>
#include <LEDSprites.h>

#ifdef STRIP_NUM_LEDS
CRGB leds[STRIP_NUM_LEDS];
#define NEOPIXEL_PIN D1 // GPIO5
#endif

// Disable fonts in many sizes
// Sketch uses 283784 bytes (27%) of program storage space. Maximum is 1044464 bytes.
// Global variables use 32880 bytes (40%) of dynamic memory, leaving 49040 bytes for local variables. Maximum is 81920 bytes.
// Uploading 287936 bytes from /tmp/arduino_build_498793/NeoMatrix-FastLED-IR.ino.bin to flash at 0x00000000
//#define NOFONTS 1
//

//---------------------------------------------------------------------------- 

#if defined(ESP32) && ! defined(ESP32_16PINS)
#pragma message "Please use https://github.com/samguyer/FastLED.git as stock FastLED is unstable with ESP32"
#endif

#ifdef ESP32_16PINS
FASTLED_USING_NAMESPACE
// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

void FastLEDshowESP32()
{
    if (userTaskHandle == 0) {
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
	// -- Store the handle of the current task, so that the show task can
	//    notify it when it's done
	userTaskHandle = xTaskGetCurrentTaskHandle();

	// -- Trigger the show task
	xTaskNotifyGive(FastLEDshowTaskHandle);

	// -- Wait to be notified that it's done
	ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
	userTaskHandle = 0;
    }
}

void FastLEDshowTask(void *pvParameters)
{
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 500 );
    // -- Run forever...
    for(;;) {
	// -- Wait for the trigger
	ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

	// -- Do the show (synchronously)
	FastLED.show();

	// -- Notify the calling task
	xTaskNotifyGive(userTaskHandle);
    }
}
#endif // ESP32_16PINS


#ifdef M32B8X3
//---------------------------------------------------------------------------- 
//
// Used by LEDMatrix
#define MATRIX_TILE_WIDTH   8 // width of EACH NEOPIXEL MATRIX (not total display)
#define MATRIX_TILE_HEIGHT  32 // height of each matrix
#define MATRIX_TILE_H       3  // number of matrices arranged horizontally
#define MATRIX_TILE_V       1  // number of matrices arranged vertically

// Used by NeoMatrix
#define mw (MATRIX_TILE_WIDTH *  MATRIX_TILE_H)
#define mh (MATRIX_TILE_HEIGHT * MATRIX_TILE_V)
#define NUMMATRIX (mw*mh)

// Compat for some other demos
#define NUM_LEDS NUMMATRIX 
#define MATRIX_HEIGHT mh
#define MATRIX_WIDTH mw
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

cLEDMatrix<-MATRIX_TILE_WIDTH, -MATRIX_TILE_HEIGHT, HORIZONTAL_ZIGZAG_MATRIX, MATRIX_TILE_H, MATRIX_TILE_V, HORIZONTAL_BLOCKS> ledmatrix;

//
//CRGB matrixleds[NUMMATRIX];
// cLEDMatrix creates a FastLED array and we need to retrieve a pointer to its first element
// to act as a regular FastLED array.
CRGB *matrixleds = ledmatrix[0];

FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(matrixleds, MATRIX_TILE_WIDTH, MATRIX_TILE_HEIGHT, MATRIX_TILE_H, MATRIX_TILE_V, 
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
    NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG + 
    NEO_TILE_TOP + NEO_TILE_LEFT +  NEO_TILE_PROGRESSIVE);

uint8_t led_brightness = 64;
uint8_t matrix_brightness = 16;

#else // M32B8X3
//---------------------------------------------------------------------------- 
//
// Used by LEDMatrix
#define MATRIX_TILE_WIDTH   64 // width of EACH NEOPIXEL MATRIX (not total display)
#define MATRIX_TILE_HEIGHT  64 // height of each matrix
#define MATRIX_TILE_H       1  // number of matrices arranged horizontally
#define MATRIX_TILE_V       1  // number of matrices arranged vertically
#define NUM_STRIPS 16
#define NUM_LEDS_PER_STRIP 256

// Used by NeoMatrix
#define mw (MATRIX_TILE_WIDTH *  MATRIX_TILE_H)
#define mh (MATRIX_TILE_HEIGHT * MATRIX_TILE_V)
#define NUMMATRIX (mw*mh)

// Compat for some other demos
#define NUM_LEDS NUMMATRIX 
#define MATRIX_HEIGHT mh
#define MATRIX_WIDTH mw
#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

cLEDMatrix<MATRIX_TILE_WIDTH, MATRIX_TILE_HEIGHT, VERTICAL_ZIGZAG_MATRIX> ledmatrix;
// cLEDMatrix creates a FastLED array and we need to retrieve a pointer to its first element
// to act as a regular FastLED array.
CRGB *matrixleds = ledmatrix[0];

FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(matrixleds, MATRIX_TILE_WIDTH, MATRIX_TILE_HEIGHT,  
    NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT +
    NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG );

uint8_t led_brightness = 64;
uint8_t matrix_brightness = 64;

#endif // M32B8X3

int XY2( int x, int y, bool wrap=false) { 
	return matrix->XY(x,MATRIX_HEIGHT-1-y);
}


uint16_t XY( uint8_t x, uint8_t y) {
    return matrix->XY(x,y);
}

#ifdef ESP8266
// D4 is also the system LED, causing it to blink on IR receive, which is great.
#define RECV_PIN D4     // GPIO2

// Turn off Wifi in setup()
// https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
//
#include "ESP8266WiFi.h"
extern "C" {
#include "user_interface.h"
}
#else
#define RECV_PIN 34
#endif // ESP8266

// min/max are broken by the ESP8266 include
#if defined(ESP8266) || defined(ESP32)
//#define min(a,b) (a<b)?(a):(b)
//#define max(a,b) (a>b)?(a):(b)
#endif // ESP8266


bool matrix_reset_demo = 1;

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
uint16_t speed = 255;

int wrapX(int x) { 
	if (x < 0 ) return 0;
	if (x >= MATRIX_WIDTH) return (MATRIX_WIDTH-1);
	return x;
}

void aurora_setup();

void matrix_clear() {
    //FastLED[1].clearLedData();
    // clear does not work properly with multiple matrices connected via parallel inputs
    memset(matrixleds, 0, NUMMATRIX*3);
}

void matrix_show() {
    //matrix->show();

#ifdef ESP32_16PINS
    FastLEDshowESP32();
#else // ESP32_16PINS
    #ifdef ESP8266
    // Disable watchdog interrupt so that it does not trigger in the middle of
    // updates. and break timing of pixels, causing random corruption on interval
    // https://github.com/esp8266/Arduino/issues/34
    // Note that with https://github.com/FastLED/FastLED/pull/596 interrupts, even
    // in parallel mode, should not affect output. That said, reducing their amount
    // is still good.
    // Well, that sure didn't work, it actually made things worse in a demo during
    // fade, so I'm turning it off again.
        //ESP.wdtDisable();
    #endif
    #ifdef NEOPIXEL_PIN
        FastLED[1].showLeds(matrix_brightness);
    #else
        FastLED.show();
    #endif
    #ifdef ESP8266
        //ESP.wdtEnable(1000);
    #endif
#endif // ESP32_16PINS
}


void matrix_setup() {
#ifdef M32B8X3
    // Init Matrix
    // Serialized, 768 pixels takes 26 seconds for 1000 updates or 26ms per refresh
    // FastLED.addLeds<NEOPIXEL,MATRIXPIN>(matrixleds, NUMMATRIX).setCorrection(TypicalLEDStrip);
    // https://github.com/FastLED/FastLED/wiki/Parallel-Output
    // WS2811_PORTA - pins 12, 13, 14 and 15 or pins 6,7,5 and 8 on the NodeMCU
    // This is much faster 1000 updates in 10sec
    //FastLED.addLeds<NEOPIXEL,PIN>(matrixleds, NUMMATRIX); 
    FastLED.addLeds<WS2811_PORTA,3>(matrixleds, NUMMATRIX/3).setCorrection(TypicalLEDStrip);
    Serial.print("Neomatrix parallel output, total LEDs: ");
    Serial.println(NUMMATRIX);
#else
    #ifdef ESP32_16PINS
	xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);
	FastLED.addLeds<WS2811_PORTA,NUM_STRIPS,((1<<2) + (1<<4) + (1<<5) + (1<<12)+ (1<<13) + (1<<14) + (1<<15) + (1<<16) + 
						(1<<18) + (1<<19) + (1<<21) + (1<<22) + (1<<23) + (1<<25) + (1<<26) + (1<<27) 
						)>(matrixleds, NUM_LEDS_PER_STRIP);
        Serial.print("Neomatrix 16 way bitbang output, total LEDs: ");
        Serial.println(NUMMATRIX);
    #else // ESP32_16PINS
        // https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples
        FastLED.addLeds<WS2812B, 2, GRB>(matrixleds, 0*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        #ifdef ESP32
        FastLED.addLeds<WS2812B, 4, GRB>(matrixleds, 1*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B, 5, GRB>(matrixleds, 2*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,12, GRB>(matrixleds, 3*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,13, GRB>(matrixleds, 4*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,14, GRB>(matrixleds, 5*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,15, GRB>(matrixleds, 6*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,16, GRB>(matrixleds, 7*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,18, GRB>(matrixleds, 8*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,19, GRB>(matrixleds, 9*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,21, GRB>(matrixleds,10*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,22, GRB>(matrixleds,11*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,23, GRB>(matrixleds,12*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,25, GRB>(matrixleds,13*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,26, GRB>(matrixleds,14*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP); 
        FastLED.addLeds<WS2812B,27, GRB>(matrixleds,15*NUM_LEDS_PER_STRIP, NUM_LEDS_PER_STRIP);
        Serial.print("Neomatrix 16 pin via RMT 8 way parallel output, total LEDs: ");
        Serial.println(NUMMATRIX);
        #endif // ESP32
    #endif // ESP32_16PINS
#endif

    FastLED.setBrightness(matrix_brightness);
}
#endif //config_h
