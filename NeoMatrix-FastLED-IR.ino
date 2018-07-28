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

// Compile WeMos D1 R2 & mini

#include "config.h"
#include "Table_Mark_Estes.h"
#include "PacMan.h"
#include "FireWorks2.h"
#include "Sublime_Demos.h"
#include "aurora.h"

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

#define MATRIXPIN D6

// How many ms used for each matrix update
#define MX_UPD_TIME 10


uint8_t matrix_state = 0;
// controls how many times a demo should run its pattern
// init at -1 to indicate that a demo is run for the first time (demo switch)
int16_t matrix_loop = -1;


//---------------------------------------------------------------------------- 

#define NUM_LEDS 48

#include <IRremoteESP8266.h>

// This file contains codes I captured and mapped myself
// using IRremote's examples/IRrecvDemo
#include "IRcodes.h"

IRrecv irrecv(RECV_PIN);

CRGB leds[NUM_LEDS];

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

uint16_t Color24toColor16(uint32_t color) {
  return ((uint16_t)(((color & 0xFF0000) >> 16) & 0xF8) << 8) |
         ((uint16_t)(((color & 0x00FF00) >>  8) & 0xFC) << 3) |
                    (((color & 0x0000FF) >>  0)         >> 3);
}

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


void matrix_show() {
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
    FastLED[1].showLeds(matrix_brightness);
#ifdef ESP8266
    //ESP.wdtEnable(1000);
#endif
    //matrix->show();
}

void matrix_clear() {
    //FastLED[1].clearLedData();
    // clear does not work properly with multiple matrices connected via parallel inputs
    memset(matrixleds, 0, NUMMATRIX*3);
}


void display_resolution() {
    static uint16_t cnt=1;

    matrix->setTextSize(1);
    // not wide enough;
    if (mw<16) return;
    matrix_clear();
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
	    matrix_clear();
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
    matrix_clear();

    matrix->setFont(&FreeMonoBold12pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    delay(1000);
    matrix_clear();

    matrix->setFont(&FreeMonoBold18pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    delay(1000);
    matrix_clear();

    matrix->setFont(&FreeMonoBold24pt7b);
    matrix->setCursor(-1, 31);
    matrix->print("T");
    matrix_show();
    matrix_clear();
    delay(1000);
#endif

    matrix_clear();
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
    uint8_t repeat = 1;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 1;
	spd = 1.0;
	startfade = -1;
    }

    matrix->setRotation(0);
    matrix->setTextSize(2);
#ifndef NOFONTS
    matrix->setFont( &Century_Schoolbook_L_Bold[9] );


    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(0, 26);
	matrix->setTextColor(matrix->Color(255,0,0));
	matrix_clear();
	matrix->print("T");
	startfade = l;
    }
    l++;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(0, 26);
	matrix->setTextColor(matrix->Color(192,192,0)); 
	matrix_clear();
	matrix->print("F");
	startfade = l;
    }
    l++;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(4, 32);
	matrix->setTextColor(matrix->Color(0,192,192));
	matrix_clear();
	matrix->print("S");
	startfade = l;
    }
    l++;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(4, 32);
	matrix->setTextColor(matrix->Color(0,255,0));
	matrix_clear();
	matrix->print("F");
	startfade = l;
    }
    l++;
#endif

