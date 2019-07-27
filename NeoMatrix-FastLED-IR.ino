// By Marc MERLIN <marc_soft@merlins.org>
// License: Apache v2.0
//
// Portions of demo code from Adafruit_NeoPixel/examples/strandtest apparently
// licensed under LGPLv3 as per the top level license file in there.

// Neopixel + IR is hard because the neopixel libs disable interrups while IR is trying to
// receive, and bad things happen :(
// https://github.com/z3t0/Arduino-IRremote/issues/314
//
// leds.show takes 1 to 3ms during which IR cannot run and codes get dropped, but it's more
// a problem if you constantly update for pixel animations and in that case the IR ISR gets
// almost no chance to run.
// The FastLED library is super since it re-enables interrupts mid update
// on chips that are capable of it. This allows IR + Neopixel to work on Teensy v3.1 and most
// other 32bit CPUs.

// Compile WeMos D1 R2 & mini or ESP32-dev

#include "nfldefines.h"
#include "Table_Mark_Estes.h"
#include "PacMan.h"
#define FIREWORKS_INCLUDE
#include "FireWorks2.h"
#define SUBLIME_INCLUDE
#include "Sublime_Demos.h"
#define TWINKLEFOX_INCLUDE
#include "TwinkleFOX.h"
#include "aurora.h"

const uint16_t PROGMEM RGB_bmp[64] = {
      // 10: multicolor smiley face
        0x000, 0x000, 0x00F, 0x00F, 0x00F, 0x00F, 0x000, 0x000, 
	0x000, 0x00F, 0x000, 0x000, 0x000, 0x000, 0x00F, 0x000, 
	0x00F, 0x000, 0xF00, 0x000, 0x000, 0xF00, 0x000, 0x00F, 
	0x00F, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x00F, 
	0x00F, 0x000, 0x0F0, 0x000, 0x000, 0x0F0, 0x000, 0x00F, 
	0x00F, 0x000, 0x000, 0x0F4, 0x0F3, 0x000, 0x000, 0x00F, 
	0x000, 0x00F, 0x000, 0x000, 0x000, 0x000, 0x00F, 0x000, 
	0x000, 0x000, 0x00F, 0x00F, 0x00F, 0x00F, 0x000, 0x000, };

// Convert a BGR 4/4/4 bitmap to RGB 5/6/5 used by Adafruit_GFX
void fixdrawRGBBitmap(int16_t x, int16_t y, const uint16_t *bitmap, int16_t w, int16_t h) {
    uint16_t RGB_bmp_fixed[w * h];
    for (uint16_t pixel=0; pixel<w*h; pixel++) {
	uint8_t r,g,b;
	uint16_t color = pgm_read_word(bitmap + pixel);

	b = (color & 0xF00) >> 8;
	g = (color & 0x0F0) >> 4;
	r = color & 0x00F;
	// expand from 4/4/4 bits per color to 5/6/5
	b = map(b, 0, 15, 0, 31);
	g = map(g, 0, 15, 0, 63);
	r = map(r, 0, 15, 0, 31);
	RGB_bmp_fixed[pixel] = (r << 11) + (g << 5) + b;
    }
    matrix->drawRGBBitmap(x, y, RGB_bmp_fixed, w, h);
}


void matrix_show() {
#ifdef SMARTMATRIX
    matrix->show();
#elif defined(ESP32_16PINS)
    //FastLEDshowESP32(); // FIXME
    matrix->show();
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
	matrix->show();
    #endif
    #ifdef ESP8266
        //ESP.wdtEnable(1000);
    #endif
#endif // ESP32_16PINS
}

// Other fonts possible on http://oleddisplay.squix.ch/#/home
// https://blog.squix.org/2016/10/font-creator-now-creates-adafruit-gfx-fonts.html
// https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
// Default is 5x7, meaning 4.8 chars wide, 4 chars high
// Picopixel: 3x5 means 6 chars wide, 5 or 6 chars high
// #include <Fonts/Picopixel.h>
// Proportional 5x5 font, 4.8 wide, 6 high
// #include <Fonts/Org_01.h>
// TomThumb: 3x5 means 6 chars wide, 5 or 6 chars high
// -> nicer font
#include <Fonts/TomThumb.h>
// 3x3 but not readable
//#include <Fonts/Tiny3x3a2pt7b.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/FreeMonoBold18pt7b.h>
//#include <Fonts/FreeMonoBold24pt7b.h>
#ifndef NOFONTS
#include "fonts.h"
#endif

// Choose your prefered pixmap
#include "smileytongue24.h"

// How many ms used for each matrix update
#define MX_UPD_TIME 10


uint8_t matrix_state = 0;
uint8_t matrix_demo = demo_mapping[matrix_state];
// controls how many times a demo should run its pattern
// init at -1 to indicate that a demo is run for the first time (demo switch)
int16_t matrix_loop = -1;


//----------------------------------------------------------------------------

#ifdef ESP8266
#include <IRremoteESP8266.h>
#else
#include <IRremote.h>
#endif

// This file contains codes I captured and mapped myself
// using IRremote's examples/IRrecvDemo
#include "IRcodes.h"

IRrecv irrecv(RECV_PIN);

typedef enum {
    f_nothing = 0,
    f_colorWipe = 1,
    f_rainbow = 2,
    f_rainbowCycle = 3,
    f_theaterChase = 4,
    f_theaterChaseRainbow = 5,
    f_cylon = 6,
    f_cylonTrail = 7,
    f_doubleConverge = 8,
    f_doubleConvergeRev = 9,
    f_doubleConvergeTrail = 10,
    f_flash = 11,
    //f_flash3 = 12,
    f_juggle = 12,
    f_bpm = 13,
} StripDemo;

StripDemo nextdemo = f_theaterChaseRainbow;
// Is the current demo linked to a color (false for rainbow demos)
bool colorDemo = true;
int32_t demo_color = 0x00FF00; // Green
static int16_t strip_speed = 50;


uint32_t last_change = millis();

// ---------------------------------------------------------------------------
// Shared functions
// ---------------------------------------------------------------------------

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    uint32_t wheel=0;

    // Serial.print(WheelPos);
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
	wheel = (((uint32_t)(255 - WheelPos * 3)) << 16) + (WheelPos * 3);
    }
    if (!wheel && WheelPos < 170) {
	WheelPos -= 85;
	wheel = (((uint32_t)(WheelPos * 3)) << 8) + (255 - WheelPos * 3);
    }
    if (!wheel) {
	WheelPos -= 170;
	wheel = (((uint32_t)(WheelPos * 3)) << 16) + (((uint32_t)(255 - WheelPos * 3)) << 8);
    }
    // Serial.print(" -> ");
    // Serial.println(wheel, HEX);
    return (wheel);
}

// ---------------------------------------------------------------------------
// Matrix Code
// ---------------------------------------------------------------------------


void display_stats() {
    static uint16_t cnt=1;

    matrix->setTextSize(1);
    // not wide enough;
    if (mw<16) return;
    matrix->clear();
    // Font is 5x7, if display is too small
    // 8 can only display 1 char
    // 16 can almost display 3 chars
    // 24 can display 4 chars
    // 32 can display 5 chars
    matrix->setCursor(0, 0);
    matrix->setTextColor(matrix->Color(255,0,0));
    if (mw>10) matrix->print(mw/10);
    matrix->setTextColor(matrix->Color(255,128,0));
    matrix->print(mw % 10);
    matrix->setTextColor(matrix->Color(0,255,0));
    matrix->print('x');
    // not wide enough to print 5 chars, go to next line
    if (mw<25) {
	if (mh==13) matrix->setCursor(6, 7);
	else if (mh>=13) {
	    matrix->setCursor(mw-12, 8);
	} else {
	    // we're not tall enough either, so we wait and display
	    // the 2nd value on top.
	    matrix_show();
	    matrix->clear();
	    matrix->setCursor(mw-11, 0);
	}
    }
    matrix->setTextColor(matrix->Color(0,255,128));
    matrix->print(mh/10);
    matrix->setTextColor(matrix->Color(0,128,255));
    matrix->print(mh % 10);
    // enough room for a 2nd line
    if ((mw>25 && mh >14) || mh>16) {
	matrix->setCursor(0, mh-7);
	matrix->setTextColor(matrix->Color(0,255,255));
	if (mw>16) matrix->print('*');
	matrix->setTextColor(matrix->Color(255,0,0));
	matrix->print('R');
	matrix->setTextColor(matrix->Color(0,255,0));
	matrix->print('G');
	matrix->setTextColor(matrix->Color(0,0,255));
	matrix->print("B");
	matrix->setTextColor(matrix->Color(255,255,0));
	// this one could be displayed off screen, but we don't care :)
	matrix->print("*");
    }

    if (mh >=31) {
	matrix->setTextColor(matrix->Color(128,255,128));
	matrix->setCursor(0, mh-21);
	matrix->print(demo_cnt);
	matrix->print(" demos");
    }
    if (mh >= 35) {
	matrix->setTextColor(matrix->Color(255,128,128));
	matrix->setCursor(0, mh-28);
	matrix->print(best_cnt);
	matrix->print(" best");
    }

    matrix->setTextColor(matrix->Color(255,0,255));
    matrix->setCursor(0, mh-14);
    matrix->print(cnt++);
    if (cnt == 10000) cnt=1;
    matrix_show();
}


void font_test() {
    static uint16_t cnt=1;
    matrix->setRotation(0);

#if 0
    matrix->setFont(&FreeMonoBold9pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    delay(1000);
    matrix->clear();

    matrix->setFont(&FreeMonoBold12pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    delay(1000);
    matrix->clear();

    matrix->setFont(&FreeMonoBold18pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    delay(1000);
    matrix->clear();

    matrix->setFont(&FreeMonoBold24pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    matrix->clear();
    delay(1000);
#endif

    matrix->clear();
    //matrix->setFont(&Picopixel);
    //matrix->setFont(&Org_01);
    matrix->setFont(&TomThumb);
    //matrix->setFont(&Tiny3x3a2pt7b);
    matrix->setTextSize(1);

    matrix->setTextColor(matrix->Color(255,0,255));
    matrix->setCursor(0, 6);
    matrix->print(cnt++);

    matrix->setCursor(0, 12);
    matrix->setTextColor(matrix->Color(255,0,0));
    matrix->print("Eat");

    matrix->setCursor(0, 18);
    matrix->setTextColor(matrix->Color(255,128,0));
    matrix->print("Sleep");

    matrix->setCursor(0, 24);
    matrix->setTextColor(matrix->Color(0,255,0));
    matrix->print("Rave");

    matrix->setCursor(0, 30);
    matrix->setTextColor(matrix->Color(0,255,128));
    matrix->print("REPEAT");

    matrix->setRotation(1);
    matrix->setCursor(1, 5);
    matrix->setTextColor(matrix->Color(255,255,0));
    matrix->print("Repeat");
    matrix_show();
}


uint8_t tfsf() {
    static uint16_t state;
    static float spd;
    static int8_t startfade;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint8_t resetspd = 5;
    uint8_t l = 0;
    uint8_t repeat = 2;
    uint8_t fontsize = 1;
    uint8_t idx = 3;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 1;
	spd = 1.0;
	startfade = -1;
    }

    matrix->setRotation(0);
    if (mw >= 48 && mh >=64) fontsize = 3;
    else if (mw >= 48 && mh >=48) fontsize = 2;
    matrix->setTextSize(fontsize);
#ifndef NOFONTS
    // biggest font is 18, but this spills over
    matrix->setFont( &Century_Schoolbook_L_Bold[16] );

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(0, mh - idx*8*fontsize/3);
	matrix->clear();
	//matrix->setTextColor(matrix->Color(255,0,0));
	matrix->setPassThruColor(CRGB(255,0,0));
	matrix->print("T");
	matrix->setPassThruColor();
	startfade = l;
    }
    l++; idx--;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(0, mh - idx*8*fontsize/3);
	matrix->clear();
	//matrix->setTextColor(matrix->Color(192,192,0));
	matrix->setPassThruColor(CRGB(192,192,0));
	matrix->print("F");
	matrix->setPassThruColor();
	startfade = l;
    }
    l++; idx--;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(2, mh - idx*8*fontsize/3);
	matrix->clear();
	//matrix->setTextColor(matrix->Color(0,192,192));
	matrix->setPassThruColor(CRGB(0,192,192));
	matrix->print("S");
	matrix->setPassThruColor();
	startfade = l;
    }
    l++; idx--;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(2, mh - idx*8*fontsize/3);
	matrix->clear();
	//matrix->setTextColor(matrix->Color(0,255,0));
	matrix->setPassThruColor(CRGB(0,255,0));
	matrix->print("F");
	matrix->setPassThruColor();
	startfade = l;
    }
    l++; idx--;
