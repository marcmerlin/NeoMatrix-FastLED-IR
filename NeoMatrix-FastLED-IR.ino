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

#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>

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
#include "fonts.h"

#include "floweranim.h"

// Choose your prefered pixmap
#include "smileytongue24.h"

#define MATRIXPIN D6

// Max is 255, 32 is a conservative value to not overload
// a USB power supply (500mA) for 12x12 pixels.
uint8_t matrix_brightness = 32;

#define mw 24
#define mh 32

CRGB matrixleds[mw*mh];

FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(matrixleds, 8, mh, mw/8, 1, 
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
    NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG + 
    NEO_TILE_TOP + NEO_TILE_LEFT +  NEO_TILE_PROGRESSIVE);

// How many ms used for each matrix update
#define MX_UPD_TIME 10

uint8_t matrix_state = 2;
int16_t matrix_loop = -1;


//---------------------------------------------------------------------------- 

#define NUM_LEDS 48

#ifdef ESP8266
#define NEOPIXEL_PIN D1 // GPIO5


// D4 is also the system LED, causing it to blink on IR receive, which is great.
#define RECV_PIN D4     // GPIO2

// Turn off Wifi in setup()
// https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
//
#include "ESP8266WiFi.h"
extern "C" {
#include "user_interface.h"
}
// min/max are broken by the ESP8266 include
#define min(a,b) (a<b)?(a):(b)
#define max(a,b) (a>b)?(a):(b)
#endif

#include <IRremoteESP8266.h>

// This file contains codes I captured and mapped myself
// using IRremote's examples/IRrecvDemo
#include "IRcodes.h"

IRrecv irrecv(RECV_PIN);

CRGB leds[NUM_LEDS];
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

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
int16_t speed = 50;

uint8_t led_brightness = 32;

// ---------------------------------------------------------------------------
// Matrix Code
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
// in parallel mode, does not affect output. That said, reducing their amount
// is still good.
    ESP.wdtDisable();
#endif
    FastLED[1].showLeds(matrix_brightness);
#ifdef ESP8266
    ESP.wdtEnable(1000);
#endif
    //matrix->show();
}

void matrix_clear() {
    //FastLED[1].clearLedData();
    // clear does not work properly with multiple matrices connected via parallel inputs
    memset(matrixleds, 0, sizeof(matrixleds));
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


bool tfsf() {
    static uint16_t state = 1;
    static float spd = 1.0;
    static int8_t startfade = -1;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint8_t resetspd = 5;
    uint8_t l = 0;

    matrix->setRotation(0);
    matrix->setTextSize(2);
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

    if (startfade < l && (state > (l*duration)/spd))  {
	matrix->setCursor(1, 29);
	matrix->setTextColor(matrix->Color(0,0,255));
	matrix_clear();
	matrix->print("8");
	startfade = l;
    }

    if (startfade > -1)  {
	for (uint16_t i = 0; i < mw*mh; i++) matrixleds[i].nscale8(248-spd*2);
    }
    l++;

    if (state++ > ((l+0.5)*duration)/spd) {
	state = 1;
	startfade = -1;
	spd += spdincr;
	if (spd > resetspd) {
	    spd = 1.0;
	    return 0;
	}
    }

    matrix_show();
    return 3;
}

// TODO: other 2 pixmaps

// type 0 = up, type 1 = up and down
// FIXME this breaks the colors displayed by the strip
bool tfsf_zoom(uint8_t zoom_type, uint8_t speed) {
    static uint16_t state = 1;
    static uint16_t direction = 1;
    static uint16_t size = 3;
    static uint8_t l = 0;
    static uint16_t delayframe = 1;
    char letters[] = { 'T', 'F', 'S', 'F', '8' };
    bool done = 0;
    uint8_t repeat = 6;

    matrix->setTextSize(1);

    if (--delayframe) {
	// reset how long a frame is shown before we switch to the next one
	// Serial.println("delayed frame");
	return repeat;
    }
    delayframe = speed;
    if (direction == 1) {
	uint16_t txtcolor = Color24toColor16(Wheel(map(letters[l], '0', 'Z', 0, 255)));
	matrix->setTextColor(txtcolor); 
	matrix_clear();
	matrix->setFont( &Century_Schoolbook_L_Bold[size] );
	matrix->setCursor(11-size*0.55, 15+size*0.75);
	matrix->print(letters[l]);
	if (size<16) size++; 
	else if (zoom_type == 0) { done = 1; delayframe = speed * 50; } 
	     else direction = 2;

    } else if (zoom_type == 1) {
	uint16_t txtcolor = Color24toColor16(Wheel(map(letters[l], '0', 'Z', 255, 0)));
	matrix->setTextColor(txtcolor); 
	matrix_clear();
	matrix->setFont( &Century_Schoolbook_L_Bold[size] );
	matrix->setCursor(11-size*0.55, 15+size*0.75);
	matrix->print(letters[l]);
	if (size>3) size--; else { done = 1; direction = 1; };
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
    return 0;
}

bool esrr() {
    static uint16_t state = 1;
    static float spd = 1.0;
    float spdincr = 0.3;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 18;
    uint8_t resetspd = 24;
    uint8_t l = 0;

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
	    spd = 1.0;
	    return 0;
	}
    }

    matrix_show();
    return 2;
}

