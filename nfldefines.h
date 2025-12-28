#ifndef nfldefines_h
#define nfldefines_h

// old strips were 48, new ones are 60
#define LED_LENGTH 60

#ifdef ESP32
    // the demo array is too long for smartmatrix to work reliably on
    // ESP32 with PSRAM, and we don't use that output, so turn it off.
    #pragma message "Framebuffer only mode, Disabling SmartMatrix output"
    #define FRAMEBUFFER
    // Now we really use ESP32 as wifi bridge no matter what
    #define WIFI
    #ifdef BOARD_HAS_PSRAM
        #pragma message "PSRAM and WIFI enabled"
    #else
	#if 0
	    #pragma message "PSRAM disabled, so WIFI disabled too"
	#else
	    // There isn't enough RAM to do Wifi with all the demos I have
	    #pragma message "WIFI without PSRAM, disabled SmartMatrix output"
	#endif
    #endif
#endif

#ifdef ESP8266
#define M32BY8X3
#endif

#define LEDMATRIX
#define DISABLE_MATRIX_TEST
#include "neomatrix_config.h"

// All my backends (ESP8266, ESP32, and ArduinoOnPC) have some kind of FS
// Backend. Technically this code could run on some chip that has no FS
// (like a teensy with sdcard), and all demos outside of GIFS will work,
// but you will have to make read_config_index work somehow, probably by
// hardcoding the file back into the code (where it used to be, but I moved
// it out so that it can be updated by a web server at runtime).
#define HAS_FS
#ifdef M32BY8X3
    #define mheight 32
    uint8_t PANELCONFNUM = 0;
    #pragma message "autoconf found height 32, panelconf 0"
#elif defined(ESP32)
    #if defined(M64BY64) || defined(GFXDISPLAY_M64BY64_multi4)
	#define mheight 64
	uint8_t PANELCONFNUM = 1;
	#pragma message "autoconf found height 64, panelconf 1"
	// Force PANELCONFNUM to 2 for burning man
    #else
	#define mheight 96
	uint8_t PANELCONFNUM = 3;
	#pragma message "autoconf found height 96, panelconf 3"
    #endif
#elif defined(ARDUINOONPC)
    #if defined(M64BY64) || defined(GFXDISPLAY_M64BY64_multi4)
	#define mheight 64
	uint8_t PANELCONFNUM = 1;
	#pragma message "autoconf found height 64, panelconf 1"
	// Force PANELCONFNUM to 2 for burning man
    #elif GFXDISPLAY_M64BY96
	#define mheight 96
	uint8_t PANELCONFNUM = 3;
	#pragma message "autoconf found height 96, panelconf 3"
    #else
	uint8_t PANELCONFNUM = 4;
	#define mheight 192
	#pragma message "autoconf found height 192, panelconf 4"
    #endif
#else
#error "Matrix config undefined, please set height"
#endif


// How many ms used for each matrix update
// This is now used to schedule Matrix_Handler every 20ms
// if possible (50 frames generated per second). 
// Note that actual framerate generation will be slowed
// down by how long things take to run.
#define MX_UPD_TIME 20

// Separate Neopixel Strip, if used. Not the NeoMatrix
uint8_t led_brightness = 64;
// matrix_brightness is defined on neomatrix_config.h


// On ESP32, I have a 64x64 direct matrix (not tiled) with 2 options of drivers.
#if mheight == 96 || mheight == 192
    uint8_t DFL_MATRIX_BRIGHTNESS_LEVEL = 5;
    // Using RGBPanel via SmartMatrix

    // Memory After GIF init:
    // before SmartMatrix::GFX bufferless change
    // Heap/32-bit Memory Available: 114048 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  28300 bytes total,  15456 bytes largest free block

    // after SmartMatrix::GFX zero copy change.
    // Heap/32-bit Memory Available: 132476 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  46728 bytes total,  40976 bytes largest free block
    #ifndef ARDUINOONPC
        #define IR_RECV_PIN 34
    #endif

    // No LED strip on rPi, but good to run the code on linux to exercise compiler warnings (and ASAN)
    #ifndef RPI
	// You can enable this on ArduinoONPC (linux) for debugging LED trip bugs
	#ifndef ARDUINOONPC
	    #define STRIP_NUM_LEDS LED_LENGTH
	    CRGB leds[STRIP_NUM_LEDS];
	    #define NEOPIXEL_PIN 13
	    #ifdef ARDUINO_WAVESHARE_ESP32_S3_ZERO
		// Check NeoMatrix-FastLED-IR.ino setup
	    #endif
	#endif
    #endif