#endif

#if 0
    if (startfade < l && (state > (l*duration)/spd))  {
	matrix->setCursor(1, 29);
	matrix->clear();
	//matrix->setTextColor(matrix->Color(0,255,0));
	matrix->setPassThruColor(CRGB(0,0,255));
	matrix->print("8");
	startfade = l;
    }
#endif

    if (startfade > -1)  {
	for (uint16_t i = 0; i < NUMMATRIX; i++) matrixleds[i].nscale8(248-spd*2);
    }
    l++;

    if (state++ > ((l+0.5)*duration)/spd) {
	state = 1;
	startfade = -1;
	spd += spdincr;
	if (spd > resetspd) {
	    matrix_reset_demo = 1;
	    return 0;
	}
    }

    matrix_show();
    return repeat;
}

// type 0 = up, type 1 = up and down
uint8_t tfsf_zoom(uint8_t zoom_type, uint8_t speed) {
    static uint16_t direction;
    static uint16_t size;
    static uint8_t l;
    static int16_t faster = 0;
    static bool dont_exit;
    static uint16_t delayframe;
    const char letters[] = { 'T', 'F', 'S', 'F' };
    bool done = 0;
    uint8_t repeat = 4;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	direction = 1;
	size = 3;
	l = 0;
	if (matrix_loop == -1) { dont_exit = 1; delayframe = 2; faster = 0; };
    }

    if (mw >= 48 && mh >=64) matrix->setTextSize(3); else matrix->setTextSize(1);

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	// Serial.println("delayed frame");
	delay(MX_UPD_TIME);
	return repeat;
    }
    delayframe = max((speed / 10) - faster , 1);
    // before exiting, we run the full delay to show the last frame long enough
    if (dont_exit == 0) { dont_exit = 1; return 0; }
    if (direction == 1) {
	int8_t offset = 0; // adjust some letters left or right as needed
	if (letters[l] == 'T') offset = -2 * size/15;
	if (letters[l] == '8') offset = 2 * size/15;

	matrix->clear();
	//uint16_t txtcolor = matrix->Color24to16(Wheel(map(letters[l], '0', 'Z', 0, 255)));
	// matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(letters[l], '0', 'Z', 0, 255)));

#ifndef NOFONTS
	matrix->setFont( &Century_Schoolbook_L_Bold[size] );
#endif

#ifdef M32B8X3
	matrix->setCursor(10-size*0.55+offset, 17+size*0.75);
#else
	matrix->setCursor(3*mw/6-size*1.75+offset, mh*7/12+size*1.60);
#endif
	matrix->print(letters[l]);
	matrix->setPassThruColor();
	if (size<18) size++;
	else if (zoom_type == 0) { done = 1; delayframe = max((speed - faster*10) * 1, 3); }
	     else direction = 2;

    } else if (zoom_type == 1) {
	int8_t offset = 0; // adjust some letters left or right as needed

	matrix->clear();
	//uint16_t txtcolor = matrix->Color24to16(Wheel(map(letters[l], '0', 'Z', 255, 0)));
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(letters[l], '0', 'Z', 255, 0)));
	if (letters[l] == 'T') offset = -2 * size/15;
	if (letters[l] == '8') offset = 2 * size/15;

#ifndef NOFONTS
	matrix->setFont( &Century_Schoolbook_L_Bold[size] );
#endif
#ifdef M32B8X3
	matrix->setCursor(10-size*0.55+offset, 17+size*0.75);
#else
	matrix->setCursor(3*mw/6-size*1.75+offset, mh*7/12+size*1.60);
#endif
	matrix->print(letters[l]);
	matrix->setPassThruColor();
	if (size>3) size--; else { done = 1; direction = 1; delayframe = max((speed-faster*10)/2, 3); };
    }

    matrix_show();
    //Serial.println("done?");
    if (! done) return repeat;
    direction = 1;
    size = 3;
    //Serial.println("more letters?");
    if (++l < sizeof(letters)) return repeat;
    l = 0;
    //Serial.println("reverse pattern?");
    if (zoom_type == 1 && direction == 2) return repeat;

    //Serial.println("Done with font animation");
    faster++;
    matrix_reset_demo = 1;
    dont_exit =  0;
    // Serial.print("delayframe on last letter ");
    // Serial.println(delayframe);
    // After showing the last letter, pause longer
    // unless it's a zoom in zoom out.
    if (zoom_type == 0) delayframe *= 5; else delayframe *= 3;
    return repeat;
}

uint8_t esrr_bbb() { // or burn baby burn
    static uint16_t state;
    static float spd;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 18;
    uint8_t resetspd = 24;
    uint8_t l = 0;


    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 1;
	spd = 1.0;
    }

    matrix->setRotation(0);
    matrix->setTextSize(1);
    if (mheight >= 64)  {
	//matrix->setFont(FreeMonoBold9pt7b);
	matrix->setFont(&Century_Schoolbook_L_Bold_12);
    } else {
	matrix->setFont(&TomThumb);
    }
    matrix->clear();

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(18, 20);
	else if (mheight >= 64) matrix->setCursor(18, 15);
	else matrix->setCursor(7, 6);

	//matrix->setTextColor(matrix->Color(255,0,0));
	matrix->setPassThruColor(CRGB(255,0,0));
	matrix->print("EAT");
	matrix->setPassThruColor();
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(10, 41);
	else if (mheight >= 64) matrix->setCursor(10, 33);
	else matrix->setCursor(3, 14);

	//matrix->setTextColor(matrix->Color(192,192,0));
	matrix->setPassThruColor(CRGB(192,192,0));
	matrix->print("SLEEP");
	matrix->setPassThruColor();
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(14, 63);
	else if (mheight >= 64) matrix->setCursor(14, 47);
	else matrix->setCursor(5, 22);

	//matrix->setTextColor(matrix->Color(0,255,0));
	matrix->setPassThruColor(CRGB(0,255,0));
	if (mheight == 64) matrix->print("BURN");
	else matrix->print("RAVE");
	matrix->setPassThruColor();
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || state < overlap/spd) || spd > displayall)  {
	if (mheight >= 64) matrix->setCursor(2, 82);
	else if (mheight >= 64) matrix->setCursor(2, 63);
	else matrix->setCursor(0, 30);

	//matrix->setTextColor(matrix->Color(0,192,192));
	matrix->setPassThruColor(CRGB(0,192,192));
	matrix->print("REPEAT");
	matrix->setPassThruColor();
    }
    l++;

    // 400 - 4x50 = 200
    if (state++ > (l*duration-l*overlap)/spd) {
	state = 1;
	spd += spdincr;
	if (spd > resetspd) {
	    matrix_reset_demo = 1;
	    return 0;
	}
    }

    matrix_show();
    return 1;
}

uint8_t bbb() {
    static uint16_t state;
    static float spd;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 18;
    uint8_t resetspd = 24;
    uint8_t l = 0;


    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 1;
	spd = 1.0;
    }

    matrix->setRotation(0);
    matrix->setTextSize(1);
    if (mw >= 64) {
	//matrix->setFont(FreeMonoBold9pt7b);
	matrix->setFont(&Century_Schoolbook_L_Bold_16);
    } else {
	matrix->setFont(&TomThumb);
    }
    matrix->clear();

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mw >= 64) {
	    matrix->setCursor(4, 20);
	} else {
	    matrix->setCursor(5, 10);
	}
	matrix->setTextColor(matrix->Color(255,0,0));
	matrix->print("BURN");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mw >= 64) {
	    matrix->setCursor(6, 41);
	} else {
	    matrix->setCursor(5, 20);
	}
	matrix->setTextColor(matrix->Color(0,255,0));
	matrix->print("BABY");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || state < overlap/spd) || spd > displayall)  {
	if (mw >= 64) {
	    matrix->setCursor(4, 62);
	} else {
	    matrix->setCursor(5, 30);
	}
	matrix->setTextColor(matrix->Color(0,0,255));
	matrix->print("BURN");
    }
    l++;

    // 400 - 4x50 = 200
    if (state++ > (l*duration-l*overlap)/spd) {
	state = 1;
	spd += spdincr;
	if (spd > resetspd) {
	    matrix_reset_demo = 1;
	    return 0;
	}
    }

    matrix_show();
    return 1;
}

// This is too jarring on the eyes at night
#if 0
uint8_t esrr_flashin() {
    #define esrflashperiod 30
    static uint16_t state = 0;
    static uint16_t period = esrflashperiod;
    static uint16_t exit = 0;
    static bool oldshow = 0;
    bool show = 0;

    matrix->setFont(&TomThumb);
    matrix->setRotation(0);
    matrix->setTextSize(1);
    matrix->clear();

    state++;
    if (!(state % period) || exit) {
	state = 1;
	show = 1;
	matrix->setCursor(7, 6);
	matrix->setTextColor(matrix->Color(255,0,255));
	matrix->print("EAT");
	matrix->setCursor(3, 14);
	matrix->setTextColor(matrix->Color(255,255,0));
	matrix->print("SLEEP");
	matrix->setCursor(5, 22);
	matrix->setTextColor(matrix->Color(0,255,255));
	matrix->print("RAVE");
	matrix->setCursor(0, 30);
	matrix->setTextColor(matrix->Color(64,255,64));
	matrix->print("REPEAT");
    }

    if (exit)
    {
	exit++;
    }
    else if (show != oldshow) {
	period = max(period - 1, 1);
	oldshow = show;
	if (period == 1) exit=1;
	//Serial.println(period);
    }
    //Serial.println(exit);
    matrix_show();

    // 100 = 1s
    if (exit == 300) {
	state = 0;
	period = esrflashperiod;
	exit = 0;
	oldshow = 0;
	return 1;
    }
    return 0;
}
#endif

