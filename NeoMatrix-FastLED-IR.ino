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
#include "Table_Mark_Estes_Impl.h"

#include "AikoEvents_Impl.h"
using namespace Aiko;

#ifdef HAS_FS
// defines FSO
#include "GifAnimViewer.h"
#endif

#ifdef WIFI
    #include <OmEspHelpers.h>
    OmWebServer s;
    OmWebPages p;    
#endif

#define DEMO_PREV -32768
#define DEMO_NEXT 32767
#define DEMO_TEXT_THANKYOU 250
#define DEMO_TEXT_INPUT 251
#define DEMO_TEXT_FIRST DEMO_TEXT_INPUT
#define DEMO_TEXT_LAST DEMO_TEXT_INPUT

#define DEMO_ARRAY_SIZE (DEMO_TEXT_LAST+1)
// Different panel configurations: 24x32, 64x64 (BM), 64x96 (BM), 64x96 (Trance), 128x192
#define CONFIGURATIONS 5

#if mheight == 192
#define panelconfnum 4
#elif mheight == 96
#define panelconfnum 3
#else 
#define panelconfnum 0
#endif

typedef struct mapping_entry_ {
    uint16_t mapping;
    // 1: enabled, 2: bestof enabled only, 3: both
    uint8_t enabled[CONFIGURATIONS];
} Mapping_Entry;

Mapping_Entry demo_mapping[DEMO_ARRAY_SIZE];


// demo_last_idx is the number of elements in the demo array, but this includes empty slots
uint16_t demo_cnt = 0; // actual number of demos available at boot (different from enabled)
uint16_t best_cnt = 0;
// last demo index (starting from 1, not 0), gets computed in read_config_index
uint16_t demo_last_idx = 0;

// index within demo_mapping of what demo is being played
uint16_t matrix_state = 0;
uint16_t matrix_demo; // this is initialized after matrix_state is updated in read_config_index

String displaytext="";

// Compute how many GIFs have been defined (called in setup)
uint8_t gif_cnt = 0;

// look for 'magic happens here' below
#ifdef ARDUINOONPC
    #include <errno.h>
    #include <fcntl.h> 
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/stat.h>


    int set_interface_attribs(int ttyfd, int speed)
    {
	struct termios tty;

	if (tcgetattr(ttyfd, &tty) < 0) {
	    printf("Error from tcgetattr: %s\n", strerror(errno));
	    return -1;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;         /* 8-bit characters */
	tty.c_cflag &= ~PARENB;     /* no parity bit */
	tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
	tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	// https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
	// Non blocking
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	if (tcsetattr(ttyfd, TCSANOW, &tty) != 0) {
	    printf("Error from tcsetattr: %s\n", strerror(errno));
	    return -1;
	}
	return 0;
    }

    int openttyUSB(const char ** devname) {
	// static because the name is sent back to the caller
	static const char *dev[] = { "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2" };
	int devidx = 0;
	int ttyfd;

	while (devidx<3 && (ttyfd = open((*devname = dev[devidx]), O_RDWR | O_NOCTTY | O_SYNC)) < 0 && ++devidx) {
	    struct stat stbuf;
	    // warn for permission denied but not for no such file or directory
	    if (!stat(*devname, &stbuf)) printf("Error opening %s: %s\n", *devname, strerror(errno));
	}
	/*baudrate 115200, 8 bits, no parity, 1 stop bit */
	if (ttyfd >= 0) {
	    set_interface_attribs(ttyfd, B115200);
	    printf("Opened %s\n", *devname);
	}
	return ttyfd;
    }

    int ttyfd = -1;
    int send_serial(const char *xstr) {
	int wlen;
	int xlen = strlen(xstr);

	wlen = write(ttyfd, xstr, xlen);
	// tcdrain(ttyfd);
	if (wlen != xlen) {
	    printf("Error from write: %d, %d\n", wlen, errno);
	    return -1;
	}
	printf(">>>ESP: %s\n", xstr);
	return 0;
    }
#endif // ARDUINOONPC


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
    // work around "a15 cannot be used in asm here" compiler bug when using an array on ESP8266
    static uint16_t *RGB_bmp_fixed = (uint16_t *) mallocordie("RGB_bmp_fixed", w*h*2, false);
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
#ifdef FASTLED_NEOMATRIX
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
#else
    matrix->show();
#endif
}

// Other fonts possible on http://oleddisplay.squix.ch/#/home
// https://blog.squix.org/2016/10/font-creator-now-creates-adafruit-gfx-fonts.html
// https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts
// Default is 5x7, meaning 4.8 chars wide, 4 chars high
// Picopixel: 3x5 means 6 chars wide, 5 or 6 chars high
// #include <Fonts/Picopixel.h>
// Proportional 5x5 font, 4.8 wide, 6 high
// #include <Fonts/Org_01.h>
// TomThumb: 3x5 means 4 chars wide, 5 or 6 chars high
// -> nicer font
#include <Fonts/TomThumb.h>
// 3x3 but not readable
//#include <Fonts/Tiny3x3a2pt7b.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/FreeMonoBold18pt7b.h>
//#include <Fonts/FreeMonoBold24pt7b.h>
#include "fonts.h"

// Choose your prefered pixmap
#include "smileytongue24.h"

// controls how many times a demo should run its pattern
// init at -1 to indicate that a demo is run for the first time (demo switch)
int16_t matrix_loop = -1;
uint32_t waitmillis = 0;


//----------------------------------------------------------------------------

// This file contains codes I captured and mapped myself
// using IRremote's examples/IRrecvDemo
#include "IRcodes.h"

#ifdef RECV_PIN
    #ifdef ESP8266
    #include <IRremoteESP8266.h>
    #else
        #ifdef ESP32RMTIR
        #include <IRRecv.h> // https://github.com/lbernstone/IR32.git
        #else
        #include <IRremote.h>
        #endif
    #endif

    #ifndef ESP32RMTIR
    IRrecv irrecv(RECV_PIN);
    #else
    IRRecv irrecv;
    #endif
#endif


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
    f_juggle = 12,
    f_bpm = 13,
} StripDemo;

StripDemo nextdemo = f_theaterChaseRainbow;
// Is the current demo linked to a color (false for rainbow demos)
bool colorDemo = true;
int32_t demo_color = 0x00FF00; // Green
int16_t strip_speed = 50;


uint32_t last_change = millis();

// ---------------------------------------------------------------------------
// Shared functions
// ---------------------------------------------------------------------------

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(uint8_t WheelPos) {
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


uint8_t tfsf_zoom(uint32_t zoom_type);
uint8_t tfsf(uint32_t unused) {
    static uint16_t state;
    static float spd;
    uint8_t l = 0;

    unused = unused;

    static int8_t startfade;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint8_t resetspd = 5;
    uint8_t repeat = 2;
    uint8_t fontsize = 1;
    uint8_t idx = 3;

    // For bigger screens, use the zoom version in static fully zoomed mode.
    if (mheight >= 64) return tfsf_zoom(99);

    // For smaller displays, flash letters one by one.
    if (matrix_reset_demo == 1) {
	matrix_reset_demo = 0;
	matrix->clear();
	state = 1;
	spd = 1.0;
	startfade = -1;
	matrix->setRotation(0);
	// biggest font is 18, but this spills over
	matrix->setFont( &Century_Schoolbook_L_Bold[16] );
	if (mw >= 48 && mh >=48) fontsize = 2;
	matrix->setTextSize(fontsize);
    }

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(0, mh - idx*8*fontsize/3);
	matrix->clear();
	matrix->setTextColor(matrix->Color(255,0,0));
	matrix->print("T");
	startfade = l;
    }
    l++; idx--;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(0, mh - idx*8*fontsize/3);
	matrix->clear();
	matrix->setTextColor(matrix->Color(192,192,0));
	matrix->print("F");
	startfade = l;
    }
    l++; idx--;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(2, mh - idx*8*fontsize/3);
	matrix->clear();
	matrix->setTextColor(matrix->Color(0,192,192));
	matrix->print("S");
	startfade = l;
    }
    l++; idx--;

    if (startfade < l && (state > (l*duration)/spd && state < ((l+1)*duration)/spd))  {
	matrix->setCursor(2, mh - idx*8*fontsize/3);
	matrix->clear();
	matrix->setTextColor(matrix->Color(0,255,0));
	matrix->print("F");
	startfade = l;
    }
    l++; idx--;

    #if 0
    if (startfade < l && (state > (l*duration)/spd))  {
	matrix->setCursor(1, 29);
	matrix->clear();
	matrix->setTextColor(matrix->Color(0,255,0));
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
// 99 = don't zoom, just display the biggest font a bit dimmer (for pictures)
// on small displays, zooms in each letter one by one
// on bigger displays, zooms in "TF" and "SF"
uint8_t tfsf_zoom(uint32_t zoom_type) {
    static uint16_t direction;
    static uint16_t size;
    static uint8_t l;
    static int16_t faster = 0;
    static bool dont_exit;
    static uint16_t delayframe;
    const char letters[] = { 'T', 'F', 'S', 'F' };
    uint8_t speed = 30;
    bool done = 0;
    static uint8_t repeat = 4;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        direction = 1;
        size = 3;
        l = 0;
        if (matrix_loop == -1) { dont_exit = 1; delayframe = 2; faster = 0; };
        matrix->setTextSize(1);
	if (mheight >= 192) matrix->setTextSize(2);
	repeat = 4;
	if (zoom_type == 99) { size = 18; repeat = 10; };
    }

    if (--delayframe) {
        // reset how long a frame is shown before we switch to the next one
        // Serial.println("delayed frame");
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
        return repeat;
    }
    delayframe = max((speed / 10) - faster , 1);
    // before exiting, we run the full delay to show the last frame long enough
    if (dont_exit == 0) { dont_exit = 1; return 0; }

    // Either show one letter at a time and change the size and color on smaller
    // displays (l is incremented at the bottom to switch letters), or on a bigger
    // display, skip that and show all the letters at once.
    if (direction == 1) {
        int8_t offset = 0; // adjust some letters left or right as needed
        matrix->clear();
        matrix->setFont( &Century_Schoolbook_L_Bold[size] );
        if (mw >= 48 && mh >=64) {
            matrix->setPassThruColor(0xD7E1EB);
	    // Something less bright for pictures
	    if (zoom_type == 99) matrix->setPassThruColor(0x77818B);
	    matrix->setCursor(20-size+offset, (mh>=128?64:36)+size*1.5);
            matrix->print("TF");
            matrix->setPassThruColor(0x05C1FF);
	    // Something less bright for pictures
	    if (zoom_type == 99) matrix->setPassThruColor(0x00618F);
            matrix->setCursor((mh>=128?50:24)-size+offset, (mh>=128?128:68)+size*1.5);
            matrix->print("SF");
        } else {
            if (letters[l] == 'T') offset = -2 * size/15;
            if (letters[l] == '8') offset = 2 * size/15;
            matrix->setPassThruColor(Wheel(map(letters[l], '0', 'Z', 255, 0)));
    #ifdef M32BY8X3
            matrix->setCursor(10-size*0.55+offset, 17+size*0.75);
    #else
            matrix->setCursor(3*mw/6-size*1.75+offset, mh*7/12+size*1.60);
    #endif
            matrix->print(letters[l]);
        }
        matrix->setPassThruColor();
	if (zoom_type != 99) {
	    if (size<18) size++;
	    else if (zoom_type == 0) { done = 1; delayframe = max((speed - faster*10) * 1, 3); }
		 else direction = 2;
	} else {
	    repeat--;
	}

    } else if (zoom_type == 1) {
        int8_t offset = 0; // adjust some letters left or right as needed

        matrix->clear();
        matrix->setPassThruColor(Wheel(map(letters[l], '0', 'Z', 64, 192)));
        matrix->setFont( &Century_Schoolbook_L_Bold[size] );
        if (mw >= 48 && mh >=64) {
            matrix->setPassThruColor(0xD7E1EB);
	    // Something less bright for pictures
	    if (zoom_type == 99) matrix->setPassThruColor(0x77818B);
            matrix->setCursor(20-size+offset, (mh>=128?64:36)+size*1.5);
            matrix->print("TF");
            matrix->setPassThruColor(0x05C1FF);
	    // Something less bright for pictures
	    if (zoom_type == 99) matrix->setPassThruColor(0x00618F);
            matrix->setCursor((mh>=128?50:24)-size+offset, (mh>=128?128:68)+size*1.5);
            matrix->print("SF");
        } else {
            if (letters[l] == 'T') offset = -2 * size/15;
            if (letters[l] == '8') offset = 2 * size/15;

    #ifdef M32BY8X3
            matrix->setCursor(10-size*0.55+offset, 17+size*0.75);
    #else
            matrix->setCursor(3*mw/6-size*1.75+offset, mh*7/12+size*1.60);
    #endif
            matrix->print(letters[l]);
        }   
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

uint8_t esrbr(uint32_t unused) { // eat sleep rave/burn repeat
    static uint16_t state;
    static float spd;
    //static bool didclear;
    static bool firstpass;
    float spdincr = 1.0;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 16;
    uint8_t resetspd = 24;
    uint8_t l = 0;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        matrix->clear();
        state = 1;
        spd = 1.0;
        firstpass = 0;
	matrix->setRotation(0);
	matrix->setTextSize(1);
	if (mheight >= 192)  {
	    matrix->setFont(&Century_Schoolbook_L_Bold_26);
	} else if (mheight >= 64)  {
	    //matrix->setFont(FreeMonoBold9pt7b);
	    matrix->setFont(&Century_Schoolbook_L_Bold_12);
	} else {
	    matrix->setFont(&TomThumb);
	}
    }

    // without this, we can use fadeall, but it fades too slowly for my opinion
    // so let's only show 2 at a time
    //if (! didclear) {
	matrix->clear();
        //didclear = 1;
    //}

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(30, 48);
        else if (mheight >= 96) matrix->setCursor(24, 20);
        else if (mheight >= 64) matrix->setCursor(18, 15);
        else matrix->setCursor(7, 6);

        matrix->setTextColor(matrix->Color(255,0,0));
        matrix->print("EAT");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        firstpass = 1;
        if (mheight >= 192) matrix->setCursor(18, 84);
        else if (mheight >= 96) matrix->setCursor(10, 41);
        else if (mheight >= 64) matrix->setCursor(10, 33);
        else matrix->setCursor(3, 14);

        matrix->setTextColor(matrix->Color(192,192,0));
        matrix->print("SLEEP");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(22, 120);
        else if (mheight >= 96) matrix->setCursor(14, 63);
        else if (mheight >= 64) matrix->setCursor(14, 47);
        else matrix->setCursor(5, 22);

        matrix->setTextColor(matrix->Color(0,255,0));
        if (mheight == 64) matrix->print("BURN");
        else matrix->print("RAVE");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || (state < overlap/spd && firstpass)) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(2, 160);
        else if (mheight >= 96) matrix->setCursor(2, 82);
        else if (mheight >= 64) matrix->setCursor(2, 63);
        else matrix->setCursor(0, 30);

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

    //if (spd < displayall) fadeToBlackBy( matrixleds, NUMMATRIX, 20*map(spd, 1, 24, 1, 4));

    matrix_show();
    return 1;
}