// This is too jarring on the eyes at night
bool esrr_flashin() {
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

bool esrr_fade() {
    static uint16_t state = 0;
    static uint8_t wheel = 0;
    static uint8_t sp = 0;
    static float spd = 1.0;
    float spdincr = 0.2;
    uint8_t resetspd = 5;
    uint16_t txtcolor;

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
	fadeToBlackBy( matrixleds, mw*mh, 20*spd);
    }

    if (state > 100/spd) {
	state = 0;
	spd += spdincr;
	//Serial.println(spd);
	//Serial.println(state);
	if (spd > resetspd) {
	    spd = 1.0;
	    return 0;
	}
    }
    matrix_show();
    return 3;
}


bool circles() {
    static uint16_t state = 0;
    static uint8_t wheel = 0;

    uint8_t x = mw/2-1;
    uint8_t y = mh/2-1;
    uint8_t radius = max(mh/2-1,mw/2-1);

    state++;
    wheel += 10;
    Serial.println(wheel);

    matrix_clear();
    for (uint8_t r = 0; r <= radius; r++) {
	matrix->drawCircle( mw/2-1, mh/2-1, r, Color24toColor16(Wheel(wheel+r*10)));
    }

    if (state > 10) {
	state = 0;
	return 0;
    }
    matrix_show();
    return 100;
}


bool webwc() {
    static uint16_t state = 1;
    static float spd = 1.0;
    static bool didclear = 0;
    static bool firstpass = 0;
    float spdincr = 0.3;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 18;
    uint8_t resetspd = 24;
    uint8_t l = 0;
    uint16_t txtcolor;

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
	    spd = 1.0;
	    didclear = 0;
	    firstpass = 0;
	    return 0;
	}
    }

    if (spd < displayall) fadeToBlackBy( matrixleds, mw*mh, 20*map(spd, 1, 24, 1, 4));

    matrix_show();
    return 3;
}




void display_scrollText() {
    uint8_t size = max(int(mw/8), 1);
    matrix_clear();
    matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
    matrix->setTextSize(1);
    matrix->setRotation(0);
    for (int8_t x=7; x>=-42; x--) {
	matrix_clear();
	matrix->setCursor(x,0);
	matrix->setTextColor(matrix->Color(0, 255, 0));
	matrix->print("Hello");
	if (mh>11) {
	    matrix->setCursor(-20-x,mh-7);
	    matrix->setTextColor(matrix->Color(255, 255, 0));
	    matrix->print("World");
	}
	matrix_show();
        delay(50);
    }

    matrix->setRotation(3);
    matrix->setTextSize(size);
    matrix->setTextColor(matrix->Color(0, 0, 255));
    for (int16_t x=8*size; x>=-6*8*size; x--) {
	matrix_clear();
	matrix->setCursor(x,mw/2-size*4);
	matrix->print("Rotate");
	matrix_show();
	// note that on a big array the refresh rate from show() will be slow enough that
	// the delay become irrelevant. This is already true on a 32x32 array.
        delay(50/size);
    }
    matrix->setRotation(0);
    matrix->setCursor(0,0);
    matrix_show();
}