uint8_t esrr_fade() {
    static uint16_t state;
    static uint8_t wheel;
    static float spd;
    float spdincr = 0.5;
    uint8_t resetspd = 5;
    //uint16_t txtcolor;


    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 0;
	wheel = 0;
	spd = 1.0;
    }

    state++;

    if (state == 1) {
	//wheel+=20;
	if (mheight >= 64) {
		//matrix->setFont(FreeMonoBold9pt7b);
	    matrix->setFont(&Century_Schoolbook_L_Bold_12);
	} else {
	    matrix->setFont(&TomThumb);
	}
	matrix->setRotation(0);
	matrix->setTextSize(1);
	matrix->clear();

	if (mheight >= 96) matrix->setCursor(18, 20);
	else if (mheight >= 64) matrix->setCursor(18, 15);
	else matrix->setCursor(7, 6);
	//txtcolor = matrix->Color24to16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(((wheel+=24))));
	matrix->print("EAT");

	if (mheight >= 96) matrix->setCursor(10, 41);
	else if (mheight >= 64) matrix->setCursor(10, 33);
	else matrix->setCursor(3, 14);
	//txtcolor = matrix->Color24to16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(((wheel+=24))));
	matrix->print("SLEEP");

	if (mheight >= 96) matrix->setCursor(14, 63);
	else if (mheight >= 64) matrix->setCursor(14, 47);
	else matrix->setCursor(5, 22);
	//txtcolor = matrix->Color24to16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(((wheel+=24))));
	if (mheight == 64) {
	    matrix->print("BURN");
	} else {
	    matrix->print("RAVE");
	}

	if (mheight >= 64) matrix->setCursor(2, 82);
	else if (mheight >= 64) matrix->setCursor(2, 63);
	else matrix->setCursor(0, 30);
	//txtcolor = matrix->Color24to16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(((wheel+=24))));
	matrix->print("REPEAT");
	matrix->setPassThruColor();
    }


    if (state > 40/spd && state < 100/spd)  {
	//fadeToBlackBy( matrixleds, NUMMATRIX, 20*spd);
	// For reasons I don't understand, fadetoblack causes display corruption in this case
	// while looping nscale (ideally mostly the same thing), works.
	for (uint16_t i = 0; i < NUMMATRIX; i++) matrixleds[i].nscale8(242-spd*4);
    }

    if (state > 100/spd) {
	state = 0;
	spd += spdincr;
	//Serial.println(spd);
	//Serial.println(state);
	if (spd > resetspd) {
	    matrix_reset_demo = 1;
	    return 0;
	}
    }
    matrix_show();
    return 2;
}


uint8_t display_text(const char *text, uint16_t x, uint16_t y) {
    static uint16_t state;
    uint16_t loopcnt = 300;


    if (matrix_reset_demo == 1) {
	state = 0;
	matrix_reset_demo = 0;
	matrix->setRotation(0);
	matrix->setTextSize(1);
	if (mheight >= 64) {
		//matrix->setFont(FreeMonoBold9pt7b);
	    matrix->setFont(&Century_Schoolbook_L_Bold_16);
	} else {
	    matrix->setFont(&TomThumb);
	}
	matrix->clear();
    }

    state++;

    matrix->setCursor(x, y);
    matrix->setPassThruColor(Wheel(map(state, 0, loopcnt, 0, 512)));
    matrix->print(text);
    matrix->setPassThruColor();
    matrix_show();

    if (state > loopcnt) {
	matrix_reset_demo = 1;
	return 0;
    }
    return 2;
}


uint8_t squares(bool reverse) {
#define sqdelay 2
    static uint16_t state;
    static uint8_t wheel;
    uint8_t repeat = 1;
    static uint16_t delayframe = sqdelay;


    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 0;
	wheel = 0;
    }

    uint8_t maxsize = max(mh/2,mw/2);

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	delay(MX_UPD_TIME);
	return repeat;
    }
    delayframe = sqdelay;
    state++;
    wheel += 10;

    matrix->clear();
    if (reverse) {
	for (uint8_t s = maxsize; s >= 1 ; s--) {
	    matrix->drawRect( mw/2-s, mh/2-s, s*2, s*2, matrix->Color24to16(Wheel(wheel+(maxsize-s)*10)));
	}
    } else {
	for (uint8_t s = 1; s <= maxsize; s++) {
	    matrix->drawRect( mw/2-s, mh/2-s, s*2, s*2, matrix->Color24to16(Wheel(wheel+s*10)));
	}
    }

    // Serial.print("state ");
    // Serial.println(state);
    if (state > 250) {
	matrix_reset_demo = 1;
	return 0;
    }
    matrix_show();
    return repeat;
}


uint8_t webwc() {
    static uint16_t state;
    static float spd;
    static bool didclear;
    static bool firstpass;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 18;
    uint8_t resetspd = 24;
    uint8_t l = 0;
    //uint16_t txtcolor;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 1;
	spd = 1.0;
	didclear = 0;
	firstpass = 0;
    }

    if (mw >= 64) {
	//matrix->setFont(FreeMonoBold9pt7b);
	matrix->setFont(&Century_Schoolbook_L_Bold_12);
    } else {
	matrix->setFont(&TomThumb);
    }
    matrix->setRotation(0);
    matrix->setTextSize(1);
    if (! didclear) {
	matrix->clear();
	didclear = 1;
    }

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(12, 16);
	else if (mheight >= 64) matrix->setCursor(12, 12);
	else matrix->setCursor(5, 6);
	//txtcolor = matrix->Color24to16(Wheel(map(l, 0, 5, 0, 255)));
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
	matrix->print("WITH");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	firstpass = 1;
	if (mheight >= 96) matrix->setCursor(7, 32);
	else if (mheight >= 64) matrix->setCursor(7, 24);
	else matrix->setCursor(3, 12);
	//txtcolor = matrix->Color24to16(Wheel(map(l, 0, 5, 0, 255)));
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
	matrix->print("EVERY");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(12, 48);
	else if (mheight >= 64) matrix->setCursor(12, 36);
	else matrix->setCursor(5, 18);
	//txtcolor = matrix->Color24to16(Wheel(map(l, 0, 5, 0, 255)));
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
	matrix->print("BEAT");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(4, 64);
	else if (mheight >= 64) matrix->setCursor(4, 48);
	else matrix->setCursor(2, 24);
	//txtcolor = matrix->Color24to16(Wheel(map(l, 0, 5, 0, 255)));
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
	matrix->print("WE ARE");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || (state < overlap/spd && firstpass)) || spd > displayall)  {
	if (mheight >= 96) matrix->setCursor(0, 82);
	else if (mheight >= 64) matrix->setCursor(0, 60);
	else matrix->setCursor(0, 30);
	//txtcolor = matrix->Color24to16(Wheel(map(l, 0, 6, 0, 255)));
	//matrix->setTextColor(txtcolor);
	matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
	matrix->print("CLOSER");
	matrix->setPassThruColor();
    }
    l++;

    if (state++ > (l*duration-l*overlap)/spd) {
	state = 1;
	spd += spdincr;
	if (spd > resetspd) {
	    matrix_reset_demo = 1;
	    return 0;
	}
    }

    if (spd < displayall) fadeToBlackBy( matrixleds, NUMMATRIX, 20*map(spd, 1, 24, 1, 4));

    matrix_show();
    return 2;
}


uint8_t scrollText(const char str[], uint8_t len) {
    static int16_t x;

    uint8_t repeat = 1;
    char chr[] = " ";
    int8_t fontsize = 14; // real height is twice that.
    int8_t fontwidth = 16;
    uint8_t stdelay = 1;
    static uint16_t delayframe = stdelay;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	x = 7;
#ifndef NOFONTS
	matrix->setFont( &Century_Schoolbook_L_Bold[fontsize] );
#endif
	matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
	matrix->setTextSize(1);
	matrix->setRotation(0);
    }

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	delay(MX_UPD_TIME);
	return repeat;
    }
    delayframe = stdelay;

    matrix->clear();
    matrix->setCursor(x, 24);
    for (uint8_t c=0; c<len; c++) {
	//uint16_t txtcolor = matrix->Color24to16(Wheel(map(c, 0, len, 0, 512)));
	//matrix->setTextColor(txtcolor);
	//Serial.print(txtcolor, HEX);
	//Serial.print(" >");
	matrix->setPassThruColor(Wheel(map(c, 0, len, 0, 512)));

	chr[0]=str[c];
	
	//Serial.println(chr);
	matrix->print(chr);
    }
    matrix_show();
    //x-=2; // faster but maybe too much?
    x--;

    if (x < (-1 * (int16_t)len * fontwidth)) {
	matrix_reset_demo = 1;
	return 0;
    }
    matrix_show();
    return repeat;
}


uint8_t DoublescrollText(const char str1[], uint8_t len1, const char str2[], uint8_t len2) {
    static int16_t x;
    int16_t len = max(len1, len2);
    int8_t fontsize = 9;
    int8_t fontwidth = 11;
    int8_t stdelay = 2;

    uint8_t repeat = 4;
    if (mw >= 64) {
	fontsize = 14;
	fontwidth = 16;
	stdelay = 1;
    }
    //uint16_t txtcolor;
    static uint16_t delayframe = stdelay;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	x = 1;
#ifndef NOFONTS
	matrix->setFont( &Century_Schoolbook_L_Bold[fontsize] );
#endif
	matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
	matrix->setTextSize(1);
	matrix->setRotation(0);
    }

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	delay(MX_UPD_TIME);
	return repeat;
    }
    delayframe = stdelay;

    matrix->clear();
    matrix->setCursor(MATRIX_WIDTH-len*fontwidth*1.5 + x, fontsize+6);
    //txtcolor = matrix->Color24to16(Wheel(map(x, 0, len*fontwidth, 0, 512)));
    //matrix->setTextColor(txtcolor);
    matrix->setPassThruColor(Wheel(map(x, 0, len*fontwidth, 0, 512)));
    matrix->print(str1);

    matrix->setCursor(MATRIX_WIDTH-x, MATRIX_HEIGHT-1);
    //txtcolor = matrix->Color24to16(Wheel(map(x, 0, len*fontwidth, 512, 0)));
    //matrix->setTextColor(txtcolor);
    matrix->setPassThruColor(Wheel(map(x, 0, len*fontwidth, 512, 0)));
    matrix->print(str2);
    matrix_show();
    x++;

    if (x > 1.8 * len * fontwidth) {
	matrix_reset_demo = 1;
	return 0;
    }
    matrix_show();
    return repeat;
}