uint8_t trancejesus(uint32_t unused) {
    static uint16_t state;
    static float spd;
    static bool didclear;
    static bool firstpass;
    float spdincr = 1.2;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 14;
    uint8_t resetspd = 24;
    uint8_t l = 0;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        matrix->clear();
        state = 1;
        spd = 1.0;
        didclear = 0;
        firstpass = 0;
        matrix->setRotation(0);
        matrix->setTextSize(1);
	if (mheight >= 192)  {
	    matrix->setFont(&Century_Schoolbook_L_Bold_22);
	} else if (mheight >= 64)  {
	    //matrix->setFont(FreeMonoBold9pt7b);
	    matrix->setFont(&Century_Schoolbook_L_Bold_8);
	} else {
	    matrix->setFont(&TomThumb);
	}
    }

    if (! didclear) {
	matrix->clear();
        didclear = 1;
    }

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(8, 25);
        else matrix->setCursor(2, 16);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("TRANCE");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        firstpass = 1;
        if (mheight >= 192) matrix->setCursor(14, 65);
        else matrix->setCursor(0, 32);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("Because");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(12, 105);
        else matrix->setCursor(5, 48);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("it's what");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(20, 145);
        else matrix->setCursor(12, 64);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("JESUS");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || (state < overlap/spd && firstpass)) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(2, 185);
        else matrix->setCursor(0, 82);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("would do!");
    }
    l++;

    matrix->setPassThruColor();
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