#if 0
    if (startfade < l && (state > (l*duration)/spd))  {
	matrix->setCursor(1, 29);
	matrix->setTextColor(matrix->Color(0,0,255));
	matrix_clear();
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
    static uint16_t state;
    static uint16_t direction;
    static uint16_t size;
    static uint8_t l;
    static int16_t faster = 0;
    static bool dont_exit;
    static uint16_t delayframe;
    char letters[] = { 'T', 'F', 'S', 'F' };
    bool done = 0;
    uint8_t repeat = 4;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	state = 1;
	direction = 1;
	size = 3;
	l = 0;
	if (matrix_loop == -1) { dont_exit = 1; delayframe = 2; faster = 0; };
    }

    matrix->setTextSize(1);

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	// Serial.println("delayed frame");
	matrix_show(); // make sure we still run at the same speed.
	return repeat;
    }
    delayframe = max((speed / 10) - faster , 1);
    // before exiting, we run the full delay to show the last frame long enough
    if (dont_exit == 0) { dont_exit = 1; return 0; }
    if (direction == 1) {
	int8_t offset = 0; // adjust some letters left or right as needed
	if (letters[l] == 'T') offset = -2 * size/15;
	if (letters[l] == '8') offset = 2 * size/15;

	uint16_t txtcolor = Color24toColor16(Wheel(map(letters[l], '0', 'Z', 0, 255)));
	matrix->setTextColor(txtcolor); 

	matrix_clear();
#ifndef NOFONTS
	matrix->setFont( &Century_Schoolbook_L_Bold[size] );
#endif
	matrix->setCursor(10-size*0.55+offset, 17+size*0.75);
	matrix->print(letters[l]);
	if (size<18) size++; 
	else if (zoom_type == 0) { done = 1; delayframe = max((speed - faster*10) * 1, 3); } 
	     else direction = 2;

    } else if (zoom_type == 1) {
	int8_t offset = 0; // adjust some letters left or right as needed
	uint16_t txtcolor = Color24toColor16(Wheel(map(letters[l], '0', 'Z', 255, 0)));
	if (letters[l] == 'T') offset = -2 * size/15;
	if (letters[l] == '8') offset = 2 * size/15;

	matrix->setTextColor(txtcolor); 
	matrix_clear();
#ifndef NOFONTS
	matrix->setFont( &Century_Schoolbook_L_Bold[size] );
#endif
	matrix->setCursor(10-size*0.55+offset, 17+size*0.75);
	matrix->print(letters[l]);
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

uint8_t esrr() {
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
	matrix_clear();
	state = 1;
	spd = 1.0;
    }

    matrix->setFont(&TomThumb);
    matrix->setRotation(0);
    matrix->setTextSize(1);
    matrix_clear();


    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	matrix->setCursor(7, 6);
	matrix->setTextColor(matrix->Color(255,0,0));
	matrix->print("EAT");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	matrix->setCursor(3, 14);
	matrix->setTextColor(matrix->Color(192,192,0)); 
	matrix->print("SLEEP");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	matrix->setCursor(5, 22);
	matrix->setTextColor(matrix->Color(0,255,0));
	matrix->print("RAVE");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || state < overlap/spd) || spd > displayall)  {
	matrix->setCursor(0, 30);
	matrix->setTextColor(matrix->Color(0,192,192));
	matrix->print("REPEAT");
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
    matrix_clear();

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

// FIXME: still get some display problems on fade
uint8_t esrr_fade() {
    static uint16_t state;
    static uint8_t wheel;
    static uint8_t sp;
    static float spd;
    float spdincr = 0.5;
    uint8_t resetspd = 5;
    uint16_t txtcolor;


    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 0;
	wheel = 0;
	sp = 0;
	spd = 1.0;
    }

    state++;

    if (state == 1) {
	//wheel+=20;
	matrix->setFont(&TomThumb);
	matrix->setRotation(0);
	matrix->setTextSize(1);
	matrix_clear();

	matrix->setCursor(7, 6);
	txtcolor = Color24toColor16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	matrix->setTextColor(txtcolor);
	matrix->print("EAT");
	matrix->setCursor(3, 14);
	txtcolor = Color24toColor16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	matrix->setTextColor(txtcolor);
	matrix->print("SLEEP");
	matrix->setCursor(5, 22);
	txtcolor = Color24toColor16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	matrix->setTextColor(txtcolor);
	matrix->print("RAVE");
	matrix->setCursor(0, 30);
	txtcolor = Color24toColor16(Wheel((wheel+=24)));
        //Serial.println(txtcolor, HEX);
	matrix->setTextColor(txtcolor);
	matrix->print("REPEAT");
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


uint8_t squares(bool reverse) {
#define sqdelay 2
    static uint16_t state;
    static uint8_t wheel;
    uint8_t repeat = 3;
    static uint16_t delayframe = sqdelay;


    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 0;
	wheel = 0;
    }

    uint8_t x = mw/2-1;
    uint8_t y = mh/2-1;
    uint8_t maxsize = max(mh/2,mw/2);

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	matrix_show(); // make sure we still run at the same speed.
	return repeat;
    }
    delayframe = sqdelay;
    state++;
    wheel += 10;

    matrix_clear();
    if (reverse) {
	for (uint8_t s = maxsize; s >= 1 ; s--) {
	    matrix->drawRect( mw/2-s, mh/2-s, s*2, s*2, Color24toColor16(Wheel(wheel+(maxsize-s)*10)));
	}
    } else {
	for (uint8_t s = 1; s <= maxsize; s++) {
	    matrix->drawRect( mw/2-s, mh/2-s, s*2, s*2, Color24toColor16(Wheel(wheel+s*10)));
	}
    }

    // Serial.print("state ");
    // Serial.println(state);
    if (state > 400) {
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
    uint16_t txtcolor;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 1;
	spd = 1.0;
	didclear = 0;
	firstpass = 0;
    }

    matrix->setFont(&TomThumb);
    matrix->setRotation(0);
    matrix->setTextSize(1);
    if (! didclear) {
	matrix_clear();
	didclear = 1;
    }

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	matrix->setCursor(5, 6);
	txtcolor = Color24toColor16(Wheel(map(l, 0, 5, 0, 255)));
	matrix->setTextColor(txtcolor); 
	matrix->print("WITH");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	firstpass = 1;
	matrix->setCursor(3, 12);
	txtcolor = Color24toColor16(Wheel(map(l, 0, 5, 0, 255)));
	matrix->setTextColor(txtcolor); 
	matrix->print("EVERY");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	matrix->setCursor(5, 18);
	txtcolor = Color24toColor16(Wheel(map(l, 0, 5, 0, 255)));
	matrix->setTextColor(txtcolor); 
	matrix->print("BEAT");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
	matrix->setCursor(2, 24);
	txtcolor = Color24toColor16(Wheel(map(l, 0, 5, 0, 255)));
	matrix->setTextColor(txtcolor); 
	matrix->print("WE ARE");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || (state < overlap/spd && firstpass)) || spd > displayall)  {
	matrix->setCursor(0, 30);
	txtcolor = Color24toColor16(Wheel(map(l, 0, 5, 0, 255)));
	matrix->setTextColor(txtcolor); 
	matrix->print("CLOSER");
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


uint8_t scrollText(char str[], uint8_t len) {
    static uint8_t wheel;
    static int16_t x;

    uint8_t repeat = 1;
    char chr[] = " ";
    int8_t fontsize = 14; // real height is twice that.
    int8_t fontwidth = 16;
    uint8_t size = max(int(mw/8), 1);
#define stdelay 1
    static uint16_t delayframe = stdelay;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	wheel = 0;
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
	matrix_show(); // make sure we still run at the same speed.
	return repeat;
    }
    delayframe = stdelay;

    matrix_clear();
    matrix->setCursor(x, 24);
    for (uint8_t c=0; c<len; c++) {
	uint16_t txtcolor = Color24toColor16(Wheel(map(c, 0, len, 0, 512)));
	matrix->setTextColor(txtcolor); 
	//Serial.print(txtcolor, HEX);
	//Serial.print(" >");
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

// Scroll within big bitmap so that all if it becomes visible or bounce a small one.
// If the bitmap is bigger in one dimension and smaller in the other one, it will
// be both panned and bounced in the appropriate dimensions.
uint8_t panOrBounceBitmap (uint8_t bitmapnum, uint8_t bitmapSize) {
    static uint16_t state;
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

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 0;
	xf = max(0, (mw-bitmapSize)/2) << 4;
	yf = max(0, (mh-bitmapSize)/2) << 4;
	xfc = 6;
	yfc = 3;
	xfdir = -1;
	yfdir = -1;
    }


    bool updDir = false;

    // Get actual x/y by dividing by 16.
    int16_t x = xf >> 4;
    int16_t y = yf >> 4;

    matrix_clear();
    // pan 24x24 pixmap
    matrix->drawRGBBitmap(x, y, (const uint16_t *) bitmap24, bitmapSize, bitmapSize);
    matrix_show();
     
    // Only pan if the display size is smaller than the pixmap
    // but not if the difference is too small or it'll look bad.
    if (bitmapSize-mw>2) {
	xf += xfc*xfdir;
	if (xf >= 0)                      { xfdir = -1; updDir = true ; };
	// we don't go negative past right corner, go back positive
	if (xf <= ((mw-bitmapSize) << 4)) { xfdir = 1;  updDir = true ; };
    }
    if (bitmapSize-mh>2) {
	yf += yfc*yfdir;
	// we shouldn't display past left corner, reverse direction.
	if (yf >= 0)                      { yfdir = -1; updDir = true ; };
	if (yf <= ((mh-bitmapSize) << 4)) { yfdir = 1;  updDir = true ; };
    }
    // only bounce a pixmap if it's smaller than the display size
    if (mw>bitmapSize) {
	xf += xfc*xfdir;
	// Deal with bouncing off the 'walls'
	if (xf >= (mw-bitmapSize) << 4) { xfdir = -1; updDir = true ; };
	if (xf <= 0)           { xfdir =  1; updDir = true ; };
    }
    if (mh>bitmapSize) {
	yf += yfc*yfdir;
	if (yf >= (mh-bitmapSize) << 4) { yfdir = -1; updDir = true ; };
	if (yf <= 0)           { yfdir =  1; updDir = true ; };
    }
    
    if (updDir) {
	// Add -1, 0 or 1 but bind result to 1 to 1.
	// Let's take 3 is a minimum speed, otherwise it's too slow.
	xfc = constrain(xfc + random(-1, 2), 3, 16);
	yfc = constrain(xfc + random(-1, 2), 3, 16);
    }
    if (state++ == 600) { 
	matrix_reset_demo = 1;
	return 0; 
    }
    return 3;
}

// FIXME: reset decoding counter to 0 between different GIFS?
uint8_t GifAnim(char *fn, uint16_t frames) {
    #define gifloop (450 * frames)
    //#define gifloop (10 * frames)
    static uint32_t gifanimloop = gifloop;
    static uint16_t delayframe = 2;
    uint8_t repeat = 1;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	// exit if the gif animation couldn't get setup.
	if (sav_newgif(fn)) return 0;
    }

    if (--delayframe) {
	// Serial.println("delayed frame");
	matrix_show(); // make sure we still run at the same speed.
	return repeat;
    }
    delayframe = 1;

    // simpleanimviewer may or may not run show() depending on whether
    // it's time to decode the next frame.
    sav_loop();
    // So we run it unconditionally, here, which adds the requested timing slowdown
    matrix_show();
    if (gifanimloop-- == 0) {
	gifanimloop = gifloop;
	return 0;
    }
    return repeat;
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
    static uint16_t delayframe = demoreeldelay;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 0;
    }

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	matrix_show(); // make sure we still run at the same speed.
	return repeat;
    }
    delayframe = demoreeldelay;
    state++;
    gHue++;

    if (state < 2000)
    {
	if (demo == 1) {
	    // random colored speckles that blink in and fade smoothly
	    fadeToBlackBy( matrixleds, NUMMATRIX, 10);
	    int pos = random16(NUMMATRIX);
	    matrixleds[pos] += CHSV( gHue + random8(64), 200, 255);
	}
	if (demo == 2) {
	    // a colored dot sweeping back and forth, with fading trails
	    fadeToBlackBy( matrixleds, NUMMATRIX, 20);
	    int pos = beatsin16( 13, 0, NUMMATRIX-1 );
	    matrixleds[pos2matrix(pos)] += CHSV( gHue, 255, 192);
	}
	if (demo == 3) {
	    // eight colored dots, weaving in and out of sync with each other
	    fadeToBlackBy( matrixleds, NUMMATRIX, 20);
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


// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing, widely-varying set of parameters.
uint8_t pride()
{
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	state = 0;
    }


    uint8_t sat8 = beatsin88( 87, 220, 250);
    uint8_t brightdepth = beatsin88( 341, 200, 250);
    uint16_t brightnessthetainc16;
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    brightnessthetainc16 = beatsin88( map(speed,1,255,150,475), (20 * 256), (40 * 256));

    uint16_t hue16 = sHue16;//gHue * 256;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis ;
    sLastMillis  = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88( 400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for ( uint16_t i = 0 ; i < NUMMATRIX; i++) {
	hue16 += hueinc16;
	uint8_t hue8 = hue16 / 256;

	brightnesstheta16  += brightnessthetainc16;
	uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

	uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
	uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
	bri8 += (255 - brightdepth);

	CRGB newcolor = CHSV( hue8, sat8, bri8);

	nblend( matrixleds[matrix->XY(i/mh, i%mh)], newcolor, 64);
    }
    
    matrix_show();
    if (state++ < 1000) return 1;
    matrix_reset_demo = 1;
    return 0;
}


uint8_t call_fireworks() {
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 0;
    }

    fireworks();
    matrix_show();
    if (state++ < 3000) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_fire() {
    static uint16_t state;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix_clear();
	state = 0;
    }

    fire();
    matrix_show();
    if (state++ < 3000) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_rain(uint8_t which) {
    #define raindelay 4
    static uint16_t state;
    static uint16_t delayframe = raindelay;

    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	state = 0;
    }

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	//Serial.print("delayed frame ");
	//Serial.println(delayframe);
	matrix_show(); // make sure we still run at the same speed.
	return 1;
    }
    delayframe = raindelay;

    if (which == 1) theMatrix();
    if (which == 2) coloredRain();
    if (which == 3) stormyRain();
    matrix_show();
    if (state++ < 500) return 1;
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
	matrix_show(); // make sure we still run at the same speed.
	return 1;
    }
    delayframe = pacmandelay;

    if (pacman_loop()) return 1;
    matrix_reset_demo = 1;
    return 0;
}