// Scroll within big bitmap so that all if it becomes visible or bounce a small one.
// If the bitmap is bigger in one dimension and smaller in the other one, it will
// be both panned and bounced in the appropriate dimensions.
void panOrBounce (uint16_t *x, uint16_t *y, uint16_t sizeX, uint16_t sizeY, bool reset = false ) {
    // keep integer math, deal with values 16 times too big
    // start by showing upper left of big bitmap or centering if the display is big
    static int16_t xf;
    static int16_t yf;
    // scroll speed in 1/16th
    static int16_t xfc;
    static int16_t yfc;
    // scroll down and right by moving upper left corner off screen
    // more up and left (which means negative numbers)
    static int16_t xfdir;
    static int16_t yfdir;

    if (reset) {
	xf = max(0, (mw-sizeX)/2) << 4;
	yf = max(0, (mh-sizeY)/2) << 4;
	xfc = 6;
	yfc = 3;
	xfdir = -1;
	yfdir = -1;
    }

    bool changeDir = false;

    // Get actual x/y by dividing by 16.
    *x = xf >> 4;
    *y = yf >> 4;

    // Only pan if the display size is smaller than the pixmap
    // but not if the difference is too small or it'll look bad.
    if (sizeX-mw>2) {
	xf += xfc*xfdir;
	if (xf >= 0)                 { xfdir = -1; changeDir = true ; };
	// we don't go negative past right corner, go back positive
	if (xf <= ((mw-sizeX) << 4)) { xfdir = 1;  changeDir = true ; };
    }
    if (sizeY-mh>2) {
	yf += yfc*yfdir;
	// we shouldn't display past left corner, reverse direction.
	if (yf >= 0)                 { yfdir = -1; changeDir = true ; };
	if (yf <= ((mh-sizeY) << 4)) { yfdir = 1;  changeDir = true ; };
    }
    // only bounce a pixmap if it's smaller than the display size
    if (mw>sizeX) {
	xf += xfc*xfdir;
	// Deal with bouncing off the 'walls'
	if (xf >= (mw-sizeX) << 4) { xfdir = -1; changeDir = true ; };
	if (xf <= 0)           { xfdir =  1; changeDir = true ; };
    }
    if (mh>sizeY) {
	yf += yfc*yfdir;
	if (yf >= (mh-sizeY) << 4) { yfdir = -1; changeDir = true ; };
	if (yf <= 0)           { yfdir =  1; changeDir = true ; };
    }

    if (changeDir) {
	// Add -1, 0 or 1 but bind result to 1 to 1.
	// Let's take 3 is a minimum speed, otherwise it's too slow.
	xfc = constrain(xfc + random(-1, 2), 3, 16);
	yfc = constrain(yfc + random(-1, 2), 3, 16);
    }
}

uint8_t panOrBounceBitmap (const uint16_t *bitmap, uint16_t bitmapSize) {
    static uint16_t state;
    uint16_t x, y;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 0;
	panOrBounce(&x, &y, bitmapSize, bitmapSize, true);
    }
    panOrBounce(&x, &y, bitmapSize, bitmapSize);

    matrix->clear();
    // pan 24x24 pixmap
    matrix->drawRGBBitmap(x, y, bitmap, bitmapSize, bitmapSize);
    matrix_show();

    if (state++ == 600) {
	matrix_reset_demo = 1;
	return 0;
    }
    return 3;
}

// FIXME: reset decoding counter to 0 between different GIFS
uint8_t GifAnim(uint8_t idx) {
    uint8_t repeat = 1;
    struct Animgif {
	const char *path;
	uint16_t looptime;
	int8_t offx;
	int8_t offy;
	int8_t factx;
	int8_t facty;
    };
    extern int OFFSETX;
    extern int OFFSETY;
    extern int FACTY;
    extern int FACTX;

    #ifdef M32B8X3
    const Animgif animgif[] = { // number of frames in the gif
    // 12 gifs
	    {"/gifs/32anim_photon.gif",		10, -4, 0, 10, 10 },	// 70
	    {"/gifs/32anim_flower.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/32anim_balls.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/32anim_dance.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/circles_swap.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/concentric_circles.gif",	10, -4, 0, 10, 10 },	// 75
	    {"/gifs/corkscrew.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/cubeconstruct.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/cubeslide.gif",		10, -4, 0, 10, 10 },
	    {"/gifs/runningedgehog.gif",	10, -4, 0, 10, 10 },
	    {"/gifs/triangles_in.gif",		10, -4, 0, 10, 10 },	// 80
	    {"/gifs/wifi.gif",			10, -4, 0, 10, 10 },
    };
    #else // M32B8M32B8X3X3
    const Animgif animgif[] = {
    // 32 gifs
	    { "/gifs64/087_net.gif",		 05, 0, 0, 10, 15 },  // 70
	    { "/gifs64/196_colorstar.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/200_circlesmoke.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/203_waterdrop.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/210_circletriangle.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/215_fallingcube.gif",	 15, 0, 0, 10, 15 },  // 75
	    { "/gifs64/255_photon.gif",		 10, 0, 0, 10, 15 }, 
	    { "/gifs64/257_mesh.gif",		 20, 0, 0, 10, 15 }, 
	    { "/gifs64/271_mj.gif",		 15, -16, 0, 15, 15 }, 
	    { "/gifs64/342_spincircle.gif",	 20, 0, 0, 10, 15 },
	    { "/gifs64/401_ghostbusters.gif",	 05, 0, 0, 10, 15 },  // 80 
	    { "/gifs64/444_hand.gif",		 10, 0, 0, 10, 15 }, 
	    { "/gifs64/469_infection.gif",	 05, 0, 0, 10, 15 }, 
	    { "/gifs64/193_redplasma.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/208_dancers.gif",	 25, 0, 0, 10, 15 },
	    { "/gifs64/284_comets.gif",		 15, 0, 0, 10, 15 },  // 85 
	    { "/gifs64/377_batman.gif",		 05, 0, 0, 10, 15 }, 
	    { "/gifs64/412_cubes.gif",		 20, 0, 0, 10, 15 }, 
	    { "/gifs64/236_spintriangle.gif",	 20, 0, 0, 10, 15 }, 
	    { "/gifs64/226_flyingfire.gif",	 10, 0, 0, 10, 15 },
	    { "/gifs64/264_expandcircle.gif",	 10, 0, 0, 10, 15 },  // 90 
	    { "/gifs64/281_plasma.gif",		 20, 0, 0, 10, 15 }, 
	    { "/gifs64/286_greenplasma.gif",	 15, 0, 0, 10, 15 }, 
	    { "/gifs64/291_circle2sphere.gif",	 15, 0, 0, 10, 15 }, 
	    { "/gifs64/364_colortoroid.gif",	 25, 0, 0, 10, 15 },
	    { "/gifs64/470_scrollcubestron.gif", 20, 0, 0, 10, 15 },  // 95
	    { "/gifs64/358_spinningpattern.gif", 10, 0, 0, 10, 15 }, 
	    { "/gifs64/328_spacetime.gif",	 20, 0, 0, 10, 15 }, 
	    { "/gifs64/218_circleslices.gif",	 10, 0, 0, 10, 15 }, 
            { "/gifs64/heartTunnel.gif",	 10, 0, 0, 10, 15 },
            { "/gifs64/sonic.gif",		 10, 0, 0, 10, 15 },  // 100
	    { "/gifs64/ani-bman-BW.gif",	 10, 0, 0, 10, 15 },  // 101
	    { "/gifs64/ab1_colors.gif",		 10, 0, 16, 10, 10 },
//	    { "/gifs64/ab2_grey.gif",		 10, 0, 16, 10, 10 },
	    { "/gifs64/ab2_lgrey.gif",		 10, 0, 16, 10, 10 },
//	    { "/gifs64/ab2_white.gif",		 10, 0, 16, 10, 10 }, 
            { "/gifs64/ab3_s.gif",		 10, 0, 16, 10, 10 },
            { "/gifs64/ab4_grey.gif",		 10, -16, 0, 15, 15 },// 105
	    //{ "/gifs64/ab4_w.gif",		 10, 0, 16, 10, 10 },



	#if 0
            { "/gifs64/149_minion1.gif",	 10, 0, 0, 10, 15 },
            { "/gifs64/341_minion2.gif",	 10, 0, 0, 10, 15 },
            { "/gifs64/233_mariokick.gif",	 10, 0, 0, 10, 15 },
            { "/gifs64/457_mariosleep.gif",	 10, 0, 0, 10, 15 },
            { "/gifs64/240_angrybird.gif",	 10, 0, 0, 10, 15 },
            { "/gifs64/323_rockface.gif",	 10, 0, 0, 10, 15 },
	    { "/gifs64/149_minion1.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/341_minion2.gif",	 10, 0, 0, 10, 15 }, 
	    { "/gifs64/222_fry.gif",		 10, 0, 0, 10, 15 }, 
	#endif
    };
    #endif
    uint8_t gifcnt = ARRAY_SIZE(animgif);
    // Avoid crashes due to overflows
    idx = idx % gifcnt;
    static uint8_t gifloopsec;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	bool savng = sav_newgif(animgif[idx].path);
	// exit if the gif animation couldn't get setup.
	if (savng) return 0;
	gifloopsec =  animgif[idx].looptime;
	OFFSETX = animgif[idx].offx;
	OFFSETY = animgif[idx].offy;
	FACTX =   animgif[idx].factx;
	FACTY =   animgif[idx].facty;
	matrix->clear();
    }

    // sav_loop may or may not run show() depending on whether
    // it's time to decode the next frame. If it did not, wait here to
    // add the matrix_show() delay that is expected by the caller
    bool savl = sav_loop();
    if (savl) { delay(MX_UPD_TIME); };

    EVERY_N_SECONDS(1) { 
	Serial.print(gifloopsec); Serial.print(" ");
	if (!gifloopsec--) { Serial.println(); return 0; };
    }
    return repeat;
}