// Scroll within big bitmap so that all if it becomes visible or bounce a small one.
// If the bitmap is bigger in one dimension and smaller in the other one, it will
// be both panned and bounced in the appropriate dimensions.
bool panOrBounceBitmap (uint8_t bitmapnum, uint8_t bitmapSize) {
    static uint16_t count = 0;
    // keep integer math, deal with values 16 times too big
    // start by showing upper left of big bitmap or centering if the display is big
    static int16_t xf = max(0, (mw-bitmapSize)/2) << 4;
    static int16_t yf = max(0, (mh-bitmapSize)/2) << 4;
    // scroll speed in 1/16th
    static int16_t xfc = 6;
    static int16_t yfc = 3;
    // scroll down and right by moving upper left corner off screen 
    // more up and left (which means negative numbers)
    static int16_t xfdir = -1;
    static int16_t yfdir = -1;

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
    if (count++ == 600) { 
	count = 0;
	return 0; 
    }
    return 3;
}

bool GifAnim() {
    static uint8_t loop = 20;
    static uint16_t frame = 0;

    for (uint8_t y = 0; y < 32; y++) {
	for (uint8_t x = 0; x < 32; x++) {
	    uint32_t loc = y*32 + x;
	    matrix->drawPixel(x-4, y, matrix->Color(
		(pgm_read_byte(&(flowerRedFrames[frame][loc]))), 
		(pgm_read_byte(&(flowerGreenFrames[frame][loc]))), 
		(pgm_read_byte(&(flowerBlueFrames[frame][loc])))
	    ));
	}
    }
    matrix_show();
    if (++frame == 30) {
	frame = 0;
	if (loop-- == 0) {
	    loop = 50;
	    return 0;
	}
    }
    return 3;
}

void matrix_next() {
    // this ensures the next demo returns the number of times it should loop
    matrix_loop = -1;
    matrix_state++;
    if (matrix_state == 9) { 
	matrix_state = 0;
    }
}

// TODO3: adjust number of runs per anim
void matrix_update() {
    uint8_t ret;

    switch (matrix_state) {
	case 0: 
	    ret = esrr();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 1: 
	    ret = tfsf();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 2: 
	    ret = circles();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    matrix_state = 2;
	    break;

	case 3: 
	    ret = tfsf_zoom(0, 20);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 4: 
	    ret = esrr_fade();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 5: 
	    ret = tfsf_zoom(1, 30);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 6: 
	    ret = webwc();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 7: 
	    ret = panOrBounceBitmap(1, 24);
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;

	case 8: 
	    ret = GifAnim();
	    if (matrix_loop == -1) matrix_loop = ret;
	    if (ret) return;
	    break;
    } 
    Serial.print("Done with demo ");
    Serial.print(matrix_state);
    Serial.print(" loop ");
    Serial.println(matrix_loop);
    if (matrix_loop-- > 0) return;
    matrix_next();
    Serial.print("Switching to matrix demo ");
    Serial.println(matrix_state);
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
    static uint8_t brightness = 4;
    static uint32_t last_brightness_change = 0 ;

    if (millis() - last_brightness_change < 300) {
	Serial.print("Too soon... Ignoring brightness change from ");
	Serial.println(brightness);
	return;
    }
    last_brightness_change = millis();
    brightness = constrain(brightness + change, 1, 8);
    led_brightness = (1 << brightness) - 1;
    matrix_brightness = (1 << brightness) - 1;

    // This is actually ignored by the currrent setup with 2 independent strings
    FastLED.setBrightness(led_brightness);

    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" value ");
    Serial.println(led_brightness);
    leds_show();
}

void change_speed(int8_t change) {
    static uint32_t last_speed_change = 0 ;

    if (millis() - last_speed_change < 200) {
	Serial.print("Too soon... Ignoring speed change from ");
	Serial.println(speed);
	return;
    }
    last_speed_change = millis();

    Serial.print("Changing speed ");
    Serial.print(speed);
    speed = constrain(speed + change, 1, 100);
    Serial.print(" to new speed ");
    Serial.println(speed);
}