// Adapted from	LEDText/examples/TextExample3 by Aaron Liddiment
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
    if (state++ < 3000) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t metd(uint8_t demo, uint8_t dfinit, uint16_t loops) {
    static uint8_t delayframe = dfinit;

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
	    bfade = 10;
	    break;
	}
	matrix_clear();
    }

    if (--delayframe) {
	matrix_show(); // make sure we still run at the same speed.
	return 1;
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
    case 36: // unused, circles of color, too similar to 34
	Raudio5();
	break;
    case 37: // unused, other kinds of loose colored pixels, too close to 29
	Raudio();
        adjuster();
	break;
    case 52:
	rmagictime();
	bkboxer();
	starer();
	if (flip && !flip2) adjuster();
	break;
    case 61: // unused, too similar to 52
	starer();
	bkboxer();
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
    if (counter < loops) return 1;
    matrix_reset_demo = 1;
    return 0;
}

extern uint8_t aurora(uint8_t item);


#define LAST_MATRIX 54
void matrix_change(int demo) {
    // Reset passthrough from previous demo
    matrix->setPassThruColor();
    // this ensures the next demo returns the number of times it should loop
    matrix_loop = -1;
    matrix_reset_demo = 1;
    if (demo==-128) if (matrix_state-- == 0) matrix_state = LAST_MATRIX;
    if (demo==+127) if (matrix_state++ == LAST_MATRIX) matrix_state = 0;
    // If existing matrix was already >90, any +- change brings it back to 0.
    if (matrix_state > 90) matrix_state = 0;
    if (demo >= 0 && demo < 127) matrix_state = demo;
    // Special one key press demos are shown once and next goes back to the normal loop
    if (demo >= 0 && demo < 90) matrix_loop = 9999;
    Serial.print("Got matrix_change ");
    Serial.print(demo);
    Serial.print(", switching to matrix demo ");
    Serial.print(matrix_state);
    Serial.print(" loop ");
    Serial.println(matrix_loop);
}