uint8_t scrollBigtext() {
    // 64x96 pixels, chars are 5(6)x7, 10.6 chars across, 13.7 lines of displays
    static uint16_t state = 0;
    static uint8_t resetcnt = 3;
    uint16_t loopcnt = 1000;
    uint16_t x, y;

#if 0
    static const char* text[] = {
	"function nodeIsImport(elt) {",
	"  return elt.localName === 'link' && elt.rel === IMPORT_LINK_TYPE; }",
	"function generateScriptDataUrl(script) {",
	"  var scriptContent = generateScriptContent(script);",
	"  return 'data:text/javascript;charset=utf-8,' + encodeURIComponent(scriptContent); }",
	"function generateScriptContent(script) {",
	"  return script.textContent + generateSourceMapHint(script); }",
	"function generateSourceMapHint(script) {",
	"  var owner = script.ownerDocument;",
	"  owner.__importedScripts = owner.__importedScripts || 0;",
	"  var moniker = script.ownerDocument.baseURI;",
	"  var num = owner.__importedScripts ? '-' + owner.__importedScripts : '';",
	"  owner.__importedScripts++;",
	"  return '\\n//# sourceURL=' + moniker + num + '.js\\n'; }",
	"function cloneStyle(style) {",
	"  var clone = style.ownerDocument.createElement('style');",
	"  clone.textContent = style.textContent;",
	"  path.resolveUrlsInStyle(clone);",
	"  return clone; }",
    };
#endif
    static const char* text[] = {
	"if (sizeX-mw>2) { xf += xfc*xfdir;",
	"  if (xf >= 0)                 { xfdir = -1; changeDir = true ; };",
	"  // we don't go negative past right corner, go back positive",
	"  if (xf <= ((mw-sizeX) << 4)) { xfdir = 1;  changeDir = true ; }; }",
	"if (sizeY-mh>2) { yf += yfc*yfdir;",
	"  // we shouldn't display past left corner, reverse direction.",
	"  if (yf >= 0)                 { yfdir = -1; changeDir = true ; };",
	"  if (yf <= ((mh-sizeY) << 4)) { yfdir = 1;  changeDir = true ; }; }",
	"  // only bounce a pixmap if it's smaller than the display size",
	"if (mw>sizeX) { xf += xfc*xfdir;",
	"  // Deal with bouncing off the 'walls'",
	"  if (xf >= (mw-sizeX) << 4) { xfdir = -1; changeDir = true ; };",
	"  if (xf <= 0)           { xfdir =  1; changeDir = true ; }; }",
	"if (mh>sizeY) { yf += yfc*yfdir;",
	"  if (yf >= (mh-sizeY) << 4) { yfdir = -1; changeDir = true ; };",
	"  if (yf <= 0)           { yfdir =  1; changeDir = true ; }; }",
	"if (changeDir) {",
	"  // Add -1, 0 or 1 but bind result to 1 to 1.",
	"  // Let's take 3 is a minimum speed, otherwise it's too slow.",
	"  xfc = constrain(xfc + random(-1, 2), 3, 16);",
	"  yfc = constrain(yfc + random(-1, 2), 3, 16); }",
    };
    const uint8_t textlines = ARRAY_SIZE(text);
    static uint32_t textcolor[textlines];

    if (matrix_reset_demo == 1) {
	state = 0;
	matrix_reset_demo = 0;
	panOrBounce(&x, &y, 54*6, ARRAY_SIZE(text)*7, true);
	matrix->setRotation(0);
	matrix->setTextSize(1);
	// default font is 5x7, but you really need 6x8 for spacing
	matrix->setFont(NULL);
	for (uint8_t i=0; i<=textlines-1; i++) {
	    textcolor[i] = random8(96) * 65536 + (127 + random8(128))* 256, random8(96);
	    //Serial.print("Setup color mapping: ");
	    //Serial.println(textcolor[i], HEX);
	}
    }

    state++;
    panOrBounce(&x, &y, 54*6, ARRAY_SIZE(text)*7);

    matrix->clear();
    for (uint8_t i=0; i<=textlines-1; i++) {
	matrix->setPassThruColor(textcolor[i]);
	matrix->setCursor(x, y+8*i);
	matrix->print(text[i]);
    }
    matrix->setPassThruColor();
    matrix_show();

    if (state > loopcnt) {
	matrix_reset_demo = 1;
	return 0;
    }
    return resetcnt;
}


// this is doing it the hard way, and only for my matrix.
// instead, use XY() I added in NeoMatrix
#if 0
uint16_t pos2matrix(uint16_t pos) {
    #define panelwidth 8
    #define panelnum 3
    #define ledwidth (panelwidth * panelnum)
    #define panelheight 32

    uint16_t newpos;
    // gives 0 1 or 2 for panel 0 1 or 2
    uint16_t panel = (pos % ledwidth)/panelwidth;
    // 0 256 or 512
    uint16_t paneloffset = panel * panelwidth * panelheight;
    // 23 => line 0 panel 2, 39, line 1 panel 1
    uint16_t line = pos/ledwidth;
    // 0-23 - [0-2]*8 gives 0 to 7 for each panel
    uint16_t lineoffset = (pos % ledwidth) - panel*panelwidth;

    return paneloffset + line*panelwidth + lineoffset;
}
#endif

// Make use of my new XY API in Neomatrix
uint16_t pos2matrix(uint16_t pos) {
    return matrix->XY(pos % mw, pos / mw);
}

uint8_t demoreel100(uint8_t demo) {
    #define demoreeldelay 1

    static uint16_t state;
    static uint8_t gHue = 0;
    uint8_t repeat = 2;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 0;
    }

#if mheight <= 64
    static uint16_t delayframe = demoreeldelay;
    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	delay(MX_UPD_TIME);
	return repeat;
    }
    delayframe = demoreeldelay;
#endif
    state++;
    gHue++;

    if (state < 2000)
    {
	if (demo == 1) {
	    // random colored speckles that blink in and fade smoothly
#if mheight > 64
	    fadeToBlackBy( matrixleds, NUMMATRIX, 3);
#else
	    fadeToBlackBy( matrixleds, NUMMATRIX, 10);
#endif
	    int pos = random16(NUMMATRIX);
	    matrixleds[pos] += CHSV( gHue + random8(64), 200, 255);
	}
	if (demo == 2) {
	    // a colored dot sweeping back and forth, with fading trails
#if mheight > 64
	    fadeToBlackBy( matrixleds, NUMMATRIX, 5);
#else
	    fadeToBlackBy( matrixleds, NUMMATRIX, 20);
#endif
	    int pos = beatsin16( 13, 0, NUMMATRIX-1 );
	    matrixleds[pos2matrix(pos)] += CHSV( gHue, 255, 192);
	}
	if (demo == 3) {
	    // eight colored dots, weaving in and out of sync with each other
#if mheight > 64
	    fadeToBlackBy( matrixleds, NUMMATRIX, 5);
#else
	    fadeToBlackBy( matrixleds, NUMMATRIX, 20);
#endif
	    byte dothue = 0;
	    for( int i = 0; i < 8; i++) {
		int pos = beatsin16( i+7, 0, NUMMATRIX-1 );
	      matrixleds[pos2matrix(pos)] |= CHSV(dothue, 200, 255);
	      dothue += 32;
	    }
	}
    } else {
	matrix_reset_demo = 1;
	return 0;
    }

    matrix_show();
    return repeat;
}


uint8_t call_twinklefox()
{
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	state = 0;
    }

    twinkle_loop();
    if (state++ < 1000) return 2;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_pride()
{
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	state = 0;
    }

    pride();
    matrix_show();
    if (state++ < 1000) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_fireworks() {
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 0;
    }

    fireworks();
    matrix_show();
    if (state++ < 5000) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_fire() {
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 0;
    }

    fire();
    matrix_show();
    if (state++ < 1500) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_rain(uint8_t which) {
    #define raindelay 4
    static uint16_t state;
    static uint16_t delayframe = raindelay;

    if (matrix_reset_demo == 1) {
	sublime_reset();
	matrix_reset_demo = 0;
	state = 0;
    }

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	delay(MX_UPD_TIME);
	return 1;
    }
    delayframe = raindelay;

    if (which == 1) theMatrix();
    if (which == 2) coloredRain();
    if (which == 3) stormyRain();
    matrix_show();
    if (state++ < 500) return 2;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_pacman(uint8_t loopcnt) {
    #define pacmandelay 5
    static uint16_t delayframe = pacmandelay;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	pacman_setup(loopcnt);
    }

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	delay(MX_UPD_TIME);
	return 1;
    }
    delayframe = pacmandelay;

    if (pacman_loop()) return 1;
    matrix_reset_demo = 1;
    return 0;
}

// Adapted from	LEDText/examples/TextExample3 by Aaron Liddiment
// bright and annoying, I took it down to just a very quick show.
uint8_t plasma() {
    #define PLASMA_X_FACTOR  24
    #define PLASMA_Y_FACTOR  24
    static uint16_t PlasmaTime, PlasmaShift;
    uint16_t OldPlasmaTime;

    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	state = 0;
    }

    PlasmaShift = (random8(0, 5) * 32) + 64;
    int16_t r, h;
    int x, y;

    for (x=0; x<MATRIX_WIDTH; x++)
    {
	for (y=0; y<MATRIX_HEIGHT; y++)
	{
	    r = sin16(PlasmaTime) / 256;
	    h = sin16(x * r * PLASMA_X_FACTOR + PlasmaTime) + cos16(y * (-r) * PLASMA_Y_FACTOR + PlasmaTime) + sin16(y * x * (cos16(-PlasmaTime) / 256) / 2);
	    // drawPixel does not accept CHSV, so we get a fastLED offset and write to it directly
	    matrixleds[matrix->XY(x, y)] = CHSV((h / 256) + 128, 255, 255);
	}
    }

    OldPlasmaTime = PlasmaTime;
    PlasmaTime += PlasmaShift;
    if (OldPlasmaTime > PlasmaTime) PlasmaShift = (random8(0, 5) * 32) + 64;

    matrix_show();
    if (state++ < 200) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t metd(uint8_t demo) {
    // 0 to 12
    // add new demos at the end or the number selections will be off
    const uint16_t metd_mapping[][3] = {
	{  10, 5, 300 },  // 5 color windows-like pattern with circles in and out
	{  11, 5, 300 },  // color worm patterns going out with circles zomming out
	{  25, 3, 500 },  // 5 circles turning together, run a bit longer
	{  29, 5, 300 },  // swirly RGB colored dots meant to go to music
	{  34, 5, 300 },  // single colored lines that extend from center.
//	{  36, 2, 200 },  // circles of color, too similar to 34 and broken on non square display
//	{  37, 6, 200 },  // other kinds of swirly colored pixels, too close to 29
	{  52, 5, 300 },  // rectangles/squares/triangles zooming out
	{  67, 5, 900 },  // two colors swirling bigger, creating hypno pattern
	{  70, 6, 200 },  // 4 fat spinning comets with shapes growing from middle
	{  73, 3, 2000 }, // circle inside fed by outside attracted color dots
	{  77, 5, 300 },  // streaming lines of colored pixels with shape zooming in or out
	{  80, 5, 200 },  // rotating triangles of color
	{ 105, 2, 400 },  // spinnig changing colors of hypno patterns
	{ 110, 5, 300 },  // bubbles going up or right
    };

    uint8_t dfinit = metd_mapping[demo][1];
    uint16_t loops = metd_mapping[demo][2];
    static uint8_t delayframe = dfinit;
    demo = metd_mapping[demo][0];

    if (matrix_reset_demo == 1) {
	Serial.print("Starting ME Table Demo #");
	Serial.println(demo);
	matrix_reset_demo = 0;
	td_init();

	switch (demo) {
	case 10:
	    driftx = MIDLX;//pin the animation to the center
	    drifty = MIDLY;
	    break;
	case 11:
	case 25:
	    adjunct = 0;
	    break;
	case 52:
	    bfade = 3;
	    break;
	case 110:
	    // this kills the trail but also makes the colors too dark
	    // not true in the original demo.
	    // bfade = 10;
	    break;
	}
	matrix->clear();
    }

    if (--delayframe) {
	delay(MX_UPD_TIME);
	return 2;
    }
    delayframe = dfinit;

    switch (demo) {
    case 10:
	corner();
	bkringer();
	ringer();
	break;
    case 11:
	whitewarp();
	ringer();
	break;
    case 25:
	spire();
	if (flip3) adjuster();
	break;
    case 29:
	Raudio();
	break;
    case 34:
	Raudio3();
	break;
#if 0
    case 36: // unused, circles of color, too similar to 34 and broken on non square display
	Raudio5();
	break;
    case 37: // unused, other kinds of loose colored pixels, too close to 29
	Raudio();
        adjuster();
	break;
#endif
    case 52:
	rmagictime();
	bkboxer();
	starer();
	if (flip && !flip2) adjuster();
	break;
    case 67:
	hypnoduck2();
	break;
    case 70:
	// TODO, inspect each sub-demo
	if (flip2) boxer();
	else if (flip3) bkringer();
	spin2();
	if (!flip && flip2 && !flip3) adjuster();
	break;
    case 73: // unused, circle that gets fed by outside streams
        homer2();
	break;
    case 77:
	if (flip2) bkstarer(); else bkringer();
	whitewarp();
	break;
    case 80:
	starz();
	break;
    case 105:
	hypnoduck4();
	break;
    case 110:
	//if (flip3) solid2();
	bubbles();
	break;
    }

    td_loop();
    matrix_show();
    if (counter < loops) return 2;
    matrix_reset_demo = 1;
    return 0;
}