#elif mheight == 64
    uint8_t DFL_MATRIX_BRIGHTNESS_LEVEL = 6;
    // Make the burning man 64x64 brighter by default, we have a big power supply :)
    #ifndef ARDUINOONPC
        #define IR_RECV_PIN 34
    #endif

#elif mheight == 32
    #define STRIP_NUM_LEDS LED_LENGTH
    CRGB leds[STRIP_NUM_LEDS];

    #ifdef ESP8266
	uint8_t DFL_MATRIX_BRIGHTNESS_LEVEL = 5;
	#define NEOPIXEL_PIN D1 // GPIO5
	#define IR_RECV_PIN D4
    #else
	uint8_t DFL_MATRIX_BRIGHTNESS_LEVEL = 7;
        #define IR_RECV_PIN 34
	#define NEOPIXEL_PIN 13
    #endif
#else
    #error "unknown matrix height, no idea what demos to run"
#endif

#ifdef ESP32
// Use https://github.com/lbernstone/IR32.git instead of IRRemote
// 0x4008fd64: invoke_abort at /Users/ficeto/Desktop/ESP32/ESP32/esp-idf-public/components/esp32/panic.c line 155
// 0x4008ff95: abort at /Users/ficeto/Desktop/ESP32/ESP32/esp-idf-public/components/esp32/panic.c line 170
// 0x4008b6f7: xRingbufferReceive at /Users/ficeto/Desktop/ESP32/ESP32/esp-idf-public/components/esp_ringbuf/ringbuf.c line 845
// 0x400f323c: IRRecv::read(char*&, bool) at /home/merlin/Arduino/libraries/IR32/src/IRRecv.cpp line 128
// 0x400e7689: check_startup_IR_serial() at /home/merlin/arduino/emulation/ArduinoOnPc-FastLED-GFX-LEDMatrix/examples/NeoMatrix-FastLED-IR/NeoMatrix-FastLED-IR.ino line 3287
// 0x400e805a: setup() at /home/merlin/arduino/emulation/ArduinoOnPc-FastLED-GFX-LEDMatrix/examples/NeoMatrix-FastLED-IR/NeoMatrix-FastLED-IR.ino line 4715
// 0x400f7e6b: loopTask(void*) at /home/merlin/Arduino/hardware/espressif/esp32/cores/esp32/main.cpp line 14
// 0x4008bb7d: vPortTaskWrapper at /Users/ficeto/Desktop/ESP32/ESP32/esp-idf-public/components/freertos/port.c line 143
//#define ESP32RMTIR

    #ifdef ARDUINO_ESP32_DEV
	// this does not work with new ESP32 core
	#ifndef ARDUINO_RUNNING_CORE
	    #pragma message "will enable IR for ESP32"
	#else
	    #undef IR_RECV_PIN
	#endif
    #else
	// New chip is Waveshare ESP32_S3_Zero
	// Built in 512KB of SRAM and 384KB ROM, onboard 4MB Flash memory and 2MB PSRAM
	#undef IR_RECV_PIN
    #endif
#endif

// Disable fonts in many sizes
// Sketch uses 283784 bytes (27%) of program storage space. Maximum is 1044464 bytes.
// Global variables use 32880 bytes (40%) of dynamic memory, leaving 49040 bytes for local variables. Maximum is 81920 bytes.
// Uploading 287936 bytes from /tmp/arduino_build_498793/NeoMatrix-FastLED-IR.ino.bin to flash at 0x00000000
//#define NOFONTS 1

void matrix_show();
void aurora_setup();
bool sav_newgif(const char *pathname);
void sav_setup();
bool sav_loop();

#endif // nfldefines_h