void matrix_update() {
    uint8_t ret;

    EVERY_N_MILLISECONDS(40) {
	gHue++;  // slowly cycle the "base color" through the rainbow
    }
    sublime_loop();
    // reset passthrough color set by some demos
    matrix->setPassThruColor();

    switch (matrix_state) {
	case 0: 
	    ret = squares(0);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 1: 
	    ret = squares(1);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    matrix_state = 2;
	    break;

	case 2: 
	    ret = esrr();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 3: 
	    ret = metd(52, 5, 300); // rectangles/squares/triangles zooming out
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 4: 
	    ret = tfsf_zoom(0, 30);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 5: 
	    ret = metd(77, 5, 300); // streaming lines of colored pixels with shape zooming in or out
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 6: 
	    ret = GifAnim("/gifs/32anim_photon.gif", 44);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 7: 
	    ret = panOrBounceBitmap(1, 24);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 8: 
	    ret = GifAnim("/gifs/32anim_flower.gif", 30);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 9: 
	    ret = tfsf();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 10: 
	    ret = metd(80, 5, 200); // rotating triangles of color
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 11: 
	    ret = GifAnim("/gifs/32anim_balls.gif", 38);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 12: 
	    ret = demoreel100(1); // Twinlking stars
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 13: 
	    ret = call_fireworks();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 14: 
	    ret = demoreel100(2); // color changing pixels sweeping up and down
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 15: 
	    ret = pride();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 16: 
	    ret = metd(105, 2, 400); // larger changing colors of hypno patterns
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 17: 
	    ret = call_rain(1); // matrix
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 18: 
	    ret = call_pacman(3);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 19: 
	    ret = plasma();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 20: 
	    ret = metd(110, 5, 300); // bubbles going up or right
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 21: 
	    ret = demoreel100(3); // colored pixels being exchanged between top and bottom
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 22: 
	    ret = metd(10, 5, 300); // 5 color windows-like pattern with circles in and out
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 23:  
	    ret = metd(11, 5, 300);// color worm patterns going out with circles zomming out
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 24: 
	    ret = tfsf_zoom(1, 30);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 25:  // audio:  colored pixels in a loose circle
	    ret = metd(29, 5, 300);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 26: // audio: colored lines streaming from center
	    ret = webwc();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 27: 
	    ret = call_rain(3); // clouds, rain, lightening
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 28: 
	    ret = metd(25, 3, 500);// 5 circles turning together, run a bit longer
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 29: 
	    ret = esrr_fade();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 30: 
	    ret = call_fire();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 31: 
	    ret = metd(67, 5, 900); // two colors swirling bigger, creating hypno pattern
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	//case 32-44: // see default
	default:
	    ret = aurora(matrix_state-32);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 45: 
	    ret = GifAnim("/gifs/32anim_dance.gif", 100);  // 277
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 46: 
	    ret = GifAnim("/gifs/circles_swap.gif", 16);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 47: 
	    ret = GifAnim("/gifs/concentric_circles.gif", 40); // 20
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 48: 
	    ret = GifAnim("/gifs/corkscrew.gif", 29);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 49: 
	    ret = GifAnim("/gifs/cubeconstruct.gif", 30); // 76
	    if (matrix_loop == -1) matrix_loop = ret; 
	    if (ret) return;
	    break;

	case 50: 
	    ret = GifAnim("/gifs/cubeslide.gif", 30); // 272
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 51: 
	    ret = GifAnim("/gifs/runningedgehog.gif", 24); // 8
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 52: 
	    ret = GifAnim("/gifs/triangles_in.gif", 48);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 53: 
	    ret = GifAnim("/gifs/wifi.gif", 50); //254
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case LAST_MATRIX: 
	    ret = metd(34, 5, 300); // single colored lines that extend from center.
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 99: 
	    char str[] = "Thank You :)";
	    ret = scrollText(str, sizeof(str));
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;


    } 
    matrix_reset_demo = 1;
    Serial.print("Done with demo ");
    Serial.print(matrix_state);
    Serial.print(" loop ");
    Serial.println(matrix_loop);
    if (matrix_loop-- > 0) return;
    matrix_change(127);
}