void matrix_change(int demo) {
    // Reset passthrough from previous demo
    matrix->setPassThruColor();
    // Clear screen when changing demos.
    matrix->clear();
    // this ensures the next demo returns the number of times it should loop
    matrix_loop = -1;
    matrix_reset_demo = 1;
    if (demo==-128) if (matrix_state-- == 0) matrix_state = demo_cnt;
    if (demo==+127) if (matrix_state++ == demo_cnt) matrix_state = 0;
    // If existing matrix was already >98, any +- change brings it back to 0.
    if (matrix_state >= 98) matrix_state = 0;
    if (demo >= 0 && demo < 127) matrix_state = demo;
#ifdef NEOPIXEL_PIN
    // Special one key press where demos are shown forever and next goes back to the normal loop
    if (demo >= 0 && demo < 126) matrix_loop = 9999;
#endif
    Serial.print("Got matrix_change ");
    Serial.print(demo);
    Serial.print(", switching to index ");
    if (matrix_state == 126) {
	Serial.print(matrix_state);
	matrix_demo = matrix_state;
    } else {
	if (show_best_demos) {
	    Serial.print(matrix_state % best_cnt);
	    Serial.print(" (bestof mode) ");
	    matrix_demo = best_mapping[matrix_state % best_cnt];
	} else {
	    Serial.print(matrix_state % demo_cnt);
	    Serial.print(" (full mode) ");
	    matrix_demo = demo_mapping[matrix_state % demo_cnt];
	}
    }
    Serial.print(", mapped to matrix demo ");
    Serial.print(matrix_demo);
    Serial.print(" loop ");
    Serial.println(matrix_loop);
}

void matrix_update() {
    uint8_t ret;

    EVERY_N_MILLISECONDS(40) {
	gHue++;  // slowly cycle the "base color" through the rainbow
    }

    switch (matrix_demo) {
	case 0:
	    ret = squares(0);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 1:
	    ret = squares(1);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 2:
	    ret = esrr_bbb(); // or burn baby burn
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 3:
	    ret = esrr_fade();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 4:
	    ret = tfsf_zoom(0, 30);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 5:
	    ret = tfsf_zoom(1, 30);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 6:
	    ret = tfsf();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 7:
	    ret = webwc();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 8:
	    ret = bbb();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 9:
	    ret = DoublescrollText("Safety", 6, "Third!", 6);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 10:
	    ret = scrollBigtext();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 12:
	    ret = panOrBounceBitmap(bitmap24, 24);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 17:
	    ret = call_fireworks();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 18:
	    ret = call_twinklefox();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 19:
	    ret = call_pride();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 20:
	    ret = demoreel100(1); // Twinlking stars
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 21:
	    ret = demoreel100(2); // color changing pixels sweeping up and down
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 22:
	    ret = demoreel100(3); // colored pixels being exchanged between top and bottom
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 23:
	    ret = call_rain(1); // matrix
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 24:
	    ret = call_rain(3); // clouds, rain, lightening
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 25:
	    ret = call_pacman(3);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 26:
	    ret = plasma();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 27:
	    ret = call_fire();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	default:
	    // aurora 13 demo
	    if      (matrix_demo >= 30 && matrix_demo <= 42) ret = aurora(matrix_demo-30);
	    // table 13 demos
	    else if (matrix_demo >= 45 && matrix_demo <= 57) ret = metd(matrix_demo-45);
#ifdef M32B8X3
	    // 12 gifs
	    else if (matrix_demo >= 70 && matrix_demo <= 81)
#else // M32B8M32B8X3X3
	    // 32 gifs
	    else if (matrix_demo >= 70 && matrix_demo <= 108)
#endif
	    {
		// Before a new GIF, give a chance for an IR command to go through
		//if (matrix_loop == -1) delay(3000);
		ret = GifAnim(matrix_demo-70);
	    }
	    else { ret = 0; Serial.print("Cannot play demo "); Serial.println(matrix_demo); };

	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 126:
#ifdef M32B8X3
	    const char str[] = "Thank You :)";
	    ret = scrollText(str, sizeof(str));
#else
	    fixdrawRGBBitmap(28, 87, RGB_bmp, 8, 8);
	    ret = display_text("Thank\n  You\n  Very\n Much", 0, 14);
#endif
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;
    }
    matrix_reset_demo = 1;
    Serial.print("Done with demo ");
    Serial.print(matrix_demo);
    Serial.print(" loop ");
    Serial.println(matrix_loop);
    if (--matrix_loop > 0) return;
    matrix_change(127);
}

// ---------------------------------------------------------------------------
// Strip Code
// ---------------------------------------------------------------------------

#ifdef NEOPIXEL_PIN
void leds_show() {
#ifdef SMARTMATRIX
    FastLED.show();
#else
    FastLED[0].showLeds(led_brightness);
#endif
}
void leds_setcolor(uint16_t i, uint32_t c) {
    leds[i] = c;
}
#endif // NEOPIXEL_PIN

void change_brightness(int8_t change) {
    static uint8_t brightness = 5;
    static uint32_t last_brightness_change = 0 ;

    if (millis() - last_brightness_change < 300) {
	Serial.print("Too soon... Ignoring brightness change from ");
	Serial.println(brightness);
	return;
    }
    last_brightness_change = millis();
    brightness = constrain(brightness + change, 2, 8);
    led_brightness = min((1 << (brightness+1)) - 1, 255);

    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" led value ");
    Serial.print(led_brightness);
#ifdef SMARTMATRIX
    uint8_t smartmatrix_brightness = min( (1 << (brightness+2)), 255);
    Serial.print(" neomatrix value ");
    Serial.println(smartmatrix_brightness);
    matrixLayer.setBrightness(smartmatrix_brightness);
#else
    matrix_brightness = (1 << (brightness-1)) - 1;
    Serial.print(" neomatrix value ");
    Serial.println(matrix_brightness);
    // This is needed if using the ESP32 driver but won't work
    // if using 2 independent fastled strips (1D + 2D matrix)
    matrix->setBrightness(matrix_brightness);
#endif
}

void change_speed(int8_t change) {
    static uint32_t last_speed_change = 0 ;

    if (millis() - last_speed_change < 200) {
	Serial.print("Too soon... Ignoring speed change from ");
	Serial.println(strip_speed);
	return;
    }
    last_speed_change = millis();

    Serial.print("Changing speed ");
    Serial.print(strip_speed);
    strip_speed = constrain(strip_speed + change, 1, 100);
    Serial.print(" to new speed ");
    Serial.println(strip_speed);
}

bool is_change(bool force=false) {
    uint32_t newmil = millis();
    // Any change after next button acts as a pattern change for 5 seconds
    // (actually more than 5 secs because millis stops during panel refresh)
    if (newmil - last_change < 5000) {
	last_change = newmil;
	return 1;
    }
    if (force) return 0;
#ifdef NEOPIXEL_PIN
    return 0;
#else
    // When not running neopixel strip, all keys have direct action without requiring
    // pressing next pattern first
    return 1;
#endif
}