uint8_t bbb(uint32_t unused) {
    static uint16_t state;
    static float spd;
    float spdincr = 0.6;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 18;
    uint8_t resetspd = 24;
    uint8_t l = 0;

    unused = unused;

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
uint8_t esrbr_flashin() {
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

uint8_t esrbr_fade(uint32_t unused) {
    static uint16_t state;
    static uint8_t wheel;
    static float spd;
    float spdincr = 0.5;
    uint8_t resetspd = 5;

    unused = unused;

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
	if (mheight >= 192)  {
	    matrix->setFont(&Century_Schoolbook_L_Bold_26);
	} else if (mheight >= 64)  {
	    //matrix->setFont(FreeMonoBold9pt7b);
            matrix->setFont(&Century_Schoolbook_L_Bold_12);
        } else {
            matrix->setFont(&TomThumb);
        }
        matrix->setRotation(0);
        matrix->setTextSize(1);
        matrix->clear();

        if (mheight >= 192) matrix->setCursor(30, 48);
        else if (mheight >= 96) matrix->setCursor(24, 20);
        else if (mheight >= 64) matrix->setCursor(18, 15);
        else matrix->setCursor(7, 6);
        matrix->setPassThruColor(Wheel(((wheel+=24))));
        matrix->print("EAT");

        if (mheight >= 192) matrix->setCursor(18, 84);
        else if (mheight >= 96) matrix->setCursor(10, 41);
        else if (mheight >= 64) matrix->setCursor(10, 33);
        else matrix->setCursor(3, 14);
        matrix->setPassThruColor(Wheel(((wheel+=24))));
        matrix->print("SLEEP");

        if (mheight >= 192) matrix->setCursor(22, 120);
        else if (mheight >= 96) matrix->setCursor(14, 63);
        else if (mheight >= 64) matrix->setCursor(14, 47);
        else matrix->setCursor(5, 22);
        matrix->setPassThruColor(Wheel(((wheel+=24))));
        if (mheight == 64) {
            matrix->print("BURN");
        } else {
            matrix->print("RAVE");
        }

        if (mheight >= 192) matrix->setCursor(2, 160);
        else if (mheight >= 96) matrix->setCursor(2, 82);
        else if (mheight == 64) matrix->setCursor(2, 63);
        else matrix->setCursor(0, 30);
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


uint8_t squares(uint32_t reverse) {
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
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
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


uint8_t webwc(uint32_t unused) {
    static uint16_t state;
    static float spd;
    static bool didclear;
    static bool firstpass;
    float spdincr = 1.2;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 14;
    uint8_t resetspd = 24;
    uint8_t l = 0;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        matrix->clear();
        state = 1;
        spd = 1.0;
        didclear = 0;
        firstpass = 0;
        matrix->setRotation(0);
        matrix->setTextSize(1);
	if (mheight >= 192)  {
	    matrix->setFont(&Century_Schoolbook_L_Bold_26);
	} else if (mheight >= 64)  {
	    //matrix->setFont(FreeMonoBold9pt7b);
	    matrix->setFont(&Century_Schoolbook_L_Bold_12);
	} else {
	    matrix->setFont(&TomThumb);
	}
    }

    if (! didclear) {
        matrix->clear();
        didclear = 1;
    }

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(22, 25);
        else if (mheight >= 96) matrix->setCursor(12, 16);
        else if (mheight >= 64) matrix->setCursor(12, 12);
        else matrix->setCursor(5, 6);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("WITH");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        firstpass = 1;
        if (mheight >= 192) matrix->setCursor(14, 65);
        else if (mheight >= 96) matrix->setCursor(7, 32);
        else if (mheight >= 64) matrix->setCursor(7, 24);
        else matrix->setCursor(3, 12);
        // going from 24 to 16 with gamma correction and back to 24 damages the wheel colors enough to make them not usable
        // 669900  01100110 10011001 00000000 
        // 64C0       01100   100110 00000
        // 1D5B00  00011101 01011011 00000000
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("EVERY");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(22, 105);
        else if (mheight >= 96) matrix->setCursor(12, 48);
        else if (mheight >= 64) matrix->setCursor(12, 36);
        else matrix->setCursor(5, 18);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("BEAT");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(2, 145);
        else if (mheight >= 96) matrix->setCursor(4, 64);
        else if (mheight >= 64) matrix->setCursor(4, 48);
        else matrix->setCursor(2, 24);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("WE ARE");
    }
    l++;

    if ((state > (l*duration-l*overlap)/spd || (state < overlap/spd && firstpass)) || spd > displayall)  {
        if (mheight >= 192) matrix->setCursor(2, 185);
        else if (mheight >= 96) matrix->setCursor(0, 82);
        else if (mheight >= 64) matrix->setCursor(0, 60);
        else matrix->setCursor(0, 30);
        matrix->setPassThruColor(Wheel(map(l, 0, 5, 0, 255)));
        matrix->print("CLOSER");
    }
    l++;

    matrix->setPassThruColor();
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
        matrix->setFont( &Century_Schoolbook_L_Bold[fontsize] );
        matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
        matrix->setTextSize(1);
        matrix->setRotation(0);
    }

    if (--delayframe) {
        // reset how long a frame is shown before we switch to the next one
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
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
    matrix->setPassThruColor();
    //x-=2; // faster but maybe too much?
    x--;

    if (x < (-1 * (int16_t)len * fontwidth)) {
        matrix_reset_demo = 1;
        return 0;
    }
    matrix_show();
    return repeat;
}


uint8_t DoublescrollText(uint32_t choice) {
    static int16_t x;
    int8_t fontsize = 9;
    int8_t fontwidth = 11;
    int8_t stdelay = 2;
    int16_t len;
    const char *str1;
    const char *str2;

    if (choice == 1) {
        str1 = "Safety";
        str2 = "Third";
    } else {
        str1 = "I Love";
        str2 = "LEDs!!!";
    }

    len = max(strlen(str1), strlen(str2));

    uint8_t repeat = 4;
    if (mw >= 64) {
        fontsize = 14;
        fontwidth = 16;
        stdelay = 1;
    }
    static uint16_t delayframe = stdelay;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        x = 1;
        matrix->setFont( &Century_Schoolbook_L_Bold[fontsize] );
        matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
        matrix->setTextSize(1);
        matrix->setRotation(0);
    }

    if (--delayframe) {
        // reset how long a frame is shown before we switch to the next one
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
        return repeat;
    }
    delayframe = stdelay;

    matrix->clear();
    matrix->setCursor(MATRIX_WIDTH-len*fontwidth*1.5 + x, fontsize+6);
    matrix->setPassThruColor(Wheel(map(x, 0, len*fontwidth, 0, 512)));
    matrix->print(str1);

    matrix->setCursor(MATRIX_WIDTH-x, MATRIX_HEIGHT-1);
    matrix->setPassThruColor(Wheel(map(x, 0, len*fontwidth, 512, 0)));
    matrix->print(str2);
    matrix->setPassThruColor();
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

uint8_t panOrBounceBitmap (uint32_t choice) {
    static uint16_t state;
    uint16_t x, y;
    const uint16_t *bitmap;
    uint16_t bitmapSize;

    if (choice == 1) {
        bitmap = bitmap24;
        bitmapSize = 24;
    } else {
        // FIXME, put something else here
        bitmap = bitmap24;
        bitmapSize = 24;
    }

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
uint8_t GifAnim(uint32_t idx) {
    uint16_t x, y;
    uint8_t repeat = 1;
    static int8_t scrollx = 0;
    static int8_t scrolly = 0;
    struct Animgif {
        const char *path;
        uint16_t looptime;
        int8_t offx;
        int8_t offy;
        int8_t factx;
        int8_t facty;
        int8_t scrollx;
        int8_t scrolly;
    };
    extern int OFFSETX;
    extern int OFFSETY;
    extern int FACTY;
    extern int FACTX;

    #if mheight == 32
    const Animgif animgif[] = {
            {"/gifs/32anim_photon.gif",		10, -4, 0, 10, 10, 0, 0 },	// 70
            {"/gifs/32anim_flower.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/32anim_balls.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/32anim_dance.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/circles_swap.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/concentric_circles.gif",	10, -4, 0, 10, 10, 0, 0 },	// 75
            {"/gifs/corkscrew.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/cubeconstruct.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/cubeslide.gif",		10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/runningedgehog.gif",	10, -4, 0, 10, 10, 0, 0 },
            {"/gifs/triangles_in.gif",		10, -4, 0, 10, 10, 0, 0 },	// 80
            {"/gifs/wifi.gif",			10, -4, 0, 10, 10, 0, 0 },
    };
    #elif mheight == 192
    #define ROOT FS_PREFIX "/gifs128x192/"
    const Animgif animgif[] = {
            { ROOT  "abstract_colorful.gif",		10, 0, 0, 10, 10, 0, 0 }, // 70
            { ROOT  "Aki5PC6_Running.gif",		10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "dancing_lady.gif",			10,-32,0, 15, 15, 0, 0 }, 
            { ROOT  "GirlSexyAnimateddance.gif",	10,-32,0, 15, 15, 0, 0 }, 
            { ROOT  "z_13xS_green_aurora.gif",		10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_19Ri_multi_aurora.gif",		10, 0, 0, 10, 10, 0, 0 }, // 75
            { ROOT  "z_19Ro_city_aurora.gif",		10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_1AYl_DJ.gif",			10, 0, 0,  8, 10, 0, 0 }, 
            { ROOT  "z_1Fvr_color_string_spirals.gif",	10,-20,0,  9, 10, 0, 0 }, 
            { ROOT  "z_1KO9_orange_shapes_spinout.gif",	10,-8, 0, 10, 10, 0, 0 },        
            { ROOT  "z_1zfD_3rdeye_spin.gif",		10,-32,0, 10, 10, 0, 0 }, // 80 
            { ROOT  "z_24RD_8_fish_spirals.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_2Htr_caniche.gif",		10, 0, 0,  7, 10, 0, 0 }, 
            { ROOT  "z_2mue_yingyang.gif",		10, 0,-52,10, 15, 0, 0 }, 
            { ROOT  "z_2QeW_color_stars_flash.gif",	10, 0, 0,  7, 10, 0, 0 },        
            { ROOT  "z_2unv_dancing_pink_back.gif",	10,10, 0, 10, 10, 0, 0 }, // 85 
            { ROOT  "z_2vCo_triangle_merge.gif",	10, 0, 0,  7, 10, 0, 0 }, 
            { ROOT  "z_2zFo_green_hal9000.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_37Ec_bird_dance.gif",		10, 0, 0,  8, 10, 0, 0 }, 
            { ROOT  "z_3bUjj_concentric_lights.gif",	10,-32,0, 10, 10, 0, 0 },        
            { ROOT  "z_3F3F_sonic.gif",			10, 0, 0,  7, 10, 0, 0 }, // 90 
            { ROOT  "z_3Mel_spiral_pentagon_dance.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_3Ppu_spin_triangles.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_3Qqj_double_stargate.gif",	10,-20,-16,9, 12, 0, 0 }, 
            { ROOT  "z_3Wfu_RGB_smirout.gif",		10,-32,0, 10, 10, 0, 0 },        
            { ROOT  "z_3wQM_fractal_zoom.gif",		10,-24,0,  8, 10, 0, 0 }, // 95
            { ROOT  "z_3zO_pacman.gif",			40,-12,0,  8, 10, 0, 0 }, 
            { ROOT  "z_47Vg_purple_hair_spiralout.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_4P4a_flip_triangles.gif",	10,-20,0,  8, 10, 0, 0 }, 
            { ROOT  "z_4RNd_rgb_color_plates.gif",	10,-24,0, 10, 10, 0, 0 },        
            { ROOT  "z_4RNj_red_round_unfold.gif",	10,-12,0,  8, 10, 0, 0 }, // 100
            { ROOT  "z_4RNm_triangrect_shapes_out.gif",	10,-32,0, 10, 10, 0, 0 },
            { ROOT  "z_5PmD_MJ_moonwalk.gif",		40, 2, 0, 12, 10, 0, 0 },
            { ROOT  "z_5x_light_tunnel.gif",		10, 0, 0,  7, 10, 0, 0 },
            { ROOT  "z_6nr_heart_rotate.gif",		10, 0, 0,  6, 10, 0, 0 },
            { ROOT  "z_6PLP_colorflowers_spiralout.gif",10,-32,0, 10, 10, 0, 0 }, // 105
            { ROOT  "z_72f8_green_mobius_rotate.gif",	10, 0, 0,  8, 10, 0, 0 },
            { ROOT  "z_72fP_cauliflower.gif",		10,-32,0, 10, 10, 0, 0 },
            { ROOT  "z_72gi_triple_3D_smiley.gif",	10,-8, 0,  7, 10, 0, 0 },
            { ROOT  "z_73O8_lightman_running.gif",	10, 0, 0,  7, 10, 0, 0 },
            { ROOT  "z_75ks_green_zoomout_lasers.gif",	10, 0, 0,  7, 10, 0, 0 }, // 110
            { ROOT  "z_75yI_BW_spiral_out.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_76dA_starship_shooting.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_78jz_blue_smoke_out.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_ZIb4_red_jacket_dancer.gif",	10,-16,0, 10, 10, 0, 0 }, 
            { ROOT  "z_7gRx_white_grey_smoke.gif",	10,-32,0, 10, 10, 0, 0 }, // 115
            { ROOT  "z_7Gtw_flowers_spinout.gif",	10,-24,0, 10, 10, 0, 0 }, 
            { ROOT  "z_7IgI_colors_pulsing_in_out.gif", 10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_7IGR_red_smoke_spiral_in.gif",	10, 0, 0,  7, 10, 0, 0 }, 
            { ROOT  "z_7MS3_grey_cubes_tunnel.gif",	10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_7OEb_blue_amber_juggler.gif",	10,-32,0, 10, 10, 0, 0 }, // 120 
            { ROOT  "z_7rq5_flying_through_pipes.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_7SHB_blue_robot_heart.gif",	10,-12,0,  8, 10, 0, 0 }, 
            { ROOT  "z_7sXr_3D_Mobius_loop.gif",	10, 0, 0,  7, 10, 0, 0 }, 
            { ROOT  "z_7U4_endless_corridor.gif",	10, 0, 0,  7, 10, 0, 0 },       
            { ROOT  "z_7VA_pulp_fiction.gif",		10, 0, 0, 10, 12, 0, 0 }, // 125 
            { ROOT  "z_7xyP_BW_zoomout_gears.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_7ZNJ_RGB_toroid.gif",		10,-12,0,  8, 10, 0, 0 }, 
            { ROOT  "z_8vFu_mushroom_spots.gif",	10,-8, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_ZJtC_you_rock.gif",		10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_9Cre_morphing_3D_shape.gif",	10,-24,0,  8, 10, 0, 0 }, // 130 
            { ROOT  "z_9vQE_flower_petals.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_9xyv_eatme.gif",			10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_A8u8_sparkling_spiralin.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_Ab6r_spingout_RGB.gif",		10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_Abml_green_cube_mobius.gif",	10,-32,0, 10, 10, 0, 0 }, // 135
            { ROOT  "z_Ajyj_3D_green_wheel_ridge.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_AOkf_colorspiral_zoomout.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_B0Te_spinning_dancer.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_B87j_color_projectors.gif",	10, 0, 0, 10, 10, 0, 0 },       
            { ROOT  "z_ZLnU_sailormoon_highdance.gif",	10, 0,-12,10, 10, 0, 0 }, // 140     
            { ROOT  "z_bh8_smiling_dancing_girl.gif",	10, 0, 0, 10, 10, 0, 0 },
            { ROOT  "z_CDno_coiling_fern.gif",		10,-32,0, 10, 10, 0, 0 },
            { ROOT  "z_Cf03_yellow_lighthouse.gif",	10,-20,0, 10, 10, 0, 0 },
            { ROOT  "z_CuQ9_color_pyramids.gif",	10, 0, 0, 10, 10, 0, 0 },
            { ROOT  "z_DemL_tunnel_spark_dancer.gif",	10,-12,0,  8, 10, 0, 0 }, // 145
            { ROOT  "z_feM_MJ_moonwalk_spin.gif",	10,-12,0, 10, 12, 0, 0 }, 
            { ROOT  "z_fxac_hyperspace.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_fxcA_conifer_zoom_in.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_fxcE_3D_hypercube_RGB.gif",	10,-24,0,  9, 10, 0, 0 },       
            { ROOT  "z_fxmf_grapefuit_zoomin.gif",	10,-32,0, 10, 10, 0, 0 }, // 150
            { ROOT  "z_fxVE_pink_flaming_circle.gif",	10,-4, 0,  7, 10, 0, 0 }, 
            { ROOT  "z_fxYU_center_moving_spiral.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_fyE2_hypnotoad.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_fyNK_pizza_zoomin.gif",		10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_fypc_RGB_spiralin.gif",		10,-32,0, 10, 10, 0, 0 }, // 155
            { ROOT  "z_fype_bluebee_zoomin.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_FZl2_green_neutron_star.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_g09P_clock_in.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_g0Af_piano_zoomin.gif",		10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_g0bg_puzzle_spiralout.gif",	10,-32,0, 10, 10, 0, 0 }, // 160
            { ROOT  "z_g0HL_shiny_snail_shell.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_g0RQ_hypercube.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_g1Jh_color_dots_spiralin.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_g1mN_3D_fractal_roll.gif",	10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_g3HP_Kaleidoscope_spiral.gif",	10,-32,0, 10, 10, 0, 0 }, // 165
            { ROOT  "z_IAkQ_acid_cat.gif",		10,-4, 0,  9, 10, 0, 0 }, 
            { ROOT  "z_IrES_purple_geometrical.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_K5bj_fly_buildings.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_K5bn_pulsing_color_rects.gif",	10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_KTLf_white_geometric_out.gif",	10,-32,0, 10, 10, 0, 0 }, // 170
            { ROOT  "z_MDdU_color_marble.gif",		10, 0,32, 10, 10, 0, 0 }, // FIXME colors
            { ROOT  "z_MDkc_passionfruit_zoomout.gif",	10,-32,0, 10, 10, 0, 0 },
            { ROOT  "z_Mdml_I_am_drugs.gif",		10, 0, 0, 10, 10, 0, 0 },
            { ROOT  "z_Nfhn_smileys_spinout.gif",	10, 0, 0, 10, 10, 0, 0 }, // FIXME colors
            { ROOT  "z_NTHQ_flyin_cavern.gif",		10,-32,0, 10, 10, 0, 0 }, // 175
            { ROOT  "z_Z6W2_mario_mushroom_dance.gif",	10,-4, 0,  9, 10, 0, 0 }, 
            { ROOT  "z_NUYV_baby_pig_fall.gif",		10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_O7TJ_color_shapes_out.gif",	10, 0, 0,  7, 10, 0, 0 }, 
            { ROOT  "z_OBYz_blue_cubes_flyin.gif",	10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_OwRt_triangles_RGB_out.gif",	10,-32,0, 10, 10, 0, 0 }, // 180
            { ROOT  "z_Oz2e_rubiks_cube.gif",		10,-16,0,  9, 10, 0, 0 },
            { ROOT  "z_P8P_fractal2_zoom.gif",		10, 0,15, 10, 10, 0, 0 },
            { ROOT  "z_PAM_color_sticks.gif",		10,-32,0, 10, 10, 0, 0 },
            { ROOT  "z_PVyt_mushroom_walk.gif",		10,-4, 0,  7, 10, 0, 0 },
            { ROOT  "z_PYZK_BW_bubbles.gif",		10,-32,0, 10, 10, 0, 0 }, // 185
            { ROOT  "z_SBMP_red_smoke.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_T2wm_flying_grass.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_tIH_blue_gecko_dance.gif",	10,-12,0,  9, 10, 0, 0 }, 
            { ROOT  "z_TlCL_dancing_flames.gif",	10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_VCq8_skeleton.gif",		10, 0, 0, 10, 10, 0, 0 }, // 190
            { ROOT  "z_VRfm_fly_purple_gates.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_WGkW_bluelady_smoke.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_WMDv_sailor_moon.gif",		10,-20,0,  9, 10, 0, 0 }, 
            { ROOT  "z_WSK_inca_spiralin.gif",		10,-32,0, 10, 10, 0, 0 },       
            { ROOT  "z_WUUT_eye.gif",			10,-32,0, 10, 10, 0, 0 }, // 195
            { ROOT  "z_XiPu_blue_shark_dance.gif",	10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_XqyP_blue_dancer.gif",		10,-32,0, 10, 10, 0, 0 }, 
            { ROOT  "z_XwIB_snoopdog_dance.gif",	10, 0, 0, 10, 10, 0, 0 }, 
            { ROOT  "z_Ysrm_walking_dead.gif",		10, 0, 0,  7, 10, 0, 0 },       
            { ROOT  "z_Yv30_street_fighter.gif",	10, 0, 0, 10, 10, 0, 0 }, // 200
    };                                                                            
    #else
    #ifdef ARDUINOONPC
    #define ROOT FS_PREFIX
    #else
    #define ROOT ""
    #endif
    const Animgif animgif[] = {
            { ROOT    "/gifs64/087_net.gif",		05, 0, 0, 10, 15, 0, 0 }, // 70
            { ROOT    "/gifs64/196_colorstar.gif",	10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/200_circlesmoke.gif",	10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/203_waterdrop.gif",	10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/210_circletriangle.gif",	10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/215_fallingcube.gif",	15, 0, 0, 10, 15, 0, 0 }, // 75
            { ROOT    "/gifs64/255_photon.gif",		10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/257_mesh.gif",		20, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/271_mj.gif",		15,-14,3, 15, 15, 0, 0 }, 
            { ROOT    "/gifs64/342_spincircle.gif",	20, 0, 0, 10, 15, 0, 0 },
            { ROOT    "/gifs64/401_ghostbusters.gif",	05, 0, 0, 10, 15, 0, 0 }, // 80 
            { ROOT    "/gifs64/444_hand.gif",		10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/469_infection.gif",	05, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/193_redplasma.gif",	10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/208_dancers.gif",	25, 0, 0, 10, 15, 0, 0 },
            { ROOT    "/gifs64/284_comets.gif",		15, 0, 0, 10, 15, 0, 0 }, // 85 
            { ROOT    "/gifs64/377_batman.gif",		07, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/412_cubes.gif",		20, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/236_spintriangle.gif",	20, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/226_flyingfire.gif",	10, 0, 0, 10, 15, 0, 0 },
            { ROOT    "/gifs64/264_expandcircle.gif",	10, 0, 0, 10, 15, 0, 0 }, // 90 
            { ROOT    "/gifs64/281_plasma.gif",		20, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/286_greenplasma.gif",	15, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/291_circle2sphere.gif",	15, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/364_colortoroid.gif",	25, 0, 0, 10, 15, 0, 0 },
            { ROOT    "/gifs64/470_scrollcubestron.gif",25, 0, 0, 10, 15, 0, 0 }, // 95
            { ROOT    "/gifs64/358_spinningpattern.gif",10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/328_spacetime.gif",	20, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/218_circleslices.gif",	10, 0, 0, 10, 15, 0, 0 }, 
            { ROOT    "/gifs64/heartTunnel.gif",	10, 0, 0, 10, 15, 0, 0 },
            { ROOT    "/gifs64/sonic.gif",		10, 0, 0, 10, 15, 0, 0 }, // 100
	    // -- non animated, those scroll up/down
            { ROOT    "/gifs64/ab1_colors.gif",		10, 0, 0, 10, 10, 64, 64 },
            { ROOT    "/gifs64/ab2_lgrey.gif",		10, 0, 0, 10, 10, 64, 64 }, // AnB sign light grey
            { ROOT    "/gifs64/ab2_grey.gif",		10, 0, 0, 10, 10, 64, 64 }, // AnB sign grey - skip
            { ROOT    "/gifs64/ab2_white.gif",		10, 0, 0, 10, 10, 64, 64 }, // AnB sign white - skip
            { ROOT    "/gifs64/ab3_s.gif",		10, 0, 0, 10, 10, 64, 64 }, // 105 color lines
            { ROOT    "/gifs64/ab4_g.gif",		10, 0, 0, 10, 10, 64, 64 }, // AnB logo
            { ROOT    "/gifs64/ab4_w.gif",		10, 0, 0, 10, 10, 64, 64 }, // AnB logo white - skip
            { ROOT    "/gifs64/BM_Man_Scroll.gif",	10, 0, 0, 10, 10, 0, 0 },  // 108
            { ROOT    "/gifs64/BM_green_arms.gif",	10, -12, 0, 10, 10, 36, 64 },
            { ROOT    "/gifs64/BM_lady_fire.gif",	10, 0, 0, 10, 10, 64, 64 },	// 110
            { ROOT    "/gifs64/BM_logo.gif",		10, 0, 0, 10, 10, 64, 64 },
            { ROOT    "/gifs64/BM_TheMan_Blue.gif",	10, -12, -2, 10, 10, 36, 64 },    // 112
    };
    #endif
    gif_cnt = ARRAY_SIZE(animgif);
    // Compute gif_cnt and exit
    if (idx == 255) return 0;
    // Avoid crashes due to overflows
    idx = idx % gif_cnt;
    static uint8_t gifloopsec;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        gifloopsec =  animgif[idx].looptime;
        OFFSETX = animgif[idx].offx;
        OFFSETY = animgif[idx].offy;
        FACTX =   animgif[idx].factx;
        FACTY =   animgif[idx].facty;
        scrollx = animgif[idx].scrollx;
        scrolly = animgif[idx].scrolly;
        matrix->clear();
        panOrBounce(&x, &y, scrollx, scrolly, true);
        // exit if the gif animation couldn't get setup.
        if (sav_newgif(animgif[idx].path)) return 0;
    }

    if (scrollx || scrolly) {
        panOrBounce(&x, &y, scrollx, scrolly);
        OFFSETX = animgif[idx].offx + x;
        OFFSETY = animgif[idx].offy + y;
        matrix->clear();
        //Serial.print(x);
        //Serial.print(" ");
        //Serial.println(y);
    }
    // sav_loop may or may not run show() depending on whether
    // it's time to decode the next frame. If it did not, wait here to
    // add the matrix_show() delay that is expected by the caller
    //bool savl = sav_loop();
    sav_loop();
    // Not needed anymore with Aiko, it handles calling frequency
    //if (savl) { delay(MX_UPD_TIME); };

    EVERY_N_SECONDS(1) { 
        Serial.print(gifloopsec); Serial.print(" ");
        if (!gifloopsec--) { Serial.println(); return 0; };
    }
    return repeat;
}

uint8_t scrollBigtext(uint32_t unused) {
    // 64x96 pixels, chars are 5(6)x7, 10.6 chars across, 13.7 lines of displays
    static uint16_t state = 0;
    static uint8_t resetcnt = 1;
    uint16_t loopcnt = 700;
    uint16_t x, y;

    unused = unused;

    static const char* text[] = {
        "if (reset) {",
        "  xf = max(0, (mw-sizeX)/2) << 4;",
        "  yf = max(0, (mh-sizeY)/2) << 4;",
        "  xfc = 6; yfc = 3; xfdir = -1; yfdir = -1; }",
        "bool changeDir = false;",
        "// Get actual x/y by dividing by 16.",
        "*x = xf >> 4; *y = yf >> 4;",
        "// Only pan if the display size is smaller than the pixmap",
        "// but not if the difference is too small or it'll look bad.",
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

uint8_t demoreel100(uint32_t demo) {
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
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
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
            int8_t dothue = 0;
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


uint8_t call_twinklefox(uint32_t unused)
{
    static uint16_t state;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        state = 0;
    }

    twinkle_loop();
    if (state++ < 1000) return 2;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_pride(uint32_t unused)
{
    static uint16_t state;

    unused = unused;

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

uint8_t call_fireworks(uint32_t unused) {
    static uint16_t state;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        matrix->clear();
        state = 0;
    }

    fireworks();
    matrix_show();
    if (state++ < 500) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_fire(uint32_t unused) {
    static uint16_t state;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        matrix->clear();
        state = 0;
    }

    // rotate palette
    sublime_loop();
    fire();
    matrix_show();
    if (state++ < 1500) return 1;
    matrix_reset_demo = 1;
    return 0;
}

uint8_t call_rain(uint32_t which) {
    #define raindelay 2
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
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
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

uint8_t call_pacman(uint32_t unused) {
    uint8_t loopcnt = 3;
    #define pacmandelay 5
    static uint16_t delayframe = pacmandelay;

    unused = unused;

    if (matrix_reset_demo == 1) {
        matrix_reset_demo = 0;
        pacman_setup(loopcnt);
    }

    if (--delayframe) {
        // reset how long a frame is shown before we switch to the next one
        //Serial.print("delayed frame ");
        //Serial.println(delayframe);
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
        return 1;
    }
    delayframe = pacmandelay;

    if (pacman_loop()) return 1;
    matrix_reset_demo = 1;
    return 0;
}

// Adapted from	LEDText/examples/TextExample3 by Aaron Liddiment
// bright and annoying, I took it down to just a very quick show.
uint8_t plasma(uint32_t unused) {
    #define PLASMA_X_FACTOR  24
    #define PLASMA_Y_FACTOR  24
    static uint16_t PlasmaTime, PlasmaShift;
    uint16_t OldPlasmaTime;

    unused = unused;

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

uint8_t tmed(uint32_t demo) {
    // 0 to 12
    // add new demos at the end or the number selections will be off
    // make sure 77 runs long enough
    const uint16_t tmed_mapping[][3] = {
        {   4, 2, 200 },  // 00 concentric colors and shapes
        {  10, 5, 300 },  //    5 color windows-like pattern with circles in and out
        {  11, 5, 300 },  //    color worm patterns going out with circles zomming out
        {  25, 3, 500 },  //    5 circles turning together, run a bit longer
        {  52, 5, 300 },  //    rectangles/squares/triangles zooming out
        {  60, 6, 200 },  // 05 opposite concentric colors and shapes (52 reversed)
        {  62, 6, 200 },  //    double color starfield with shapes in/out
        {  67, 5, 900 },  //    two colors swirling bigger, creating hypno pattern
        {  70, 6, 200 },  //    4 fat spinning comets with shapes growing from middle sometimes
        {  77, 5, 300 },  //    streaming lines of colored pixels with shape zooming in or out
        {  80, 5, 200 },  // 10 rotating triangles of color
        { 104, 6, 200 },  //    circles mixing in the middle
        { 105, 2, 400 },  //    hypno

    };
//	{  29, 5, 300 },  // XX swirly RGB colored dots meant to go to music
//	{  34, 5, 300 },  // XX single colored lines that extend from center, meant for music
//	{  73, 3, 2000 }, // XX circle inside fed by outside attracted color dots
//	{ 110, 5, 300 },  // XX bubbles going up or right

    uint8_t dfinit = tmed_mapping[demo][1];
    uint16_t loops = tmed_mapping[demo][2];
    static uint8_t delayframe = dfinit;
    demo = tmed_mapping[demo][0];

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
        // Remove delay on pattern only based on ringer/bkringer
        case 60:
          ringdelay = 0;
          bringdelay = 0;
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
        //delay(MX_UPD_TIME); Aiko emulates this delay for us
        return 2;
    }
    delayframe = dfinit;

    switch (demo) {
    case 4:
      Roundhole();
      if (flip3)
        bkstarer();
      else
        bkringer();
      // boxer();
      adjuster();
      break;
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
    case 52:
        rmagictime();
        bkboxer();
        starer();
        if (flip && !flip2) adjuster();
        break;
    case 60:
      if (flip3)
        bkringer();
      else
        bkboxer();
      if (flip2)
        ringer();
      else
        starer();
      break;
    case 62:
      // MM FLAGS
      flip3 = 1; // I don't like confetti2
      if (flip)
        boxer();
      else
        ringer();
      if (flip2)
        bkstarer();
      else
        bkboxer();
      if (flip3)
        warp();
      else {
        confetti2();
        adjuster();
      }
      break;
    case 67:
        // MM FLAGS
        flip = 0; // force whitewarp
        //flip2 controls white or color
        flip3 = 0; // force whitewarp

        hypnoduck2();
        break;
    case 70:
        // MM FLAGS
        if (!flip2) flip3 = 1;

        if (flip2) boxer(); // outward going box
        else if (flip3) bkringer(); // back collapsing circles
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
    case 104:
        circlearc();
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



// ================================================================================

typedef struct demo_entry_ {
    const char *name;
    uint8_t (*func)(uint32_t);
    int arg;
    // what is the index for this demo (they are not played in order)
    uint16_t position;
} Demo_Entry;

Demo_Entry demo_list[DEMO_ARRAY_SIZE] = {
/* 00 */ { "", NULL, -1, 0 },
/* 01 */ { "Squares In",  squares, 0, 0 },
/* 02 */ { "Squares Out", squares, 1, 0 },
/* 03 */ { "EatSleepRaveBurnRepeat", esrbr, -1, 0 },
/* 04 */ { "EatSleepRaveBurnRepeat Fade", esrbr_fade, -1, 0 },
/* 05 */ { "TFSF Zoom InOut", tfsf_zoom, 1, 0 },
/* 06 */ { "TFSF Display", tfsf, -1, 0 },
/* 07 */ { "With Every Beat...", webwc, -1, 0 },
/* 08 */ { "Burn Baby Burn", bbb, -1, 0 },
/* 09 */ { "Trance Jesus Do", trancejesus, -1, 0 },
/* 10 */ { "", NULL, -1, 0 },
/* 11 */ { "", NULL, -1, 0 },
/* 12 */ { "", NULL, -1, 0 },
/* 13 */ { "", NULL, -1, 0 },
/* 14 */ { "Safety Third", DoublescrollText, 1, 0 },    // adjusts // fixme I love LEDs
/* 15 */ { "ScrollBigtext", scrollBigtext, -1, 0 },     // code of scrolling code
/* 16 */ { "Bounce Smiley", panOrBounceBitmap, 1, 0 },  // currently only 24x32
/* 17 */ { "Fireworks", call_fireworks, -1, 0 },
/* 18 */ { "TwinkleFox", call_twinklefox, -1, 0 },
/* 19 */ { "Pride", call_pride, -1, 0 },		    // not nice for higher res (64 and above)
/* 20 */ { "Demoreel Stars", demoreel100, 1, 0 },	    // Twinlking stars
/* 21 */ { "Demoreel Sweeper", demoreel100, 2, 0 },    // color changing pixels sweeping up and down
/* 22 */ { "Demoreel Dbl Sweeper", demoreel100, 3, 0 },// colored pixels being exchanged between top and bottom
/* 23 */ { "Matrix", call_rain, 1, 0 },		    // matrix
/* 24 */ { "Storm", call_rain, 3, 0 },		    // clouds, rain, lightening
/* 25 */ { "Pac Man", call_pacman, -1, 0 },	    // currently only designed for 24x32
/* 26 */ { "Plasma", plasma, -1, 0 },
/* 27 */ { "Fire", call_fire, -1, 0 },
/* 28 */ { "", NULL, -1, 0 },
/* 29 */ { "", NULL, -1, 0 },
/* 30 */ { "Aurora Attract", aurora,  0, 0 },
/* 31 */ { "Aurora Bounce", aurora,  1, 0 },
/* 32 */ { "Aurora Cube", aurora,  2, 0 },
/* 33 */ { "Aurora Flock", aurora,  3, 0 },
/* 34 */ { "Aurora Flowfield", aurora,  4, 0 },
/* 35 */ { "Aurora Incremental Drift", aurora,  5, 0 },
/* 36 */ { "Aurora Incremental Drift2", aurora,  6, 0 },
/* 37 */ { "Aurora Pendulum Wave ", aurora,  7, 0 },
/* 38 */ { "Aurora Radar", aurora,  8, 0 },	    // 8 not great on non square
/* 39 */ { "Aurora Spiral/Line Screensave", aurora,  9, 0 },
/* 40 */ { "Aurora Spiro", aurora, 10, 0 },
/* 41 */ { "Aurora Swirl", aurora, 11, 0 },	    // 11 not great on bigger display
/* 42 */ { "Aurora Wave", aurora, 12, 0 },
/* 43 */ { "", NULL, -1, 0 },
/* 44 */ { "", NULL, -1, 0 },
/* 45 */ { "TMED  0 Zoom in shapes", tmed,  0, 0 }, // concentric colors and shapes
/* 46 */ { "TMED  1 Concentric circles", tmed,  1, 0 }, // 5 color windows-like pattern with circles in and out
/* 47 */ { "TMED  2 Color Starfield", tmed,  2, 0 }, // color worm patterns going out with circles zomming out
/* 48 */ { "TMED  3 Dancing Circles", tmed,  3, 0 }, // 5 circles turning together, run a bit longer
/* 49 */ { "TMED  4 Zoom out Shapes", tmed,  4, 0 }, // rectangles/squares/triangles zooming out
/* 50 */ { "TMED  5 Shapes In/Out", tmed,  5, 0 }, // opposite concentric colors and shapes (52 reversed)
/* 51 */ { "TMED  6 Double Starfield&Shapes", tmed,  6, 0 }, // double color starfield with shapes in/out
/* 52 */ { "TMED  7 Hypnoswirl Starfield", tmed,  7, 0 }, // two colors swirling bigger, creating hypno pattern
/* 53 */ { "TMED  8 4 Dancing Balls&Shapes", tmed,  8, 0 }, // 4 fat spinning comets with shapes growing from middle sometimes
/* 54 */ { "TMED  9 Starfield BKringer", tmed,  9, 0 }, // streaming lines of colored pixels with shape zooming in or out
/* 55 */ { "TMED 10 Spinning Triangles", tmed, 10, 0 }, // rotating triangles of color
/* 56 */ { "TMED 11 Circles Mixing", tmed, 11, 0 }, // circles mixing in the middle
/* 57 */ { "TMED 12 Hypno", tmed, 12, 0 }, // hypno
/* 58 */ { "", NULL, -1, 0 },
/* 59 */ { "", NULL, -1, 0 },
/* 60 */ { "", NULL, -1, 0 },
/* 61 */ { "", NULL, -1, 0 },
/* 62 */ { "", NULL, -1, 0 },
/* 63 */ { "", NULL, -1, 0 },
/* 64 */ { "", NULL, -1, 0 },
/* 65 */ { "", NULL, -1, 0 },
/* 66 */ { "", NULL, -1, 0 },
/* 67 */ { "", NULL, -1, 0 },
/* 68 */ { "", NULL, -1, 0 },
/* 69 */ { "", NULL, -1, 0 },
#if mheight == 32
/* 70 */ { "GIF photon"		, GifAnim,  0, 0 },
/* 71 */ { "GIF flower"		, GifAnim,  1, 0 },
/* 72 */ { "GIF balls"	    	, GifAnim,  2, 0 },
/* 73 */ { "GIF dance"		, GifAnim,  3, 0 },
/* 74 */ { "GIF circles_swap"	, GifAnim,  4, 0 },
/* 75 */ { "GIF concentric_circles", GifAnim,  5, 0 },
/* 76 */ { "GIF corkscrew"	, GifAnim,  6, 0 },
/* 77 */ { "GIF cubeconstruct"	, GifAnim,  7, 0 },
/* 78 */ { "GIF cubeslide"	, GifAnim,  8, 0 },
/* 79 */ { "GIF runningedgehog"	, GifAnim,  9, 0 },
/* 80 */ { "GIF triangles_in"	, GifAnim, 10, 0 },
/* 81 */ { "GIF wifi"		, GifAnim, 11, 0 },
#elif mheight == 96
/* 70 */ { "GIF net"		, GifAnim,  0, 0 },
/* 71 */ { "GIF colorstar"	, GifAnim,  1, 0 },
/* 72 */ { "GIF circlesmoke"	, GifAnim,  2, 0 },
/* 73 */ { "GIF waterdrop"	, GifAnim,  3, 0 },
/* 74 */ { "GIF circletriangle"	, GifAnim,  4, 0 },
/* 75 */ { "GIF fallingcube"	, GifAnim,  5, 0 },
/* 76 */ { "GIF photon"		, GifAnim,  6, 0 },
/* 77 */ { "GIF mesh"		, GifAnim,  7, 0 },
/* 78 */ { "GIF Michael Jackson", GifAnim,  8, 0 },
/* 79 */ { "GIF spincircle"	, GifAnim,  9, 0 },
/* 80 */ { "GIF ghostbusters"	, GifAnim, 10, 0 },
/* 81 */ { "GIF hand"		, GifAnim, 11, 0 },
/* 82 */ { "GIF infection"	, GifAnim, 12, 0 },
/* 83 */ { "GIF redplasma"	, GifAnim, 13, 0 },
/* 84 */ { "GIF dancers"	, GifAnim, 14, 0 },
/* 85 */ { "GIF comets"		, GifAnim, 15, 0 },
/* 86 */ { "GIF batman"		, GifAnim, 16, 0 },
/* 87 */ { "GIF cubes"		, GifAnim, 17, 0 },
/* 88 */ { "GIF spintriangle"	, GifAnim, 18, 0 },
/* 89 */ { "GIF flyingfire"	, GifAnim, 19, 0 },
/* 90 */ { "GIF expandcircle"	, GifAnim, 20, 0 },
/* 91 */ { "GIF plasma"		, GifAnim, 21, 0 },
/* 92 */ { "GIF greenplasma"	, GifAnim, 22, 0 },
/* 93 */ { "GIF circle2sphere"	, GifAnim, 23, 0 },
/* 94 */ { "GIF colortoroid"	, GifAnim, 24, 0 },
/* 95 */ { "GIF scrollcubestron", GifAnim, 25, 0 },
/* 96 */ { "GIF spinningpattern", GifAnim, 26, 0 },
/* 97 */ { "GIF spacetime"	, GifAnim, 27, 0 },
/* 98 */ { "GIF circleslices"	, GifAnim, 28, 0 },
/* 99 */ { "GIF heartTunnel"	, GifAnim, 29, 0 },
/*100 */ { "GIF sonic"		, GifAnim, 30, 0 },
/*101 */ { "GIF AB colors circles", GifAnim, 31, 0 },
/*102 */ { "GIF A&B lgrey"	, GifAnim, 32, 0 },
/*103 */ { "GIF A&B greyer"	, GifAnim, 33, 0 },
/*104 */ { "GIF A&B brightwhite", GifAnim, 34, 0 },
/*105 */ { "GIF AB colorstrings", GifAnim, 35, 0 },
/*106 */ { "GIF ABlogo Grey"	, GifAnim, 36, 0 },
/*107 */ { "GIF ABlogo White"	, GifAnim, 37, 0 },
/*108 */ { "GIF BM Man Scroll"	, GifAnim, 38, 0 },
/*109 */ { "GIF BM green arms"	, GifAnim, 39, 0 },
/*110 */ { "GIF BM lady fire"	, GifAnim, 40, 0 },
/*111 */ { "GIF BM logo"	, GifAnim, 41, 0 },
/*112 */ { "GIF BM TheMan Blue"	, GifAnim, 42, 0 },
/*113 */ { "", NULL, -1, 0 },
/*114 */ { "", NULL, -1, 0 },
/*115 */ { "", NULL, -1, 0 },
/*116 */ { "", NULL, -1, 0 },
/*117 */ { "", NULL, -1, 0 },
/*118 */ { "", NULL, -1, 0 },
/*119 */ { "", NULL, -1, 0 },
/*120 */ { "", NULL, -1, 0 },
/*121 */ { "", NULL, -1, 0 },
/*122 */ { "", NULL, -1, 0 },
/*123 */ { "", NULL, -1, 0 },
/*124 */ { "", NULL, -1, 0 },
/*125 */ { "", NULL, -1, 0 },
/*126 */ { "", NULL, -1, 0 },
/*127 */ { "", NULL, -1, 0 },
#else
 /*070 */ { "GIF abstract colorful",	 GifAnim,   0, 0 },
 /*071 */ { "GIF Aki5PC6 Running",	 GifAnim,   1, 0 },
 /*072 */ { "GIF dancing lady",		 GifAnim,   2, 0 },
 /*073 */ { "GIF GirlSexyAnimateddance", GifAnim,   3, 0 },
 /*074 */ { "GIF green aurora",		 GifAnim,   4, 0 },
 /*075 */ { "GIF multi aurora",		 GifAnim,   5, 0 },
 /*076 */ { "GIF city aurora",		 GifAnim,   6, 0 },
 /*077 */ { "GIF DJ",			 GifAnim,   7, 0 },
 /*078 */ { "GIF color string spirals",	 GifAnim,   8, 0 },
 /*079 */ { "GIF orange shapes spinout", GifAnim,   9, 0 },
 /*080 */ { "GIF 3rdeye spin",		 GifAnim,  10, 0 },
 /*081 */ { "GIF 8 fish spirals",	 GifAnim,  11, 0 },
 /*082 */ { "GIF caniche",		 GifAnim,  12, 0 },
 /*083 */ { "GIF yingyang",		 GifAnim,  13, 0 },
 /*084 */ { "GIF color stars flash",	 GifAnim,  14, 0 },
 /*085 */ { "GIF dancing pink back",	 GifAnim,  15, 0 },
 /*086 */ { "GIF triangle merge",	 GifAnim,  16, 0 },
 /*087 */ { "GIF green hal9000",	 GifAnim,  17, 0 },
 /*088 */ { "GIF bird dance",		 GifAnim,  18, 0 },
 /*089 */ { "GIF concentric lights",	 GifAnim,  19, 0 },
 /*090 */ { "GIF sonic",		 GifAnim,  20, 0 },
 /*091 */ { "GIF spiral pentagon dance", GifAnim,  21, 0 },
 /*092 */ { "GIF spin triangles",	 GifAnim,  22, 0 },
 /*093 */ { "GIF double stargate",	 GifAnim,  23, 0 },
 /*094 */ { "GIF RGB smirout",		 GifAnim,  24, 0 },
 /*095 */ { "GIF fractal zoom",		 GifAnim,  25, 0 },
 /*096 */ { "GIF pacmac",		 GifAnim,  26, 0 },
 /*097 */ { "GIF purple hair spiralout", GifAnim,  27, 0 },
 /*098 */ { "GIF flip triangles",	 GifAnim,  28, 0 },
 /*099 */ { "GIF rgb color plates",	 GifAnim,  29, 0 },
 /*100 */ { "GIF red round unfold",	 GifAnim,  30, 0 },
 /*101 */ { "GIF triangrect shapes out", GifAnim,  31, 0 },
 /*102 */ { "GIF MJ moonwalk",		 GifAnim,  32, 0 },
 /*103 */ { "GIF ight tunnel",		 GifAnim,  33, 0 },
 /*104 */ { "GIF heart rotate",		 GifAnim,  34, 0 },
 /*105 */ { "GIF colorflowers spiralout",GifAnim,  35, 0 },
 /*106 */ { "GIF green mobius rotate",	 GifAnim,  36, 0 },
 /*107 */ { "GIF cauliflower",		 GifAnim,  37, 0 },
 /*108 */ { "GIF triple 3D smiley",	 GifAnim,  38, 0 },
 /*109 */ { "GIF lightman running",	 GifAnim,  39, 0 },
 /*110 */ { "GIF green zoomout lasers",	 GifAnim,  40, 0 },
 /*111 */ { "GIF BW spiral out",	 GifAnim,  41, 0 },
 /*112 */ { "GIF starship shooting",	 GifAnim,  42, 0 },
 /*113 */ { "GIF blue smoke out",	 GifAnim,  43, 0 },
 /*114 */ { "GIF red jacket dancer",	 GifAnim,  44, 0 },
 /*115 */ { "GIF white grey smoke",	 GifAnim,  45, 0 },
 /*116 */ { "GIF flowers spinout",	 GifAnim,  46, 0 },
 /*117 */ { "GIF colors pulsing in out", GifAnim,  47, 0 },
 /*118 */ { "GIF red smoke spiral in",	 GifAnim,  48, 0 },
 /*119 */ { "GIF grey cubes tunnel",	 GifAnim,  49, 0 },
 /*120 */ { "GIF blue amber juggler",	 GifAnim,  50, 0 },
 /*121 */ { "GIF flying through pipes",	 GifAnim,  51, 0 },
 /*122 */ { "GIF blue robot heart",	 GifAnim,  52, 0 },
 /*123 */ { "GIF 3D Mobius loop",	 GifAnim,  53, 0 },
 /*124 */ { "GIF endless corridor",	 GifAnim,  54, 0 },
 /*125 */ { "GIF pulp fiction",		 GifAnim,  55, 0 },
 /*126 */ { "GIF BW zoomout gears",	 GifAnim,  56, 0 },
 /*127 */ { "GIF RGB toroid",		 GifAnim,  57, 0 },
 /*128 */ { "GIF mushroom spots",	 GifAnim,  58, 0 },
 /*129 */ { "GIF you rock",		 GifAnim,  59, 0 },
 /*130 */ { "GIF morphing 3D shape",	 GifAnim,  60, 0 },
 /*131 */ { "GIF flower petals",	 GifAnim,  61, 0 },
 /*132 */ { "GIF eatme",		 GifAnim,  62, 0 },
 /*133 */ { "GIF sparkling spiralin",	 GifAnim,  63, 0 },
 /*134 */ { "GIF spingout RGB",		 GifAnim,  64, 0 },
 /*135 */ { "GIF green cube mobius",	 GifAnim,  65, 0 },
 /*136 */ { "GIF 3D green wheel ridge",	 GifAnim,  66, 0 },
 /*137 */ { "GIF colorspiral zoomout",	 GifAnim,  67, 0 },
 /*138 */ { "GIF spinning dancer",	 GifAnim,  68, 0 },
 /*139 */ { "GIF color projectors",	 GifAnim,  69, 0 },
 /*140 */ { "GIF sailormoon highdance",	 GifAnim,  70, 0 },
 /*141 */ { "GIF smiling dancing girl",	 GifAnim,  71, 0 },
 /*142 */ { "GIF coiling fern",		 GifAnim,  72, 0 },
 /*143 */ { "GIF yellow lighthouse",	 GifAnim,  73, 0 },
 /*144 */ { "GIF color pyramids",	 GifAnim,  74, 0 },
 /*145 */ { "GIF tunnel spark dancer",	 GifAnim,  75, 0 },
 /*146 */ { "GIF MJ moonwalk spin",	 GifAnim,  76, 0 },
 /*147 */ { "GIF hyperspace",		 GifAnim,  77, 0 },
 /*148 */ { "GIF conifer zoom in",	 GifAnim,  78, 0 },
 /*149 */ { "GIF 3D hypercube RGB",	 GifAnim,  79, 0 },
 /*150 */ { "GIF grapefuit zoomin",	 GifAnim,  80, 0 },
 /*151 */ { "GIF pink flaming circle",	 GifAnim,  81, 0 },
 /*152 */ { "GIF center moving spiral",	 GifAnim,  82, 0 },
 /*153 */ { "GIF hypnotoad",		 GifAnim,  83, 0 },
 /*154 */ { "GIF pizza zoomin",		 GifAnim,  84, 0 },
 /*155 */ { "GIF RGB spiralin",		 GifAnim,  85, 0 },
 /*156 */ { "GIF bluebee zoomin",	 GifAnim,  86, 0 },
 /*157 */ { "GIF green neutron star",	 GifAnim,  87, 0 },
 /*158 */ { "GIF clock in",		 GifAnim,  88, 0 },
 /*159 */ { "GIF piano zoomin",		 GifAnim,  89, 0 },
 /*160 */ { "GIF puzzle spiralout",	 GifAnim,  90, 0 },
 /*161 */ { "GIF shiny snail shell",	 GifAnim,  91, 0 },
 /*162 */ { "GIF hypercube",		 GifAnim,  92, 0 },
 /*163 */ { "GIF color dots spiralin",	 GifAnim,  93, 0 },
 /*164 */ { "GIF 3D fractal roll",	 GifAnim,  94, 0 },
 /*165 */ { "GIF Kaleidoscope spiral",	 GifAnim,  95, 0 },
 /*166 */ { "GIF acid cat",		 GifAnim,  96, 0 },
 /*167 */ { "GIF purple geometrical",	 GifAnim,  97, 0 },
 /*168 */ { "GIF fly buildings",	 GifAnim,  98, 0 },
 /*169 */ { "GIF pulsing color rects",	 GifAnim,  99, 0 },
 /*170 */ { "GIF white geometric out",	 GifAnim, 100, 0 },
 /*171 */ { "GIF color marble",		 GifAnim, 101, 0 },
 /*172 */ { "GIF passionfruit zoomout",	 GifAnim, 102, 0 },
 /*173 */ { "GIF I am drugs",		 GifAnim, 103, 0 },
 /*174 */ { "GIF smileys spinout",	 GifAnim, 104, 0 },
 /*175 */ { "GIF flyin cavern",		 GifAnim, 105, 0 },
 /*176 */ { "GIF mario mushroom dance",	 GifAnim, 106, 0 },
 /*177 */ { "GIF baby pig fall",	 GifAnim, 107, 0 },
 /*178 */ { "GIF color shapes out",	 GifAnim, 108, 0 },
 /*179 */ { "GIF blue cubes flyin",	 GifAnim, 109, 0 },
 /*180 */ { "GIF triangles RGB out",	 GifAnim, 110, 0 },
 /*181 */ { "GIF rubiks cube",		 GifAnim, 111, 0 },
 /*182 */ { "GIF fractal2 zoom",	 GifAnim, 112, 0 },
 /*183 */ { "GIF color sticks",		 GifAnim, 113, 0 },
 /*184 */ { "GIF mushroom walk",	 GifAnim, 114, 0 },
 /*185 */ { "GIF BW bubbles",		 GifAnim, 115, 0 },
 /*186 */ { "GIF red smoke",		 GifAnim, 116, 0 },
 /*187 */ { "GIF flying grass",		 GifAnim, 117, 0 },
 /*188 */ { "GIF blue gecko dance",	 GifAnim, 118, 0 },
 /*189 */ { "GIF dancing flames",	 GifAnim, 119, 0 },
 /*190 */ { "GIF skeleton",		 GifAnim, 120, 0 },
 /*191 */ { "GIF fly purple gates",	 GifAnim, 121, 0 },
 /*192 */ { "GIF bluelady smoke",	 GifAnim, 122, 0 },
 /*193 */ { "GIF sailor moon",		 GifAnim, 123, 0 },
 /*194 */ { "GIF inca spiralin",	 GifAnim, 124, 0 },
 /*195 */ { "GIF eye",			 GifAnim, 125, 0 },
 /*196 */ { "GIF blue shark dance",	 GifAnim, 126, 0 },
 /*197 */ { "GIF blue dancer",		 GifAnim, 127, 0 },
 /*198 */ { "GIF snoopdog dance",	 GifAnim, 128, 0 },
 /*199 */ { "GIF walking dead",		 GifAnim, 129, 0 },
 /*200 */ { "GIF street fighter",	 GifAnim, 130, 0 },
 /*201 */ { "", NULL, -1, 0 },
 /*202 */ { "", NULL, -1, 0 },
 /*203 */ { "", NULL, -1, 0 },
 /*204 */ { "", NULL, -1, 0 },
 /*205 */ { "", NULL, -1, 0 },
 /*206 */ { "", NULL, -1, 0 },
 /*207 */ { "", NULL, -1, 0 },
 /*208 */ { "", NULL, -1, 0 },
 /*209 */ { "", NULL, -1, 0 },
 /*210 */ { "", NULL, -1, 0 },
#endif
};

// text demos are manually added to the array in setup()
          
void matrix_change(int16_t demo, bool directmap=false) {
    // Reset passthrough from previous demo
    matrix->setPassThruColor();
    // Clear screen when changing demos.
    matrix->clear();
    // this ensures the next demo returns the number of times it should loop
    matrix_loop = -1;
    matrix_reset_demo = 1;

    // demo is the requested demo index in the mapping array or prev/next
    // matrix_state is the index to look up in demo_list array
    // matrix_demo is the actual demo# in demo_list array
    if (directmap) {
        // bypass matrix_state (which will now be incorrect) and point directly
        // to the desired demo without going though demo_mapping array.
        matrix_demo = demo;
        matrix_loop = 100;
    } else {
        if (demo != DEMO_PREV && demo != DEMO_NEXT) matrix_state = demo;

        #ifdef neopixel_pin
        // special one key press where demos are shown forever and next goes back to the normal loop
        if (demo >= 0 && demo < DEMO_TEXT_INPUT) matrix_loop = 9999;
        #endif
        Serial.print("got matrix_change code ");
        Serial.print(demo);
        Serial.print(", switching to index ");
        // special text demos that bypass the array mapping
        if (demo >= DEMO_TEXT_THANKYOU && demo != DEMO_NEXT) {
            matrix_state = demo;
            Serial.print(matrix_state);
            matrix_demo = matrix_state;
        } else {
            do {
                if (demo==DEMO_PREV) if (matrix_state-- == 0) matrix_state = demo_last_idx;
                if (demo==DEMO_NEXT) if (++matrix_state > demo_last_idx) matrix_state = 0;

                matrix_state = (matrix_state % (demo_last_idx+1));
                Serial.print(matrix_state);
                matrix_demo = demo_mapping[matrix_state].mapping;
                if (show_best_demos) {
                    Serial.print(" (bestof mode) ");
                    if (demo_mapping[matrix_state].enabled[panelconfnum] & 2) break;
                } else {
                    Serial.print(" (full mode) ");
                    if (demo_mapping[matrix_state].enabled[panelconfnum] & 1) break;
                }
                // If we're here for a demo # that doesn't exist, looping will not change
                // the demo number, so force a change.
                if (demo != DEMO_PREV && demo != DEMO_NEXT) matrix_state++;
            } while (1);
        }
    }
    Serial.print(", mapped to matrix demo ");
    Serial.print(matrix_demo);
    Serial.print(" (");
    Serial.print(demo_list[matrix_demo].name);
    Serial.print(") loop ");
    Serial.println(matrix_loop);
    #ifndef ARDUINOONPC
        Serial.flush();
        Serial.print("|D:");
        char buf[4];
        sprintf(buf, "%3d", matrix_state);
        Serial.println(buf);
        Serial.flush();
    #endif
}

           

void Matrix_Handler() {
    uint8_t ret;
    Demo_Entry demo_entry = demo_list[matrix_demo];

    if (matrix_demo == DEMO_TEXT_THANKYOU) {
#ifdef M32BY8X3
            const char str[] = "Thank You :)";
            ret = scrollText(str, sizeof(str));
#else
            fixdrawRGBBitmap(28, 87, RGB_bmp, 8, 8);
            ret = display_text("Thank\n  You\n  Very\n Much", 0, 14);
#endif
            if (matrix_loop == -1) matrix_loop = ret;
            if (ret) return;
    } else if (matrix_demo == DEMO_TEXT_INPUT) {
            //ret = scrollText(str, sizeof(str));
            ret = display_text(displaytext.c_str(), 0, 14);
            if (matrix_loop == -1) matrix_loop = ret;
            if (ret) return;
    } else {
        if (! demo_entry.func) {
            Serial.print(">>> ERROR: No demo for ");
            Serial.println(matrix_demo);
            matrix_change(DEMO_NEXT);
            return;
        }

        ret = demo_entry.func(demo_entry.arg);
        if (matrix_loop == -1) matrix_loop = ret;
        if (ret) return;
    }

    matrix_reset_demo = 1;
    Serial.print("Done with demo ");
    Serial.print(matrix_demo);
    Serial.print(" loop ");
    Serial.println(matrix_loop);
    if (--matrix_loop > 0) return;
    matrix_change(DEMO_NEXT);
}

// ---------------------------------------------------------------------------
// Strip Code
// ---------------------------------------------------------------------------

#ifdef NEOPIXEL_PIN
void leds_show() {
#ifndef FASTLED_NEOMATRIX
    FastLED.show();
#else
    FastLED[0].showLeds(led_brightness);
#endif
}
void leds_setcolor(uint16_t i, uint32_t c) {
    leds[i] = c;
}
#endif // NEOPIXEL_PIN

void change_brightness(int8_t change, bool absolute=false) {
    static uint8_t brightness = dfl_matrix_brightness_level;
    static uint32_t last_brightness_change = 0 ;

    if (!absolute && millis() - last_brightness_change < 300) {
        Serial.print("Too soon... Ignoring brightness change from ");
        Serial.println(brightness);
        return;
    }
    last_brightness_change = millis();
    if (absolute) {
        brightness = change;
    } else {
        brightness = constrain(brightness + change, 2, 8);
    }
    led_brightness = min((1 << (brightness+1)) - 1, 255);
    uint8_t smartmatrix_brightness = min( (1 << (brightness+2)), 255);
    matrix_brightness = (1 << (brightness-1)) - 1;

    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" led value: ");
    Serial.print(led_brightness);
    Serial.print(" / smartmatrix value: ");
    Serial.print(smartmatrix_brightness);
    Serial.print(" / neomatrix value: ");
    Serial.println(matrix_brightness);
    Serial.print("|B:");
    Serial.println(brightness);
#ifndef ARDUINOONPC
    Serial.flush();
#endif
#ifdef SMARTMATRIX
    matrixLayer.setBrightness(smartmatrix_brightness);
#else
    // This is needed if using the ESP32 driver but won't work
    // if using 2 independent fastled strips (1D + 2D matrix)
    matrix->setBrightness(matrix_brightness);
#endif
}

void change_speed(int8_t change, bool absolute=false) {
    static uint32_t last_speed_change = 0 ;

    if (!absolute && millis() - last_speed_change < 200) {
        Serial.print("Too soon... Ignoring speed change from ");
        Serial.println(strip_speed);
        return;
    }
    last_speed_change = millis();

    Serial.print("Changing speed ");
    Serial.print(strip_speed);
    Serial.print(" to new speed ");
    if (absolute) {
        strip_speed = change;
    } else {
        strip_speed = constrain(strip_speed + change, 1, 100);
    }
    Serial.println(strip_speed);
    Serial.print("|S:");
    char buf[4];
    sprintf(buf, "%3d", strip_speed);
    Serial.println(buf);
#ifndef ARDUINOONPC
    Serial.flush();
#endif
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

// Allow checking for a pause command, p over serial or power over IR
bool check_startup_IR_serial() {
    char readchar;

    if (Serial.available()) readchar = Serial.read(); else readchar = 0;
    if (readchar == 'p') return 1;

#ifdef RECV_PIN
    uint32_t result = 0; 

    #ifndef ESP32RMTIR
        decode_results IR_result;
        if (irrecv.decode(&IR_result)) {
            irrecv.resume(); // Receive the next value
            result = IR_result.value;
    #else
        char* rcvGroup = NULL;
        if (irrecv.available()) {
            result = irrecv.read(rcvGroup);
    #endif
        switch (result) {
        case IR_RGBZONE_POWER2:
            Serial.println("CheckIS Got IR: Power2");
        case IR_RGBZONE_POWER:
            return 1;

        case IR_JUNK:
            return 0;

        default:
            Serial.print("CheckIS Got unknown IR value: ");
        #ifdef ESP32RMTIR
            Serial.print(rcvGroup);
            Serial.print(" / ");
        #endif
            Serial.println(result, HEX);
            return 0;
        }
    }
#endif
    return 0;
}



void IR_Serial_Handler() {
    int16_t new_pattern = 0;
    char readchar;
    static bool remotesend = false;

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
	    if (remotesend) {
		char numstr[4];
		snprintf( numstr, 3, "%d", new_pattern );
		send_serial(numstr);
		remotesend = false;
	    } else {
		matrix_change(new_pattern);
	    }
        } else {
            Serial.print("Got serial char ");
            Serial.println(readchar);
	    remotesend = false;
        }
    }

    if (readchar == 'n')      { Serial.println("Serial => next"); matrix_change(DEMO_NEXT);}
    else if (readchar == 'p') { Serial.println("Serial => previous"); matrix_change(DEMO_PREV);}
    else if (readchar == 'N') { Serial.println("Serial => next");     send_serial("n");}
    else if (readchar == 'P') { Serial.println("Serial => previous"); send_serial("p");}
    else if (readchar == 'B') { Serial.println("Serial => Bestof"); show_best_demos = true;}
    else if (readchar == 'b') { Serial.println("Serial => All Demos"); show_best_demos = false;}
    else if (readchar == 't') { Serial.println("Serial => text thankyou"); matrix_change(DEMO_TEXT_THANKYOU);}
    else if (readchar == '-') { Serial.println("Serial => dim"   ); change_brightness(-1);}
    else if (readchar == '+') { Serial.println("Serial => bright"); change_brightness(+1);}
#ifdef ARDUINOONPC
    else if (readchar == '<') { Serial.println("ESP => dim"   ); send_serial("-");}
    else if (readchar == '>') { Serial.println("ESP => bright"); send_serial("+");}
    else if (readchar == 'R') { Serial.println("ESP => send next number"); remotesend = true;}
#endif

    // allow working on hardware that doens't have IR. In that case, we use serial only and avoid
    // compiling the IR code that won't build.
#ifdef RECV_PIN
    uint32_t result; 

    #ifndef ESP32RMTIR
        decode_results IR_result;
        if (irrecv.decode(&IR_result)) {
            irrecv.resume(); // Receive the next value
            result = IR_result.value;
    #else
        char* rcvGroup = NULL;
        if (irrecv.available()) {
            result = irrecv.read(rcvGroup);
    #endif
        switch (result) {
        case IR_RGBZONE_BRIGHT:
        case IR_RGBZONE_BRIGHT2:
            if (is_change(true)) { show_best_demos = true; Serial.println("Got IR: Bright, Only show best demos"); return; }
            change_brightness(+1);
            Serial.println("Got IR: Bright");
            return;

        case IR_RGBZONE_DIM:
        case IR_RGBZONE_DIM2:
            if (is_change(true)) {
                Serial.println("Got IR: Dim, show all demos again and Hang on this demo");
                matrix_loop = 9999;
                show_best_demos = false;
                return;
            }

            change_brightness(-1);
            Serial.println("Got IR: Dim");
            return;

        case IR_RGBZONE_NEXT2:
            Serial.println("Got IR: Next2");
        case IR_RGBZONE_NEXT:
            last_change = millis();
            matrix_change(DEMO_NEXT);
            Serial.print("Got IR: Next to matrix state ");
            Serial.println(matrix_state);
            return;

        case IR_RGBZONE_POWER2:
            Serial.println("Got IR: Power2");
        case IR_RGBZONE_POWER:
            if (is_change()) { matrix_change(DEMO_PREV); return; }
            Serial.println("Got IR: Power, show all demos again and Hang on this demo");
            matrix_loop = 9999;
            show_best_demos = false;
            return;

        case IR_RGBZONE_RED:
            Serial.println("Got IR: Red (0)");
            if (is_change()) { matrix_change(0); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xFF0000;
            return;

        case IR_RGBZONE_GREEN:
            Serial.println("Got IR: Green (3)");
            if (is_change()) { matrix_change(3); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x00FF00;
            return;

        case IR_RGBZONE_BLUE:
            Serial.println("Got IR: Blue (5)");
            if (is_change()) { matrix_change(5); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x0000FF;
            return;

        case IR_RGBZONE_WHITE:
            Serial.println("Got IR: White (7)");
            if (is_change()) { matrix_change(7); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xFFFFFF;
            return;



        case IR_RGBZONE_RED2:
            Serial.println("Got IR: Red2 (9)");
            if (is_change()) { matrix_change(9); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xCE6800;
            return;

        case IR_RGBZONE_GREEN2:
            Serial.println("Got IR: Green2 (11)");
            if (is_change()) { matrix_change(11); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x00BB00;
            return;

        case IR_RGBZONE_BLUE2:
            Serial.println("Got IR: Blue2 (13)");
            if (is_change()) { matrix_change(13); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x0000BB;
            return;

        case IR_RGBZONE_PINK:
            Serial.println("Got IR: Pink (15)");
            if (is_change()) { matrix_change(15); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xFF50FE;
            return;



        case IR_RGBZONE_ORANGE:
            Serial.println("Got IR: Orange (17)");
            if (is_change()) { matrix_change(17); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xFF8100;
            return;

        case IR_RGBZONE_BLUE3:
            Serial.println("Got IR: Green2 (19)");
            if (is_change()) { matrix_change(19); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x00BB00;
            return;

        case IR_RGBZONE_PURPLED:
            Serial.println("Got IR: DarkPurple (21)");
            if (is_change()) { matrix_change(21); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x270041;
            return;

        case IR_RGBZONE_PINK2:
            Serial.println("Got IR: Pink2 (23)");
            if (is_change()) { matrix_change(23); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xFFB9FF;
            Serial.println("Got IR: Pink2");
            return;



        case IR_RGBZONE_ORANGE2:
            Serial.println("Got IR: Orange2 (25)");
            if (is_change()) { matrix_change(25); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xFFCA49;
            return;

        case IR_RGBZONE_GREEN3:
            Serial.println("Got IR: Green2 (27)");
            if (is_change()) { matrix_change(27); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x006A00;
            return;

        case IR_RGBZONE_PURPLE:
            Serial.println("Got IR: DarkPurple2 (29)");
            if (is_change()) { matrix_change(29); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x2B0064;
            return;

        case IR_RGBZONE_BLUEL:
            Serial.println("Got IR: BlueLight (31)");
            if (is_change()) { matrix_change(31); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x50A7FF;
            return;



        case IR_RGBZONE_YELLOW:
            Serial.println("Got IR: Yellow (33)");
            if (is_change()) { matrix_change(33); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0xF0FF00;
            return;

        case IR_RGBZONE_GREEN4:
            Serial.println("Got IR: Green2 (35)");
            if (is_change()) { matrix_change(35); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x00BB00;
            return;

        case IR_RGBZONE_PURPLE2:
            Serial.println("Got IR: Purple2 (37)");
            if (is_change()) { matrix_change(37); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x660265;
            return;

        case IR_RGBZONE_BLUEL2:
            Serial.println("Got IR: BlueLight2 (39)");
            if (is_change()) { matrix_change(39); return; }
            if (!colorDemo) nextdemo = f_colorWipe;
            demo_color = 0x408BD8;
            return;


        case IR_RGBZONE_RU:
            matrix_change(41);
            Serial.print("Got IR: Red UP switching to matrix state 41");
            Serial.println(matrix_state);
            return;

        case IR_RGBZONE_GU:
            matrix_change(42);
            Serial.print("Got IR: Green UP switching to matrix state 42");
            Serial.println(matrix_state);
            return;

        case IR_RGBZONE_BU:
            matrix_change(43);
            Serial.print("Got IR: Blue UP switching to matrix state 43");
            Serial.println(matrix_state);
            return;


        case IR_RGBZONE_RD:
            Serial.print("Got IR: Red DOWN switching to matrix state 44");
            matrix_change(44);
            Serial.println(matrix_state);
            return;

        case IR_RGBZONE_GD:
            Serial.print("Got IR: Green DOWN switching to matrix state 45");
            matrix_change(45);
            Serial.println(matrix_state);
            return;

        case IR_RGBZONE_BD:
            Serial.print("Got IR: Blue DOWN switching to matrix state 47");
            matrix_change(47);
            Serial.println(matrix_state);
            return;

        case IR_RGBZONE_QUICK:
            Serial.println("Got IR: Quick");
            //if (is_change()) { matrix_change(24); return; }
            change_speed(-10);
            return;

        case IR_RGBZONE_SLOW:
            Serial.println("Got IR: Slow)");
            //if (is_change()) { matrix_change(28); return; }
            change_speed(+10);
            return;




        case IR_RGBZONE_DIY1:
            Serial.println("Got IR: DIY1 (49)");
            if (is_change()) { matrix_change(49); return; }
            // this uses the last color set
            nextdemo = f_colorWipe;
            Serial.println("Got IR: DIY1 colorWipe");
            return;

        case IR_RGBZONE_DIY2:
            Serial.println("Got IR: DIY2 (51)");
            if (is_change()) { matrix_change(51); return; }
            // this uses the last color set
            nextdemo = f_theaterChase;
            Serial.println("Got IR: DIY2/theaterChase");
            return;

        case IR_RGBZONE_DIY3:
            Serial.println("Got IR: DIY3 (53)");
            if (is_change()) { matrix_change(53); return; }
            nextdemo = f_juggle;
            Serial.println("Got IR: DIY3/Juggle");
            return;

        case IR_RGBZONE_AUTO:
            Serial.println("Got IR: AUTO/bpm (55)");
            if (is_change()) { matrix_change(55); return; }
            matrix_change(DEMO_TEXT_THANKYOU);
            nextdemo = f_bpm;
            return;


        // From here, we jump numbers more quickly to cover ranges of demos 32-44
        case IR_RGBZONE_DIY4:
            Serial.println("Got IR: DIY4 (57)");
            if (is_change()) { matrix_change(57); return; }
            nextdemo = f_rainbowCycle;
            Serial.println("Got IR: DIY4/rainbowCycle");
            return;

        case IR_RGBZONE_DIY5:
            Serial.println("Got IR: DIY5 (59)");
            if (is_change()) { matrix_change(59); return; }
            nextdemo = f_theaterChaseRainbow;
            Serial.println("Got IR: DIY5/theaterChaseRainbow");
            return;

        case IR_RGBZONE_DIY6:
            Serial.println("Got IR: DIY6 (61)");
            if (is_change()) { matrix_change(61); return; }
            nextdemo = f_doubleConvergeRev;
            Serial.println("Got IR: DIY6/DoubleConvergeRev");
            return;

        case IR_RGBZONE_FLASH:
            Serial.println("Got IR: FLASH (63)");
            if (is_change()) { matrix_change(63); return; }
            nextdemo = f_flash;
            return;

        // And from here, jump across a few GIF anims (45-53)
        case IR_RGBZONE_JUMP3:
            Serial.println("Got IR: JUMP3/Cylon (65)");
            if (is_change()) { matrix_change(65); return; }
            nextdemo = f_cylon;
            return;

        case IR_RGBZONE_JUMP7:
            Serial.println("Got IR: JUMP7/CylonWithTrail (67)");
            if (is_change()) { matrix_change(67); return; }
            nextdemo = f_cylonTrail;
            return;

        case IR_RGBZONE_FADE3:
            Serial.println("Got IR: FADE3/DoubleConverge (69)");
            if (is_change()) { matrix_change(69); return; }
            nextdemo = f_doubleConverge;
            return;

        case IR_RGBZONE_FADE7:
            Serial.println("Got IR: FADE7/DoubleConvergeTrail (71)");
            if (is_change()) { matrix_change(71); return; }
            nextdemo = f_doubleConvergeTrail;
            return;

        case IR_JUNK:
            return;

        default:
            Serial.print("Got unknown IR value: ");
        #ifdef ESP32RMTIR
            Serial.print(rcvGroup);
            Serial.print(" / ");
        #endif
            Serial.println(result, HEX);
            return;
        }
    }
#endif
}



#ifdef NEOPIXEL_PIN
    // Demos from FastLED
    // fade in 0 to x/256th of the previous value
    void fadeall(uint8_t fade) {
        for(uint16_t i = 0; i < STRIP_NUM_LEDS; i++) {  leds[i].nscale8(fade); }
    }

    void cylon(bool trail, uint8_t wait) {
        static bool forward = 1;
        static int16_t i = 0;
        static uint8_t hue = 0;

        if (! (forward  && i < STRIP_NUM_LEDS)) forward = false;
        if (!forward) {
            i--;
            if (i < 0) { i = 0; forward = true; }
        }

        // Set the i'th led to red
        leds_setcolor(i, Wheel(hue++));
        // Show the leds
        leds_show();
        // now that we've shown the leds, reset the i'th led to black
        //if (!trail) leds_setcolor(i, 0);
        if (trail) fadeall(224); else fadeall(92);
        if (forward) i++;

        // Wait a little bit before we loop around and do it again
        waitmillis = millis() + wait/6;
    }

    void doubleConverge(bool trail, uint8_t wait, bool rev) {
        static uint8_t hue;
        static int16_t i = 0;

        if (i < STRIP_NUM_LEDS/2) {
            if (!rev) {
                leds_setcolor(i, Wheel(hue++));
                leds_setcolor(STRIP_NUM_LEDS - 1 - i, Wheel(hue++));
            } else {
                leds_setcolor(STRIP_NUM_LEDS/2 -1 -i, Wheel(hue++));
                leds_setcolor(STRIP_NUM_LEDS/2 + i, Wheel(hue++));
            }
        }
        i++;
        if (i >= STRIP_NUM_LEDS/2 + 4) i = 0;
        if (trail) fadeall(224); else fadeall(92);
        leds_show();
        waitmillis = millis() + wait/3;
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
        int8_t dothue = 0;
        for(uint8_t i = 0; i < 8; i++) {
            leds[beatsin16( i+7, 0, STRIP_NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
            dothue += 32;
        }
        leds_show();
        waitmillis = millis() + wait/3;
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
        waitmillis = millis() + wait/3;
    }

    // Slightly different, this makes the rainbow equally distributed throughout
    void rainbowCycle(uint8_t wait) {
        static uint16_t j = 0;

        j++;
        if (j >= 256) j = 0;
        fill_rainbow( leds, STRIP_NUM_LEDS, gHue, 7);
        add1DGlitter(80);
        gHue++;
        leds_show();
        waitmillis = millis() + wait/5;
    }




    // The animations below are from Adafruit_NeoPixel/examples/strandtest
    // Fill the dots one after the other with a color
    void colorWipe(uint32_t c, uint8_t wait) {
        static uint16_t j = 0;

        j++;
        if (j == STRIP_NUM_LEDS) j = 0;
        leds_setcolor(j, c);
        leds_show();
        waitmillis = millis() + wait/5;
    }

    //Theatre-style crawling lights.
    void theaterChase(uint32_t c, uint8_t wait) {
        static uint16_t q = 0;
        
        q++;
        if (q == 3) q = 0;
        
        for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
            leds_setcolor(i+q, c);    //turn every third pixel on
        }
        leds_show();

        #if 0
        for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
            leds_setcolor(i+q, 0);        //turn every third pixel off
        }
        #endif
        fadeall(16);
        waitmillis = millis() + wait;
    }

    //Theatre-style crawling lights with rainbow effect
    void theaterChaseRainbow(uint8_t wait) {
        static uint16_t j = 0;
        static uint16_t q = 0;
        
        q++;
        if (q == 3) {
            q = 0;
            j += 7;
            if (j >= 256) j = 0;   // cycle all 256 colors in the wheel
        }

        for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
            leds_setcolor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
        }
        leds_show();

        #if 0
        for (uint16_t i=0; i < STRIP_NUM_LEDS; i=i+3) {
            leds_setcolor(i+q, 0);        //turn every third pixel off
        }
        #endif
        fadeall(16);
        waitmillis = millis() + wait;
    }


    // Local stuff I wrote
    // Flash 25 colors in the color wheel
    void flash(uint8_t wait) {
        static uint16_t j = 0;
        static boolean on = true;

        j++;
        if (j == 26) j = 0;

        if (on) {
            for (uint16_t i=0; i < STRIP_NUM_LEDS; i++) {
                leds_setcolor(i, Wheel(j * 10));
            }
            on = false;
        } else {
            for (uint16_t i=0; i < STRIP_NUM_LEDS; i++) {
                leds_setcolor(i, 0);
            }
            on = true;
        }
        leds_show();
        waitmillis = millis() + wait*3;
    }

    void Neopixel_Anim_Handler() {
        if (millis() < waitmillis) return;

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
            doubleConverge(true, strip_speed, false);
            break;

        // Flash color wheel
        case f_flash:
            colorDemo = false;
            flash(strip_speed);
            break;

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
    }
#endif // NEOPIXEL_PIN

// ================================================================================

#ifdef WIFI
#define HTML_BRIGHT  101
#define HTML_SPEED   102
#define HTML_BUTPREV 110
#define HTML_BUTNEXT 111
#define HTML_DEMOCHOICE 120

void actionProc(const char *pageName, const char *parameterName, int value, int ref1, void *ref2) {
    static int actionCount = 0;
    actionCount++;
    Serial.printf("%4d %s.%s value=0x%08x/%dslider 2 ref1=%d\n", actionCount, pageName, parameterName, value, value, ref1);

    switch (ref1) {
    case HTML_BUTPREV:
        if (!value) break;
        Serial.println("PREVIOUS pressed");
        matrix_change(DEMO_PREV);
        break;

    case HTML_BUTNEXT:
        if (!value) break;
        Serial.println("NEXT pressed");
        matrix_change(DEMO_NEXT);
        break;

    case HTML_BRIGHT:
        if (!value) break;
        Serial.print("Brightness change to ");
        Serial.println(value);
        change_brightness(value, true);
        break;

    case HTML_SPEED:
        if (!value) break;
        Serial.print("Speed change to ");
        Serial.println(value);
        change_speed(value, true);
        break;

    case HTML_DEMOCHOICE:
        if (!value) break;
        Serial.print("Got HTML Demo Choice ");
        Serial.println(value);
        matrix_change(value, true);
        break;

    }   
}

void wildcardProc(OmXmlWriter &w, OmWebRequest &request, int ref1, void *ref2) {
    p.renderHttpResponseHeader("text/plain", 200);

    if (! strcmp(request.path, "/form")) {
        uint8_t k = (int)request.query.size();
        uint8_t idx = 0;

        if (! strcmp(request.query[0], "text")) {
            displaytext = request.query[1];
            matrix_change(DEMO_TEXT_INPUT);
        }
        for (int ix = idx; ix < k - 1; ix += 2)
            w.putf("arg %d: %s = %s\n", ix / 2, request.query[ix], request.query[ix + 1]);
        return;
    }

    Serial.print("Serving ");
    Serial.println(request.path);
    File file;

    if (! (file = FSO.open(request.path)  
    #ifdef FSOSPIFFS
                                    , "r"
    #endif
                                            ) ) {
        Serial.println("Error opening file");
        return;
    }

    while (file.available()) w.put(file.read());

    file.close();
}

void connectionStatus(const char *ssid, bool trying, bool failure, bool success)
{
  const char *what = "?";
  if (trying) what = "trying";
  else if (failure) what = "failure";
  else if (success) what = "success";

  Serial.printf("%s: connectionStatus for '%s' is now '%s'\n", __func__, ssid, what);
}

// Apparently I can't send a method s.tick to Aiko handler, so I need this glue function
void wifi_html_tick() { 
    s.tick();
}

void setup_wifi() {
    Serial.println("Configuring access point...");

    #include "wifi_secrets.h"
    s.addWifi(WIFI_SSID, WIFI_PASSWORD);

    s.setStatusCallback(connectionStatus);

    p.setBuildDateAndTime(__DATE__, __TIME__);

    p.beginPage("Main");
    p.addSlider(1, 8, "Brightness", actionProc, dfl_matrix_brightness_level, HTML_BRIGHT);
    p.addSlider("Speed",      actionProc, 50, HTML_SPEED);

    /*
         A button sends a value "1" when pressed (in the web page),
         and zero when released.
    */
    p.addButton("prev", actionProc, HTML_BUTPREV);
    p.addButton("next", actionProc, HTML_BUTNEXT);

    /*
         A select control shown as a dropdown on desktop browsers,
         and some other multiple-choice control on phones.
         You can add any number of options, and specify
         the value of each.
    */
    p.addSelect("Choose Demo", actionProc, 2, HTML_DEMOCHOICE);
    for (uint16_t i=1; i < demo_last_idx; i++) {
	uint16_t pos = demo_list[i].position;
	//Serial.print(i);
	//Serial.print(" -> ");
	//Serial.println(pos);

        if (!demo_list[i].func) continue; 
        char *option = (char *) malloc (strlen (demo_list[i].name) + 13);
	sprintf(option, "%03d->%03d/%1d: ", i, pos, demo_mapping[pos].enabled[panelconfnum]);
	strcpy(option+12, demo_list[i].name);
        p.addSelectOption(option, i);
    }
    
    p.addUrlHandler(wildcardProc);
    p.addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
        w.beginElement("a", "href", "/demo_map.txt");
        w.addContent("Demo Map");
        w.endElement(); // a 
    });

    p.addHtml([] (OmXmlWriter & w, int ref1, void *ref2) 
    {
        const char* inputstr="<FORM METHOD=GET ACTION=/form><INPUT NAME=text></FORM>";
        w.puts(inputstr);
    });

    p.addButtonWithLink("Demo Map", "/demo_map.txt", NULL, 0);

    // And lastly, introduce the web pages to the wifi connection.
    s.setHandler(p);

    // Make sure that aiko calls the HTML handler at 10Hz
    Events.addHandler(wifi_html_tick, 100);
}
#endif

void read_config_index() {
    uint16_t index = 0;
    // Demo enabled for 24x32, 64x64 (BM), 64x96 (BM), 64x96 (Trance), 128x192
    // 1: enables, 3 enables demo and adds to BestOf selection
    int d32, d64, d96bm, d96, d192;
    int dmap;
    char pathname[] = FS_PREFIX "/demo_map.txt";
    const char *confname[] = {"Neopixel Shirt 24x32", "Burning Man Neopixel 64x64", "BM RGBPanel 64x96",
        "RGBPanel Shirt 64x96", "RGBPanel rPi 128x192"};

    Serial.print("*********** Reading ");
    Serial.print(pathname);
    Serial.print(" for display ");
    Serial.print(panelconfnum);
    Serial.print(" / ");
    Serial.print(confname[panelconfnum]);
    Serial.println( "***********");

    #ifdef ARDUINOONPC
    FILE *file;
    if (! (file = fopen(pathname, "r"))) die ("Error opening " FS_PREFIX "demo_map.txt");

    while ( fscanf(file, "%d %d %d %d %d %d\n", &d32, &d64, &d96bm, &d96, &d192, &dmap) != EOF) {
    #else
    File file;

    if (! (file = FSO.open(pathname)  
    #ifdef FSOSPIFFS
                                    , "r"
    #endif
                                            ) ) die ("Error opening demo_map.txt");
    while (file.available()) {
        // 1 0 0 0 0 012
        String line = file.readStringUntil('\n');
        sscanf(line.c_str(), "%d %d %d %d %d %d\n", &d32, &d64, &d96bm, &d96, &d192, &dmap);
    #endif
        demo_mapping[index].mapping = dmap;
        demo_mapping[index].enabled[0] = d32;
        demo_mapping[index].enabled[1] = d64;
        demo_mapping[index].enabled[2] = d96bm;
        demo_mapping[index].enabled[3] = d96;
        demo_mapping[index].enabled[4] = d192;
        if (demo_list[dmap].name == NULL) {
            Serial.print("Error ");
            Serial.print(index++);
            Serial.print(" is mapped to an undefined demo ");
            Serial.println(dmap);
            delay((uint32_t) 100);
            continue;
        }
	#ifdef ESP32
	    Serial.printf("%3d: %d, %d, %d, %d, %d -> %3d (ena:%d) => %s", index, d32,  d64,  d96bm,  d96,  d192,  dmap, 
			  demo_mapping[index].enabled[panelconfnum], demo_list[dmap].name);
	#else
	    Serial.print(index);
	    Serial.print(" ");
	    Serial.print(dmap);
	    Serial.print(" ");
	    Serial.print(demo_mapping[index].enabled[panelconfnum]);
	    Serial.print(" ");
	    Serial.print(demo_list[dmap].name);
	#endif
	// reverse position mapping used in setup_wifi dropdown creation
	demo_list[dmap].position = index;
        if (demo_mapping[index].enabled[panelconfnum] && demo_list[dmap].func == NULL) {
            Serial.println(" (undefined)");
            index++;
            continue;
        }
        Serial.println("");
        // keep track of the highest demo index for modulo in matrix_change
        if (demo_mapping[index].enabled[panelconfnum]) { 
            if (! matrix_state) matrix_state = index; // find first playable demo
            demo_cnt++;
            demo_last_idx = index;
            //Serial.print(index); Serial.print(" "); Serial.print(demo_cnt); Serial.print(" "); Serial.println(demo_last_idx);
        }
        index++;
    }

    #ifdef ARDUINOONPC
    fclose(file);
    #else
    file.close();
    #endif
}


void loop() {
    // Run the Aiko event loop, all the magic is in there.
    Events.loop();
    EVERY_N_MILLISECONDS(40) {
        gHue++;  // slowly cycle the "base color" through the rainbow
    }
    delay((uint32_t) 1);

    // Do not put loop code after the code block below as it can return in the middle.

    // Ok, magic happens here. This code supports running on 2 devices at the same time
    // Device #1 is ESP32 or teensy or similar, and handles the IR and the neopixels
    // Dev1 receives commands via IR and its web server.
    // Dev1 in turn outputs some special codes on serial like '|D: 76' to say 'demo 76'
    // Dev2 is a rPi which has every single output pin used to drive 3 panels in parallel.
    // It however can receive serial via its USB ports (FTDI). It reads those '|D: 76' 
    // commands and uses them to generate the output in sync with what dev#1 did.
    // Why all this madness? Because an rPi can run 3 to 12 times more RGBPanel pixels than
    // an ESP32 or teensy with SmartMatrix.
    #ifdef ARDUINOONPC
	static const char *serialdev = NULL;
	static char buf[1024];
	static char *ptr = buf;
        char s;
        int rdlen;
	struct stat stbuf;

	if (ttyfd > -1 && stat(serialdev, &stbuf)) { 
	    printf("ttyfd closed %d, (%s)\n", ttyfd, serialdev);
	    close(ttyfd); 
	    ttyfd = -1; 
	    serialdev = NULL;
	}
	EVERY_N_SECONDS(5) { 
	    EVERY_N_SECONDS(30) { 
		if (serialdev && (ttyfd < 0)) printf("Serial closed, (re-)opening\n");
	    }
	    if (ttyfd < 0) ttyfd = openttyUSB(&serialdev);
	}
	if (ttyfd < 0) return;
		
	// Read up to 40 characters in the buffer and then continue the loop
	// this should cause a loop hang of up to 7ms at 115200bps.
	uint8_t readcnt = 80;
	while (readcnt-- && (rdlen = read(ttyfd, &s, 1)) > 0) {
	    ptr[0] = s;
	    ptr[1] = 0;
	    ptr++;
	    if (s == '\n' ) {
		printf("ESP> %s", buf);
		ptr = buf;
		char numbuf[4];
		// If we are getting serial pings, run the current demo for 
		// a long time, because we expect the ESP to send us 'next'
		if (! strncmp(buf, "FrameBuffer::GFX", 16)) matrix_loop = 20;
		if (! strncmp(buf, "Done with demo", 14)) matrix_loop = 20;
		if (! strncmp(buf, "|D:", 3)) {
		    int num;
		    strncpy(numbuf, buf+3, 3);
		    numbuf[3] = 0;
		    num = atoi(numbuf);
		    printf("Got demo %d\n", num);
		    matrix_change(num);
		}
		if (! strncmp(buf, "|B:", 3)) {
		    int num;
		    strncpy(numbuf, buf+3, 1);
		    numbuf[1] = 0;
		    num = atoi(numbuf);
		    printf("Got brightness %d\n", num);
		    change_brightness(num, true);
		}
	    }
        }
	if (rdlen < 0) {
	    printf("Error from read: %d: %s\n", rdlen, strerror(errno));
	}
    #endif
}


void setup() {
    Serial.begin(115200);
    Serial.println("Hello World");
#ifdef ESP8266
    Serial.println("Init ESP8266");
    // Turn off Wifi
    // https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
    WiFi.forceSleepBegin();                  // turn off ESP8266 RF
    // No blink13 in ESP8266 lib
    // this doesn't exist in the ESP8266 IR library, but by using pin D4
    // IR receive happens to make the system LED blink, so it's all good
    //irrecv.blink13(true);
#elif !defined(ARDUINOONPC)
    #ifdef ESP32
        Serial.println("If things hang very soon, make sure you don't have PSRAM enabled on a non PSRAM board");
        #ifndef ESP32RMTIR
        Serial.println("Init ESP32, set IR receive NOT to blink system LED");
        irrecv.blink13(false);
        #endif
    #else
        Serial.println("Init NON ESP8266/ESP32, set IR receive to blink system LED");
        irrecv.blink13(true);
    #endif
#endif

#ifdef HAS_FS
    Serial.println("Init GIF Viewer");
    sav_setup();
#endif
    // This is now required, if there is no arduino FS support, you need to replace this function
    Serial.println("Read config file");
    read_config_index();

#ifdef WIFI
//  Before Wifi
//  Heap/32-bit Memory Available     : 274076 bytes total, 113792 bytes largest free block
//  8-bit/malloc/DMA Memory Available: 231496 bytes total, 113792 bytes largest free block
//  Total PSRAM used: 0 bytess total, 4194252 PSRAM bytes free
//  Configuring access point...
//  After Wifi/Before SPIFFS/FFat
//  Heap/32-bit Memory Available     : 215172 bytes total, 113792 bytes largest free block
//  8-bit/malloc/DMA Memory Available: 172592 bytes total, 113792 bytes largest free block
//  Total PSRAM used: 3760 bytes total, 4190276 PSRAM bytes free
    show_free_mem("Before Wifi");
    setup_wifi();
    show_free_mem("After Wifi/Before SPIFFS/FFat");
#endif

    Serial.println("\nEnabling Neopixels strip (if any) and Configured Display.");
#ifdef NEOPIXEL_PIN
    Serial.print("Using FastLED on pin ");
    Serial.print(NEOPIXEL_PIN);
    Serial.print(" to drive LEDs: ");
    Serial.println(STRIP_NUM_LEDS);
    FastLED.addLeds<NEOPIXEL,NEOPIXEL_PIN>(leds, STRIP_NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(led_brightness);
    // Turn off all LEDs and light 3 of them for debug.
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Blue;
    leds[2] = CRGB::Green;
    leds[10] = CRGB::Blue;
    leds[20] = CRGB::Green;
    leds_show();
    Serial.println("LEDs on");
#endif // NEOPIXEL_PIN

    Serial.println("Init Smart or FastLED Matrix");
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
    matrix_setup(false, 25000);
    Serial.println("Init Aurora");
    aurora_setup();
    Serial.println("Init TwinkleFox");
    twinklefox_setup();
    Serial.println("Init Fireworks");
    fireworks_setup();
    Serial.println("Init sublime");
    sublime_setup();
    Serial.println("Enabling IRin");
#ifdef RECV_PIN
    #ifndef ESP32RMTIR
        irrecv.enableIRIn(); // Start the receiver
    #else
        irrecv.start(RECV_PIN);
    #endif
    Serial.print("Enabled IRin on pin ");
    Serial.println(RECV_PIN);
#endif

    GifAnim(255); // Compute how many GIFs are defined
    Serial.println("vvvvvvvvvvvvvvvvvvvvvvvv");
    Serial.print("Number of GIFs defined: ");
    Serial.println(gif_cnt);

    Serial.print("Number of Demos defined: ");
    Serial.println(demo_cnt);

    Serial.print("Last Playable Demo Index: ");
    Serial.println(demo_last_idx);

    // matrix_state is set in read_config_index()
    matrix_demo = demo_mapping[matrix_state].mapping;
    Serial.print("First playable demo: ");
    Serial.print(matrix_state);

    Serial.print(" mapped to: ");
    Serial.println(matrix_demo);
    Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^");

    Serial.print("Matrix Size: ");
    Serial.print(mw);
    Serial.print(" ");
    Serial.println(mh);
    // Turn off dithering https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
    FastLED.setDither( 0 );
    // Set brightness as appropriate for backend by sending a null change from the default
    change_brightness( 0 );
    matrix->setTextWrap(false);
    Serial.println("Matrix Test");
    // speed test
    // I only get 50fps with SmartMatrix 96x64, but good enough I guess
    // while (1) { display_stats(); yield();};
    // init first matrix demo
    matrix->fillScreen(matrix->Color(0xA0, 0xA0, 0xA0));
    matrix_show();
    int i = 100;
    Serial.println("Pause to check that all the pixels work ('p' or power to stay here)");
    while (i--) {
	if (check_startup_IR_serial()) {
	    Serial.println("Will pause on debug screen");
	    i = 60000;
	}
	delay((uint32_t) 10);
    }
    Serial.println("Done with debug grey screen, display stats");

    display_stats();
        delay((uint32_t) 2000);

    Serial.println("Matrix Libraries Test done");
    //font_test();

#ifdef NEOPIXEL_PIN
    // init first strip demo
    colorWipe(0x0000FF00, 10);
    Serial.println("Neopixel strip init done");
    Events.addHandler(Neopixel_Anim_Handler, 10);
#endif // NEOPIXEL_PIN

    // Argument is how many ms between each call to the handler
    Events.addHandler(IR_Serial_Handler, 10);
    // This is tricky, with FastLED output, sending all the pixels will take
    // most of the CPU time, but Aiko should ensure that other handlers get
    // run too, even if this handler never really hits its time target and 
    // runs every single time aiko triggers. 
    // Note however that you can't reasonably have any other handler
    // running faster than MX_UPD_TIME since it would be waiting on this one
    // being done.
    Events.addHandler(Matrix_Handler, MX_UPD_TIME);

    demo_list[DEMO_TEXT_THANKYOU] = { "Thank you", NULL, -1, 0  }; 
    demo_list[DEMO_TEXT_INPUT]    = { "Web Text Input", NULL, -1, 0  }; 
    Serial.println("Starting loop");
}

// vim:sts=4:sw=4