// ---------------------------------------------------------------------------
// Strip Code
// ---------------------------------------------------------------------------

void leds_show() {
    FastLED[0].showLeds(led_brightness);
}
void leds_setcolor(uint16_t i, uint32_t c) {
    leds[i] = c;
}

void change_brightness(int8_t change) {
    static uint8_t brightness = 5;
    static uint32_t last_brightness_change = 0 ;

    if (millis() - last_brightness_change < 300) {
	Serial.print("Too soon... Ignoring brightness change from ");
	Serial.println(brightness);
	return;
    }
    last_brightness_change = millis();
    brightness = constrain(brightness + change, 3, 8);
    led_brightness = min((1 << (brightness+1)) - 1, 255);
    matrix_brightness = (1 << (brightness-1)) - 1;

    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" led value ");
    Serial.print(led_brightness);
    Serial.print(" matrix value ");
    Serial.println(matrix_brightness);
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

bool is_change() {
    uint32_t newmil = millis();
    // Any change after next button acts as a pattern change for 5 seconds
    // (actually more than 5 secs because millis stops during panel refresh)
    if (newmil - last_change < 5000) {
	last_change = newmil;
	return 1;
    }
    return 0;
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
	    change_brightness(+1);
	    Serial.println("Got IR: Bright");
	    return 1;

	case IR_RGBZONE_DIM:
	    change_brightness(-1);
	    Serial.println("Got IR: Dim");
	    return 1;

	case IR_RGBZONE_NEXT:
	    last_change = millis();
	    matrix_change(127);
	    Serial.print("Got IR: Next to matrix state ");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_POWER:
	    if (is_change()) { matrix_change(-128); return 1; }
	    nextdemo = f_colorWipe;
	    demo_color = 0x000000;
	    strip_speed = 1;
	    Serial.println("Got IR: Power");
	    Serial.println("Hit slower speed to restart panel anim");
	    return 1;

	case IR_RGBZONE_QUICK:
	    if (is_change()) { ; return 1; }
	    change_speed(-10);
	    Serial.println("Got IR: Quick");
	    return 1;

	case IR_RGBZONE_SLOW:
	    change_speed(+10);
	    Serial.println("Got IR: Slow");
	    return 1;


	case IR_RGBZONE_RED:
	    Serial.println("Got IR: Red (1)");
	    if (is_change()) { matrix_change(1); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF0000;
	    return 1;

	case IR_RGBZONE_GREEN:
	    Serial.println("Got IR: Green (2)");
	    if (is_change()) { matrix_change(2); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00FF00;
	    return 1;

	case IR_RGBZONE_BLUE:
	    Serial.println("Got IR: Blue (3)");
	    if (is_change()) { matrix_change(3); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x0000FF;
	    return 1;

	case IR_RGBZONE_WHITE:
	    Serial.println("Got IR: White (4)");
	    if (is_change()) { matrix_change(4); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFFFFF;
	    return 1;



	case IR_RGBZONE_RED2:
	    Serial.println("Got IR: Red2 (5)");
	    if (is_change()) { matrix_change(5); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xCE6800;
	    return 1;

	case IR_RGBZONE_GREEN2:
	    Serial.println("Got IR: Green2 (6)");
	    if (is_change()) { matrix_change(6); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    return 1;

	case IR_RGBZONE_BLUE2:
	    Serial.println("Got IR: Blue2 (7)");
	    if (is_change()) { matrix_change(7); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x0000BB;
	    return 1;

	case IR_RGBZONE_PINK:
	    Serial.println("Got IR: Pink (8)");
	    if (is_change()) { matrix_change(8); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF50FE;
	    return 1;



	case IR_RGBZONE_ORANGE:
	    Serial.println("Got IR: Orange (9)");
	    if (is_change()) { matrix_change(9); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF8100;
	    return 1;

	case IR_RGBZONE_BLUE3:
	    Serial.println("Got IR: Green2 (10)");
	    if (is_change()) { matrix_change(10); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    return 1;

	case IR_RGBZONE_PURPLED:
	    Serial.println("Got IR: DarkPurple (11)");
	    if (is_change()) { matrix_change(11); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x270041;
	    return 1;

	case IR_RGBZONE_PINK2:
	    Serial.println("Got IR: Pink2 (12)");
	    if (is_change()) { matrix_change(12); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFB9FF;
	    Serial.println("Got IR: Pink2");
	    return 1;



	case IR_RGBZONE_ORANGE2:
	    Serial.println("Got IR: Orange2 (13)");
	    if (is_change()) { matrix_change(13); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFCA49;
	    return 1;

	case IR_RGBZONE_GREEN3:
	    Serial.println("Got IR: Green2 (14)");
	    if (is_change()) { matrix_change(14); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x006A00;
	    return 1;

	case IR_RGBZONE_PURPLE:
	    Serial.println("Got IR: DarkPurple2 (15)");
	    if (is_change()) { matrix_change(15); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x2B0064;
	    return 1;

	case IR_RGBZONE_BLUEL:
	    Serial.println("Got IR: BlueLight (16)");
	    if (is_change()) { matrix_change(16); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x50A7FF;
	    return 1;



	case IR_RGBZONE_YELLOW:
	    Serial.println("Got IR: Yellow (17)");
	    if (is_change()) { matrix_change(17); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xF0FF00;
	    return 1;

	case IR_RGBZONE_GREEN4:
	    Serial.println("Got IR: Green2 (18)");
	    if (is_change()) { matrix_change(18); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    return 1;

	case IR_RGBZONE_PURPLE2:
	    Serial.println("Got IR: Purple2 (19)");
	    if (is_change()) { matrix_change(19); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x660265;
	    return 1;

	case IR_RGBZONE_BLUEL2:
	    Serial.println("Got IR: BlueLight2 (20)");
	    if (is_change()) { matrix_change(20); return 1; }
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x408BD8;
	    return 1;


	case IR_RGBZONE_RU:
	    matrix_change(21);
	    Serial.print("Got IR: Red UP switching to matrix state 21");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_RD:
	    matrix_change(22);
	    Serial.print("Got IR: Red DOWN switching to matrix state 22");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_GU:
	    matrix_change(23);
	    Serial.print("Got IR: Green UP switching to matrix state 23");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_GD:
	    matrix_change(24);
	    Serial.print("Got IR: Green DOWN switching to matrix state 24");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_BU:
	    matrix_change(25);
	    Serial.print("Got IR: Blue UP switching to matrix state 25");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_BD:
	    matrix_change(26);
	    Serial.print("Got IR: Blue DOWN switching to matrix state 26");
	    Serial.println(matrix_state);
	    return 1;

	case IR_RGBZONE_DIY1:
	    Serial.println("Got IR: DIY1 (27)");
	    if (is_change()) { matrix_change(27); return 1; }
	    // this uses the last color set
	    nextdemo = f_colorWipe;
	    Serial.println("Got IR: DIY1 colorWipe");
	    return 1;

	case IR_RGBZONE_DIY2:
	    Serial.println("Got IR: DIY2 (28)");
	    if (is_change()) { matrix_change(28); return 1; }
	    // this uses the last color set
	    nextdemo = f_theaterChase;
	    Serial.println("Got IR: DIY2/theaterChase");
	    return 1;

	case IR_RGBZONE_DIY3:
	    Serial.println("Got IR: DIY3 (29)");
	    if (is_change()) { matrix_change(29); return 1; }
	    nextdemo = f_juggle;
	    Serial.println("Got IR: DIY3/Juggle");
	    return 1;

	case IR_RGBZONE_DIY4:
	    Serial.println("Got IR: DIY4 (30)");
	    if (is_change()) { matrix_change(30); return 1; }
	    nextdemo = f_rainbowCycle;
	    Serial.println("Got IR: DIY4/rainbowCycle");
	    return 1;

	case IR_RGBZONE_DIY5:
	    Serial.println("Got IR: DIY5 (31)");
	    if (is_change()) { matrix_change(31); return 1; }
	    nextdemo = f_theaterChaseRainbow;
	    Serial.println("Got IR: DIY5/theaterChaseRainbow");
	    return 1;

	case IR_RGBZONE_DIY6:
	    Serial.println("Got IR: DIY6 (32)");
	    if (is_change()) { matrix_change(32); return 1; }
	    nextdemo = f_doubleConvergeRev;
	    Serial.println("Got IR: DIY6/DoubleConvergeRev");
	    return 1;

	case IR_RGBZONE_AUTO:
	    matrix_change(99);
	    nextdemo = f_bpm;
	    Serial.println("Got IR: AUTO/bpm");
	    return 1;

	case IR_RGBZONE_JUMP3:
	    nextdemo = f_cylon;
	    Serial.println("Got IR: JUMP3/Cylon");
	    return 1;

	case IR_RGBZONE_JUMP7:
	    nextdemo = f_cylonTrail;
	    Serial.println("Got IR: JUMP7/CylonWithTrail");
	    return 1;

	case IR_RGBZONE_FADE3:
	    nextdemo = f_doubleConverge;
	    Serial.println("Got IR: FADE3/DoubleConverge");
	    return 1;

	case IR_RGBZONE_FADE7:
	    nextdemo = f_doubleConvergeTrail;
	    Serial.println("Got IR: FADE7/DoubleConvergeTrail");
	    return 1;

	case IR_RGBZONE_FLASH:
	    nextdemo = f_flash;
	    Serial.println("Got IR: FLASH");
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


// Demos from FastLED

// fade in 0 to x/256th of the previous value
void fadeall(uint8_t fade) {
    for(uint16_t i = 0; i < NUM_LEDS; i++) {  leds[i].nscale8(fade); }
}

void cylon(bool trail, uint8_t wait) {
    static uint8_t hue = 0;
    // First slide the led in one direction
    for(uint16_t i = 0; i < NUM_LEDS; i++) {
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
    for(uint16_t i = (NUM_LEDS)-1; i > 0; i--) {
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
    for(uint16_t i = 0; i < NUM_LEDS/2 + 4; i++) {

	if (i < NUM_LEDS/2) {
	    if (!rev) {
		leds_setcolor(i, Wheel(hue++));
		leds_setcolor(NUM_LEDS - 1 - i, Wheel(hue++));
	    } else {
		leds_setcolor(NUM_LEDS/2 -1 -i, Wheel(hue++));
		leds_setcolor(NUM_LEDS/2 + i, Wheel(hue++));
	    }
	}
#if 0
	if (!trail && i>3) {
	    if (!rev) {
		leds_setcolor(i - 4, 0);
		leds_setcolor(NUM_LEDS - 1 - i + 4, 0);
	    } else {
		leds_setcolor(NUM_LEDS/2 -1 -i +4, 0);
		leds_setcolor(NUM_LEDS/2 + i -4, 0);
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
void addGlitter( fract8 chanceOfGlitter) 
{
    if (random8() < chanceOfGlitter) {
	leds[ random16(NUM_LEDS) ] += CRGB::White;
    }
}


void juggle(uint8_t wait) {
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy( leds, NUM_LEDS, 20);
    byte dothue = 0;
    for( int i = 0; i < 8; i++) {
	leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
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
    for( int i = 0; i < NUM_LEDS; i++) { //9948
	leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
    }
    gHue++;
    leds_show();
    if (handle_IR(wait/3)) return;
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
#if 0
	for(i=0; i< NUM_LEDS; i++) {
	    leds_setcolor(i, Wheel(((i * 256 / NUM_LEDS) + j) & 255));
	}
#endif
	fill_rainbow( leds, NUM_LEDS, gHue, 7);
	addGlitter(80);
	gHue++;
	leds_show();
	if (handle_IR(wait/5)) return;
    }
}




// The animations below are from Adafruit_NeoPixel/examples/strandtest
// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
    for(uint16_t i=0; i<NUM_LEDS; i++) {
	leds_setcolor(i, c);
	leds_show();
	if (handle_IR(wait/5)) return;
    }
}

#if 0
void rainbow(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256; j++) {
	for(i=0; i<NUM_LEDS; i++) {
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
	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
		leds_setcolor(i+q, c);    //turn every third pixel on
	    }
	    leds_show();

	    if (handle_IR(wait)) return;

	    fadeall(16);
#if 0
	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
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
	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
		leds_setcolor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
	    }
	    leds_show();

	    if (handle_IR(wait)) return;

	    fadeall(16);
#if 0
	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
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
	for(i=0; i< NUM_LEDS; i++) {
	    leds_setcolor(i, Wheel(j * 10));
	}
	leds_show();
	if (handle_IR(wait*3)) return;
	for(i=0; i< NUM_LEDS; i++) {
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
	for(i=0; i< NUM_LEDS-1; i+=2) {
	    leds_setcolor(i, Wheel(j * 5));
	    leds_setcolor(i+1, 0);
	}
	leds_show();
	if (handle_IR(wait*2)) return;

	for(i=1; i< NUM_LEDS-1; i+=2) {
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
	for(i=0; i< NUM_LEDS; i+=3) {
	    leds_setcolor(i, Wheel(j * 5));
	    leds_setcolor(i+1, Wheel(j * 5+128));
	    leds_setcolor(i+2, 0);
	}
	leds_show();
	if (handle_IR(wait)) return;

	for(i=1; i< NUM_LEDS-2; i+=3) {
	    leds_setcolor(i, Wheel(j * 5 + 48));
	    leds_setcolor(i+1, Wheel(j * 5+128));
	    leds_setcolor(i+2, 0);
	}
	leds_show();
	if (handle_IR(wait)) return;

	for(i=2; i< NUM_LEDS-2; i+=3) {
	    leds_setcolor(i, Wheel(j * 5 + 96));
	    leds_setcolor(i+1, Wheel(j * 5+128));
	    leds_setcolor(i+2, 0);
	}
	leds_show();
	if (handle_IR(wait)) return;
    }
}
#endif

void loop() {
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
    sublime_loop();
}



void setup() {
    // Time for serial port to work?
    delay(1000);
    Serial.begin(115200);
    Serial.println("Enabling IRin");
    irrecv.enableIRIn(); // Start the receiver
    Serial.print("Enabled IRin on pin ");
    Serial.println(RECV_PIN);
#ifdef ESP8266
    Serial.println("Init ESP8266");
    // Turn off Wifi
    // https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
    WiFi.forceSleepBegin();                  // turn off ESP8266 RF
#else
    // this doesn't exist in the ESP8266 IR library, but by using pin D4
    // IR receive happens to make the system LED blink, so it's all good
    Serial.println("Init NON ESP8266, set IR receive to blink system LED");
    irrecv.blink13(true);
#endif

    Serial.print("Using FastLED on pin ");
    Serial.print(NEOPIXEL_PIN);
    Serial.print(" to drive LEDs: ");
    Serial.println(NUM_LEDS);
    FastLED.addLeds<NEOPIXEL,NEOPIXEL_PIN>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(led_brightness);
    // Turn off all LEDs first, and then light 3 of them for debug.
    leds_show();
    delay(1000);
    leds[0] = CRGB::Red;
    leds[10] = CRGB::Blue;
    leds[20] = CRGB::Green;
    leds_show();
    Serial.println("LEDs on");
    delay(1000);

    aurora_setup();
    sav_setup();

    // Init Matrix
    // Serialized, 768 pixels takes 26 seconds for 1000 updates or 26ms per refresh
    // FastLED.addLeds<NEOPIXEL,MATRIXPIN>(matrixleds, NUMMATRIX).setCorrection(TypicalLEDStrip);
    // https://github.com/FastLED/FastLED/wiki/Parallel-Output
    // WS2811_PORTA - pins 12, 13, 14 and 15 or pins 6,7,5 and 8 on the NodeMCU
    // This is much faster 1000 updates in 10sec
    //FastLED.addLeds<NEOPIXEL,PIN>(leds, NUMMATRIX); 
    FastLED.addLeds<WS2811_PORTA,3>(matrixleds, NUMMATRIX/3).setCorrection(TypicalLEDStrip);
    Serial.print("Matrix Size: ");
    Serial.print(mw);
    Serial.print(" ");
    Serial.println(mh);
    // Turn off dithering https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
    FastLED.setDither( 0 );
    matrix->begin();
    matrix->setBrightness(matrix_brightness);
    matrix->setTextWrap(false);
    Serial.println("Init Pixels");
    ledmatrix.DrawLine (0, 0, ledmatrix.Width() - 1, ledmatrix.Height() - 1, CRGB(0, 255, 0));
    ledmatrix.DrawPixel(0, 0, CRGB(255, 0, 0));
    ledmatrix.DrawPixel(ledmatrix.Width() - 1, ledmatrix.Height() - 1, CRGB(0, 0, 255));
    matrix->show();
    delay(1000);
    // speed test
    //while (1) { display_resolution(); yield();};

    matrix->clear();
    // init first matrix demo
    display_resolution();
    delay(1000);
    matrix->clear();

    //font_test();

    // init first strip demo
    colorWipe(0x0000FF00, 10);
}

// vim:sts=4:sw=4