bool handle_IR(uint32_t delay_time) {
    int8_t new_pattern = 0;
    char readchar;

    decode_results IR_result;

    // Instead of waiting without doing anything, update the matrix pattern.
    for (uint16_t i=0; i<delay_time / MX_UPD_TIME; i++) {
	matrix_update();
    }

    // Whatever time is left over, we wait with a real delay
    // 45 msec wait = 4 loops of matrix updates of 10msec each + 5ms delay

    delay(delay_time % MX_UPD_TIME);
    // Don't run update continuously, or the IR interrupts don't get in.
    // if (delay_time % MX_UPD_TIME > MX_UPD_TIME/2) matrix_update();


    if (Serial.available()) readchar = Serial.read(); else readchar = 0;
    if (readchar) {
	while ((readchar >= '0') && (readchar <= '9')) {
	    new_pattern = 10 * new_pattern + (readchar - '0');
	    readchar = 0;
	    if (Serial.available()) readchar = Serial.read();
	}

	if (new_pattern) {
	    Serial.print("Got new pattern via serial ");
	    Serial.println(new_pattern);
	    matrix_change(new_pattern);
	} else {
	    Serial.print("Got serial char ");
	    Serial.println(readchar);
	}
    }

    if (readchar == 'n')      { Serial.println("Serial => next"); matrix_change(127);}
    else if (readchar == 'p') { Serial.println("Serial => previous"); matrix_change(-128);}
    else if (readchar == '-') { Serial.println("Serial => dim"   ); change_brightness(-1);}
    else if (readchar == '+') { Serial.println("Serial => bright"); change_brightness(+1);}


    if (irrecv.decode(&IR_result)) {
    	irrecv.resume(); // Receive the next value
	switch (IR_result.value) {

	case IR_RGBZONE_BRIGHT:
	    if (is_change()) { show_best_demos = true; Serial.println("Got IR: Bright, Only show best demos"); return 1; }
	    change_brightness(+1);
	    Serial.println("Got IR: Bright");
	    return 1;

	case IR_RGBZONE_DIM:
	    if (is_change()) {
		Serial.println("Got IR: Dim, show all demos again and Hang on this demo");
		matrix_loop = 9999;
		show_best_demos = false;
		return 1;
	    }

	    change_brightness(-1);
	    Serial.println("Got IR: Dim");
	    return 1;

	case IR_RGBZONE_NEXT2:
	    Serial.println("Got IR: Next2");
	case IR_RGBZONE_NEXT:
	    last_change = millis();
	    matrix_change(127);
	    Serial.print("Got IR: Next to matrix state ");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_POWER2:
	    Serial.println("Got IR: Power2");
	case IR_RGBZONE_POWER:
	    if (is_change()) { matrix_change(-128); return 1; }
	    Serial.println("Got IR: Power, show all demos again and Hang on this demo");
	    matrix_loop = 9999;
	    show_best_demos = false;
	    return 1;

	case IR_RGBZONE_RED:
	    Serial.println("Got IR: Red (0)");
	    if (is_change()) { matrix_change(0); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF0000;
	    return 1;

	case IR_RGBZONE_GREEN:
	    Serial.println("Got IR: Green (3)");
	    if (is_change()) { matrix_change(3); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00FF00;
	    return 1;

	case IR_RGBZONE_BLUE:
	    Serial.println("Got IR: Blue (5)");
	    if (is_change()) { matrix_change(5); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x0000FF;
	    return 1;

	case IR_RGBZONE_WHITE:
	    Serial.println("Got IR: White (7)");
	    if (is_change()) { matrix_change(7); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFFFFF;
	    return 1;



	case IR_RGBZONE_RED2:
	    Serial.println("Got IR: Red2 (9)");
	    if (is_change()) { matrix_change(9); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xCE6800;
	    return 1;

	case IR_RGBZONE_GREEN2:
	    Serial.println("Got IR: Green2 (11)");
	    if (is_change()) { matrix_change(11); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    return 1;

	case IR_RGBZONE_BLUE2:
	    Serial.println("Got IR: Blue2 (13)");
	    if (is_change()) { matrix_change(13); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x0000BB;
	    return 1;

	case IR_RGBZONE_PINK:
	    Serial.println("Got IR: Pink (15)");
	    if (is_change()) { matrix_change(15); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF50FE;
	    return 1;



	case IR_RGBZONE_ORANGE:
	    Serial.println("Got IR: Orange (17)");
	    if (is_change()) { matrix_change(17); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF8100;
	    return 1;

	case IR_RGBZONE_BLUE3:
	    Serial.println("Got IR: Green2 (19)");
	    if (is_change()) { matrix_change(19); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    return 1;

	case IR_RGBZONE_PURPLED:
	    Serial.println("Got IR: DarkPurple (21)");
	    if (is_change()) { matrix_change(21); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x270041;
	    return 1;

	case IR_RGBZONE_PINK2:
	    Serial.println("Got IR: Pink2 (23)");
	    if (is_change()) { matrix_change(23); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFB9FF;
	    Serial.println("Got IR: Pink2");
	    return 1;



	case IR_RGBZONE_ORANGE2:
	    Serial.println("Got IR: Orange2 (25)");
	    if (is_change()) { matrix_change(25); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFCA49;
	    return 1;

	case IR_RGBZONE_GREEN3:
	    Serial.println("Got IR: Green2 (27)");
	    if (is_change()) { matrix_change(27); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x006A00;
	    return 1;

	case IR_RGBZONE_PURPLE:
	    Serial.println("Got IR: DarkPurple2 (29)");
	    if (is_change()) { matrix_change(29); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x2B0064;
	    return 1;

	case IR_RGBZONE_BLUEL:
	    Serial.println("Got IR: BlueLight (31)");
	    if (is_change()) { matrix_change(31); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x50A7FF;
	    return 1;



	case IR_RGBZONE_YELLOW:
	    Serial.println("Got IR: Yellow (33)");
	    if (is_change()) { matrix_change(33); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xF0FF00;
	    return 1;

	case IR_RGBZONE_GREEN4:
	    Serial.println("Got IR: Green2 (35)");
	    if (is_change()) { matrix_change(35); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    return 1;

	case IR_RGBZONE_PURPLE2:
	    Serial.println("Got IR: Purple2 (37)");
	    if (is_change()) { matrix_change(37); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x660265;
	    return 1;

	case IR_RGBZONE_BLUEL2:
	    Serial.println("Got IR: BlueLight2 (39)");
	    if (is_change()) { matrix_change(39); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x408BD8;
	    return 1;


	case IR_RGBZONE_RU:
	    matrix_change(41);
	    Serial.print("Got IR: Red UP switching to matrix state 41");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_GU:
	    matrix_change(42);
	    Serial.print("Got IR: Green UP switching to matrix state 42");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_BU:
	    matrix_change(43);
	    Serial.print("Got IR: Blue UP switching to matrix state 43");
	    Serial.println(matrix_state);
	    return 1;


	case IR_RGBZONE_RD:
	    Serial.print("Got IR: Red DOWN switching to matrix state 44");
	    matrix_change(44);
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_GD:
	    Serial.print("Got IR: Green DOWN switching to matrix state 45");
	    matrix_change(45);
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_BD:
	    Serial.print("Got IR: Blue DOWN switching to matrix state 47");
	    matrix_change(47);
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_QUICK:
	    Serial.println("Got IR: Quick");
	    //if (is_change()) { matrix_change(24); return 1; }
	    change_speed(-10);
	    return 1;

	case IR_RGBZONE_SLOW:
	    Serial.println("Got IR: Slow)");
	    //if (is_change()) { matrix_change(28); return 1; }
	    change_speed(+10);
	    return 1;




	case IR_RGBZONE_DIY1:
	    Serial.println("Got IR: DIY1 (49)");
	    if (is_change()) { matrix_change(49); return 1; }
	    // this uses the last color set
	    nextdemo = f_colorWipe;
	    Serial.println("Got IR: DIY1 colorWipe");
	    return 1;

	case IR_RGBZONE_DIY2:
	    Serial.println("Got IR: DIY2 (51)");
	    if (is_change()) { matrix_change(51); return 1; }
	    // this uses the last color set
	    nextdemo = f_theaterChase;
	    Serial.println("Got IR: DIY2/theaterChase");
	    return 1;

	case IR_RGBZONE_DIY3:
	    Serial.println("Got IR: DIY3 (53)");
	    if (is_change()) { matrix_change(53); return 1; }
	    nextdemo = f_juggle;
	    Serial.println("Got IR: DIY3/Juggle");
	    return 1;

	case IR_RGBZONE_AUTO:
	    Serial.println("Got IR: AUTO/bpm (55)");
	    if (is_change()) { matrix_change(55); return 1; }
	    matrix_change(126);
	    nextdemo = f_bpm;
	    return 1;


	// From here, we jump numbers more quickly to cover ranges of demos 32-44
	case IR_RGBZONE_DIY4:
	    Serial.println("Got IR: DIY4 (57)");
	    if (is_change()) { matrix_change(57); return 1; }
	    nextdemo = f_rainbowCycle;
	    Serial.println("Got IR: DIY4/rainbowCycle");
	    return 1;

	case IR_RGBZONE_DIY5:
	    Serial.println("Got IR: DIY5 (59)");
	    if (is_change()) { matrix_change(59); return 1; }
	    nextdemo = f_theaterChaseRainbow;
	    Serial.println("Got IR: DIY5/theaterChaseRainbow");
	    return 1;

	case IR_RGBZONE_DIY6:
	    Serial.println("Got IR: DIY6 (61)");
	    if (is_change()) { matrix_change(61); return 1; }
	    nextdemo = f_doubleConvergeRev;
	    Serial.println("Got IR: DIY6/DoubleConvergeRev");
	    return 1;

	case IR_RGBZONE_FLASH:
	    Serial.println("Got IR: FLASH (63)");
	    if (is_change()) { matrix_change(63); return 1; }
	    nextdemo = f_flash;
	    return 1;

	// And from here, jump across a few GIF anims (45-53)
	case IR_RGBZONE_JUMP3:
	    Serial.println("Got IR: JUMP3/Cylon (65)");
	    if (is_change()) { matrix_change(65); return 1; }
	    nextdemo = f_cylon;
	    return 1;

	case IR_RGBZONE_JUMP7:
	    Serial.println("Got IR: JUMP7/CylonWithTrail (67)");
	    if (is_change()) { matrix_change(67); return 1; }
	    nextdemo = f_cylonTrail;
	    return 1;

	case IR_RGBZONE_FADE3:
	    Serial.println("Got IR: FADE3/DoubleConverge (69)");
	    if (is_change()) { matrix_change(69); return 1; }
	    nextdemo = f_doubleConverge;
	    return 1;

	case IR_RGBZONE_FADE7:
	    Serial.println("Got IR: FADE7/DoubleConvergeTrail (71)");
	    if (is_change()) { matrix_change(71); return 1; }
	    nextdemo = f_doubleConvergeTrail;
	    return 1;

	case IR_JUNK:
	    return 0;

	default:
	    Serial.print("Got unknown IR value: ");
	    Serial.println(IR_result.value, HEX);
	    // Allow pausing the current demo to inspect it in slow motion
	    //delay(1000);
	    return 0;
	}
    }
    return 0;
}



#ifdef NEOPIXEL_PIN
    // Demos from FastLED
    // fade in 0 to x/256th of the previous value
    void fadeall(uint8_t fade) {
        for(uint16_t i = 0; i < STRIP_NUM_LEDS; i++) {  leds[i].nscale8(fade); }
    }

    void cylon(bool trail, uint8_t wait) {
        static uint8_t hue = 0;
        // First slide the led in one direction
        for(uint16_t i = 0; i < STRIP_NUM_LEDS; i++) {
    	// Set the i'th led to red
    	leds_setcolor(i, Wheel(hue++));
    	// Show the leds
    	leds_show();
    	// now that we've shown the leds, reset the i'th led to black
    	//if (!trail) leds_setcolor(i, 0);
    	if (trail) fadeall(224); else fadeall(92);
    	// Wait a little bit before we loop around and do it again
    	if (handle_IR(wait/4)) return;
        }

        // Now go in the other direction.
        for(uint16_t i = (STRIP_NUM_LEDS)-1; i > 0; i--) {
    	// Set the i'th led to red
    	leds_setcolor(i, Wheel(hue++));
    	// Show the leds
    	leds_show();
    	// now that we've shown the leds, reset the i'th led to black
    	//if (!trail) leds_setcolor(i, 0);
    	if (trail) fadeall(224); else fadeall(92);
    	// Wait a little bit before we loop around and do it again
    	if (handle_IR(wait/4)) return;
        }
    }

    void doubleConverge(bool trail, uint8_t wait, bool rev) {
        static uint8_t hue;
        for(uint16_t i = 0; i < STRIP_NUM_LEDS/2 + 4; i++) {

    	if (i < STRIP_NUM_LEDS/2) {
    	    if (!rev) {
    		leds_setcolor(i, Wheel(hue++));
    		leds_setcolor(STRIP_NUM_LEDS - 1 - i, Wheel(hue++));
    	    } else {
    		leds_setcolor(STRIP_NUM_LEDS/2 -1 -i, Wheel(hue++));
    		leds_setcolor(STRIP_NUM_LEDS/2 + i, Wheel(hue++));
    	    }
    	}
    #if 0
    	if (!trail && i>3) {
    	    if (!rev) {
    		leds_setcolor(i - 4, 0);
    		leds_setcolor(STRIP_NUM_LEDS - 1 - i + 4, 0);
    	    } else {
    		leds_setcolor(STRIP_NUM_LEDS/2 -1 -i +4, 0);
    		leds_setcolor(STRIP_NUM_LEDS/2 + i -4, 0);
    	    }
    	}
    #endif
    	if (trail) fadeall(224); else fadeall(92);
    	leds_show();
    	if (handle_IR(wait/3)) return;
        }
    }

    // From FastLED's DemoReel
    // ---------------------------------------------
    void add1DGlitter( fract8 chanceOfGlitter)
    {
        if (random8() < chanceOfGlitter) {
    	leds[ random16(STRIP_NUM_LEDS) ] += CRGB::White;
        }
    }


    void juggle(uint8_t wait) {
        // eight colored dots, weaving in and out of sync with each other
        fadeToBlackBy( leds, STRIP_NUM_LEDS, 20);
        byte dothue = 0;
        for( int i = 0; i < 8; i++) {
    	leds[beatsin16( i+7, 0, STRIP_NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    	dothue += 32;
        }
        leds_show();
        if (handle_IR(wait/3)) return;
    }

    void bpm(uint8_t wait)
    {
        // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
        uint8_t BeatsPerMinute = 62;
        CRGBPalette16 palette = PartyColors_p;
        uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
        for( int i = 0; i < STRIP_NUM_LEDS; i++) { 
    	leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
        }
        gHue++;
        leds_show();
        if (handle_IR(wait/3)) return;
    }

    // Slightly different, this makes the rainbow equally distributed throughout
    void rainbowCycle(uint8_t wait) {
        uint16_t j;

        for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    #if 0
        uint16_t i;
    	for(i=0; i< STRIP_NUM_LEDS; i++) {
    	    leds_setcolor(i, Wheel(((i * 256 / STRIP_NUM_LEDS) + j) & 255));
    	}
    #endif
    	fill_rainbow( leds, STRIP_NUM_LEDS, gHue, 7);
    	add1DGlitter(80);
    	gHue++;
    	leds_show();
    	if (handle_IR(wait/5)) return;
        }
    }




    // The animations below are from Adafruit_NeoPixel/examples/strandtest
    // Fill the dots one after the other with a color
    void colorWipe(uint32_t c, uint8_t wait) {
        for(uint16_t i=0; i<STRIP_NUM_LEDS; i++) {
    	leds_setcolor(i, c);
    	leds_show();
    	if (handle_IR(wait/5)) return;
        }
    }

    #if 0
    void rainbow(uint8_t wait) {
        uint16_t i, j;

        for(j=0; j<256; j++) {
    	for(i=0; i<STRIP_NUM_LEDS; i++) {
    	    leds_setcolor(i, Wheel((i+j) & 255));
    	}
    	leds_show();
    	if (handle_IR(wait)) return;
        }
    }
    #endif

    //Theatre-style crawling lights.
    void theaterChase(uint32_t c, uint8_t wait) {
        for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    	for (int q=0; q < 3; q++) {
    	    for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
    		leds_setcolor(i+q, c);    //turn every third pixel on
    	    }
    	    leds_show();

    	    if (handle_IR(wait)) return;

    	    fadeall(16);
    #if 0
    	    for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
    		leds_setcolor(i+q, 0);        //turn every third pixel off
    	    }
    #endif
    	}
        }
    }

    //Theatre-style crawling lights with rainbow effect
    void theaterChaseRainbow(uint8_t wait) {
        for (int j=0; j < 256; j+=7) {     // cycle all 256 colors in the wheel
    	for (int q=0; q < 3; q++) {
    	    for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
    		leds_setcolor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
    	    }
    	    leds_show();

    	    if (handle_IR(wait)) return;

    	    fadeall(16);
    #if 0
    	    for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
    		leds_setcolor(i+q, 0);        //turn every third pixel off
    	    }
    #endif
    	}
        }
    }


    // Local stuff I wrote
    // Flash 25 colors in the color wheel
    void flash(uint8_t wait) {
        uint16_t i, j;

        for(j=0; j<26; j++) {
    	for(i=0; i< STRIP_NUM_LEDS; i++) {
    	    leds_setcolor(i, Wheel(j * 10));
    	}
    	leds_show();
    	if (handle_IR(wait*3)) return;
    	for(i=0; i< STRIP_NUM_LEDS; i++) {
    	    leds_setcolor(i, 0);
    	}
    	leds_show();
    	if (handle_IR(wait*3)) return;
        }
    }

    #if 0
    // Flash different color on every other led
    // Not currently called, looks too much like TheatreRainbow
    void flash2(uint8_t wait) {
        uint16_t i, j;

        for(j=0; j<52; j++) {
    	for(i=0; i< STRIP_NUM_LEDS-1; i+=2) {
    	    leds_setcolor(i, Wheel(j * 5));
    	    leds_setcolor(i+1, 0);
    	}
    	leds_show();
    	if (handle_IR(wait*2)) return;

    	for(i=1; i< STRIP_NUM_LEDS-1; i+=2) {
    	    leds_setcolor(i, Wheel(j * 5 + 48));
    	    leds_setcolor(i+1, 0);
    	}
    	leds_show();
    	if (handle_IR(wait*2)) return;
        }
    }

    // Flash different colors on every other 2 out of 3 leds
    // not a great demo, really...
    void flash3(uint8_t wait) {
        uint16_t i, j;

        for(j=0; j<52; j++) {
    	for(i=0; i< STRIP_NUM_LEDS; i+=3) {
    	    leds_setcolor(i, Wheel(j * 5));
    	    leds_setcolor(i+1, Wheel(j * 5+128));
    	    leds_setcolor(i+2, 0);
    	}
    	leds_show();
    	if (handle_IR(wait)) return;

    	for(i=1; i< STRIP_NUM_LEDS-2; i+=3) {
    	    leds_setcolor(i, Wheel(j * 5 + 48));
    	    leds_setcolor(i+1, Wheel(j * 5+128));
    	    leds_setcolor(i+2, 0);
    	}
    	leds_show();
    	if (handle_IR(wait)) return;

    	for(i=2; i< STRIP_NUM_LEDS-2; i+=3) {
    	    leds_setcolor(i, Wheel(j * 5 + 96));
    	    leds_setcolor(i+1, Wheel(j * 5+128));
    	    leds_setcolor(i+2, 0);
    	}
    	leds_show();
    	if (handle_IR(wait)) return;
        }
    }
    #endif
#endif // NEOPIXEL_PIN

void loop() {
    #ifdef NEOPIXEL_PIN
	switch (nextdemo) {
	// Colors on DIY1-3
	case f_colorWipe:
	    colorDemo = true;
	    colorWipe(demo_color, strip_speed);
	    break;
	case f_theaterChase:
	    colorDemo = true;
	    theaterChase(demo_color, strip_speed);
	    break;

	// Rainbow anims on DIY4-6
    // This is not cool/wavy enough, the cycle version is, though
    //     case f_rainbow:
    // 	colorDemo = false;
    // 	rainbow(strip_speed);
    // 	break;
	case f_rainbowCycle:
	    colorDemo = false;
	    rainbowCycle(strip_speed);
	    break;
	case f_theaterChaseRainbow:
	    colorDemo = false;
	    theaterChaseRainbow(strip_speed);
	    break;

	case f_doubleConvergeRev:
	    colorDemo = false;
	    doubleConverge(false, strip_speed, true);
	    break;

	// Jump3 to Jump7
	case f_cylon:
	    colorDemo = false;
	    cylon(false, strip_speed);
	    break;
	case f_cylonTrail:
	    colorDemo = false;
	    cylon(true, strip_speed);
	    break;
	case f_doubleConverge:
	    colorDemo = false;
	    doubleConverge(false, strip_speed, false);
	    break;
	case f_doubleConvergeTrail:
	    colorDemo = false;
	    doubleConverge(false, strip_speed, false);
	    doubleConverge(true, strip_speed, false);
	    break;

	// Flash color wheel
	case f_flash:
	    colorDemo = false;
	    flash(strip_speed);
	    break;

    #if 0
	case f_flash3:
	    colorDemo = false;
	    flash3(strip_speed);
	    break;
    #endif
	case f_juggle:
	    colorDemo = false;
	    juggle(strip_speed);
	    break;

	case f_bpm:
	    colorDemo = false;
	    bpm(strip_speed);
	    break;

	default:
	    break;
	}

	#if 0
	if ((uint8_t) nextdemo > 0) {
	    Serial.print("Running new demo: ");
	    Serial.println((uint8_t) nextdemo);
	    Serial.print(" at speed ");
	} else {
	    Serial.print("Loop done, restarting demo at speed ");
	}
	Serial.println(strip_speed);
	#endif

	EVERY_N_MILLISECONDS(40) {
	    gHue++;  // slowly cycle the "base color" through the rainbow
	}
    #else // NEOPIXEL_PIN
    // Force the matrix update code to run if the matrix is being updated
    // without 1D neopixel effects that would otherwise call handle_IR to slow
    // themselves down and as a side effect cause a matrix display update
    handle_IR(MX_UPD_TIME);
    #endif // NEOPIXEL_PIN

    sublime_loop();
}



void setup() {
    // Time for serial port to work?
    delay(1000);
    Serial.begin(115200);
    Serial.println("Hello World");
#ifdef ESP8266
    Serial.println("Init ESP8266");
    // Turn off Wifi
    // https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
    WiFi.forceSleepBegin();                  // turn off ESP8266 RF
    // No blink13 in ESP8266 lib
#else // ESP8266
    // this doesn't exist in the ESP8266 IR library, but by using pin D4
    // IR receive happens to make the system LED blink, so it's all good
#ifdef ESP32
    Serial.println("Init ESP32, set IR receive NOT to blink system LED");
    irrecv.blink13(false);
#else
    Serial.println("Init NON ESP8266/ESP32, set IR receive to blink system LED");
    irrecv.blink13(true);
#endif
#endif // ESP8266

#ifdef NEOPIXEL_PIN
    Serial.print("Using FastLED on pin ");
    Serial.print(NEOPIXEL_PIN);
    Serial.print(" to drive LEDs: ");
    Serial.println(STRIP_NUM_LEDS);
    FastLED.addLeds<NEOPIXEL,NEOPIXEL_PIN>(leds, STRIP_NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(led_brightness);
    // Turn off all LEDs first, and then light 3 of them for debug.
    leds_show();
    delay(1000);
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Blue;
    leds[2] = CRGB::Green;
    leds[10] = CRGB::Blue;
    leds[20] = CRGB::Green;
    leds_show();
    Serial.println("LEDs on");
    delay(1000);
#endif // NEOPIXEL_PIN

    Serial.println("Init SmartMatrix");
    // Leave enough RAM for other code.
    // lsbMsbTransitionBit of 2 requires 12288 RAM, 39960 available, leaving 27672 free: 
    // Raised lsbMsbTransitionBit to 2/7 to fit in RAM
    // lsbMsbTransitionBit of 2 gives 100 Hz refresh, 120 requested: 
    // lsbMsbTransitionBit of 3 gives 191 Hz refresh, 120 requested: 
    // Raised lsbMsbTransitionBit to 3/7 to meet minimum refresh rate
    // Descriptors for lsbMsbTransitionBit 3/7 with 16 rows require 6144 bytes of DMA RAM
    // SmartMatrix Mallocs Complete
    // Heap/32-bit Memory Available: 181472 bytes total,  85748 bytes largest free block
    // 8-bit/DMA Memory Available  :  95724 bytes total,  39960 bytes largest free block
    matrix_setup(25000);
    Serial.println("Init SPIFFS/FFat");
    sav_setup();
    Serial.println("Init Aurora");
    aurora_setup();
    Serial.println("Init TwinkleFox");
    twinklefox_setup();
    Serial.println("Init Fireworks");
    fireworks_setup();
    Serial.println("Init sublime");
    sublime_setup();
    Serial.println("Enabling IRin");
    irrecv.enableIRIn(); // Start the receiver
    Serial.print("Enabled IRin on pin ");
    Serial.println(RECV_PIN);

    Serial.print("Last Playable Demo Index: ");
    Serial.println(demo_cnt);

    Serial.print("Matrix Size: ");
    Serial.print(mw);
    Serial.print(" ");
    Serial.println(mh);
    // Turn off dithering https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
    FastLED.setDither( 0 );
    // Set brightness as appropriate for backend
    change_brightness( 0 );
    matrix->setTextWrap(false);
    Serial.println("NeoMatrix Test");
    // speed test
    // I only get 50fps with SmartMatrix 96x64, but good enough I guess
    // while (1) { display_stats(); yield();};
    // init first matrix demo
    display_stats();
    delay(3000);
    matrix->clear();

    Serial.println("Matrix Libraries Test done");
    //font_test();

    // init first strip demo
#ifdef NEOPIXEL_PIN
    colorWipe(0x0000FF00, 10);
    Serial.println("Neopixel strip init done");
#endif // NEOPIXEL_PIN

    Serial.println("Starting loop");
}

// vim:sts=4:sw=4