bool handle_IR(uint32_t delay_time) {
    decode_results IR_result;
    for (uint16_t i=0; i<delay_time / MX_UPD_TIME; i++) {
	matrix_update();
    }
    delay(delay_time % MX_UPD_TIME);

    if (irrecv.decode(&IR_result)) {
    	irrecv.resume(); // Receive the next value
	switch (IR_result.value) {
	case IR_RGBZONE_POWER:
	    nextdemo = f_colorWipe;
	    demo_color = 0x000000;
	    speed = 1;
	    Serial.println("Got IR: Power");
	    Serial.println("Hit slower speed to restart panel anim");
	    return 1;

	case IR_RGBZONE_BRIGHT:
	    change_brightness(+1);
	    Serial.println("Got IR: Bright");
	    return 1;

	case IR_RGBZONE_DIM:
	    change_brightness(-1);
	    Serial.println("Got IR: Dim");
	    return 1;

	case IR_RGBZONE_NEXT:
	    matrix_next();
	    Serial.println("Got IR: Next");
	    return 1;

	case IR_RGBZONE_QUICK:
	    change_speed(-10);
	    Serial.println("Got IR: Quick");
	    return 1;

	case IR_RGBZONE_SLOW:
	    change_speed(+10);
	    Serial.println("Got IR: Slow");
	    return 1;


	case IR_RGBZONE_RED:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF0000;
	    Serial.println("Got IR: Red");
	    return 1;

	case IR_RGBZONE_GREEN:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00FF00;
	    Serial.println("Got IR: Green");
	    return 1;

	case IR_RGBZONE_BLUE:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x0000FF;
	    Serial.println("Got IR: Blue");
	    return 1;

	case IR_RGBZONE_WHITE:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFFFFF;
	    Serial.println("Got IR: White");
	    return 1;



	case IR_RGBZONE_RED2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xCE6800;
	    Serial.println("Got IR: Red2");
	    return 1;

	case IR_RGBZONE_GREEN2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    Serial.println("Got IR: Green2");
	    return 1;

	case IR_RGBZONE_BLUE2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x0000BB;
	    Serial.println("Got IR: Blue2");
	    return 1;

	case IR_RGBZONE_PINK:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF50FE;
	    Serial.println("Got IR: Pink");
	    return 1;



	case IR_RGBZONE_ORANGE:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFF8100;
	    Serial.println("Got IR: Orange");
	    return 1;

	case IR_RGBZONE_BLUE3:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    Serial.println("Got IR: Green2");
	    return 1;

	case IR_RGBZONE_PURPLED:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x270041;
	    Serial.println("Got IR: DarkPurple");
	    return 1;

	case IR_RGBZONE_PINK2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFB9FF;
	    Serial.println("Got IR: Pink2");
	    return 1;



	case IR_RGBZONE_ORANGE2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xFFCA49;
	    Serial.println("Got IR: Orange2");
	    return 1;

	case IR_RGBZONE_GREEN3:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x006A00;
	    Serial.println("Got IR: Green2");
	    return 1;

	case IR_RGBZONE_PURPLE:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x2B0064;
	    Serial.println("Got IR: DarkPurple2");
	    return 1;

	case IR_RGBZONE_BLUEL:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x50A7FF;
	    Serial.println("Got IR: BlueLigh");
	    return 1;



	case IR_RGBZONE_YELLOW:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0xF0FF00;
	    Serial.println("Got IR: Yellow");
	    return 1;

	case IR_RGBZONE_GREEN4:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x00BB00;
	    Serial.println("Got IR: Green2");
	    return 1;

	case IR_RGBZONE_PURPLE2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x660265;
	    Serial.println("Got IR: Purple2");
	    return 1;

	case IR_RGBZONE_BLUEL2:
	    if (!colorDemo) nextdemo = f_colorWipe;
	    demo_color = 0x408BD8;
	    Serial.println("Got IR: BlueLight2");
	    return 1;


	case IR_RGBZONE_RU:
	    Serial.println("Got IR: Red UP");
	    matrix_loop = 9999;
	    matrix_state = 0;
	    return 1;

	case IR_RGBZONE_RD:
	    Serial.println("Got IR: Red DOWN");
	    matrix_loop = 9999;
	    matrix_state = 2;
	    return 1;

	case IR_RGBZONE_GU:
	    Serial.println("Got IR: Green UP");
	    matrix_loop = 9999;
	    matrix_state = 4;
	    return 1;

	case IR_RGBZONE_GD:
	    Serial.println("Got IR: Green DOWN");
	    matrix_loop = 9999;
	    matrix_state = 6;
	    return 1;

	case IR_RGBZONE_BU:
	    Serial.println("Got IR: Blue UP");
	    matrix_loop = 9999;
	    matrix_state = 7;
	    return 1;

	case IR_RGBZONE_BD:
	    Serial.println("Got IR: Blue DOWN");
	    matrix_loop = 9999;
	    matrix_state = 8;
	    return 1;

	case IR_RGBZONE_DIY1:
	    // this uses the last color set
	    nextdemo = f_colorWipe;
	    Serial.println("Got IR: DIY1/colorWipe");
	    return 1;

	case IR_RGBZONE_DIY2:
	    // this uses the last color set
	    nextdemo = f_theaterChase;
	    Serial.println("Got IR: DIY2/theaterChase");
	    return 1;

	case IR_RGBZONE_DIY3:
	    nextdemo = f_juggle;
	    Serial.println("Got IR: DIY3/Juggle");
	    return 1;

	case IR_RGBZONE_DIY4:
	    nextdemo = f_rainbowCycle;
	    Serial.println("Got IR: DIY4/rainbowCycle");
	    return 1;

	case IR_RGBZONE_DIY5:
	    nextdemo = f_theaterChaseRainbow;
	    Serial.println("Got IR: DIY5/theaterChaseRainbow");
	    return 1;

	case IR_RGBZONE_DIY6:
	    nextdemo = f_doubleConvergeRev;
	    Serial.println("Got IR: DIY6/DoubleConvergeRev");
	    return 1;

	case IR_RGBZONE_AUTO:
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

void doubleConverge(bool trail, uint8_t wait, bool rev=false) {
    static uint8_t hue;
    for(uint16_t i = 0; i < NUM_LEDS/2 + 4; i++) {
	// fade everything out
	//leds.fadeToBlackBy(60);

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
	if (handle_IR(wait/20)) return;
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
	colorWipe(demo_color, speed);
	break;
    case f_theaterChase:
	colorDemo = true;
	theaterChase(demo_color, speed);
	break;

    // Rainbow anims on DIY4-6
// This is not cool/wavy enough, the cycle version is, though
//     case f_rainbow:
// 	colorDemo = false;
// 	rainbow(speed);
// 	break;
    case f_rainbowCycle:
	colorDemo = false;
	rainbowCycle(speed);
	break;
    case f_theaterChaseRainbow:
	colorDemo = false;
	theaterChaseRainbow(speed);
	break;

    case f_doubleConvergeRev:
	colorDemo = false;
	doubleConverge(false, speed, true);
	break;

    // Jump3 to Jump7
    case f_cylon:
	colorDemo = false;
	cylon(false, speed);
	break;
    case f_cylonTrail:
	colorDemo = false;
	cylon(true, speed);
	break;
    case f_doubleConverge:
	colorDemo = false;
	doubleConverge(false, speed);
	break;
    case f_doubleConvergeTrail:
	colorDemo = false;
	doubleConverge(false, speed);
	doubleConverge(true, speed);
	break;

    // Flash color wheel
    case f_flash:
	colorDemo = false;
	flash(speed);
	break;

#if 0
    case f_flash3:
	colorDemo = false;
	flash3(speed);
	break;
#endif
    case f_juggle:
	colorDemo = false;
	juggle(speed);
	break;

    case f_bpm:
	colorDemo = false;
	bpm(speed);
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
    Serial.println(speed);
    #endif

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
    Serial.println("Start code");

    // Init Matrix
    // Serialized, 768 pixels takes 26 seconds for 1000 updates or 26ms per refresh
    // FastLED.addLeds<NEOPIXEL,MATRIXPIN>(matrixleds, mw*mh).setCorrection(TypicalLEDStrip);
    // https://github.com/FastLED/FastLED/wiki/Parallel-Output
    // WS2811_PORTA - pins 12, 13, 14 and 15 or pins 6,7,5 and 8 on the NodeMCU
    // This is much faster 1000 updates in 10sec
    FastLED.addLeds<WS2811_PORTA,3>(matrixleds, mw*mh/3).setCorrection(TypicalLEDStrip);
    Serial.print("Matrix Size: ");
    Serial.print(mw);
    Serial.print(" ");
    Serial.println(mh);
    matrix->begin();
    matrix->setBrightness(matrix_brightness);
    matrix->setTextWrap(false);
    // speed test
    //while (1) { display_resolution(); yield();};

    // init first matrix demo
    display_resolution();
    delay(500);

    //font_test();

    // init first strip demo
    colorWipe(0x0000FF00, 10);
}

// vim:sts=4:sw=4
