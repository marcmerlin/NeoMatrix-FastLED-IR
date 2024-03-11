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

// Compile WeMos D1 R2 & mini, ESP32-dev, or ArduinoOnPC for linux

// Force 24x32 on ESP32 for bright daylight display
// as a hack, we enable this for non PSRAM boards but
// a non PSRAM board could also run a framebuffer less
// setup to drive a rPi
//#ifdef ESP32
//#ifndef BOARD_HAS_PSRAM
//#define M32BY8X3
//#endif
//#endif

// on Rpi, this is ignored and it uses a larger size
#ifdef M32BY8X3
#define gif_size 32
#else
#define gif_size 64
#endif
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

#ifdef ARDUINOONPC
#include "v4lcapture.h"
#endif

#ifdef HAS_FS
// defines FSO
#include "GifAnimViewer.h"
#endif

#ifdef WIFI
    #include "wifi_secrets.h"
    #include <OmEspHelpers.h>
    OmWebServer s;
    OmWebPages *p = NULL;
    // Every time things change like the current demo, brightness,
    // bestof, etc, rebuild the page show to the user.
    // Unfortunately this does not force a page reload.
    void rebuild_main_page(bool show_summary=false);
    void rebuild_advanced_page();

    // Allows a connected device via serial to feed a remote IP as a 32bit number
    IPAddress Remote_IP = IPAddress(0, 0, 0, 0);
#else
    // Without WIFI (like rPi/ArduinoOnPC), this is a no-op
    void rebuild_main_page(bool show_summary=false) { show_summary = show_summary; }
    void rebuild_advanced_page() {};
#endif

#define DEMO_PREV -32768
#define DEMO_NEXT 32767
#define DEMO_TEXT_THANKYOU 90
#define DEMO_TEXT_INPUT 91
#define DEMO_CAMERA 99
#define DEMO_TEXT_FIRST DEMO_TEXT_THANKYOU
#define DEMO_TEXT_LAST DEMO_TEXT_INPUT

#ifdef ESP8266
#define DEMO_ARRAY_SIZE 120
#else
#define DEMO_ARRAY_SIZE 480
#endif

// number of lines read in demo_map.txt
uint16_t CFG_LAST_INDEX = 0;

// By default text demos rotate but they can be made to stay still for a picture
bool ROTATE_TEXT = true;
// By default DJ images get scrolled a bit to add movement
bool SCROLL_IMAGE = true;

// show all demos by default,
bool SHOW_BEST_DEMOS = false;

bool MATRIX_RESET_DEMO = true;

// Different panel configurations: 24x32, 64x64 (BM), 64x96 (BM), 64x96 (Trance), 128x192
#define CONFIGURATIONS 5
const char *panelconfnames[CONFIGURATIONS] = {
    "Neopixel Shirt 24x32 ESP8266",
    "Burning Man Neopixel Panel 64x64 ESP32",
    "SmartMatrix Shirt BM 64x96 ESP32",
    "SmartMatrix Shirt Dance 64x96 ESP32",
    "RPI-RGB-Panels Shirt Dance 128x192 rPi" };

typedef struct demo_entry_ {
    const char *name;
    uint8_t (*func)(uint32_t);
    int arg;
    // This is generated for the wifi menu (if used in the generated drop down menu)
    // it could remain NULL, so don't dereference it blindly.
    char *menu_str;
} Demo_Entry;

// demoidx() does the mapping from demo index number to a slot in demo_list
// this it to given slots like 100 to a different demo depending on the runtime
// chosen (like ESP32 64x96 to 128x192)
// Demo_Entry demo_list[] is defined below

// Those are the lines defined in demo_map.txt config
// 400(0-399) lines for 365 in demo_list
typedef struct mapping_entry_ {
    // reverse mapping to the original demo slot (for demos that are
    // swapped to a different slot)
    // get filled with dmap which is the the one colum before last in demo_map.txt
    uint16_t mapping;
    // 1: enabled, 2: bestof enabled only, 3: both
    uint8_t enabled[CONFIGURATIONS];
    // allow reversing a demo to its original index (each demo points to its new
    // slot and that new slot as an index back with .reverse)
    uint16_t reverse;
} Mapping_Entry;

// demo_mapping actually does not need to be as big as DEMO_ARRAY_SIZE
// because demo_list contains elements that share the same demo slots
// (different demos can be defined for the same slot to account for different
// platforms). 
// It's safe and easy to define it a bit too large, though.
Mapping_Entry demo_mapping[DEMO_ARRAY_SIZE];


// DEMO_LAST_IDX is the number of elements in the demo array, but this includes empty slots
uint16_t DEMO_CNT; // actual number of demos available at boot (different from enabled)
uint16_t BEST_CNT;
// last demo index (starting from 1, not 0), gets computed in read_config_index
uint16_t DEMO_LAST_IDX;

// index within demo_mapping of what demo is being played
uint16_t MATRIX_STATE = 0;
uint16_t MATRIX_DEMO; // this is initialized after MATRIX_STATE is updated in read_config_index

// computed in Matrix_Handler, displayed in ShowMHfps
uint32_t LAST_FPS = 0;
bool SHOW_LAST_FPS = false;

String DISPLAYTEXT="00,00,0100,0,1,Hello\nWorld";

// Compute how many GIFs have been defined (called in setup)
uint16_t GIF_CNT = 0;

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
int16_t MATRIX_LOOP = -1;
uint16_t GIFLOOPSEC;
uint32_t waitmillis = 0;

//----------------------------------------------------------------------------

// This file contains codes I captured and mapped myself
// using IRremote's examples/IRrecvDemo
#include "IRcodes.h"

#ifdef IR_RECV_PIN
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
    IRrecv irrecv(IR_RECV_PIN);
    #else
    IRRecv irrecv;
    #endif
#endif

uint32_t last_change = millis();

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


const char *StripDemoName[] = {
    "nothing",
    "colorWipe",
    "rainbow",
    "rainbowCycle",
    "theaterChase",
    "theaterChaseRainbow",
    "cylon",
    "cylonTrail",
    "doubleConverge",
    "doubleConvergeRev",
    "doubleConvergeTrail",
    "flash",
    "juggle",
    "bpm",
};
uint8_t StripDemoCnt = ARRAY_SIZE(StripDemoName)-1;

StripDemo STRIPDEMO = f_theaterChaseRainbow;
// Is the current demo linked to a color (false for rainbow demos)
bool colorDemo = true;
int32_t demo_color = 0x00FF00; // Green
uint8_t strip_speed = 50;


//----------------------------------------------------------------------------

// look for 'magic happens here' below
#ifdef ARDUINOONPC
    #define RPISERIALINPUTSIZE 10240
    char ttyusbbuf[RPISERIALINPUTSIZE];
    bool esp32_connected = false;

    #include <errno.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/stat.h>
    // inet_addr
    #include <netinet/in.h>
    #include <arpa/inet.h>
    
    #define Swap4Bytes(val) \
     ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
       (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )

    char rPI_IP[20];
    char rPI_IPn[11];
    char *get_rPI_IP() {
	    FILE *fp;
	    //fp = popen("hostname -I", "r");
	    fp = popen("cat /root/IP", "r");
	    fgets(rPI_IP, 19, fp);
	    uint32_t addr = Swap4Bytes(inet_addr(rPI_IP));
	    snprintf(rPI_IPn, 11, "%ud", addr);
	    //Serial.println(IP);
	    //Serial.println(addr, HEX);
	    return rPI_IPn;
    }

    // if we're not talking to anything, ttyfd will be reset to -1
    // however >=0 does not mean there is anything transmitting
    // but we'll assume that it is. If this ends up being untrue later
    // look for 'serial watchdog' lower down.
    int ttyfd = -1;

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
	tty.c_cflag |= CS8;	 /* 8-bit characters */
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

    int send_serial(const char *xstr) {
	int wlen;
	int xlen = strlen(xstr);

	wlen = write(ttyfd, xstr, xlen);
	// tcdrain(ttyfd);
	if (wlen != xlen) {
	    printf("Error from write: %d, %s\n", wlen, strerror(errno));
	    return -1;
	}
	printf("ESP<<< %s\n", xstr);
	return 0;
    }

    void openttyUSB(const char ** devname) {
	static bool firstconnection = true;
	// static because the name is sent back to the caller
	static const char *dev[] = { "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2" };
	int devidx = 0;

	while (devidx<3 && (ttyfd = open((*devname = dev[devidx]), O_RDWR | O_NOCTTY | O_SYNC)) < 0 && ++devidx) {
	    struct stat stbuf;
	    // warn for permission denied but not for no such file or directory
	    if (!stat(*devname, &stbuf)) printf("Error opening %s: %s\n", *devname, strerror(errno));
	}
	/* baudrate 115200, 8 bits, no parity, 1 stop bit */
	if (ttyfd >= 0) {
	    char s;

	    set_interface_attribs(ttyfd, B115200);
	    // empty input buffer of possible garbage
	    while (read(ttyfd, &s, 1)) {}
	    esp32_connected = true;
	    printf("Opened %s telling ESP32 to switch to PANELCONFNUM 4\n", *devname);
	    // Assume we just connected to an ESP32, which starts in its PANELCONFNUM 3 (ESP32 mode)
	    // and switch it to PANELCONFNUM 4 (rPI with longer menus).
	    delay(10);
	    send_serial("|");
	    delay(10);
	    send_serial("d");
	    delay(10);
	    if (firstconnection) {
		// First time code starts, tell ESP32 to trigger the IP screen.
		send_serial("i");
		firstconnection = false;
	    } else {
		// on USB reconnect, feed the local IP to ESP32 but don't cause ESP32 to tell us to switch to the IP screen
		send_serial(get_rPI_IP());
	    }
	}
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

void(* resetFunc) (void) = 0; // jump to 0 to cause a sofware reboot

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


void ShowMHfps() {
    uint8_t print_width = 2;

    if (SHOW_LAST_FPS) {
	matrix->setTextSize(1);
	matrix->setFont(&TomThumb);
	matrix->fillRect(0, 0, 4 * print_width, 7, 0);
	matrix->setCursor(1, 6);
	matrix->setTextColor(matrix->Color(255,255,255));
	matrix->print(LAST_FPS);
    }
}


void matrix_show() {
    ShowMHfps();
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
	#ifdef ESP8266
	    FastLED[1].showLeds(matrix_brightness);
	#else
	    matrix->show();
	    //FastLED[1].showLeds(matrix_brightness);
	#endif
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

    // Reset to default font
    matrix->setFont();
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
	matrix->print(DEMO_CNT);
	matrix->print(" demos");
    }
    if (mh >= 35) {
	matrix->setTextColor(matrix->Color(255,128,128));
	matrix->setCursor(0, mh-28);
	matrix->print(BEST_CNT);
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
    if (mheight >= 64) { tfsf_zoom(99); return 255; }

    // For smaller displays, flash letters one by one.
    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	matrix->clear();
	state = 1;
	spd = 1.0;
	startfade = -1;
	// biggest font is 18, but this spills over
	if (mw >= 48 && mh >=48) fontsize = 2;
    }

    matrix->setFont( &Century_Schoolbook_L_Bold[16] );
    matrix->setTextSize(fontsize);

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
	    MATRIX_RESET_DEMO = true;
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
    static uint16_t delayframe = 1;
    const char letters[] = { 'T', 'F', 'S', 'F' };
    uint8_t speed = 30;
    bool done = 0;
    static uint8_t repeat = 4;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	direction = 1;
	size = 3;
	l = 0;
	if (MATRIX_LOOP == -1) { dont_exit = 1; delayframe = 2; faster = 0; };
	matrix->setTextSize(1);
	repeat = 4;
	if (zoom_type == 99) { size = 18; repeat = 10; };
    }
    if (mheight >= 192) matrix->setTextSize(2);

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
	    matrix->setCursor(20-size+offset, (mh>=128?64:20)+size*1.5);
	    matrix->print("TF");
	    matrix->setPassThruColor(0x05C1FF);
	    // Something less bright for pictures
	    if (zoom_type == 99) matrix->setPassThruColor(0x00618F);
	    matrix->setCursor((mh>=128?50:24)-size+offset, (mh>=128?128:52)+size*1.5);
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
	    matrix->setCursor(20-size+offset, (mh>=128?64:20)+size*1.5);
	    matrix->print("TF");
	    matrix->setPassThruColor(0x05C1FF);
	    // Something less bright for pictures
	    if (zoom_type == 99) matrix->setPassThruColor(0x00618F);
	    matrix->setCursor((mh>=128?50:24)-size+offset, (mh>=128?128:50)+size*1.5);
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
    MATRIX_RESET_DEMO = true;
    dont_exit =  0;
    // Serial.print("delayframe on last letter ");
    // Serial.println(delayframe);
    // After showing the last letter, pause longer
    // unless it's a zoom in zoom out.
    if (zoom_type == 0) delayframe *= 5; else delayframe *= 3;
    return repeat;
}


uint8_t rotate_text(uint32_t whichone=0) {
    static uint16_t state;
    static float spd;
    static bool didclear;
    static bool firstpass;
    float spdincr = 1.2;
    uint16_t duration = 100;
    uint16_t overlap = 50;
    uint8_t displayall = 14;
    uint8_t resetspd = 24;

    uint8_t numlines[] = { 
	4, // 0
	4,
	4,
	4,
	5,
	6, // 5
	6,
	3,
	5,
	5,
	5, // 10
	3,
	4,
	3,
	5,
	3,
	4, // 15
	4,
    };

    const char *text[][6] = {
	{ "EAT",    "SLEEP",	"RAVE",		"REPEAT",   "",		    "" },	// 0
	{ "EAT",    "SLEEP",	"TRANCE",	"REPEAT",   "",		    "" },
	{ "Trance.","Because",	"I'm socially",	"awkward.", "",		    "" },
	{ "Trance.","Because",	"I'm a Music",	"snob.",    "",		    "" },
	{ "Trance", "Because",	"Rich said",	"EDM is",   "dead.",	    "" },
	{ "Trance", "Because",	"I'm not",	"hipster",  "enough for",   "Techno." },// 5
	{ "Trance", "Because",	"I don't love",	"my sister","enough for",   "Country." },
	{ "Trance", "Because",	"Of Course",	"",	    "",		    "" },
	{ "Trance", "Because",	"JESUS",	"has it",   "on vinyl",	    "" },
	{ "Trance", "Because",	"It's what",	"JESUS",    "would do!",    "" },
	{ "She said","TRANCE",	"or ME",	"Sometimes","I miss her",   "" },	// 10
	{ "IT'S",   "TIME",	"TO PLAY",	"",	    "",		    "" },
	{ "TRANCE", "IS MUSIC",	"WITH A",	"SOUL",	    "",		    "" },
	{ "I'M A",  "MUSIC",	"LOVER",	"",	    "",		    "" },
	{ "Trance", "Because",	"You Don't",	"Need a",   "Microphone",   "" },
	{ "I'm",    "Fucking",	"Famous",	"",	    "",		    "" },	// 15
	{ "Fuck",   "Me",	"I'm",		"Famous",   "",		    "" },
	{ "Say",    "Perhaps",	"To",		"Drugs",    "",		    "" },
    };

    if (PANELCONFNUM == 2 || PANELCONFNUM == 3) text[0][2] = "BURN";

    uint32_t color[][6] = {
	{ 0xFF0000, 0xB0B000,	0x00FF00,	0x00B0B0,   0,		    0 },	// 0
	{ 0xFF0000, 0xB0B000,	0x00FF00,	0x00B0B0,   0,		    0 },
	{ 0xFFA500, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0,		    0 },
	{ 0xFFC0CB, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0,		    0 },
	{ 0x00FF00, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0xFFFFFF,	    0 },
	{ 0xFFFF00, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0xFFFFFF,	    0xFFFFFF }, // 5
	{ 0x0000FF, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0xFFFFFF,	    0xFFFFFF },
	{ 0xFF0000, 0xFFFFFF,	0xFFFFFF,	0,	    0,		    0 },
	{ 0xFFFFFF, 0xFF0000,	0xFFFFFF,	0xFF0000,   0xFF0000,	    0 },
	{ 0xFFFF00, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0xFFFFFF,	    0 },
	{ 0xFFFFFF, 0xFFFF00,	0x00FF00,	0xFFFFFF,   0x00FFFF,	    0 },	// 10
	{ 0x00FF00, 0x00FF00,	0x00FF00,	0,	    0,		    0 },
	{ 0xFFFF00, 0xFFFF00,	0xFFFF00,	0xFFFF00,   0,		    0 },
	{ 0xFFFF00, 0xFFFF00,	0xFFFF00,	0,	    0,		    0 },
	{ 0xFF0000, 0xFFFFFF,	0xFFFFFF,	0xFFFFFF,   0xFFFFFF,	    0 },
	{ 0xFFC0CB, 0xFFC0CB,	0xFFC0CB,	0,	    0,		    0 },	// 15
	{ 0xFFC0CB, 0xFFC0CB,	0xFFC0CB,	0xFFC0CB,   0,		    0 },
	{ 0xFFC0CB, 0xFFC0CB,	0xFFC0CB,	0xFFC0CB,   0,		    0 },
    };

    uint16_t y_offset_192[][6] = { 
	{ 0,	0,	0,	0,	0,	0 },
	{ 90,	0,	0,	0,	0,	0 },
	{ 70,	140,	0,	0,	0,	0 },
	{ 65,	105,	145,	0,	0,	0 },
	{ 48,	84,	120,	160,	0,	0 }, // 4
	{ 25,	65,	105,	145,	185,	0 },
	{ 30,	60,	90,	120,	150,	180 },
    };

    uint16_t y_offset_96[][6] = { 
	{ 0,	0,	0,	0,	0,	0 },
	{ 48,	0,	0,	0,	0,	0 },
	{ 30,	60,	0,	0,	0,	0 },
	{ 32,	48,	64,	0,	0,	0 },
	{ 20,	41,	60,	82,	0,	0 }, // 4
	{ 16,	32,	48,	64,	82,	0 },
	{ 10,	26,	42,	58,	76,	95 },
    };

    uint16_t y_offset_64[][6] = { 
	{ 0,	0,	0,	0,	0,	0 },
	{ 16,	0,	0,	0,	0,	0 },
	{ 10,	20,	0,	0,	0,	0 },
	{ 20,	41,	62,	0,	0,	0 },
	{ 15,	33,	47,	63,	0,	0 }, // 4
	{ 0,	0,	0,	0,	0,	0 },
	{ 0,	0,	0,	0,	0,	0 },
    };

    uint16_t y_offset_32[][6] = { 
	{ 0,	0,	0,	0,	0,	0 },
	{ 16,	0,	0,	0,	0,	0 },
	{ 15,	25,	0,	0,	0,	0 },
	{ 10,	20,	30,	0,	0,	0 },
	{ 6,	14,	22,	30,	0,	0 }, // 4
	{ 0,	0,	0,	0,	0,	0 },
	{ 0,	0,	0,	0,	0,	0 },
    };



    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	matrix->clear();
	state = 1;
	spd = 1.0;
	didclear = 0;
	firstpass = 0;
    }

    if (! ROTATE_TEXT) spd = displayall+1;

    matrix->setTextSize(1);
    if (mheight >= 192)  {
	matrix->setFont(&Century_Schoolbook_L_Bold_22);
	if (whichone == 0) matrix->setFont(&Century_Schoolbook_L_Bold_26);
	if (whichone == 1) matrix->setFont(&Century_Schoolbook_L_Bold_26);
	if (whichone == 2) matrix->setFont(&Century_Schoolbook_L_Bold_20);
	if (whichone == 3) matrix->setFont(&Century_Schoolbook_L_Bold_20);
	if (whichone == 6) matrix->setFont(&Century_Schoolbook_L_Bold_20);
	if (whichone == 10) matrix->setFont(&Century_Schoolbook_L_Bold_20);
	if (whichone == 14) matrix->setFont(&Century_Schoolbook_L_Bold_18);
    } else if (mheight >= 64)  {
	//matrix->setFont(FreeMonoBold9pt7b);
	matrix->setFont(&Century_Schoolbook_L_Bold_8);
	if (whichone == 0) matrix->setFont(&Century_Schoolbook_L_Bold_12);
	if (whichone == 1) matrix->setFont(&Century_Schoolbook_L_Bold_12);
	if (whichone == 5) matrix->setFont(&TomThumb);
	if (whichone == 6) matrix->setFont(&TomThumb);
    } else {
	matrix->setFont(&TomThumb);
    }

    if (! didclear) {
	matrix->clear();
	didclear = 1;
    }

    for (uint8_t l = 0; l < numlines[whichone]; l++) {
	uint16_t line;

	if      (mheight >= 192) line = y_offset_192[numlines[whichone]][l];
	else if (mheight >=  96) line =  y_offset_96[numlines[whichone]][l];
	else if (mheight >=  64) line =  y_offset_64[numlines[whichone]][l];
	else if (mheight >=  32) line =  y_offset_32[numlines[whichone]][l];
	String textdisp = String(text[whichone][l]);
	if (mheight <=  96) textdisp.toUpperCase();

	if (l != numlines[whichone]-1) {
	    if ((state > (l*duration-l*overlap)/spd && state < ((l+1)*duration-l*overlap)/spd) || spd > displayall) {
		matrix->setPassThruColor(color[whichone][l]);
		matrix->setCursor(text_xcenter((char *)textdisp.c_str()), line);
		matrix->print(textdisp);
		// Don't show the last line on the first pass.
		if (l>0) firstpass = 1;
	    }
	} else {
	    if ((state > (l*duration-l*overlap)/spd || (state < overlap/spd && firstpass)) || spd > displayall)  {
		matrix->setPassThruColor(color[whichone][l]);
		matrix->setCursor(text_xcenter((char *)textdisp.c_str()), line);
		matrix->print(textdisp);
	    }
	}
    }

    matrix->setPassThruColor();
    if (state++ > (numlines[whichone]*(duration-overlap))/spd) {
	state = 1;
	spd += spdincr;
	if (spd > resetspd) {
	    MATRIX_RESET_DEMO = true;
	    return 0;
	}
    }

    if (spd < displayall) fadeToBlackBy( matrixleds, NUMMATRIX, 20*map(spd, 1, 24, 1, 4));

    matrix_show();
    return 2;
}



uint8_t esrbtr_flash(uint32_t trance=false) {
    static uint16_t state;
    static uint8_t wheel;
    static float spd;
    float spdincr = 0.5;
    uint8_t resetspd = 5;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
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
	matrix->setTextSize(1);
	matrix->clear();

	if (mheight >= 192) matrix->setCursor(30, 48);
	else if (mheight >= 96) matrix->setCursor(18, 20);
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

	matrix->setPassThruColor(Wheel(((wheel+=24))));
	if (mheight >= 192) matrix->setCursor(trance?2:22, 120);
	else if (mheight >= 96) matrix->setCursor(trance?2:14, 60);
	else if (mheight >= 64) matrix->setCursor(trance?2:14, 47);
	else matrix->setCursor(trance?0:5, 22);
	if (PANELCONFNUM == 2 || PANELCONFNUM == 3) matrix->print("BURN");
	else matrix->print(trance?"TRANCE":"RAVE");

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
	    MATRIX_RESET_DEMO = true;
	    return 0;
	}
    }
    matrix_show();
    return 2;
}


// Font can be passed by name, or by index number (if f is NULL)
uint8_t display_text(const char *text, int16_t x, int16_t y, uint16_t loopcnt = 50, GFXfont *f = NULL, uint8_t fontsel=0, uint8_t zoom=1) {
    static uint16_t state;
    // displayx and displayy
    static int16_t  dx=x, dy=y;
    // bounds X, Y for text size measuring
    int16_t  bx, by;
    uint16_t w, h, fontheight;

    if (strcmp((char *)text, "") == 0) {
	Serial.println("No text to display");
	return 0;
    }

    if (MATRIX_RESET_DEMO) {
	if (!f) {
	    if (!fontsel) {
		Serial.print("Need to display: '");
		Serial.println(text);
		Serial.println("'");
		for (int8_t fibx = ARRAY_SIZE(Century_Schoolbook_L_Bold)-1; fibx >= 0; fibx--) {
		    f = (GFXfont *) &Century_Schoolbook_L_Bold[fibx];

		    matrix->setFont(f);
		    matrix->getTextBounds((char *) "X", 0, 0, &bx, &by, &w, &fontheight);
		    Serial.print("Font Size: ");
		    Serial.print(fibx);
		    Serial.print(", height: ");
		    Serial.print(fontheight);
		    Serial.print(" -> ");

		    matrix->getTextBounds((char *) text, 0, fontheight, &bx, &by, &w, &h);
		    Serial.print("Square: ");
		    Serial.print(bx);
		    Serial.print(", ");
		    Serial.print(by);
		    Serial.print(", ");
		    Serial.print(w);
		    Serial.print(", ");
		    Serial.println(h);

		    if (w > MATRIX_WIDTH) {
			Serial.print("Too wide, will try a smaller font: ");
			continue;
		    }
		    if (h > MATRIX_HEIGHT) {
			Serial.print("Too tall, will try a smaller font: ");
			continue;
		    }
		    break;
		}
		dx = 0;
		dy = (MATRIX_HEIGHT - h)/2 + fontheight;
	    } else {
		switch (fontsel) {
		case 1:
		    // 32 UPPERCASE chars, or 32 wide lowercase
		    // |T:00,05,0100,1,1,HELLOLOLO0LOLO5OLOL2LOLO5LOLO3LO
		    // |T:00,05,0100,1,1,hellololo0lolo5olol2lolo5lolo3lo
		    f = (GFXfont *) &TomThumb;
		    break;

		case 2:
		    // 13.5 UPPERCASE chars, or 18 wide lowercase
		    // |T:00,09,0100,2,1,HELLOLOL0LOLO
		    // |T:00,09,0100,2,1,hellololo0lolo5olo
		    f = (GFXfont *) &Century_Schoolbook_L_Bold_12;
		    break;

		case 3:
		    // 10 UPPERCASE chars, or 13 wide lowercase
		    // |T:00,12,0100,3,1,HELLOLOL01
		    // |T:00,12,0100,3,1,hellololo01lolol
		    f = (GFXfont *) &Century_Schoolbook_L_Bold_16;
		    break;

		case 4:
		    // 8 UPPERCASE chars, or 11.5 wide lowercase
		    // |T:00,16,0100,4,1,HELLOLOL
		    // |T:00,16,0100,4,1,hellololo012
		    f = (GFXfont *) &Century_Schoolbook_L_Bold_20;
		    break;

		case 5:
		    // 6.6 UPPERCASE chars, or 10 wide lowercase
		    // |T:00,20,0100,5,1,HELLOLO
		    // |T:00,20,0100,5,1,hellololo01
		    f = (GFXfont *) &Century_Schoolbook_L_Bold_24;
		    break;

		case 6:
		    // 5.2 UPPERCASE chars, or 8 wide lowercase
		    // |T:00,24,0100,6,1,HELLO
		    // |T:00,24,0100,6,1,hellololo
		    f = (GFXfont *) &Century_Schoolbook_L_Bold_30;
		    break;

		case 7:
		    // 28 wide, 4.5 UPPERCASE chars, or 6-7 wide lowercase
		    // |T:00,28,0100,7,1,HELLO
		    // |T:00,28,0100,7,1,hellolo
		    f = (GFXfont *) &Century_Schoolbook_L_Bold_36;
		    break;

		default:
		    if (mheight >= 64) {
			    // f = FreeMonoBold9pt7b;
			    f = (GFXfont *) &Century_Schoolbook_L_Bold_16;
		    } else {
			    f = (GFXfont *) &TomThumb;
		    }
		}
	    }
	}
	matrix->setFont(f);
	matrix->setTextSize(zoom);

	state = 0;
	MATRIX_RESET_DEMO = false;
	matrix->clear();
    }

    matrix->setCursor(dx, dy);
    state++;

    matrix->setPassThruColor(Wheel(map(state, 0, loopcnt, 0, 512)));
    matrix->print(text);
    matrix->setPassThruColor();
    matrix_show();

    matrix->setTextSize(1);
    if (state > loopcnt) {
	MATRIX_RESET_DEMO = true;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
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
	MATRIX_RESET_DEMO = true;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	matrix->clear();
	state = 1;
	spd = 1.0;
	didclear = 0;
	firstpass = 0;
    }

    unused = unused;
    if (! ROTATE_TEXT) spd = displayall+1;

    matrix->setTextSize(1);
    if (mheight >= 192)  {
	matrix->setFont(&Century_Schoolbook_L_Bold_26);
    } else if (mheight >= 64)  {
	//matrix->setFont(FreeMonoBold9pt7b);
	matrix->setFont(&Century_Schoolbook_L_Bold_12);
    } else {
	matrix->setFont(&TomThumb);
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
	    MATRIX_RESET_DEMO = true;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	x = 7;
    }
    matrix->setFont( &Century_Schoolbook_L_Bold[fontsize] );
    matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
    matrix->setTextSize(1);

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
	MATRIX_RESET_DEMO = true;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	x = 1;
    }
    matrix->setFont( &Century_Schoolbook_L_Bold[fontsize] );
    matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
    matrix->setTextSize(1);

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
	MATRIX_RESET_DEMO = true;
	return 0;
    }
    matrix_show();
    return repeat;
}

// Scroll within big bitmap so that all if it becomes visible or bounce a small one.
// If the bitmap is bigger in one dimension and smaller in the other one, it will
// be both panned and bounced in the appropriate dimensions.
// x and y can return negative coordinates if sizeX/Y is bigger than the display size
// must be called with reset=true once before changing pixmap
void panOrBounce (int16_t *x, int16_t *y, uint16_t sizeX, uint16_t sizeY, bool reset = false ) {

    // use integer math, deal with values 16 times too big
    static int16_t xf;
    static int16_t yf;
    // scroll speed in 1/16th
    static int16_t xfc;
    static int16_t yfc;
    // which direction the scroll speed is applied (-1 or 1)
    static int8_t xfdir;
    static int8_t yfdir;

    // how much scrolling there is to do (mismatch between bitmap
    // size and display size).
    static int16_t diffX;
    static int16_t diffY;

    // How far the top left corner can be moved so as not to exceed the
    // bottom right corner max position
    static int16_t maxXf;
    static int16_t maxYf;

    if (reset) {
	diffX = abs(mw - sizeX);
	diffY = abs(mh - sizeY);
	maxXf = (mw-sizeX) << 4;
	maxYf = (mh-sizeY) << 4;
	// start by showing upper left of big bitmap or centering if the display is big
	xf = max(0, (mw-sizeX)/2) << 4;
	yf = max(0, (mh-sizeY)/2) << 4;
	/// Pick a default change rate for xf/yf, not too fast, not too slow
	xfc = mmax(diffX/8, 3);
	yfc = mmax(diffY/8, 3);
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
	if (xf >= 0)     { xfdir = -1; changeDir = true ; };
	// we don't go negative past right corner, go back positive
	if (xf <= maxXf) { xfdir = 1;  changeDir = true ; };
    }
    if (sizeY-mh>2) {
	yf += yfc*yfdir;
	// we shouldn't display past left corner, reverse direction.
	if (yf >= 0)     { yfdir = -1; changeDir = true ; };
	if (yf <= maxYf) { yfdir = 1;  changeDir = true ; };
    }
    // only bounce a pixmap if it's smaller than the display size
    if (mw>sizeX) {
	xf += xfc*xfdir;
	// Deal with bouncing off the 'walls'
	if (xf >= maxXf) { xfdir = -1; changeDir = true ; };
	if (xf <= 0)	 { xfdir =  1; changeDir = true ; };
    }
    if (mh>sizeY) {
	yf += yfc*yfdir;
	if (yf >= maxYf) { yfdir = -1; changeDir = true ; };
	if (yf <= 0)	 { yfdir =  1; changeDir = true ; };
    }

    if (changeDir) {
	// Change speeds by -1, 0 or 1 but cap result to 3 to 16.
	// Let's take 3 is a minimum speed, otherwise it's too slow.
	//xfc = constrain(xfc + random(-1, 2), 3, 16);
	//yfc = constrain(yfc + random(-1, 2), 3, 16);
	// Better adjust speed to the difference between the object size and screen size
	xfc = constrain(xfc + random(-1, 2), mmin(diffX/8, 10), mmin(diffX/6, mw/6));
	yfc = constrain(yfc + random(-1, 2), mmin(diffY/8, 10), mmin(diffY/6, mh/6));
    }

    //printf("x: %d (xf:%d) mw: %d (sX: %d, diffX: %d, maxXf: %d), y: %d (yf:%d) mh: %d (sY: %d, diffY: %d, maxYf %d)\n", *x, xf, mw, sizeX, diffX, maxXf, *y, yf, mh, sizeY, diffY, maxYf);
}

uint8_t panOrBounceBitmap (uint32_t choice) {
    static uint16_t state;
    int16_t x, y;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
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
	MATRIX_RESET_DEMO = true;
	return 0;
    }
    return 3;
}

// TODO, return of all functions should be upgraded to uint16_t
// to allow gifanim to return a gif count bigger than 255
uint8_t GifAnim(uint32_t idx) {
    struct Animgif {
	const char *path;
	uint16_t looptime;
	int8_t offx;
	int8_t offy;
	int8_t factx;
	int8_t facty;
	uint16_t scrollx;
	uint16_t scrolly;
    };

    #ifdef ARDUINOONPC
	#if mheight == 192
	#define ROOT FS_PREFIX "/gifs128x192/"
	#else
	#define ROOT FS_PREFIX "/gifs64/"
	#endif
    #else
	#define ROOT "/gifs64/"
    #endif
    #if mheight == 64
	#define YMUL 10
    #else
	#define YMUL 15
    #endif

    // force the last index to detect too many initializers
    const Animgif animgif[360] = {
	#if mheight <= 32
/* 000 */   {"/gifs/32anim_photon.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/32anim_flower.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/32anim_balls.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/32anim_dance.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/circles_swap.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/concentric_circles.gif",		10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/corkscrew.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/cubeconstruct.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/cubeslide.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/runningedgehog.gif",		10, -4, 0, 10, 10, 0, 0 },
/* 010 */   {"/gifs/triangles_in.gif",			10, -4, 0, 10, 10, 0, 0 },
	    {"/gifs/wifi.gif",				10, -4, 0, 10, 10, 0, 0 },
/* 012 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 013 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 014 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 015 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 016 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 017 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 018 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 019 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
	#elif mheight <= 96
/* 000 */   { ROOT  "215_fallingcube.gif",		15, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "257_colormesh_wave.gif",		20, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "271_mj.gif",			15,-14,3, 15, YMUL, 0, 0 },
	    { ROOT  "193_redplasma.gif",		10, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "208_pulpfictiondance.gif",		25, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "412_bluecube_slide.gif",		20, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "236_spintriangle.gif",		20, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "281_plasma.gif",			20, 0, 0, 10, YMUL, 0, 0 },
	    { ROOT  "sonic.gif",			10, 0, 0, 10, YMUL, 0, 0 },
/* 009 */   { ROOT  "272_mj_spindance.gif",		15, 0, 0, 10, YMUL, 0, 0 },
	#else
/* 000 */   { ROOT  "failingcube.gif",			10, 0,10, 10,   10, 0, 0 },
	    { ROOT  "colormesh_wave.gif",		20,-20,0,  9,   10, 0, 0 },
	    { ROOT  "z_5PmD_MJ_moonwalk.gif",		40, 2, 0, 12,   10, 0, 0 },
	    { ROOT  "z_SBMP_redplasma.gif",		10,-32,0, 10,   10, 0, 0 },
	    { ROOT  "z_7VA_pulp_fiction_dance.gif",	25, 0, 0, 10,   12, 0, 0 },
	    { ROOT  "bluecube_slidepup.gif",		10, 0 ,0, 10,   10, 0, 0 },
	    { ROOT  "z_3Ppu_spin_triangles.gif",	10, 0, 0, 10,   10, 0, 0 },
	    { ROOT  "z_Egph_plasma_.gif",		10,-12,0,  8,   10, 0, 0 },
	    { ROOT  "z_3F3F_sonic.gif",			10, 0, 0,  7,   10, 0, 0 },
/* 009 */   { ROOT  "z_feM_MJ_moonwalk_spin.gif",	10,-12,0, 10,   12, 0, 0 },
	#endif

/* 010 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 011 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 012 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 013 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 014 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 015 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 016 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 017 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 018 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 019 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 020 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 021 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 022 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 023 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 024 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 025 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 026 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 027 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 028 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },
/* 029 */   { NULL, 0, 0, 0, 0, 0, 0, 0 },

	    // -- non animated, those scroll up/down
	#if mheight <= 96
/* 030 */   { ROOT  "AnB_colorballs_black.gif",	10, 0,  0, 10, 10, 64, 64 },
	    { ROOT  "AnB_color_bands.gif",	10, 0,  0, 10, 10, 64, 64 },
	    { ROOT  "AnB_color_bands_heart.gif",10, 0,  0, 10, 10, 64, 64 },
	    { ROOT  "AnB_logo_lgrey.gif",	10, 0,  0, 10, 10, 64, 64 },
	    { ROOT  "AnB_sign_lgrey.gif",	10, 0,  0, 10, 10, 64, 32 },
/* 035 */   { NULL,				10, 0,  0, 10, 10,128,128  },
	    { NULL,				10, 0,  0, 10, 10,128,128  },
	    { NULL,				10, 0,  0, 10, 10,128,128  },
	    { NULL,				10, 0,  0, 10, 10,128,128  },
	    { NULL,				10, 0,  0, 10, 10,128,128  },
/* 040 */   { ROOT  "BM_Lady_Fire.gif",		10, 0,  0, 10, 10, 64, 64 },
/* 041 */   { ROOT  "BM_Logo_lgrey.gif",	10, 0,  0, 10, 10, 64, 64 },
/* 042 */   { ROOT  "BM_Man_Scroll.gif",	10, 0, 16, 10, 10, 0,  0  },
/* 043 */   { ROOT  "BM_TheMan_Blue.gif",	10,-12,-2, 10, 10, 36, 64 },
/* 044 */   { ROOT  "BM_TheMan_Red.gif",	10,-12, 0, 10, 10, 36, 64 },
/* 045 */   { ROOT  "BM_TheMan_Green.gif",	10, 0,  4, 10, 10, 64,104 },


	#else
/* 030 */   { ROOT  "AnB_colorballs_black.gif",	10, 0,  0, 10, 10,128,128 },
	    { ROOT  "AnB_color_bands.gif",	10, 0,  0, 10, 10,128,128 },
	    { ROOT  "AnB_color_bands_heart.gif",10, 0,  0, 10, 10,128,128 },
	    { ROOT  "AnB_logo_lgrey.gif",	10, 0,  0, 10, 10,128,128 },
	    { ROOT  "AnB_sign_lgrey.gif",	10, 0,  0, 10, 10,128, 64 },
/* 035 */   { ROOT  "AnB_hand.gif",		10, 0,  0, 10, 10,128,148 },
	    { ROOT  "7lions_gold.gif",		10, 0,  0, 10, 10,128,128 },
	    { ROOT  "7lions_colors.gif",	10, 6,  9, 10, 10,140,210 },
	    { NULL,				10, 0,  0, 10, 10,128,128  },
	    { NULL,				10, 0,  0, 10, 10,128,128  },
/* 040 */   { ROOT  "BM_Lady_Fire.gif",		10, 0, 10, 10, 10,128,212  },
/* 041 */   { ROOT  "BM_Logo_lgrey.gif",	10, 0,  0, 10, 10,128,128  },
/* 042 */   { NULL,				10, 0,  0, 10, 10,128,128  },
/* 043 */   { ROOT  "BM_TheMan_Blue.gif",	10, 0,  6, 10, 10,128,212  },
/* 044 */   { ROOT  "BM_TheMan_Red.gif",	10, 0,  0, 10, 10,128,128  },
/* 045 */   { ROOT  "BM_TheMan_Green.gif",	10, 0, 10, 10, 10,128,212  },
	#endif
/* 046 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 047 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 048 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 049 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 050 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 051 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 052 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 053 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 054 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 055 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 056 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 057 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 058 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },
/* 059 */   { ROOT  "",				10, 0 , 0, 10, 10, 64, 64  },

// update FIRSTESP32 if you change the index
/* 060 */   { ROOT  "087_net.gif",		05, 0, 0, 10, YMUL, 0, 0 },
/* 061 */   { ROOT  "196_colorstar.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 062 */   { ROOT  "200_circlesmoke.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 063 */   { ROOT  "203_waterdrop.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 064 */   { ROOT  "210_circletriangle.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 065 */   { ROOT  "255_photon.gif",		10, 0, 0, 10, YMUL, 0, 0 },
/* 066 */   { ROOT  "342_spincircle.gif",	20, 0, 0, 10, YMUL, 0, 0 },
/* 067 */   { ROOT  "401_ghostbusters.gif",	05, 0, 0, 10, YMUL, 0, 0 },
/* 068 */   { ROOT  "444_hand.gif",		10, 0, 0, 10, YMUL, 0, 0 },
/* 069 */   { ROOT  "469_infection.gif",	05, 0, 0, 10, YMUL, 0, 0 },
/* 070 */   { ROOT  "284_comets.gif",		15, 0, 0, 10, YMUL, 0, 0 },
/* 071 */   { ROOT  "377_batman.gif",		07, 0, 0, 10, YMUL, 0, 0 },
/* 072 */   { ROOT  "226_flyingfire.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 073 */   { ROOT  "264_expandcircle.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 074 */   { ROOT  "286_greenplasma.gif",	15, 0, 0, 10, YMUL, 0, 0 },
/* 075 */   { ROOT  "291_circle2sphere.gif",	15, 0, 0, 10, YMUL, 0, 0 },
/* 076 */   { ROOT  "364_colortoroid.gif",	25, 0, 0, 10, YMUL, 0, 0 },
/* 077 */   { ROOT  "470_scrollcubestron.gif",	25, 0, 0, 10, YMUL, 0, 0 },
/* 078 */   { ROOT  "358_spinningpattern.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 079 */   { ROOT  "328_spacetime.gif",	20, 0, 0, 10, YMUL, 0, 0 },
/* 080 */   { ROOT  "218_circleslices.gif",	10, 0, 0, 10, YMUL, 0, 0 },
/* 081 */   { ROOT  "heartTunnel.gif",		10, 0, 0, 10, YMUL, 0, 0 },
// Update LASTESP32 if you add gifs
/* 082 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 083 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 084 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 085 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 086 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 087 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 088 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 089 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 090 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 091 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 092 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 093 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 094 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 095 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 096 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 097 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 098 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 099 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 100 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 101 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 102 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 103 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 104 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 105 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 106 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 107 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 108 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 109 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 110 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 111 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 112 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 113 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 114 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 115 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 116 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 117 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 118 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },
/* 119 */   { NULL, 0, 0 , 0, 0 , 0, 0 , 0  },   // If you extend this, please change LASTESP32IDX
/* 120 */   { ROOT  "abstract_colorful.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 121 */   { ROOT  "Aki5PC6_Running.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 122 */   { ROOT  "dancing_lady.gif",			10,-32,0, 15, 15, 0, 0 },
/* 123 */   { ROOT  "GirlSexyAnimateddance.gif",	10,-32,0, 15, 15, 0, 0 },
/* 124 */   { ROOT  "z_13xS_green_aurora.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 125 */   { ROOT  "z_19Ri_multi_aurora.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 126 */   { ROOT  "z_19Ro_city_aurora.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 127 */   { ROOT  "z_1AYl_DJ.gif",			10, 0, 0,  8, 10, 0, 0 },
/* 128 */   { ROOT  "z_1Fvr_color_string_spirals.gif",	10,-20,0,  9, 10, 0, 0 },
/* 129 */   { ROOT  "z_1KO9_orange_shapes_spinout.gif",	10,-8, 0, 10, 10, 0, 0 },
/* 130 */   { ROOT  "z_1zfD_3rdeye_spin.gif",		10,-32,0, 10, 10, 0, 0 },
/* 131 */   { ROOT  "z_24RD_8_fish_spirals.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 132 */   { ROOT  "z_2Htr_caniche.gif",		10, 0, 0,  7, 10, 0, 0 },
/* 133 */   { ROOT  "z_2mue_yingyang.gif",		10, 0,-52,10, 15, 0, 0 },
/* 134 */   { ROOT  "z_2QeW_color_stars_flash.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 135 */   { ROOT  "z_2unv_dancing_pink_back.gif",	10,10, 0, 10, 10, 0, 0 },
/* 136 */   { ROOT  "z_2vCo_triangle_merge.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 137 */   { ROOT  "z_2zFo_green_hal9000.gif",		10,-32,0, 10, 10, 0, 0 },
/* 138 */   { ROOT  "z_37Ec_bird_dance.gif",		10, 0, 0,  8, 10, 0, 0 },
/* 139 */   { ROOT  "z_3bUjj_concentric_lights.gif",	10,-32,0, 10, 10, 0, 0 },
/* 140 */   { ROOT  "z_3Mel_spiral_pentagon_dance.gif",	10,-32,0, 10, 10, 0, 0 },
/* 141 */   { ROOT  "z_3Qqj_double_stargate.gif",	10,-20,-16,9, 12, 0, 0 },
/* 142 */   { ROOT  "z_3Wfu_RGB_smirout.gif",		10,-32,0, 10, 10, 0, 0 },
/* 143 */   { ROOT  "z_3wQM_fractal_zoom.gif",		10,-24,0,  8, 10, 0, 0 },
/* 144 */   { ROOT  "z_3zO_pacman.gif",			40,-12,0,  8, 10, 0, 0 },
/* 145 */   { ROOT  "z_47Vg_purple_hair_spiralout.gif",	10,-32,0, 10, 10, 0, 0 },
/* 146 */   { ROOT  "z_4P4a_flip_triangles.gif",	10,-20,0,  8, 10, 0, 0 },
/* 147 */   { ROOT  "z_4RNd_rgb_color_plates.gif",	10,-24,0, 10, 10, 0, 0 },
/* 148 */   { ROOT  "z_4RNj_red_round_unfold.gif",	10,-12,0,  8, 10, 0, 0 },
/* 149 */   { ROOT  "z_4RNm_triangrect_shapes_out.gif",	10,-32,0, 10, 10, 0, 0 },
/* 150 */   { ROOT  "z_5x_light_tunnel.gif",		10, 0, 0,  7, 10, 0, 0 },
/* 151 */   { ROOT  "z_6nr_heart_rotate.gif",		10, 0, 0,  6, 10, 0, 0 },
/* 152 */   { ROOT  "z_6PLP_colorflowers_spiralout.gif",10,-32,0, 10, 10, 0, 0 },
/* 153 */   { ROOT  "z_72f8_green_mobius_rotate.gif",	10, 0, 0,  8, 10, 0, 0 },
/* 154 */   { ROOT  "z_72fP_cauliflower.gif",		10,-32,0, 10, 10, 0, 0 },
/* 155 */   { ROOT  "z_72gi_triple_3D_smiley.gif",	10,-8, 0,  7, 10, 0, 0 },
/* 156 */   { ROOT  "z_73O8_lightman_running.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 157 */   { ROOT  "z_75ks_green_zoomout_lasers.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 158 */   { ROOT  "z_75yI_BW_spiral_out.gif",		10,-32,0, 10, 10, 0, 0 },
/* 159 */   { ROOT  "z_76dA_starship_shooting.gif",	10,-32,0, 10, 10, 0, 0 },
/* 160 */   { ROOT  "z_78jz_blue_smoke_out.gif",	10,-32,0, 10, 10, 0, 0 },
/* 161 */   { ROOT  "z_ZIb4_red_jacket_dancer.gif",	10,-16,0, 10, 10, 0, 0 },
/* 162 */   { ROOT  "z_7gRx_white_grey_smoke.gif",	10,-32,0, 10, 10, 0, 0 },
/* 163 */   { ROOT  "z_7Gtw_flowers_spinout.gif",	10,-24,0, 10, 10, 0, 0 },
/* 164 */   { ROOT  "z_7IgI_colors_pulsing_in_out.gif", 10,-32,0, 10, 10, 0, 0 },
/* 165 */   { ROOT  "z_7IGR_red_smoke_spiral_in.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 166 */   { ROOT  "z_7MS3_grey_cubes_tunnel.gif",	10,-32,0, 10, 10, 0, 0 },
/* 167 */   { ROOT  "z_7OEb_blue_amber_juggler.gif",	10,-32,0, 10, 10, 0, 0 },
/* 168 */   { ROOT  "z_7rq5_flying_through_pipes.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 169 */   { ROOT  "z_7SHB_blue_robot_heart.gif",	10,-12,0,  8, 10, 0, 0 },
/* 170 */   { ROOT  "z_7sXr_3D_Mobius_loop.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 171 */   { ROOT  "z_7U4_endless_corridor.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 172 */   { ROOT  "z_7xyP_BW_zoomout_gears.gif",	10,-32,0, 10, 10, 0, 0 },
/* 173 */   { ROOT  "z_7ZNJ_RGB_toroid.gif",		10,-12,0,  8, 10, 0, 0 },
/* 174 */   { ROOT  "z_8vFu_mushroom_spots.gif",	10,-8, 0, 10, 10, 0, 0 },
/* 175 */   { ROOT  "z_ZJtC_you_rock.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 176 */   { ROOT  "z_9Cre_morphing_3D_shape.gif",	10,-24,0,  8, 10, 0, 0 },
/* 177 */   { ROOT  "z_9vQE_flower_petals.gif",		10,-32,0, 10, 10, 0, 0 },
/* 178 */   { ROOT  "z_9xyv_eatme.gif",			10, 0, 0, 10, 10, 0, 0 },
/* 179 */   { ROOT  "z_A8u8_sparkling_spiralin.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 180 */   { ROOT  "z_Ab6r_spingout_RGB.gif",		10,-32,0, 10, 10, 0, 0 },
/* 181 */   { ROOT  "z_Abml_green_cube_mobius.gif",	10,-32,0, 10, 10, 0, 0 },
/* 182 */   { ROOT  "z_Ajyj_3D_green_wheel_ridge.gif",	10,-32,0, 10, 10, 0, 0 },
/* 183 */   { ROOT  "z_AOkf_colorspiral_zoomout.gif",	10,-32,0, 10, 10, 0, 0 },
/* 184 */   { ROOT  "z_B0Te_spinning_dancer.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 185 */   { ROOT  "z_B87j_color_projectors.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 186 */   { ROOT  "z_ZLnU_sailormoon_highdance.gif",	10, 0,-12,10, 10, 0, 0 },
/* 187 */   { ROOT  "z_bh8_smiling_dancing_girl.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 188 */   { ROOT  "z_CDno_coiling_fern.gif",		10,-32,0, 10, 10, 0, 0 },
/* 189 */   { ROOT  "z_Cf03_yellow_lighthouse.gif",	10,-20,0, 10, 10, 0, 0 },
/* 190 */   { ROOT  "z_CuQ9_color_pyramids.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 191 */   { ROOT  "z_DemL_tunnel_spark_dancer.gif",	10,-12,0,  8, 10, 0, 0 },
/* 192 */   { ROOT  "cube_sphere_pill.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 193 */   { ROOT  "z_fxac_hyperspace.gif",		10,-32,0, 10, 10, 0, 0 },
/* 194 */   { ROOT  "z_fxcA_conifer_zoom_in.gif",	10,-32,0, 10, 10, 0, 0 },
/* 195 */   { ROOT  "z_fxcE_3D_hypercube_RGB.gif",	10,-24,0,  9, 10, 0, 0 },
/* 196 */   { ROOT  "z_fxmf_grapefuit_zoomin.gif",	10,-32,0, 10, 10, 0, 0 },
/* 197 */   { ROOT  "z_fxVE_pink_flaming_circle.gif",	10,-4, 0,  7, 10, 0, 0 },
/* 198 */   { ROOT  "z_fxYU_center_moving_spiral.gif",	10,-32,0, 10, 10, 0, 0 },
/* 199 */   { ROOT  "z_fyE2_hypnotoad.gif",		10,-32,0, 10, 10, 0, 0 },
/* 200 */   { ROOT  "z_fyNK_pizza_zoomin.gif",		10,-32,0, 10, 10, 0, 0 },
/* 201 */   { ROOT  "z_fypc_RGB_spiralin.gif",		10,-32,0, 10, 10, 0, 0 },
/* 202 */   { ROOT  "z_fype_bluebee_zoomin.gif",	10,-32,0, 10, 10, 0, 0 },
/* 203 */   { ROOT  "z_FZl2_green_neutron_star.gif",	10,-32,0, 10, 10, 0, 0 },
/* 204 */   { ROOT  "z_g09P_clock_in.gif",		10,-32,0, 10, 10, 0, 0 },
/* 205 */   { ROOT  "z_g0Af_piano_zoomin.gif",		10,-32,0, 10, 10, 0, 0 },
/* 206 */   { ROOT  "z_g0bg_puzzle_spiralout.gif",	10,-32,0, 10, 10, 0, 0 },
/* 207 */   { ROOT  "z_g0HL_shiny_snail_shell.gif",	10,-32,0, 10, 10, 0, 0 },
/* 208 */   { ROOT  "z_g0RQ_hypercube.gif",		10,-32,0, 10, 10, 0, 0 },
/* 209 */   { ROOT  "z_g1Jh_color_dots_spiralin.gif",	10,-32,0, 10, 10, 0, 0 },
/* 210 */   { ROOT  "z_g1mN_3D_fractal_roll.gif",	10,-32,0, 10, 10, 0, 0 },
/* 211 */   { ROOT  "z_g3HP_Kaleidoscope_spiral.gif",	10,-32,0, 10, 10, 0, 0 },
/* 212 */   { ROOT  "z_IAkQ_acid_cat.gif",		10,-4, 0,  9, 10, 0, 0 },
/* 213 */   { ROOT  "z_IrES_purple_geometrical.gif",	10,-32,0, 10, 10, 0, 0 },
/* 214 */   { ROOT  "z_K5bj_fly_buildings.gif",		10,-32,0, 10, 10, 0, 0 },
/* 215 */   { ROOT  "z_K5bn_pulsing_color_rects.gif",	10,-32,0, 10, 10, 0, 0 },
/* 216 */   { ROOT  "z_KTLf_white_geometric_out.gif",	10,-32,0, 10, 10, 0, 0 },
/* 217 */   { ROOT  "z_MDdU_color_marble.gif",		10, 0,32, 10, 10, 0, 0 },
/* 218 */   { ROOT  "z_MDkc_passionfruit_zoomout.gif",	10,-32,0, 10, 10, 0, 0 },
/* 219 */   { ROOT  "z_Mdml_I_am_drugs.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 220 */   { ROOT  "z_Nfhn_smileys_spinout.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 221 */   { ROOT  "z_NTHQ_flyin_cavern.gif",		10,-32,0, 10, 10, 0, 0 },
/* 222 */   { ROOT  "z_Z6W2_mario_mushroom_dance.gif",	10,-4, 0,  9, 10, 0, 0 },
/* 223 */   { ROOT  "z_NUYV_baby_pig_fall.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 224 */   { ROOT  "z_O7TJ_color_shapes_out.gif",	10, 0, 0,  7, 10, 0, 0 },
/* 225 */   { ROOT  "z_OBYz_blue_cubes_flyin.gif",	10,-32,0, 10, 10, 0, 0 },
/* 226 */   { ROOT  "z_OwRt_triangles_RGB_out.gif",	10,-32,0, 10, 10, 0, 0 },
/* 227 */   { ROOT  "z_Oz2e_rubiks_cube.gif",		10,-16,0,  9, 10, 0, 0 },
/* 228 */   { ROOT  "z_P8P_fractal2_zoom.gif",		10, 0,15, 10, 10, 0, 0 },
/* 229 */   { ROOT  "z_PAM_color_sticks.gif",		10,-32,0, 10, 10, 0, 0 },
/* 230 */   { ROOT  "z_PVyt_mushroom_walk.gif",		10,-4, 0,  7, 10, 0, 0 },
/* 231 */   { ROOT  "z_PYZK_BW_bubbles.gif",		10,-32,0, 10, 10, 0, 0 },
/* 232 */   { ROOT  "z_T2wm_flying_grass.gif",		10,-32,0, 10, 10, 0, 0 },
/* 233 */   { ROOT  "z_tIH_blue_gecko_dance.gif",	10,-12,0,  9, 10, 0, 0 },
/* 234 */   { ROOT  "z_TlCL_dancing_flames.gif",	10,-32,0, 10, 10, 0, 0 },
/* 235 */   { ROOT  "z_VCq8_skeleton.gif",		10, 0, 0, 10, 10, 0, 0 },
/* 236 */   { ROOT  "z_VRfm_fly_purple_gates.gif",	10,-32,0, 10, 10, 0, 0 },
/* 237 */   { ROOT  "z_WGkW_bluelady_smoke.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 238 */   { ROOT  "z_WMDv_sailor_moon.gif",		10,-20,0,  9, 10, 0, 0 },
/* 239 */   { ROOT  "z_WSK_inca_spiralin.gif",		10,-32,0, 10, 10, 0, 0 },
/* 240 */   { ROOT  "z_WUUT_eye.gif",			10,-32,0, 10, 10, 0, 0 },
/* 241 */   { ROOT  "z_XiPu_blue_shark_dance.gif",	10,-32,0, 10, 10, 0, 0 },
/* 242 */   { ROOT  "z_XqyP_blue_dancer.gif",		10,-32,0, 10, 10, 0, 0 },
/* 243 */   { ROOT  "z_XwIB_snoopdog_dance.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 244 */   { ROOT  "z_Ysrm_walking_dead.gif",		10, 0, 0,  7, 10, 0, 0 },
/* 245 */   { ROOT  "z_Yv30_street_fighter.gif",	10, 0, 0, 10, 10, 0, 0 },
/* 246 */   { ROOT  "Dreamstate1.gif",			10, 0, 0, 10, 10,182,192},
/* 247 */   { ROOT  "Dreamstate2.gif",			10, 0, 0, 10, 10, 0, 0  },
/* 248 */   { ROOT  "Dreamstate3.gif",			10, 0, 0, 10, 10,128,140},
/* 249 */   { ROOT  "Dreamstate4.gif",			10, 0, 0, 10, 10,128,154},
/* 250 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 251 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 252 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 253 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 254 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 255 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 256 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 257 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 258 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 259 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 260 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 261 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 262 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 263 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 264 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 265 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 266 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 267 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 268 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 269 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 270 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 271 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 272 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 273 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 274 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 275 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 276 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 277 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 278 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 279 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 280 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 281 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 282 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 283 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 284 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 285 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 286 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 287 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 288 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 289 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 290 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 291 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 292 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 293 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 294 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 295 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 296 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 297 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 298 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 299 */   { NULL, 10, 0 , 0, 10, 10, 0, 0  },
/* 300 */   { ROOT  "F_Anna.gif",		10, 6,  9, 10, 10,140,210  },
/* 301 */   { ROOT  "F_Anna.gif",               10, 6,  9, 10, 10,140,210  },
/* 302 */   { ROOT  "F_Anna2.gif",              10, 6,  9, 10, 10,140,210  },
/* 303 */   { ROOT  "DJ_Monique.gif",           10, 6,  9, 10, 10,140,210  },
/* 304 */   { ROOT  "DJ_BryanKearney.gif",      10, 6,  9, 10, 10,140,210  },
/* 305 */   { ROOT  "DJ_Sony.gif",              10, 6,  9, 10, 10,140,210  },
/* 306 */   { ROOT  "DJ_Blur.gif",              10, 6,  9, 10, 10,140,210  },
/* 307 */   { ROOT  "DJ_Ulrich.gif",		10, 6,  9, 10, 10,140,210  },
/* 308 */   { ROOT  "DJ_7lions.gif",	        10, 6,  9, 10, 10,140,210  },
/* 309 */   { ROOT  "DJ_ATB.gif",		10, 6,  9, 10, 10,140,210  },
/* 310 */   { ROOT  "DJ_Above_and_Beyond3.gif",	10, 6,  9, 10, 10,140,210  },
/* 311 */   { ROOT  "DJ_Afik_Cohen.gif",	10, 6,  9, 10, 10,140,210  },
/* 312 */   { ROOT  "DJ_Alessandra_Roncone.gif",10, 6,  9, 10, 10,140,210  },
/* 313 */   { ROOT  "DJ_Alex_MORPH.gif",	10, 6,  9, 10, 10,140,210  },
/* 314 */   { ROOT  "DJ_Alpha9.gif",            10, 6,  9, 10, 10,140,210  },
/* 315 */   { ROOT  "DJ_Alvin.gif",		10, 6,  9, 10, 10,140,210  },
/* 316 */   { ROOT  "DJ_AlynFila_Fadi.gif",	10, 0 , 0, 10, 10,140,210  },
/* 317 */   { ROOT  "DJ_AlynFila_lgrey.gif",	10, 6,  9, 10, 10,100,192  },
/* 318 */   { ROOT  "DJ_Aname.gif",             10, 6,  9, 10, 10,140,210  },
/* 319 */   { ROOT  "DJ_AndrewRayel.gif",	10, 6,  9, 10, 10,140,210  },
/* 320 */   { ROOT  "DJ_Astrix.gif",		10, 6,  9, 10, 10,140,210  },
/* 321 */   { ROOT  "DJ_AvB.gif",	    	10, 6,  9, 10, 10,140,210  },
/* 322 */   { ROOT  "DJ_Blastoyz.gif",		10, 6,  9, 10, 10,140,210  },
/* 323 */   { ROOT  "DJ_CosmicGate.gif",	10, 6,  9, 10, 10,140,210  },
/* 324 */   { ROOT  "DJ_Craig_Connelly.gif",	10, 6,  9, 10, 10,140,210  },
/* 325 */   { ROOT  "DJ_Dave_Dresden.gif",	10, 6,  9, 10, 10,140,210  },
/* 326 */   { ROOT  "DJ_Emma_Hewitt.gif",	10, 6,  9, 10, 10,140,210  },
/* 327 */   { ROOT  "DJ_FactorB.gif",		10, 6,  9, 10, 10,140,210  },
/* 328 */   { ROOT  "DJ_FerryCorsten.gif",	10, 6,  9, 10, 10,140,210  },
/* 329 */   { ROOT  "DJ_Ferry_Tayle.gif",       10, 6,  9, 10, 10,140,210  },
/* 330 */   { ROOT  "DJ_Gareth_Emery.gif",	10, 6,  9, 10, 10,140,210  },
/* 331 */   { ROOT  "DJ_Giuseppe_Logo.gif",	10, 0 , 0, 10, 10,128,117 },
/* 332 */   { ROOT  "DJ_Giuseppe.gif",		10, 6,  9, 10, 10,140,210  },
/* 333 */   { ROOT  "DJ_Haliene.gif",		10, 6,  9, 10, 10,140,210  },
/* 334 */   { ROOT  "DJ_Ilan_Bluestone.gif",	10, 6,  9, 10, 10,140,210  },
/* 335 */   { ROOT  "DJ_JOC.gif",		10, 6,  9, 10, 10,140,210  },
/* 336 */   { ROOT  "DJ_John_00_Flemming.gif",	10, 6,  9, 10, 10,140,210  },
/* 337 */   { ROOT  "DJ_Khromata.gif",		10, 6,  9, 10, 10,140,210  },
/* 338 */   { ROOT  "DJ_Lange.gif",             10, 6,  9, 10, 10,140,210  },
/* 339 */   { ROOT  "DJ_Liquid_Soul.gif",	10, 6,  9, 10, 10,140,210  },
/* 340 */   { ROOT  "DJ_Markus_Schulz.gif",     10, 6,  9, 10, 10,140,210  },
/* 341 */   { ROOT  "DJ_Marlo.gif",		10, 6,  9, 10, 10,140,210  },
/* 342 */   { ROOT  "DJ_Miyuki.gif",		10, 6,  9, 10, 10,140,210  },
/* 343 */   { ROOT  "DJ_Nifra.gif",             10, 6,  9, 10, 10,140,210  },
/* 344 */   { ROOT  "DJ_Paul_Oakenfold.gif",	10, 6,  9, 10, 10,140,210  },
/* 345 */   { ROOT  "DJ_PaulVanDyk.gif",	10, 6,  9, 10, 10,140,210  },
/* 346 */   { ROOT  "DJ_RDR_lgrey.gif",		10, 0 , 0, 10, 10,128,128 },
/* 347 */   { ROOT  "DJ_Reality_Test.gif",	10, 6,  9, 10, 10,140,210  },
/* 348 */   { ROOT  "DJ_Richard_Durand.gif",    10, 6,  9, 10, 10,140,210  },
/* 349 */   { ROOT  "DJ_Rinaly.gif",		10, 6,  9, 10, 10,140,210  },
/* 350 */   { ROOT  "DJ_Ruben.gif",		10, 0 , 0, 10, 10,128,170 },
/* 351 */   { ROOT  "DJ_Solarstone.gif",	10, 6,  9, 10, 10,140,210  },
/* 352 */   { ROOT  "DJ_Somna.gif",		10, 6,  9, 10, 10,140,210  },
/* 353 */   { ROOT  "DJ_Susana.gif",		10, 6,  9, 10, 10,140,210  },
/* 354 */   { ROOT  "DJ_Talla-2XLC.gif",	10, 6,  9, 10, 10,140,210  },
/* 355 */   { ROOT  "DJ_Thrillseekers.gif",	10, 6,  9, 10, 10,140,210  },
/* 356 */   { ROOT  "DJ_Triode.gif",	        10, 6,  9, 10, 10,140,210  },
/* 357 */   { ROOT  "DJ_Vini_Vici.gif",		10, 0 , 0, 10, 10,128,140 },
/* 358 */   { ROOT  "DJ_WhiteNo1se.gif",	10, 6,  9, 10, 10,140,210  },
// Leave AvB as last, add below first one
/* 359 */   { ROOT  "DJ_AvB_logo_lgrey.gif",	10, 0 , 0, 10, 10,100,192 },
        };

    //
    #define FIRSTESP32 60
    #define LASTESP32 81
    #define LASTESP32IDX 119

    int16_t x, y;
    uint8_t repeat = 1;
    static uint16_t scrollx = 0;
    static uint16_t scrolly = 0;
    extern int OFFSETX;
    extern int OFFSETY;
    extern int FACTY;
    extern int FACTX;
    const char *path;

    GIF_CNT = ARRAY_SIZE(animgif);
    // Compute GIF_CNT and exit
    if (idx == 65535) return 0;
    // Avoid crashes due to overflows
    idx = idx % GIF_CNT;

    if (MATRIX_RESET_DEMO) {
        MATRIX_RESET_DEMO = false;
        GIFLOOPSEC =  animgif[idx].looptime;
        // ARDUINOONPC make the loop last longer
        #ifdef ARDUINOONPC
            // if we are connected to a remote device, let it change patterns for us
            if (ttyfd >= 0 && esp32_connected) GIFLOOPSEC = 1000;
	#else
            GIFLOOPSEC *= 2;
        #endif
	#ifdef ESP32
	    if (idx > LASTESP32IDX) {
		// when using ESP32 to control an rPi, we can get GIF indexes that do
		// not exist on ESP32. If so, convert them to a locally viewable one.
		idx = ((idx - LASTESP32IDX -1) % (LASTESP32 - FIRSTESP32)) + FIRSTESP32;
	    }
	#endif
        OFFSETX = animgif[idx].offx;
        OFFSETY = animgif[idx].offy;
        FACTX =   animgif[idx].factx;
        FACTY =   animgif[idx].facty;
        scrollx = animgif[idx].scrollx;
        scrolly = animgif[idx].scrolly;
	path = animgif[idx].path;
        if (path == NULL) {
	    Serial.print(">>>> ERROR: NO GIF for index ");
	    Serial.print(idx);
	    Serial.println(". Putting default one... <<<<");
	    path = animgif[1].path;
	}
        matrix->clear();
        // initialize x/y by sending the reset flag
        panOrBounce(&x, &y, scrollx, scrolly, true);
        // exit if the gif animation couldn't get setup.
        if (sav_newgif(path)) return 0;
    }

    if (SCROLL_IMAGE) {
	if (scrollx || scrolly) {
	    // Get back an x and y to use for offset display
	    panOrBounce(&x, &y, scrollx, scrolly);
	    OFFSETX = animgif[idx].offx + x;
	    OFFSETY = animgif[idx].offy + y;
	    matrix->clear();
	    //Serial.print(x);
	    //Serial.print(" ");
	    //Serial.println(y);
	}
    } else {
	OFFSETX = 0;
	OFFSETY = 0;
    }
    // sav_loop may or may not run show() depending on whether
    // it's time to decode the next frame. If it did not, wait here to
    // add the matrix_show() delay that is expected by the caller
    // Not needed anymore with Aiko, it handles calling frequency
    //bool savl = sav_loop();
    //if (savl) { delay(MX_UPD_TIME); };

    sav_loop();
    ShowMHfps();

    EVERY_N_SECONDS(1) {
	Serial.print(GIFLOOPSEC); Serial.print(" ");
	if (!GIFLOOPSEC--) { Serial.println(); return 0; };
    }
    return repeat;
}

uint8_t scrollBigtext(uint32_t unused) {
    // 64x96 pixels, chars are 5(6)x7, 10.6 chars across, 13.7 lines of displays
    static uint16_t state = 0;
    static uint8_t resetcnt = 1;
    uint16_t loopcnt = 700;
    int16_t x, y;

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

    if (MATRIX_RESET_DEMO) {
	state = 0;
	MATRIX_RESET_DEMO = false;
	panOrBounce(&x, &y, 54*6, ARRAY_SIZE(text)*7, true);
	for (uint8_t i=0; i<=textlines-1; i++) {
	    textcolor[i] = random8(96) * 65536 + (127 + random8(128))* 256, random8(96);
	    //Serial.print("Setup color mapping: ");
	    //Serial.println(textcolor[i], HEX);
	}
    }
    matrix->setTextSize(1);
    // default font is 5x7, but you really need 6x8 for spacing
    matrix->setFont(NULL);

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
	MATRIX_RESET_DEMO = true;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
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
	MATRIX_RESET_DEMO = true;
	return 0;
    }

    matrix_show();
    return repeat;
}


uint8_t call_twinklefox(uint32_t unused)
{
    static uint16_t state;

    unused = unused;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	state = 0;
    }

    twinkle_loop();
    ShowMHfps();
    if (state++ < 1000) return 2;
    MATRIX_RESET_DEMO = true;
    return 0;
}

uint8_t call_pride(uint32_t unused)
{
    static uint16_t state;

    unused = unused;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	state = 0;
    }

    pride();
    matrix_show();
    if (state++ < 1000) return 1;
    MATRIX_RESET_DEMO = true;
    return 0;
}

uint8_t call_fireworks(uint32_t unused) {
    static uint16_t state;

    unused = unused;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	matrix->clear();
	state = 0;
    }

    fireworks();
    matrix_show();
    if (state++ < 500) return 1;
    MATRIX_RESET_DEMO = true;
    return 0;
}

uint8_t call_fire(uint32_t unused) {
    static uint16_t state;

    unused = unused;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	matrix->clear();
	state = 0;
    }

    // rotate palette
    sublime_loop();
    fire();
    matrix_show();
    if (state++ < 1500) return 1;
    MATRIX_RESET_DEMO = true;
    return 0;
}

uint8_t call_rain(uint32_t which) {
    #define raindelay 2
    static uint16_t state;
    static uint16_t delayframe = raindelay;

    if (MATRIX_RESET_DEMO) {
	sublime_reset();
	MATRIX_RESET_DEMO = false;
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
    MATRIX_RESET_DEMO = true;
    return 0;
}

uint8_t call_pacman(uint32_t unused) {
    uint8_t loopcnt = 3;
    #define pacmandelay 5
    static uint16_t delayframe = pacmandelay;

    unused = unused;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
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
    MATRIX_RESET_DEMO = true;
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

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
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
    MATRIX_RESET_DEMO = true;
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
	{  14, 3, 200 },  //    
	{  23, 3, 200 },  //    

    };
//	{  29, 5, 300 },  // XX swirly RGB colored dots meant to go to music
//	{  34, 5, 300 },  // XX single colored lines that extend from center, meant for music
//	{  73, 3, 2000 }, // XX circle inside fed by outside attracted color dots
//	{ 110, 5, 300 },  // XX bubbles going up or right

    uint8_t dfinit = tmed_mapping[demo][1];
    uint16_t loops = tmed_mapping[demo][2];
    static uint8_t delayframe = dfinit;
    demo = tmed_mapping[demo][0];

    if (MATRIX_RESET_DEMO) {
	Serial.print("Starting ME Table Demo #");
	Serial.println(demo);
	MATRIX_RESET_DEMO = false;
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

    // New TME
    case 14:
      spire2();
      if (flop[0] && flop[1])
        adjustme();
      break;

    case 23:
      triforce();
      if (flop[6] && (flop[1] || !flop[2])) adjustme();
      break;
    }

    td_loop();
    matrix_show();
    if (counter < loops) return 2;
    MATRIX_RESET_DEMO = true;
    return 0;
}

uint8_t call_v4lcapture(uint32_t mirror) {
    static uint32_t state;

    if (MATRIX_RESET_DEMO) {
	MATRIX_RESET_DEMO = false;
	state = 0;
    }

#ifdef ARDUINOONPC
    if (v4lcapture_loop(mirror)) return 0;
    //printf("RPI Video Loop: %d, state %d\n", MATRIX_LOOP, state);
#endif
    if (state++ < 5000) return 1; // ESP32 will command 1 loops if called from next
    // if called from the menu, it will last 100 loops instead. The ESP32 loops are
    // empty loops, so the time is estimated
    MATRIX_RESET_DEMO = true;
#ifndef ARDUINOONPC
    Serial.printf("ESP Video Loop: %d\n", MATRIX_LOOP);
#endif
    return 0;
}

uint8_t thank_you(uint32_t unused) {
    static uint8_t ret;

#ifdef M32BY8X3
    const char str[] = "Thank You :)";
    ret = scrollText(str, sizeof(str));
#else
    ret = display_text("Thank\nYou\nVery\nMuch", 0, 0, 10, NULL, 0, 1);
    fixdrawRGBBitmap(104, 86, RGB_bmp, 8, 8);

    fixdrawRGBBitmap(110, 82, RGB_bmp, 8, 8);
    fixdrawRGBBitmap(115, 89, RGB_bmp, 8, 8);
    fixdrawRGBBitmap(110, 96, RGB_bmp, 8, 8);
    fixdrawRGBBitmap(104,102, RGB_bmp, 8, 8);
    fixdrawRGBBitmap( 98, 96, RGB_bmp, 8, 8);
    fixdrawRGBBitmap( 93, 89, RGB_bmp, 8, 8);
    fixdrawRGBBitmap( 98, 82, RGB_bmp, 8, 8);
#endif

    return ret;
}

uint8_t web_text_input(uint32_t unused) {
    static uint8_t ret;
    // If first char is a digit, we assume it's a full string with coordinates
    // to display a number, prepend '>'
    if (DISPLAYTEXT.c_str()[0] < 58 && DISPLAYTEXT.c_str()[2] == ',') {
	// 0123456789012345
	// XX,YY,LOOP,F,Z,TEXT  (offset of first char, # of times to loop, Font idx, zoom, text (with newlines)
	uint16_t x, y, loop, fontidx, zoom;
	String displaystr;

	x =	    atoi(DISPLAYTEXT.c_str()+0);
	y =	    atoi(DISPLAYTEXT.c_str()+3);
	loop =	    atoi(DISPLAYTEXT.c_str()+6);
	fontidx =   atoi(DISPLAYTEXT.c_str()+11);
	zoom =	    atoi(DISPLAYTEXT.c_str()+13);
	displaystr= DISPLAYTEXT.substring(15);
	ret = display_text(displaystr.c_str(), x, y, loop, NULL, fontidx, zoom);
    } else {
	ret = display_text(DISPLAYTEXT.c_str(), 0, 0, 40, NULL, 0, 1);
    }

    return ret;
}




// ================================================================================

// demo_map.txt contains a logical demo number (0 to 99 for generated, 100-129 for
// animated gifs on 32h, or shared animated gifs on 64/96/192h, and 160+ for unshared
// animated gifs in 64/96/192h)
// however, at runtime the ESP32 can switch from its own resolution demos
// PANELCONFNUM 0:24x32, 1:64x64 (BM), 2:64x96 (BM), 3:64x96 (Trance), 4:128x192
uint16_t demoidx(uint16_t idx) {
    if (idx<100) return idx;

    // If the demo# is too high, map to the null demo
    if (PANELCONFNUM == 0 && idx>129) return 0;
    if (PANELCONFNUM < 4  && idx>219) return 0;

    // panelconf0 has direct mapping to 100, the others start at 120
    if (PANELCONFNUM == 0) return idx;
    if (PANELCONFNUM < 4) return (idx+20);
    // 192h demos are 120 to 180 (for 100 to 160) and then jump to 240
    if (idx < 160) return (idx+20);
    return (idx+80);
}

Demo_Entry demo_list[DEMO_ARRAY_SIZE] = {
/* 000 */ { "NULL Demo", NULL, -1, NULL },
/* 001 */ { "Squares In",  squares, 0, NULL },
/* 002 */ { "Squares Out", squares, 1, NULL },
/* 003 */ { "EatSleepTranceRepeat Flash", esrbtr_flash, 1, NULL },
/* 004 */ { "EatSleepRaveBurnRepeat Flash", esrbtr_flash, 0, NULL },
/* 005 */ { "TFSF Zoom InOut", tfsf_zoom, 1, NULL },
/* 006 */ { "TFSF Display", tfsf, -1, NULL },
/* 007 */ { "With Every Beat...", webwc, -1, NULL },
/* 008 */ { "", NULL, -1, NULL },
/* 009 */ { "", NULL, -1, NULL },
/* 010 */ { "", NULL, -1, NULL },
/* 011 */ { "", NULL, -1, NULL },
/* 012 */ { "", NULL, -1, NULL },
/* 013 */ { "", NULL, -1, NULL },
/* 014 */ { "", NULL, -1, NULL },
/* 015 */ { "", NULL, -1, NULL },
/* 016 */ { "Bounce Smiley", panOrBounceBitmap, 1, NULL },  // currently only 24x32
/* 017 */ { "Fireworks", call_fireworks, -1, NULL },
/* 018 */ { "TwinkleFox", call_twinklefox, -1, NULL },
/* 019 */ { "Pride", call_pride, -1, NULL },		// not nice for higher res (64 and above)
/* 020 */ { "Demoreel Stars", demoreel100, 1, NULL },	// Twinlking stars
/* 021 */ { "Demoreel Sweeper", demoreel100, 2, NULL },	// color changing pixels sweeping up and down
/* 022 */ { "Demoreel Dbl Sweeper", demoreel100, 3, NULL },	// colored pixels being exchanged between top and bottom
/* 023 */ { "Matrix", call_rain, 1, NULL },			// matrix
/* 024 */ { "Storm", call_rain, 3, NULL },			// clouds, rain, lightening
/* 025 */ { "Pac Man", call_pacman, -1, NULL },		// currently only designed for 24x32
/* 026 */ { "Plasma", plasma, -1, NULL },
/* 027 */ { "Fire", call_fire, -1, NULL },
/* 028 */ { "Safety Third", DoublescrollText, 1, NULL },    // adjusts
/* 029 */ { "ScrollBigtext", scrollBigtext, -1, NULL },     // code of scrolling code
/* 030 */ { "Aurora Attract", aurora,  0, NULL },
/* 031 */ { "Aurora Bounce", aurora,  1, NULL },
/* 032 */ { "Aurora Cube", aurora,  2, NULL },
/* 033 */ { "Aurora Flock", aurora,  3, NULL },
/* 034 */ { "Aurora Flowfield", aurora,  4, NULL },
/* 035 */ { "Aurora Incremental Drift", aurora,  5, NULL },
/* 036 */ { "Aurora Incremental Drift2", aurora,  6, NULL },
/* 037 */ { "Aurora Pendulum Wave ", aurora,  7, NULL },
/* 038 */ { "Aurora Radar", aurora,  8, NULL },		// 8 not great on non square
/* 039 */ { "Aurora Spiral/Line Screensave", aurora,  9, NULL },
/* 040 */ { "Aurora Spiro", aurora, 10, NULL },
/* 041 */ { "Aurora Swirl", aurora, 11, NULL },		// 11 not great on bigger display
/* 042 */ { "Aurora Wave", aurora, 12, NULL },
/* 043 */ { "", NULL, -1, NULL },
/* 044 */ { "", NULL, -1, NULL },
/* 045 */ { "TMED  0 Zoom in shapes", tmed,  0, NULL },	// concentric colors and shapes
/* 046 */ { "TMED  1 Concentric circles", tmed,  1, NULL }, // 5 color windows-like pattern with circles in and out
/* 047 */ { "TMED  2 Color Starfield", tmed,  2, NULL },	// color worm patterns going out with circles zomming out
/* 048 */ { "TMED  3 Dancing Circles", tmed,  3, NULL },	// 5 circles turning together, run a bit longer
/* 049 */ { "TMED  4 Zoom out Shapes", tmed,  4, NULL },	// rectangles/squares/triangles zooming out
/* 050 */ { "TMED  5 Shapes In/Out", tmed,  5, NULL },	// opposite concentric colors and shapes (52 reversed)
/* 051 */ { "TMED  6 Double Starfield&Shapes", tmed,  6, NULL }, // double color starfield with shapes in/out
/* 052 */ { "TMED  7 New Hypnoswirl", tmed,  7, NULL }, // two colors swirling bigger, creating hypno pattern
/* 053 */ { "TMED  8 4 Dancing Balls&Shapes", tmed,  8, NULL }, // 4 fat spinning comets with shapes growing from middle sometimes
/* 054 */ { "TMED  9 Starfield BKringer", tmed,  9, NULL }, // streaming lines of colored pixels with shape zooming in or out
/* 055 */ { "TMED 10 Spinning Triangles", tmed, 10, NULL }, // rotating triangles of color
/* 056 */ { "TMED 11 Circles Mixing", tmed, 11, NULL },	// circles mixing in the middle
/* 057 */ { "TMED 12 Hypno", tmed, 12, NULL },		// hypno
/* 058 */ { "TMED 13 Circling circles", tmed, 13, NULL },// 3 color circles circling around 3 middle ones
/* 059 */ { "TMED 14 Spinning Offset Triangle", tmed, 14, NULL },// FIXME
/* 060 */ { "", NULL, -1, NULL },
/* 061 */ { "", NULL, -1, NULL },
/* 062 */ { "", NULL, -1, NULL },
/* 063 */ { "", NULL, -1, NULL },
/* 064 */ { "", NULL, -1, NULL },
/* 065 */ { "", NULL, -1, NULL },
/* 066 */ { "", NULL, -1, NULL },
/* 067 */ { "", NULL, -1, NULL },
/* 068 */ { "", NULL, -1, NULL },
/* 069 */ { "", NULL, -1, NULL },
/* 070 */ { "", NULL, -1, NULL },
/* 071 */ { "", NULL, -1, NULL },
/* 072 */ { "", NULL, -1, NULL },
/* 073 */ { "", NULL, -1, NULL },
/* 074 */ { "", NULL, -1, NULL },
/* 075 */ { "Trance Microphone", rotate_text, 14, NULL },
/* 076 */ { "Music Lover", rotate_text, 13, NULL },
/* 077 */ { "Trance Soul", rotate_text, 12, NULL },
/* 078 */ { "Time to Play", rotate_text, 11, NULL },
/* 079 */ { "Trance Or Me", rotate_text, 10, NULL },
/* 080 */ { "Trance Jesus Do", rotate_text, 9, NULL },
/* 081 */ { "Trance Jesus Vinyl", rotate_text, 8, NULL },
/* 082 */ { "Trance Because Of Course", rotate_text, 7, NULL },
/* 083 */ { "Trance Country", rotate_text, 6, NULL },
/* 084 */ { "Trance Techno", rotate_text, 5, NULL },
/* 085 */ { "Trance RichEDM", rotate_text, 4, NULL },
/* 086 */ { "Trance Snob", rotate_text, 3, NULL },
/* 087 */ { "Trance Awkward", rotate_text, 2, NULL },
/* 088 */ { "EatSleepTranceRepeat", rotate_text, 1, NULL },
/* 089 */ { "EatSleepRaveBurnRepeat", rotate_text, 0, NULL },
// Give a fake demo we won't call. We actually call display_text but it takes
// more arguments, so it can't be used in this struct.and the function called
// manually is display_text with more arguments
/* 090 */ { "Thank you",	thank_you, -1, NULL },		// DEMO_TEXT_THANKYOU
/* 091 */ { "Web Text Input",	web_text_input, -1, NULL },	// DEMO_TEXT_INPUT
/* 092 */ { "I'm fucking Famous", rotate_text, 15, NULL },
/* 093 */ { "Fuck Me I'm Famous", rotate_text, 16, NULL },
/* 094 */ { "Perhaps Drugs", rotate_text, 17, NULL },
/* 095 */ { "", NULL, -1, NULL },
/* 096 */ { "", NULL, -1, NULL },
/* 097 */ { "", NULL, -1, NULL },
/* 098 */ { "Camera", call_v4lcapture, 0, NULL },
/* 099 */ { "Camera Mirror", call_v4lcapture, 1, NULL },
// Up to here, there is a 1-1 mapping from the ID
// mheight == 32
/* 100 */ { "GIF photon"	, GifAnim,  0, NULL }, // mapped to 100
/* 101 */ { "GIF flower"	, GifAnim,  1, NULL },
/* 102 */ { "GIF balls"	    	, GifAnim,  2, NULL },
/* 103 */ { "GIF dance"		, GifAnim,  3, NULL },
/* 104 */ { "GIF circles_swap"	, GifAnim,  4, NULL },
/* 105 */ { "GIF concentric_circles", GifAnim,  5, NULL },
/* 106 */ { "GIF corkscrew"	, GifAnim,  6, NULL },
/* 107 */ { "GIF cubeconstruct"	, GifAnim,  7, NULL },
/* 108 */ { "GIF cubeslide"	, GifAnim,  8, NULL },
/* 109 */ { "GIF runningedgehog", GifAnim,  9, NULL },
/* 110 */ { "GIF triangles_in"	, GifAnim, 10, NULL },
/* 111 */ { "GIF wifi"		, GifAnim, 11, NULL },
/* 112 */ { "", NULL, -1, NULL },
/* 113 */ { "", NULL, -1, NULL },
/* 114 */ { "", NULL, -1, NULL },
/* 115 */ { "", NULL, -1, NULL },
/* 116 */ { "", NULL, -1, NULL },
/* 117 */ { "", NULL, -1, NULL },
/* 118 */ { "", NULL, -1, NULL },
/* 119 */ { "", NULL, -1, NULL },
// Since ESP8266 has less memory and fewer GIFS, let's set DEMO_ARRAY_SIZE
// to 120 on that platform
//
// ESP32 and rPi continue below
// mheight > 32, shared animated gifs
// They need to be mapped to a different IDs in the mapping array so
// that the display size and mapping can be changed at runtime
// It is however ok to map them to different filenames depending on the
// backend as long as they are the same name
/* 120 */ { "GIF fallingcube"	,GifAnim,  0, NULL }, // mapped to 100
/* 121 */ { "GIF colormesh wave",GifAnim,  1, NULL },
/* 122 */ { "GIF Michael Jackson",GifAnim,  2, NULL },
/* 123 */ { "GIF redplasma"	,GifAnim,  3, NULL },
/* 124 */ { "GIF pulpfictiondance",GifAnim, 4, NULL },
/* 125 */ { "GIF bluecube_slide",GifAnim,  5, NULL },
/* 126 */ { "GIF spintriangle"	,GifAnim,  6, NULL },
/* 127 */ { "GIF plasma"	,GifAnim,  7, NULL },
/* 128 */ { "GIF sonic"		,GifAnim,  8, NULL },
/* 129 */ { "GIF MJ2 spin dance",GifAnim,  9, NULL },
/* 130 */ { "", NULL, -1, NULL },
/* 131 */ { "", NULL, -1, NULL },
/* 132 */ { "", NULL, -1, NULL },
/* 133 */ { "", NULL, -1, NULL },
/* 134 */ { "", NULL, -1, NULL },
/* 135 */ { "", NULL, -1, NULL },
/* 136 */ { "", NULL, -1, NULL },
/* 137 */ { "", NULL, -1, NULL },
/* 138 */ { "", NULL, -1, NULL },
/* 139 */ { "", NULL, -1, NULL },
/* 140 */ { "", NULL, -1, NULL },
/* 141 */ { "", NULL, -1, NULL },
/* 142 */ { "", NULL, -1, NULL },
/* 143 */ { "", NULL, -1, NULL },
/* 144 */ { "", NULL, -1, NULL },
/* 145 */ { "", NULL, -1, NULL },
/* 146 */ { "", NULL, -1, NULL },
/* 147 */ { "", NULL, -1, NULL },
/* 148 */ { "", NULL, -1, NULL },
/* 149 */ { "", NULL, -1, NULL }, // mapped to 129
// mheight > 32, shared non animated gifs
// It is however ok to map them to different filenames depending on the
// backend as long as they are the same name
/* 150 */ { "GIF A&B colors balls",  GifAnim,  30, NULL }, // mapped to 130
/* 151 */ { "GIF A&B Color Bands",   GifAnim,  31, NULL },
/* 152 */ { "GIF A&B CBands Heart",  GifAnim,  32, NULL },
/* 153 */ { "GIF A&B Logo lgrey",    GifAnim,  33, NULL },
/* 154 */ { "GIF A&B Sign lgrey",    GifAnim,  34, NULL },
/* 155 */ { "GIF A&B Hand",	     GifAnim,  35, NULL },
/* 156 */ { "GIF 7 Lions Gold",	     GifAnim,  36, NULL },
/* 157 */ { "GIF 7 Lions Colors",    GifAnim,  37, NULL },
/* 158 */ { "", NULL,  38, NULL },
/* 159 */ { "", NULL,  39, NULL },
/* 160 */ { "GIF BM Lady Fire",	     GifAnim,  40, NULL },
/* 161 */ { "GIF BM Logo lgrey",     GifAnim,  41, NULL },
/* 162 */ { "GIF BM Man Scroll",     GifAnim,  42, NULL },
/* 163 */ { "GIF BM TheMan Blue",    GifAnim,  43, NULL },
/* 164 */ { "GIF BM TheMan_Green",   GifAnim,  44, NULL },
/* 165 */ { "GIF BM TheMan Red",     GifAnim,  45, NULL },
/* 166 */ { "", NULL,  46, NULL },
/* 167 */ { "", NULL,  47, NULL },
/* 168 */ { "", NULL,  48, NULL },
/* 169 */ { "", NULL,  49, NULL },
/* 170 */ { "", NULL,  50, NULL },
/* 171 */ { "", NULL,  51, NULL },
/* 172 */ { "", NULL,  52, NULL },
/* 173 */ { "", NULL,  53, NULL },
/* 174 */ { "", NULL,  54, NULL },
/* 175 */ { "", NULL,  55, NULL },
/* 176 */ { "", NULL,  56, NULL },
/* 177 */ { "", NULL,  57, NULL },
/* 178 */ { "", NULL,  58, NULL },
/* 179 */ { "", NULL,  59, NULL },
// GIFs from here are mheight == 96, assigned to 60
// Other resolution will also be assigned from the same index
// so that some gif gets displayed, even if it's the wrong one.
/* 180 */ { "GIF net"		,	 GifAnim, 60, NULL }, // mapped to 160
/* 181 */ { "GIF colorstar"	,	 GifAnim, 61, NULL },
/* 182 */ { "GIF circlesmoke"	,	 GifAnim, 62, NULL },
/* 183 */ { "GIF waterdrop"	,	 GifAnim, 63, NULL },
/* 184 */ { "GIF circletriangle",	 GifAnim, 64, NULL },
/* 185 */ { "GIF photon"	,	 GifAnim, 65, NULL },
/* 186 */ { "GIF spincircle"	,	 GifAnim, 66, NULL },
/* 187 */ { "GIF ghostbusters"	,	 GifAnim, 67, NULL },
/* 188 */ { "GIF hand"		,	 GifAnim, 68, NULL },
/* 189 */ { "GIF infection"	,	 GifAnim, 69, NULL },
/* 190 */ { "GIF comets"	,	 GifAnim, 70, NULL },
/* 191 */ { "GIF batman"	,	 GifAnim, 71, NULL },
/* 192 */ { "GIF flyingfire"	,	 GifAnim, 72, NULL },
/* 193 */ { "GIF expandcircle"	,	 GifAnim, 73, NULL },
/* 194 */ { "GIF greenplasma"	,	 GifAnim, 74, NULL },
/* 195 */ { "GIF circle2sphere"	,	 GifAnim, 75, NULL },
/* 196 */ { "GIF colortoroid"	,	 GifAnim, 76, NULL },
/* 197 */ { "GIF scrollcubestron",	 GifAnim, 77, NULL },
/* 198 */ { "GIF spinningpattern",	 GifAnim, 78, NULL },
/* 199 */ { "GIF spacetime"	,	 GifAnim, 79, NULL },
/* 200 */ { "GIF circleslices"	,	 GifAnim, 80, NULL },
/* 201 */ { "GIF heartTunnel"	,	 GifAnim, 81, NULL }, // mapped to 181
/* 202 */ { "", NULL, -1, NULL },
/* 203 */ { "", NULL, -1, NULL },
/* 204 */ { "", NULL, -1, NULL },
/* 205 */ { "", NULL, -1, NULL },
/* 206 */ { "", NULL, -1, NULL },
/* 207 */ { "", NULL, -1, NULL },
/* 208 */ { "", NULL, -1, NULL },
/* 209 */ { "", NULL, -1, NULL },
/* 210 */ { "", NULL, -1, NULL },
/* 211 */ { "", NULL, -1, NULL },
/* 212 */ { "", NULL, -1, NULL },
/* 213 */ { "", NULL, -1, NULL },
/* 214 */ { "", NULL, -1, NULL },
/* 215 */ { "", NULL, -1, NULL },
/* 216 */ { "", NULL, -1, NULL },
/* 217 */ { "", NULL, -1, NULL },
/* 218 */ { "", NULL, -1, NULL },
/* 219 */ { "", NULL, -1, NULL },
/* 220 */ { "", NULL, -1, NULL },
/* 221 */ { "", NULL, -1, NULL },
/* 222 */ { "", NULL, -1, NULL },
/* 223 */ { "", NULL, -1, NULL },
/* 224 */ { "", NULL, -1, NULL },
/* 225 */ { "", NULL, -1, NULL },
/* 226 */ { "", NULL, -1, NULL },
/* 227 */ { "", NULL, -1, NULL },
/* 228 */ { "", NULL, -1, NULL },
/* 229 */ { "", NULL, -1, NULL },
/* 230 */ { "", NULL, -1, NULL },
/* 231 */ { "", NULL, -1, NULL },
/* 232 */ { "", NULL, -1, NULL },
/* 233 */ { "", NULL, -1, NULL },
/* 234 */ { "", NULL, -1, NULL },
/* 235 */ { "", NULL, -1, NULL },
/* 236 */ { "", NULL, -1, NULL },
/* 237 */ { "", NULL, -1, NULL },
/* 238 */ { "", NULL, -1, NULL },
/* 239 */ { "", NULL, -1, NULL }, // 119

/* 240 */ { "GIF abstract colorful",	 GifAnim, 120, NULL }, // mapped to 160
/* 241 */ { "GIF Aki5PC6 Running",	 GifAnim, 121, NULL },
/* 242 */ { "GIF dancing lady",		 GifAnim, 122, NULL },
/* 243 */ { "GIF GirlSexyAnimateddance", GifAnim, 123, NULL },
/* 244 */ { "GIF green aurora",		 GifAnim, 124, NULL },
/* 245 */ { "GIF multi aurora",		 GifAnim, 125, NULL },
/* 246 */ { "GIF city aurora",		 GifAnim, 126, NULL },
/* 247 */ { "GIF DJ",			 GifAnim, 127, NULL },
/* 248 */ { "GIF color string spirals",	 GifAnim, 128, NULL },
/* 249 */ { "GIF orange shapes spinout", GifAnim, 129, NULL },
/* 250 */ { "GIF 3rdeye spin",		 GifAnim, 130, NULL },
/* 251 */ { "GIF 8 fish spirals",	 GifAnim, 131, NULL },
/* 252 */ { "GIF caniche",		 GifAnim, 132, NULL },
/* 253 */ { "GIF yingyang",		 GifAnim, 133, NULL },
/* 254 */ { "GIF color stars flash",	 GifAnim, 134, NULL },
/* 255 */ { "GIF dancing pink back",	 GifAnim, 135, NULL },
/* 256 */ { "GIF triangle merge",	 GifAnim, 136, NULL },
/* 257 */ { "GIF green hal9000",	 GifAnim, 137, NULL },
/* 258 */ { "GIF bird dance",		 GifAnim, 138, NULL },
/* 259 */ { "GIF concentric lights",	 GifAnim, 139, NULL },
/* 260 */ { "GIF spiral pentagon dance", GifAnim, 140, NULL },
/* 261 */ { "GIF double stargate",	 GifAnim, 141, NULL },
/* 262 */ { "GIF RGB smirout",		 GifAnim, 142, NULL },
/* 263 */ { "GIF fractal zoom",		 GifAnim, 143, NULL },
/* 264 */ { "GIF pacman",		 GifAnim, 144, NULL },
/* 265 */ { "GIF purple hair spiralout", GifAnim, 145, NULL },
/* 266 */ { "GIF flip triangles",	 GifAnim, 146, NULL },
/* 267 */ { "GIF rgb color plates",	 GifAnim, 147, NULL },
/* 268 */ { "GIF red round unfold",	 GifAnim, 148, NULL },
/* 269 */ { "GIF triangrect shapes out", GifAnim, 149, NULL },
/* 270 */ { "GIF light tunnel",		 GifAnim, 150, NULL },
/* 271 */ { "GIF heart rotate",		 GifAnim, 151, NULL },
/* 272 */ { "GIF colorflowers spiralout",GifAnim, 152, NULL },
/* 273 */ { "GIF green mobius rotate",	 GifAnim, 153, NULL },
/* 274 */ { "GIF cauliflower",		 GifAnim, 154, NULL },
/* 275 */ { "GIF triple 3D smiley",	 GifAnim, 155, NULL },
/* 276 */ { "GIF lightman running",	 GifAnim, 156, NULL },
/* 277 */ { "GIF green zoomout lasers",	 GifAnim, 157, NULL },
/* 278 */ { "GIF BW spiral out",	 GifAnim, 158, NULL },
/* 279 */ { "GIF starship shooting",	 GifAnim, 159, NULL },
/* 280 */ { "GIF blue smoke out",	 GifAnim, 160, NULL },
/* 281 */ { "GIF red jacket dancer",	 GifAnim, 161, NULL },
/* 282 */ { "GIF white grey smoke",	 GifAnim, 162, NULL },
/* 283 */ { "GIF flowers spinout",	 GifAnim, 163, NULL },
/* 284 */ { "GIF colors pulsing in out", GifAnim, 164, NULL },
/* 285 */ { "GIF red smoke spiral in",	 GifAnim, 165, NULL },
/* 286 */ { "GIF grey cubes tunnel",	 GifAnim, 166, NULL },
/* 287 */ { "GIF blue amber juggler",	 GifAnim, 167, NULL },
/* 288 */ { "GIF flying through pipes",	 GifAnim, 168, NULL },
/* 289 */ { "GIF blue robot heart",	 GifAnim, 169, NULL },
/* 290 */ { "GIF 3D Mobius loop",	 GifAnim, 170, NULL },
/* 291 */ { "GIF endless corridor",	 GifAnim, 171, NULL },
/* 292 */ { "GIF BW zoomout gears",	 GifAnim, 172, NULL },
/* 293 */ { "GIF RGB toroid",		 GifAnim, 173, NULL },
/* 294 */ { "GIF mushroom spots",	 GifAnim, 174, NULL },
/* 295 */ { "GIF you rock",		 GifAnim, 175, NULL },
/* 296 */ { "GIF morphing 3D shape",	 GifAnim, 176, NULL },
/* 297 */ { "GIF flower petals",	 GifAnim, 177, NULL },
/* 298 */ { "GIF eatme",		 GifAnim, 178, NULL },
/* 299 */ { "GIF sparkling spiralin",	 GifAnim, 179, NULL },
/* 300 */ { "GIF spingout RGB",		 GifAnim, 180, NULL },
/* 301 */ { "GIF green cube mobius",	 GifAnim, 181, NULL },
/* 302 */ { "GIF 3D green wheel ridge",	 GifAnim, 182, NULL },
/* 303 */ { "GIF colorspiral zoomout",	 GifAnim, 183, NULL },
/* 304 */ { "GIF spinning dancer",	 GifAnim, 184, NULL },
/* 305 */ { "GIF color projectors",	 GifAnim, 185, NULL },
/* 306 */ { "GIF sailormoon highdance",	 GifAnim, 186, NULL },
/* 307 */ { "GIF smiling dancing girl",	 GifAnim, 187, NULL },
/* 308 */ { "GIF coiling fern",		 GifAnim, 188, NULL },
/* 309 */ { "GIF yellow lighthouse",	 GifAnim, 189, NULL },
/* 310 */ { "GIF color pyramids",	 GifAnim, 190, NULL },
/* 311 */ { "GIF tunnel spark dancer",	 GifAnim, 191, NULL },
/* 312 */ { "GIF cube sphere pill",	 GifAnim, 192, NULL },
/* 313 */ { "GIF hyperspace",		 GifAnim, 193, NULL },
/* 314 */ { "GIF conifer zoom in",	 GifAnim, 194, NULL },
/* 315 */ { "GIF 3D hypercube RGB",	 GifAnim, 195, NULL },
/* 316 */ { "GIF grapefuit zoomin",	 GifAnim, 196, NULL },
/* 317 */ { "GIF pink flaming circle",	 GifAnim, 197, NULL },
/* 318 */ { "GIF center moving spiral",	 GifAnim, 198, NULL },
/* 319 */ { "GIF hypnotoad",		 GifAnim, 199, NULL },
/* 320 */ { "GIF pizza zoomin",		 GifAnim, 200, NULL },
/* 321 */ { "GIF RGB spiralin",		 GifAnim, 201, NULL },
/* 322 */ { "GIF bluebee zoomin",	 GifAnim, 202, NULL },
/* 323 */ { "GIF green neutron star",	 GifAnim, 203, NULL },
/* 324 */ { "GIF clock in",		 GifAnim, 204, NULL },
/* 325 */ { "GIF piano zoomin",		 GifAnim, 205, NULL },
/* 326 */ { "GIF puzzle spiralout",	 GifAnim, 206, NULL },
/* 327 */ { "GIF shiny snail shell",	 GifAnim, 207, NULL },
/* 328 */ { "GIF hypercube",		 GifAnim, 208, NULL },
/* 329 */ { "GIF color dots spiralin",	 GifAnim, 209, NULL },
/* 330 */ { "GIF 3D fractal roll",	 GifAnim, 210, NULL },
/* 331 */ { "GIF Kaleidoscope spiral",	 GifAnim, 211, NULL },
/* 332 */ { "GIF acid cat",		 GifAnim, 212, NULL },
/* 333 */ { "GIF purple geometrical",	 GifAnim, 213, NULL },
/* 334 */ { "GIF fly buildings",	 GifAnim, 214, NULL },
/* 335 */ { "GIF pulsing color rects",	 GifAnim, 215, NULL },
/* 336 */ { "GIF white geometric out",	 GifAnim, 216, NULL },
/* 337 */ { "GIF color marble",		 GifAnim, 217, NULL },
/* 338 */ { "GIF passionfruit zoomout",	 GifAnim, 218, NULL },
/* 339 */ { "GIF I am drugs",		 GifAnim, 219, NULL },
/* 340 */ { "GIF smileys spinout",	 GifAnim, 220, NULL },
/* 341 */ { "GIF flyin cavern",		 GifAnim, 221, NULL },
/* 342 */ { "GIF mario mushroom dance",	 GifAnim, 222, NULL },
/* 343 */ { "GIF baby pig fall",	 GifAnim, 223, NULL },
/* 344 */ { "GIF color shapes out",	 GifAnim, 224, NULL },
/* 345 */ { "GIF blue cubes flyin",	 GifAnim, 225, NULL },
/* 346 */ { "GIF triangles RGB out",	 GifAnim, 226, NULL },
/* 347 */ { "GIF rubiks cube",		 GifAnim, 227, NULL },
/* 348 */ { "GIF fractal2 zoom",	 GifAnim, 228, NULL },
/* 349 */ { "GIF color sticks",		 GifAnim, 229, NULL },
/* 350 */ { "GIF mushroom walk",	 GifAnim, 230, NULL },
/* 351 */ { "GIF BW bubbles",		 GifAnim, 231, NULL },
/* 352 */ { "GIF flying grass",		 GifAnim, 232, NULL },
/* 353 */ { "GIF blue gecko dance",	 GifAnim, 233, NULL },
/* 354 */ { "GIF dancing flames",	 GifAnim, 234, NULL },
/* 355 */ { "GIF skeleton",		 GifAnim, 235, NULL },
/* 356 */ { "GIF fly purple gates",	 GifAnim, 236, NULL },
/* 357 */ { "GIF bluelady smoke",	 GifAnim, 237, NULL },
/* 358 */ { "GIF sailor moon",		 GifAnim, 238, NULL },
/* 359 */ { "GIF inca spiralin",	 GifAnim, 239, NULL },
/* 360 */ { "GIF eye",			 GifAnim, 240, NULL },
/* 361 */ { "GIF blue shark dance",	 GifAnim, 241, NULL },
/* 362 */ { "GIF blue dancer",		 GifAnim, 242, NULL },
/* 363 */ { "GIF snoopdog dance",	 GifAnim, 243, NULL },
/* 364 */ { "GIF walking dead",		 GifAnim, 244, NULL },
/* 365 */ { "GIF street fighter",	 GifAnim, 245, NULL },
/* 366 */ { "Dreamstate 1",		 GifAnim, 246, NULL },
/* 367 */ { "Dreamstate 2",		 GifAnim, 247, NULL },
/* 368 */ { "Dreamstate 3",		 GifAnim, 248, NULL },
/* 369 */ { "Dreamstate 4",		 GifAnim, 249, NULL }, 
// mapped to 289, auto stored in DEMO_LAST_IDX
/* 370 */ { "", NULL, 250, NULL },
/* 371 */ { "", NULL, 251, NULL },
/* 372 */ { "", NULL, 252, NULL },
/* 373 */ { "", NULL, 253, NULL },
/* 374 */ { "", NULL, 254, NULL },
/* 375 */ { "", NULL, 255, NULL },
/* 376 */ { "", NULL, 256, NULL },
/* 377 */ { "", NULL, 257, NULL },
/* 378 */ { "", NULL, 258, NULL },
/* 379 */ { "", NULL, 259, NULL },
/* 380 */ { "", NULL, 260, NULL },
/* 381 */ { "", NULL, 261, NULL },
/* 382 */ { "", NULL, 262, NULL },
/* 383 */ { "", NULL, 263, NULL },
/* 384 */ { "", NULL, 264, NULL },
/* 385 */ { "", NULL, 265, NULL },
/* 386 */ { "", NULL, 266, NULL },
/* 387 */ { "", NULL, 267, NULL },
/* 388 */ { "", NULL, 268, NULL },
/* 389 */ { "", NULL, 269, NULL },
/* 390 */ { "", NULL, 270, NULL },
/* 391 */ { "", NULL, 271, NULL },
/* 392 */ { "", NULL, 272, NULL },
/* 393 */ { "", NULL, 273, NULL },
/* 394 */ { "", NULL, 274, NULL },
/* 395 */ { "", NULL, 275, NULL },
/* 396 */ { "", NULL, 276, NULL },
/* 397 */ { "", NULL, 277, NULL },
/* 398 */ { "", NULL, 278, NULL },
/* 399 */ { "", NULL, 279, NULL },
/* 400 */ { "", NULL, 280, NULL },
/* 401 */ { "", NULL, 281, NULL },
/* 402 */ { "", NULL, 282, NULL },
/* 403 */ { "", NULL, 283, NULL },
/* 404 */ { "", NULL, 284, NULL },
/* 405 */ { "", NULL, 285, NULL },
/* 406 */ { "", NULL, 286, NULL },
/* 407 */ { "", NULL, 287, NULL },
/* 408 */ { "", NULL, 288, NULL },
/* 409 */ { "", NULL, 289, NULL },
/* 410 */ { "", NULL, 290, NULL },
/* 411 */ { "", NULL, 291, NULL },
/* 412 */ { "", NULL, 292, NULL },
/* 413 */ { "", NULL, 293, NULL },
/* 414 */ { "", NULL, 294, NULL },
/* 415 */ { "", NULL, 295, NULL },
/* 416 */ { "", NULL, 296, NULL },
/* 417 */ { "", NULL, 297, NULL },
/* 418 */ { "", NULL, 298, NULL },
/* 419 */ { "", NULL, 299, NULL },
// Demo 340
/* 420 */ { "FIF Anna",		     GifAnim, 301, NULL },
/* 421 */ { "FIF Anna",              GifAnim, 301, NULL },
/* 422 */ { "FIF Anna2",             GifAnim, 302, NULL },
/* 423 */ { "GIF Monique",           GifAnim, 303, NULL },
/* 424 */ { "GIF Bryan Kearney",     GifAnim, 304, NULL },
/* 425 */ { "GIF Sony",              GifAnim, 305, NULL },
/* 426 */ { "GIF Blur",              GifAnim, 306, NULL },
/* 427 */ { "GIF Ulrich",	     GifAnim, 307, NULL },
/* 428 */ { "GIF 7lions",            GifAnim, 308, NULL },
/* 429 */ { "GIF ATB",               GifAnim, 309, NULL },
/* 430 */ { "GIF Above and Beyond",  GifAnim, 310, NULL }, // demo 350
/* 431 */ { "GIF Afik Cohen",        GifAnim, 311, NULL },
/* 432 */ { "GIF Alessandra Roncone",GifAnim, 312, NULL },
/* 433 */ { "GIF Alex MORPH",        GifAnim, 313, NULL },
/* 434 */ { "GIF Alpha9",            GifAnim, 314, NULL },
/* 435 */ { "GIF Alvin",             GifAnim, 315, NULL },
/* 436 */ { "GIF Aly & Fila Fadi",   GifAnim, 316, NULL },
/* 437 */ { "GIF Aly & Fila",        GifAnim, 317, NULL },
/* 438 */ { "GIF Aname",             GifAnim, 318, NULL },
/* 439 */ { "GIF Andrew Rayel",      GifAnim, 319, NULL },
/* 440 */ { "GIF Astrix",            GifAnim, 320, NULL }, // demo 360
/* 441 */ { "GIF AvB",               GifAnim, 321, NULL },
/* 442 */ { "GIF Blastoyz",          GifAnim, 322, NULL },
/* 443 */ { "GIF Cosmic Gate",       GifAnim, 323, NULL },
/* 444 */ { "GIF Craig Connelly",    GifAnim, 324, NULL },
/* 445 */ { "GIF Dave Dresden",      GifAnim, 325, NULL },
/* 446 */ { "GIF Emma Hewitt",       GifAnim, 326, NULL },
/* 447 */ { "GIF FactorB",           GifAnim, 327, NULL },
/* 448 */ { "GIF Ferry Corsten",     GifAnim, 328, NULL },
/* 449 */ { "GIF Ferry Tayle",       GifAnim, 329, NULL },
/* 450 */ { "GIF Gareth Emery",      GifAnim, 330, NULL }, // demo 370
/* 451 */ { "GIF Giuseppe Logo",     GifAnim, 331, NULL },
/* 452 */ { "GIF Giuseppe",          GifAnim, 332, NULL },
/* 453 */ { "GIF Haliene",           GifAnim, 333, NULL },
/* 454 */ { "GIF Ilan Bluestone",    GifAnim, 334, NULL },
/* 455 */ { "GIF JOC",               GifAnim, 335, NULL },
/* 456 */ { "GIF John 00 Flemming",  GifAnim, 336, NULL },
/* 457 */ { "GIF Khromata",          GifAnim, 337, NULL },
/* 458 */ { "GIF Lange",             GifAnim, 338, NULL },
/* 459 */ { "GIF Liquid Soul",       GifAnim, 339, NULL },
/* 460 */ { "GIF Markus Schulz",     GifAnim, 340, NULL }, // demo 380
/* 461 */ { "GIF Marlo",             GifAnim, 341, NULL },
/* 462 */ { "GIF Miyuki",            GifAnim, 342, NULL },
/* 463 */ { "GIF Nifra",             GifAnim, 343, NULL },
/* 464 */ { "GIF Paul Oakenfold",    GifAnim, 344, NULL },
/* 465 */ { "GIF Paul Van Dyk",      GifAnim, 345, NULL },
/* 466 */ { "GIF RDR Logo",          GifAnim, 346, NULL },
/* 467 */ { "GIF Reality Test",      GifAnim, 347, NULL },
/* 468 */ { "GIF Richard Durand",    GifAnim, 348, NULL },
/* 469 */ { "GIF Rinaly",            GifAnim, 349, NULL },
/* 470 */ { "GIF RubenDeRonde",      GifAnim, 350, NULL }, // demo 390
/* 471 */ { "GIF Solarstone",        GifAnim, 351, NULL },
/* 472 */ { "GIF Somna",             GifAnim, 352, NULL },
/* 473 */ { "GIF Susana",            GifAnim, 353, NULL },
/* 474 */ { "GIF Talla-2XLC",        GifAnim, 354, NULL },
/* 475 */ { "GIF Thrillseekers",     GifAnim, 355, NULL },
/* 476 */ { "GIF Triode",            GifAnim, 356, NULL },
/* 477 */ { "GIF Vini Vici",         GifAnim, 357, NULL },
/* 478 */ { "GIF WhiteNo1se",        GifAnim, 358, NULL }, // demo 398
// Last, do not move/resort
/* 479 */ { "GIF Armin Logo",	     GifAnim, 359, NULL }, // mapped to 399, on for earlier demos to work
    // mapping is the index number in demo_map.txt to account for
    // demos for multiple platforms that share the same slot numbers
    // If you add elements past 479, please update DEMO_ARRAY_SIZE
};

void matrix_change(int16_t demo, bool directmap=false, int16_t loop=-1) {
    // Reset passthrough from previous demo
    matrix->setPassThruColor();
    // Clear screen when changing demos.
    matrix->clear();
    // this ensures the next demo returns the number of times it should loop
    MATRIX_LOOP = loop;
    MATRIX_RESET_DEMO = true;

    Serial.print("got matrix_change code ");
    Serial.print(demo);
    // demo is the requested demo index in the demo_mapping array or prev/next
    // demo_mapping is filled in by read_config_index as per demo_map.txt
    // demo goes into MATRIX_STATE and is the index to look up in demo_mapping array
    // result is MATRIX_DEMO and is the actual demo# in demo_list array
    // Caller can also decide to bypass demo_mapping by setting directmap
    if (directmap) {
	// bypass MATRIX_STATE (which will now be incorrect) and point directly
	// to the desired demo without going though demo_mapping array.
	MATRIX_DEMO = demo;
	MATRIX_STATE = demo_mapping[MATRIX_DEMO].reverse;
	if (MATRIX_LOOP==-1) MATRIX_LOOP = 100;
    } else {
	if (demo != DEMO_PREV && demo != DEMO_NEXT) MATRIX_STATE = demo;

	#ifdef NEOPIXEL_PIN
	// special one key press where demos are shown forever and next goes back to the normal loop
	if ((demo >= 0 && demo < DEMO_NEXT) && MATRIX_LOOP==-1) MATRIX_LOOP = 9999;
	#endif
	Serial.print(", switching to index ");
	if (demo >= DEMO_TEXT_FIRST && demo <= DEMO_TEXT_LAST) {
	    Serial.print(MATRIX_STATE);
	    MATRIX_DEMO = MATRIX_STATE;
	} else {
	    do {
		// Otherwise prev/next go to the next demo
		if (demo==DEMO_PREV) if (MATRIX_STATE-- == 0) MATRIX_STATE = DEMO_LAST_IDX;
		if (demo==DEMO_NEXT) if (++MATRIX_STATE > DEMO_LAST_IDX) MATRIX_STATE = 0;
		// skip text demos on prev/next
		if (MATRIX_STATE >= DEMO_TEXT_FIRST && MATRIX_STATE <= DEMO_TEXT_LAST) { 
		    if (demo==DEMO_PREV) MATRIX_STATE--; else MATRIX_STATE++; 
		    continue;
		}

		MATRIX_STATE = (MATRIX_STATE % (DEMO_LAST_IDX+1));
		Serial.print(MATRIX_STATE);
		MATRIX_DEMO = demo_mapping[MATRIX_STATE].mapping;
		if (SHOW_BEST_DEMOS) {
		    Serial.print(" (bestof mode) ");
		    if (demo_mapping[MATRIX_STATE].enabled[PANELCONFNUM] & 2) break;
		} else {
		    Serial.print(" (full mode) ");
		    if (demo_mapping[MATRIX_STATE].enabled[PANELCONFNUM] & 1) break;
		}
		// If we're here for a demo # that doesn't exist, looping will not change
		// the demo number, so force a change.
		if (demo != DEMO_PREV && demo != DEMO_NEXT) MATRIX_STATE++;
	    } while (1);
	}
	Serial.print(", mapped to matrix demo ");
	Serial.print(MATRIX_DEMO);
	Serial.print("  / demoidx map: ");
	Serial.print(demoidx(MATRIX_DEMO));
	Serial.print(")");
    }
    #ifdef ARDUINOONPC
	// if we are connected to a remote device, let it change patterns for us
	if (ttyfd >= 0 && esp32_connected) MATRIX_LOOP = 999;
    #endif
    Serial.print(" (");
    Serial.print(demo_list[demoidx(MATRIX_DEMO)].name);
    Serial.print(") loop ");
    Serial.println(MATRIX_LOOP);
    #ifndef ARDUINOONPC
	Serial.flush();
	Serial.print("|D:");
	char buf[4];
	sprintf(buf, "%3d", MATRIX_DEMO);
	Serial.println(buf);
	Serial.flush();
    #endif

    // We changed the current demo, update the selection in the big dropdown
    rebuild_main_page();
}



void Matrix_Handler() {
    static uint32_t count = 0;
    static uint32_t count_last = 0;
    static uint32_t time_last = 0;
    uint32_t time_now = millis();
    uint8_t ret;

    count++;
    if (time_now - time_last > 1000) {
	static uint16_t cnt = 0;
	// Note that the matrix_handler FPS is what fills the framebuffer.
	// How quickly the framebuffer memory is pushed to the display,
	// depends on the display, used and its capabilities. Ideally
	// it'll be more than 50fps (especially for any display like
	// RGBPanels where pixels get turned off between refreshes)
	LAST_FPS = (count - count_last) * 1000 / (time_now - time_last);
	// Display on serial every 10 seconds
	if (cnt++ % 10 == 0) {
#ifdef ESP32
	    Serial.print("avg freq of matrix handler compared to theorical 50 fps: ");
	    Serial.println(LAST_FPS);
#endif
	}
	time_last = time_now;
	count_last = count;
    }


    Demo_Entry demo_entry = demo_list[demoidx(MATRIX_DEMO)];
    if (! demo_entry.func) {
	Serial.print(">>> ERROR: No demo for ");
	Serial.println(MATRIX_DEMO);
	matrix_change(DEMO_NEXT);
	goto exit;
    }

    ret = demo_entry.func(demo_entry.arg);
    if (MATRIX_LOOP == -1) MATRIX_LOOP = ret;
    if (ret) goto exit;

    MATRIX_RESET_DEMO = true;
    Serial.print("Done with demo ");
    Serial.print(MATRIX_DEMO);
    Serial.print(" loop ");
    Serial.println(MATRIX_LOOP);
    if (--MATRIX_LOOP > 0) return;
    matrix_change(DEMO_NEXT);
    return;

    exit:
    return;
}

// ---------------------------------------------------------------------------
// Strip Code
// ---------------------------------------------------------------------------

#ifdef NEOPIXEL_PIN
void leds_show() {
#ifndef FASTLED_NEOMATRIX
    FastLED.show();
#else
    #ifdef ESP8266
    FastLED[0].showLeds(led_brightness);
    #else
    FastLED.show();
    // testing independent brightness works if I2S is turned off
    // however it causes bad flashing, so we can't use this.
    //FastLED[0].showLeds(255);
    #endif
#endif
}
void leds_setcolor(uint16_t i, uint32_t c) {
    leds[i] = c;
}
#endif // NEOPIXEL_PIN

void change_brightness(int8_t change, bool absolute=false) {
    static uint8_t brightness = DFL_MATRIX_BRIGHTNESS_LEVEL;
    static int32_t last_brightness_change = 0 ;
    static bool firsttime = true ;

    if (firsttime) 
    {
	firsttime=false;
    } else if (!absolute && millis() - last_brightness_change < 300) {
	Serial.print("Too soon... Ignoring brightness change from ");
	Serial.println(brightness);
	Serial.println();
	return;
    }
    last_brightness_change = millis();
    if (absolute) {
	brightness = change;
    } else {
	brightness = constrain(brightness + change, 0, 8);
    }
    led_brightness = min((1 << (brightness+1)) - 1, 255);
    if (! brightness) led_brightness = 0;
#ifndef FASTLED_NEOMATRIX
    FastLED.setBrightness(led_brightness);
#endif

    // rgbpanels are dim, bump up brightness
    uint8_t rgbpanel_brightness = 31+min( (1 << (brightness+2)), 224);
    // neopixels are bright, we tone brightness down
    matrix_brightness = (1 << (brightness-1)) - 1;
    if (! brightness) { matrix_brightness = 0; rgbpanel_brightness = 0; }

    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" led value: ");
    Serial.print(led_brightness);
    Serial.print(" / RGBPanel value: ");
    Serial.print(rgbpanel_brightness);
    Serial.print(" / neomatrix value: ");
    Serial.println(matrix_brightness);
    Serial.print("|B:");
    Serial.println(brightness);
#ifndef ARDUINOONPC
    Serial.flush();
#endif
#ifdef SMARTMATRIX
    matrixLayer.setBrightness(rgbpanel_brightness);
#elif RPIRGBPANEL
    matrix->setBrightness(rgbpanel_brightness);
#else
    // This is needed if using the ESP32 driver but won't work
    // if using 2 independent fastled strips (1D + 2D matrix)
    matrix->setBrightness(matrix_brightness);
#endif
    // TODO: brightness on rPi (ArduinoOnPC)

    // We changed the current brightness, update the selection in the big dropdown
    rebuild_main_page();
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

    // We changed the current speed, update the selection in the big dropdown
    rebuild_main_page();
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
uint8_t check_startup_IR_serial() {
    char readchar;

    if (Serial.available()) readchar = Serial.read(); else readchar = 0;
    if (readchar == 'p') return 1;
    if (readchar == 'w') return 2;

#ifdef IR_RECV_PIN
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

void changeBestOf(bool bestof) {
    SHOW_BEST_DEMOS = bestof;
    rebuild_advanced_page();
}

void changePanelConf(uint8_t conf, bool ) {
    Serial.print("ChangePanel to conf ");
    Serial.println(panelconfnames[conf]);
    PANELCONFNUM = conf;
    rebuild_main_page(true);
    rebuild_advanced_page();
}

#ifdef ARDUINOONPC
void handle_rpi_serial_cmd() {
    char numbuf[4];

    if (! strncmp(ttyusbbuf, "|St", 3)) {
	Serial.println("Got ESP start, enable ESP serial input");
	delay(10);
	send_serial("|");
	delay(10);
	send_serial("d");
	// When we receive St, ESP32 will auto-run showip()
    }
    if (! strncmp(ttyusbbuf, "|D:", 3)) {
	int num;
	strncpy(numbuf, ttyusbbuf+3, 3);
	numbuf[3] = 0;
	num = atoi(numbuf);
	printf("Got direct mapped demo %d\n", num);
	matrix_change(num, true);
    }
    if (! strncmp(ttyusbbuf, "|B:", 3)) {
	int num;
	strncpy(numbuf, ttyusbbuf+3, 1);
	numbuf[1] = 0;
	num = atoi(numbuf);
	printf("Got brightness %d\n", num);
	change_brightness(num, true);
    }
    if (! strncmp(ttyusbbuf, "|I:", 3)) {
	send_serial(get_rPI_IP());
	DISPLAYTEXT = String("ESP32:\n") + String(ttyusbbuf+3) + "\nlocal:\n" + String(rPI_IP);
	Serial.print("Got IP from ");
	Serial.print(DISPLAYTEXT);
	Serial.print("|T:");
	Serial.println(DISPLAYTEXT);
	matrix_change(DEMO_TEXT_INPUT, false, 20);
    }
    // Allow ESP32 to send string to rPi
    if (! strncmp(ttyusbbuf, "|T:", 3)) {
	DISPLAYTEXT = String(ttyusbbuf+3);
	Serial.print("Got string to display: ");
	Serial.print(DISPLAYTEXT);
    }
    if (! strncmp(ttyusbbuf, "|RS", 3)) {
	Serial.println("Got restart");
	exit(0);
    }
    if (! strncmp(ttyusbbuf, "|RB", 3)) {
	Serial.println("Got reboot");
	system("/root/rebootme");
    }
    if (! strncmp(ttyusbbuf, "|zz", 3)) {
	Serial.println("Switching to rotating text");
	ROTATE_TEXT = true;
    }
    if (! strncmp(ttyusbbuf, "|ZZ", 3)) {
	Serial.println("Switching to stable text for pictures");
	ROTATE_TEXT = false;
    }
    if (! strncmp(ttyusbbuf, "|yy", 3)) {
	Serial.println("Switching to scrolling images");
	SCROLL_IMAGE = true;
    }
    if (! strncmp(ttyusbbuf, "|YY", 3)) {
	Serial.println("Switching to stable images for pictures");
	SCROLL_IMAGE = false;
    }
}
#endif

void IR_Serial_Handler() {
    uint32_t new_pattern = 0;
    char readchar = 0;
    static bool remotesend = false;
    static bool startcmd = false;

    // on rPi, we accept 'serial' (really tty) input right away
    #ifdef ARDUINOONPC
    static bool rpireadserialcmd = false;
    static char *ptr = ttyusbbuf;
    startcmd = true;
    #endif

    // Serial on rPi actually reads from STDIN (ssh)
    // rPi gets remote serial commands from /dev/ttyUSB at end of main loop
    if (Serial.available()) { 
	readchar = Serial.read();
	if (readchar) {

	    // Have ESP32 ignore serial input until it gets the remote start command
	    if (readchar == '|' && !startcmd) {
		Serial.println("Got remote serial start, now accepting serial commands");
		startcmd = true;
	    }
	    if (! startcmd) { Serial.println("Ignoring input without startcmd |St"); goto endserial; }

	    #ifdef ARDUINOONPC
		// Allow feeding rPi commands that would normally be received over ttyUSB*
		if (readchar == '|') rpireadserialcmd = true;

		if (rpireadserialcmd) {
		    // Note that there is a race condition between his and ttyUSB read, they both write in the
		    // same buffer. The good news is that arduino emulation sends all characters on a line all at once
		    // so this reduces the race. Also, this is really meant to be used for debugging when most of the
		    // time the ESP32 isn't even plugged in, so there will be no race.
		    if (readchar == '\n' ) {
			ptr[0] = readchar;
			ptr[1] = 0;
			// reset pointer for next time
			ptr = ttyusbbuf;
			printf("RPI received serial cmd: %s", ttyusbbuf);
			rpireadserialcmd = false;
			handle_rpi_serial_cmd();
			goto endserial;
		    }
		    if (readchar == '\\') readchar = '\n';
		    ptr[0] = readchar;
		    ptr[1] = 0;
		    ptr++;
		    goto endserial;
		}
	    #endif

	    // Feeding a number either tells us what pattern to change to or feeds a remote IP
	    while ((readchar >= '0') && (readchar <= '9')) {
		new_pattern = 10 * new_pattern + (readchar - '0');
		readchar = 0;
		#ifdef WIFI
		if (new_pattern > DEMO_LAST_IDX) {
		    Remote_IP = IPAddress(
			(new_pattern & 0xFF000000) / 0x1000000,
			(new_pattern & 0x00FF0000) / 0x10000,
			(new_pattern & 0x0000FF00) / 0x100,
			(new_pattern & 0x000000FF) / 1
		    );
		}
		#endif
		if (Serial.available()) readchar = Serial.read();
	    }

	    if (new_pattern > DEMO_LAST_IDX) new_pattern = 0;
	    if (new_pattern) {
		Serial.print("Got new");
		Serial.print(remotesend?" REMOTE":"");
		Serial.print(" pattern via serial ");
		Serial.println(new_pattern);
		#ifdef ARDUINOONPC
		if (remotesend) {
		    send_serial(String(new_pattern).c_str());
		    remotesend = false;
		} else
		#endif
		    matrix_change(new_pattern, true);
	    } else {
		if (readchar != '\n') {
		    Serial.print("Got serial char '");
		    Serial.print(readchar);
		    Serial.println("'");
		}
	    }

	    #ifdef ARDUINOONPC
		// Remotesend shouldn't be needed for commands as all commands have an rPI resent
		// equivalent
		if (remotesend) { 
		    char str[]= { readchar, 0 };
		    remotesend = false; 
		    Serial.print("ESP => forward "); 
		    Serial.print(readchar); 
		    send_serial((const char *) str);
		    goto endserial;
		} 
	    #endif
	    
	    if (readchar == 'n') { Serial.println("Serial => next");	matrix_change(DEMO_NEXT);}
	    else if (readchar == 'p') { Serial.println("Serial => previous");   matrix_change(DEMO_PREV);}
	    else if (readchar == 'b') { Serial.println("Serial => Bestof");	changeBestOf(true); }
	    else if (readchar == 'a') { Serial.println("Serial => All Demos");  changeBestOf(false);}
	    else if (readchar == 'z') { Serial.println("Serial => Rotating Text"); ROTATE_TEXT = true; }
	    else if (readchar == 'Z') { Serial.println("Serial => Stable Text");   ROTATE_TEXT = false;}
	    else if (readchar == 't') { Serial.println("Serial => text thankyou"); matrix_change(DEMO_TEXT_THANKYOU);}
	    else if (readchar == 'f') { SHOW_LAST_FPS = !SHOW_LAST_FPS; }
	    else if (readchar == '=') { Serial.println("Serial => keep demo?"); MATRIX_LOOP = MATRIX_LOOP > 1000 ? 3 : 9999; }
	    else if (readchar == '-') { Serial.println("Serial => dim"   );	change_brightness(-1);}
	    else if (readchar == '+') { Serial.println("Serial => bright");	change_brightness(+1);}
	    else if (readchar == 'c') { changePanelConf(3, true); }
	    else if (readchar == 'd') { changePanelConf(4, true); }
	#ifdef ARDUINOONPC
	    else if (readchar == 'N') { Serial.println("ESP => next");		send_serial("n");}
	    else if (readchar == 'P') { Serial.println("ESP => previous");	send_serial("p");}
	    else if (readchar == 'F') { Serial.println("ESP => togglefps");	send_serial("f"); SHOW_LAST_FPS = !SHOW_LAST_FPS;}
	    else if (readchar == '<') { Serial.println("ESP => dim"   );        send_serial("-"); change_brightness(-1);}
	    else if (readchar == '>') { Serial.println("ESP => bright");        send_serial("+"); change_brightness(+1);}
	    else if (readchar == 'B') { Serial.println("ESP => Bestof");	send_serial("B"); changeBestOf(true); }
	    else if (readchar == 'A') { Serial.println("ESP => All Demos");	send_serial("b"); changeBestOf(false);}
	    else if (readchar == '_') { Serial.println("ESP => Keep Demo?");    send_serial("=");}
	    else if (readchar == 'C') { Serial.println("ESP => ChangePanel3");  send_serial("c");}
	    else if (readchar == 'D') { Serial.println("ESP => ChangePanel4");  send_serial("d");}
	    else if (readchar == 'R') { Serial.println("ESP => send next number/char");  remotesend = true;}
	    // Don't use a character that can be easily received by ESP32
	    else if (readchar == '~') { Serial.println("exit");			exit(0);}
	    else if (readchar == 'r') { Serial.println("ESP => Reboot");	send_serial("r"); }
	#else
	    else if (readchar == 'r') { Serial.println("Reboot"); resetFunc(); }
	    else if (readchar == 'i') { Serial.println("Serial => showip");     showip();}
	#endif
	}
    }

    endserial:

    // allow working on hardware that doens't have IR. In that case, we use serial only and avoid
    // compiling the IR code that won't build.
#ifdef IR_RECV_PIN
    uint32_t result;

#ifndef ESP32RMTIR
    decode_results IR_result;
    if (irrecv.decode(&IR_result)) 
#else
    char* rcvGroup = NULL;
    if (irrecv.available())
#endif
				    {
#ifndef ESP32RMTIR
	irrecv.resume(); // Receive the next value
	result = IR_result.value;
#else
	result = irrecv.read(rcvGroup);
#endif
	switch (result) {
	case IR_RGBZONE_BRIGHT:
	case IR_RGBZONE_BRIGHT2:
	    if (is_change(true)) { Serial.println("Got IR: Bright, Only show best demos"); changeBestOf(true); return; }
	    change_brightness(+1);
	    Serial.println("Got IR: Bright");
	    return;

	case IR_RGBZONE_DIM:
	case IR_RGBZONE_DIM2:
	    if (is_change(true)) {
		Serial.println("Got IR: Dim, show all demos again and Hang on this demo");
		MATRIX_LOOP = 9999;
		changeBestOf(false);
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
	    Serial.println(MATRIX_STATE);
	    return;

	case IR_RGBZONE_POWER2:
	    Serial.println("Got IR: Power2");
	case IR_RGBZONE_POWER:
	    if (is_change()) { matrix_change(DEMO_PREV); return; }
	    Serial.println("Got IR: Power, show all demos again and Hang on this demo");
	    MATRIX_LOOP = 9999;
	    changeBestOf(false);
	    return;

	case IR_RGBZONE_RED:
	    Serial.println("Got IR: Red (0)");
	    if (is_change()) { matrix_change(0); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xFF0000;
	    return;

	case IR_RGBZONE_GREEN:
	    Serial.println("Got IR: Green (3)");
	    if (is_change()) { matrix_change(3); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x00FF00;
	    return;

	case IR_RGBZONE_BLUE:
	    Serial.println("Got IR: Blue (5)");
	    if (is_change()) { matrix_change(5); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x0000FF;
	    return;

	case IR_RGBZONE_WHITE:
	    Serial.println("Got IR: White (7)");
	    if (is_change()) { matrix_change(7); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xFFFFFF;
	    showip();
	    return;


	case IR_RGBZONE_RED2:
	    Serial.println("Got IR: Red2 (9)");
	    if (is_change()) { matrix_change(9); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xCE6800;
	    return;

	case IR_RGBZONE_GREEN2:
	    Serial.println("Got IR: Green2 (11)");
	    if (is_change()) { matrix_change(11); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x00BB00;
	    return;

	case IR_RGBZONE_BLUE2:
	    Serial.println("Got IR: Blue2 (13)");
	    if (is_change()) { matrix_change(13); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x0000BB;
	    return;

	case IR_RGBZONE_PINK:
	    Serial.println("Got IR: Pink (15)");
	    if (is_change()) { matrix_change(15); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xFF50FE;
	    return;



	case IR_RGBZONE_ORANGE:
	    Serial.println("Got IR: Orange (17)");
	    if (is_change()) { matrix_change(17); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xFF8100;
	    return;

	case IR_RGBZONE_BLUE3:
	    Serial.println("Got IR: Green2 (19)");
	    if (is_change()) { matrix_change(19); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x00BB00;
	    return;

	case IR_RGBZONE_PURPLED:
	    Serial.println("Got IR: DarkPurple (21)");
	    if (is_change()) { matrix_change(21); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x270041;
	    return;

	case IR_RGBZONE_PINK2:
	    Serial.println("Got IR: Pink2 (23)");
	    if (is_change()) { matrix_change(23); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xFFB9FF;
	    Serial.println("Got IR: Pink2");
	    return;



	case IR_RGBZONE_ORANGE2:
	    Serial.println("Got IR: Orange2 (25)");
	    if (is_change()) { matrix_change(25); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xFFCA49;
	    return;

	case IR_RGBZONE_GREEN3:
	    Serial.println("Got IR: Green2 (27)");
	    if (is_change()) { matrix_change(27); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x006A00;
	    return;

	case IR_RGBZONE_PURPLE:
	    Serial.println("Got IR: DarkPurple2 (29)");
	    if (is_change()) { matrix_change(29); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x2B0064;
	    return;

	case IR_RGBZONE_BLUEL:
	    Serial.println("Got IR: BlueLight (31)");
	    if (is_change()) { matrix_change(31); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x50A7FF;
	    return;



	case IR_RGBZONE_YELLOW:
	    Serial.println("Got IR: Yellow (33)");
	    if (is_change()) { matrix_change(33); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0xF0FF00;
	    return;

	case IR_RGBZONE_GREEN4:
	    Serial.println("Got IR: Green2 (35)");
	    if (is_change()) { matrix_change(35); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x00BB00;
	    return;

	case IR_RGBZONE_PURPLE2:
	    Serial.println("Got IR: Purple2 (37)");
	    if (is_change()) { matrix_change(37); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x660265;
	    return;

	case IR_RGBZONE_BLUEL2:
	    Serial.println("Got IR: BlueLight2 (39)");
	    if (is_change()) { matrix_change(39); return; }
	    if (!colorDemo) STRIPDEMO = f_colorWipe;
	    demo_color = 0x408BD8;
	    return;


	case IR_RGBZONE_RU:
	    matrix_change(41);
	    Serial.print("Got IR: Red UP switching to matrix state 41");
	    Serial.println(MATRIX_STATE);
	    return;

	case IR_RGBZONE_GU:
	    matrix_change(42);
	    Serial.print("Got IR: Green UP switching to matrix state 42");
	    Serial.println(MATRIX_STATE);
	    return;

	case IR_RGBZONE_BU:
	    matrix_change(43);
	    Serial.print("Got IR: Blue UP switching to matrix state 43");
	    Serial.println(MATRIX_STATE);
	    return;


	case IR_RGBZONE_RD:
	    Serial.print("Got IR: Red DOWN switching to matrix state 44");
	    matrix_change(44);
	    Serial.println(MATRIX_STATE);
	    return;

	case IR_RGBZONE_GD:
	    Serial.print("Got IR: Green DOWN switching to matrix state 45");
	    matrix_change(45);
	    Serial.println(MATRIX_STATE);
	    return;

	case IR_RGBZONE_BD:
	    Serial.print("Got IR: Blue DOWN switching to matrix state 47");
	    matrix_change(47);
	    Serial.println(MATRIX_STATE);
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
	    STRIPDEMO = f_colorWipe;
	    Serial.println("Got IR: DIY1 colorWipe");
	    return;

	case IR_RGBZONE_DIY2:
	    Serial.println("Got IR: DIY2 (51)");
	    if (is_change()) { matrix_change(51); return; }
	    // this uses the last color set
	    STRIPDEMO = f_theaterChase;
	    Serial.println("Got IR: DIY2/theaterChase");
	    return;

	case IR_RGBZONE_DIY3:
	    Serial.println("Got IR: DIY3 (53)");
	    if (is_change()) { matrix_change(53); return; }
	    STRIPDEMO = f_juggle;
	    Serial.println("Got IR: DIY3/Juggle");
	    return;

	case IR_RGBZONE_AUTO:
	    Serial.println("Got IR: AUTO/bpm (55)");
	    if (is_change()) { matrix_change(55); return; }
	    matrix_change(DEMO_TEXT_THANKYOU);
	    STRIPDEMO = f_bpm;
	    return;


	// From here, we jump numbers more quickly to cover ranges of demos 32-44
	case IR_RGBZONE_DIY4:
	    Serial.println("Got IR: DIY4 (57)");
	    if (is_change()) { matrix_change(57); return; }
	    STRIPDEMO = f_rainbowCycle;
	    Serial.println("Got IR: DIY4/rainbowCycle");
	    return;

	case IR_RGBZONE_DIY5:
	    Serial.println("Got IR: DIY5 (59)");
	    if (is_change()) { matrix_change(59); return; }
	    STRIPDEMO = f_theaterChaseRainbow;
	    Serial.println("Got IR: DIY5/theaterChaseRainbow");
	    return;

	case IR_RGBZONE_DIY6:
	    Serial.println("Got IR: DIY6 (100)");
	    if (is_change()) { matrix_change(100); return; }
	    STRIPDEMO = f_doubleConvergeRev;
	    Serial.println("Got IR: DIY6/DoubleConvergeRev");
	    return;

	case IR_RGBZONE_FLASH:
	    Serial.println("Got IR: FLASH/RPI restart");
	    if (is_change()) { Serial.println("Restart PI pressed"); Serial.println("|RS"); return; }
	    STRIPDEMO = f_flash;
	    return;

	// And from here, jump across a few GIF anims (45-53)
	case IR_RGBZONE_JUMP3:
	    Serial.println("Got IR: JUMP3/Cylon (130)");
	    if (is_change()) { matrix_change(130); return; }
	    STRIPDEMO = f_cylon;
	    return;

	case IR_RGBZONE_JUMP7:
	    Serial.println("Got IR: JUMP7/CylonWithTrail (160)");
	    if (is_change()) { matrix_change(160); return; }
	    STRIPDEMO = f_cylonTrail;
	    return;

	case IR_RGBZONE_FADE3:
	    Serial.println("Got IR: FADE3/DoubleConverge (69)");
	    if (is_change()) { matrix_change(69); return; }
	    STRIPDEMO = f_doubleConverge;
	    return;

	case IR_RGBZONE_FADE7:
	    Serial.println("Got IR: FADE7/DoubleConvergeTrail (108)");
	    // TFSF display
	    if (is_change()) { matrix_change(108); return; }
	    STRIPDEMO = f_doubleConvergeTrail;
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
#else
    return;
#endif // IR_RECV_PIN
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

	for (uint16_t i=0; i < STRIP_NUM_LEDS-2; i=i+3) {
	    leds_setcolor(i+q, c);    //turn every third pixel on
	}
	leds_show();

	#if 0
	for (uint16_t i=0; i < STRIP_NUM_LEDS-2; i=i+3) {
	    leds_setcolor(i+q, 0);	//turn every third pixel off
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

	for (uint16_t i=0; i < STRIP_NUM_LEDS-2; i=i+3) {
	    leds_setcolor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
	}
	leds_show();

	#if 0
	for (uint16_t i=0; i < STRIP_NUM_LEDS-2; i=i+3) {
	    leds_setcolor(i+q, 0);	//turn every third pixel off
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

	switch (STRIPDEMO) {
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

void process_config(bool show_summary=false) {
    DEMO_CNT = 0;
    BEST_CNT = 0;
    DEMO_LAST_IDX = 0;
    
    for (uint16_t index = 0; index <= CFG_LAST_INDEX; index++) {
	if (demo_mapping[index].enabled[PANELCONFNUM] & 2) BEST_CNT++;
	// keep track of the highest demo index for modulo in matrix_change
	if (demo_mapping[index].enabled[PANELCONFNUM]) {
	    if (! MATRIX_STATE) MATRIX_STATE = index; // find first playable demo
	    DEMO_CNT++;
	    DEMO_LAST_IDX = index;
	    //Serial.print(index); Serial.print(" "); Serial.print(DEMO_CNT); Serial.print(" "); Serial.println(DEMO_LAST_IDX);
	}
    }
    if (show_summary) {
	Serial.println("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv");
	Serial.print("Number of Demos enabled: ");
	Serial.print(DEMO_CNT);
	Serial.print(". Number of Best Demos: ");
	Serial.println(BEST_CNT);
	Serial.print("Number of lines in CFG File: ");
	Serial.print(CFG_LAST_INDEX + 1);
	Serial.print(". Last Playable Demo idx: ");
	Serial.print(DEMO_LAST_IDX);
	Serial.print(". Next Demo to play: ");
	Serial.println(MATRIX_STATE);
	Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
    }
}


#ifdef WIFI
#define HTML_DEMOLIST_CHOICE 90
#define HTML_BESTOF	    100
#define HTML_BRIGHT	    101
#define HTML_SPEED	    102
#define HTML_ROTATE_TEXT    103
#define HTML_SCROLL_IMAGE   104
#define HTML_BUTPREV	    110
#define HTML_BUTNEXT	    111
#define HTML_SHOWIP	    120
#define HTML_RESTARTPI	    121
#define HTML_REBOOTPI	    122
#define HTML_STRIPCHOICE    130
#define HTML_DEMOCHOICE	    131

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

    case HTML_SHOWIP:
	if (!value) break;
	Serial.println("ShowIP pressed");
	showip();
	break;

    case HTML_RESTARTPI:
	if (!value) break;
	Serial.println("Restart PI pressed");
	Serial.println("|RS");
	break;

    case HTML_REBOOTPI:
	if (!value) break;
	Serial.println("Reboot PI pressed");
	Serial.println("|RB");
	break;

    case HTML_BESTOF:
	Serial.print("Bestof on/off: ");
	Serial.println(value);
	changeBestOf(value);
	break;

    case HTML_ROTATE_TEXT:
	Serial.print("Rotate Text on/off: ");
	Serial.println(value);
	if (value) Serial.println("|zz"); else Serial.println("|ZZ");
	ROTATE_TEXT = value;
	break;

    case HTML_SCROLL_IMAGE:
	Serial.print("Scroll Image on/off: ");
	Serial.println(value);
	if (value) Serial.println("|yy"); else Serial.println("|YY");
	SCROLL_IMAGE = value;
	break;

    case HTML_BRIGHT:
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

    case HTML_DEMOLIST_CHOICE:
	Serial.print("Got HTML Demolist Choice ");
	Serial.print(value);
	if (value<0 || value >= CONFIGURATIONS) {
	    Serial.println(" => invalid, skipping");
	    break;
	}
	Serial.print(": ");
	Serial.println(panelconfnames[value]);
	changePanelConf(value, true);
	break;

    case HTML_STRIPCHOICE:
	if (!value) break;
	Serial.print("Got Strip Demo Choice ");
	Serial.println(value);
	STRIPDEMO = (StripDemo) value;
	break;

    case HTML_DEMOCHOICE:
	if (!value) break;
	Serial.print("Got HTML Demo Choice ");
	Serial.println(value);
	matrix_change(value, true);
	break;

    }
}

// opens a file and output one line at a time until empty
bool PutFileLine(OmXmlWriter &w, const char *path) {
    static File file;
    char c;

    if (strcmp(path, "/favicon.ico") == 0) return 0;

    if (! file) {
	Serial.print("Opening file for line by line read: ");
	Serial.println(path);
	if (! (file = FSO.open(path)
	#ifdef FSOSPIFFS
					, "r"
	#endif
						) ) {
	    Serial.println("Error opening file");
	    return 0;
	}
    }

    if (!file.available()) {
	Serial.print("Done reading file ");
	Serial.println(path);
	file.close();
	return 0;
    }

    while (1) {    
	c = file.read();
	if (c == '\n') break;
	w.put(c);
    };
    return 1;
}

void wildcardProc(OmXmlWriter &w, OmWebRequest &request, int ref1, void *ref2) {
    if (strcmp(request.path, "/form") == 0) {
	uint8_t k = (int)request.query.size();

	for (int ix = 0; ix < k - 1; ix += 2)
	    Serial.printf("FORM arg %d of %d: %s = %s\n", ix / 2, k/2, request.query[ix], request.query[ix + 1]);

	if (strcmp(request.query[0], "REBOOT") == 0) { 
	    p->renderHttpResponseHeader("text/html", 200);
	    w.puts("<meta http-equiv=refresh content=\"1; URL=/FS\" />\n");
	    delay(1000);
	    resetFunc();
	}
	
	if (strcmp(request.query[0], "text") == 0) {
	    p->renderHttpResponseHeader("text/plain", 200);
	    DISPLAYTEXT = request.query[1];
	    Serial.print("|T:");
	    Serial.println(DISPLAYTEXT);
	    matrix_change(DEMO_TEXT_INPUT, false, 10);
	}

	// Changes to config file lines, look like this:
	// arg 0: 0001 = 1 3 3 3 3 106
	if (strcmp(request.query[0], "mtext") == 0) {
	    uint16_t ix = 0;
	    // k counts arg + value, we only want the number of pairs
	    uint8_t cnt = k/2;
	    DISPLAYTEXT="00,00,9999,0,1,";

	    p->renderHttpResponseHeader("text/html", 200);
	    while (cnt-->0 && (strcmp(request.query[ix], "mtext") == 0) && (strcmp(request.query[ix+1], "") != 0)) {
		ix++;
		String newline = String(request.query[ix]) + "\\";
		DISPLAYTEXT += newline;
		//Serial.print("String build: ");
		//Serial.println(DISPLAYTEXT);
		ix++;
	    }
	    Serial.print("|T:");
	    Serial.println(DISPLAYTEXT);
	    matrix_change(DEMO_TEXT_INPUT, false, 1);
	    //w.puts("<meta http-equiv=refresh content=\"6; URL=/Text\" /><BR>\n");
	    w.puts("Str: ");
	    w.puts(DISPLAYTEXT.c_str());
	    w.puts("<br>\n");
	}

	// Changes to config file lines, look like this:
	// arg 0: 0001 = 1 3 3 3 3 106
	if (request.query[0][0] == '0') {
	    uint16_t ix = 0;
	    // k counts arg + value, we only want the number of pairs
	    k = k/2;

	    p->renderHttpResponseHeader("text/html", 200);
	    while (k-->0 && (request.query[ix][0] == '0')) {
		// This is a perfect example of how you should never scan untrusted
		// input, but honestly if specially crafted input causes a crash, I
		// don't care :)
		int d32, d64, d96bm, d96, d192;
		int index, dmap;
		index = atoi(request.query[ix]);
		if (6 == sscanf(request.query[++ix], "%d %d %d %d %d %d\n", &d32, &d64, &d96bm, &d96, &d192, &dmap)) {
		    // Serial.printf("0:%s, 1:%s\n", request.query[0], request.query[1]);
		    w.putf("Got demo_list update for index %d: %d %d %d %d %d, mapped to %d<BR>\n", index, d32, d64, d96bm, d96, d192, dmap);
		    demo_mapping[index].mapping = dmap;
		    demo_mapping[index].enabled[0] = d32;
		    demo_mapping[index].enabled[1] = d64;
		    demo_mapping[index].enabled[2] = d96bm;
		    demo_mapping[index].enabled[3] = d96;
		    demo_mapping[index].enabled[4] = d192;
		    demo_mapping[dmap].reverse = index;
		    
		} else { 
		    w.putf("sscanf failed on %s\n", request.query[++ix]);
		    Serial.printf("sscanf failed on %s\n", request.query[++ix]);
		}
		ix++;
	    }
	    // rebuilding the page also re-processes the config
	    w.puts("Rebuilding index but not saving to flash<BR>\n");
	    rebuild_main_page(true);
	    // no need to rebuild the config page because it generates
	    // its html at runtime
	    // register_config_page();
	    w.puts("<meta http-equiv=refresh content=\"3; URL=/Config\" />\n");
	    return;
	}

	if (strcmp(request.query[1], "SAVE") == 0) {
	    p->renderHttpResponseHeader("text/html", 200);
	    w.putf("<meta http-equiv=refresh content=\"4; URL=/Config\" />\n");
	    write_config_index(w);
	    return;
	}

	// We assume that any pathname means file rename or delete
	// delete looks like this:
	// arg 0: /demo_map.txt_headers = /demo_map.txt_headers
	// arg 1: really delete = delete
	// rename looks like this:
	// arg 0: /demo_map.txt_headers = /demo_map.txt_headers
	if (request.query[0][0] == '/') {
	    if (k>2 && strcmp(request.query[3], "delete") == 0) {
		Serial.printf("Delete %s\n", request.query[0]);
		FSO.remove(request.query[0]);
	    } else {
		Serial.printf("Rename %s to %s\n", request.query[0], request.query[1]);
		FSO.rename(request.query[0], request.query[1]);
	    }
	    p->renderHttpResponseHeader("text/html", 200);
	    w.puts("<meta http-equiv=refresh content=\"0; URL=/FS\" />\n");
	    return;
	}

	// else show arguments sent (for debugging)
	for (int ix = 0; ix < k - 1; ix += 2)
	    w.putf("arg %d: '%s' = '%s'<br>\n", ix / 2, request.query[ix], request.query[ix + 1]);
	return;
    }

    // else assume the path is a filename to read on the FS
    // this is also not safe, it allows jumping outside the root path with "../foo"
    // but on ESP32, there is nowhere to go, so who cares?
    p->renderHttpResponseHeader("text/plain", 200);
    while (PutFileLine(w, request.path)) { w.putf("\n"); };
}

void connectionStatus(const char *ssid, bool trying, bool failure, bool success)
{
  static uint8_t failure_cnt = 0;
  static bool ap = false;
  const char *what = "?";

  if (trying)       { what = "trying"; }
  else if (failure) { what = "failure"; failure_cnt++; }
  else if (success) { what = "success"; failure_cnt = 0; }

  Serial.printf("%s: connectionStatus for '%s' is now '%s' fail cnt: %d\n", __func__, ssid, what, failure_cnt);

  if (!ap and failure_cnt > 2) {
  // FIXME, clear some wifi stuff if connection worked to the client
    Serial.println("Too many failures setting up Wifi client, swicthing to Wifi AP mode");
    ap = true;
    s.clearWifis();
    WiFi.disconnect();
    s.setAccessPoint(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    failure_cnt = 0;

  }
}

// Apparently I can't send a method s.tick to Aiko handler, so I need this glue function
void wifi_html_tick() {
    s.tick();
}

void rebuild_main_page(bool show_summary) {
    static uint8_t count=0;
    count++;

    // First time around, process_config is run by read_config_index;
    if (count > 1) process_config(show_summary);

    p->beginPage("Main");

    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	// Keep track of how many times the web page is rebuilt
	w.puts("Version: ");
	w.puts(String(count).c_str());
	w.puts("<BR>\n");
    });
    if (PANELCONFNUM == 4) {
	p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
	{
	    w.puts("RPI IP: ");
	    w.puts(Remote_IP.toString().c_str());
	    w.puts("<BR>\n");
	});
    }
    p->addSelect("Demo Mode", actionProc, PANELCONFNUM, HTML_DEMOLIST_CHOICE);
    for (uint16_t i=0; i <= 4; i++) {
	p->addSelectOption(panelconfnames[i], i);
    }

    p->addSlider(0, 1, "Text Demos Rotate",   actionProc, ROTATE_TEXT, HTML_ROTATE_TEXT);
    p->addSlider(0, 1, "Image Scrolling ",   actionProc, SCROLL_IMAGE, HTML_SCROLL_IMAGE);
    p->addSlider(0, 1, "Enable BestOf Only?", actionProc, SHOW_BEST_DEMOS, HTML_BESTOF);
    p->addSlider(0, 8, "Brightness", actionProc, DFL_MATRIX_BRIGHTNESS_LEVEL, HTML_BRIGHT);
    p->addSlider("Speed",      actionProc, 50, HTML_SPEED);

    /*
	 A button sends a value "1" when pressed (in the web page),
	 and zero when released.
    */
    p->addButton("prev", actionProc, HTML_BUTPREV);
    p->addButton("next", actionProc, HTML_BUTNEXT);

    /*
	 A select control shown as a dropdown on desktop browsers,
	 and some other multiple-choice control on phones.
	 You can add any number of options, and specify
	 the value of each.
    */
    p->addSelect("Strip Demo", actionProc, STRIPDEMO, HTML_STRIPCHOICE);
    for (uint8_t i=1; i <= StripDemoCnt; i++) {
	p->addSelectOption(StripDemoName[i], i);
    }

    p->addSelect("Panel Demo", actionProc, MATRIX_STATE, HTML_DEMOCHOICE);
    for (uint16_t i=0; i <= DEMO_LAST_IDX; i++) {
	uint16_t pos = demo_mapping[i].reverse;
	//Serial.print(i);
	//Serial.print(" -> ");
	//Serial.println(pos);

	if (demo_list[demoidx(i)].menu_str != NULL) free(demo_list[demoidx(i)].menu_str);
	if (!demo_list[demoidx(i)].func) continue;
	// It may not be safe to use PSRAM with code called inside wifi callback
	// actually it seems like it is now that PSRAM is turned off in SmartMatrix
	char *option = (char *) mallocordie("wifi addselectoption", strlen (demo_list[demoidx(i)].name) + 13);
	sprintf(option, "%03d->%03d/%1d: ", i, pos, demo_mapping[pos].enabled[PANELCONFNUM]);
	strcpy(option+12, demo_list[demoidx(i)].name);
	p->addSelectOption(option, i);
	demo_list[demoidx(i)].menu_str = option;
    }

    // Catches random non registered URLs
    p->addUrlHandler(wildcardProc);

    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	w.puts("Demo Text Input: <FORM METHOD=GET ACTION=/form><INPUT NAME=text></FORM>");
    });
}

void rebuild_advanced_page() {
    p->beginPage("Advanced");

    if (PANELCONFNUM == 4) {
	p->addButton("Show RPI IP", actionProc, HTML_SHOWIP);
	p->addButton("Restart RPI", actionProc, HTML_RESTARTPI);
	p->addButton("Reboot RPI", actionProc,  HTML_REBOOTPI);
    }

    // p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    // {
    //	w.beginElement("a", "href", "/demo_map.txt");
    //	w.addContent("Demo Map");
    //	w.endElement(); // a
    // });
    // This replaces the above
    p->addButtonWithLink("Demo Map", "/demo_map.txt", NULL, 0);
}


void register_text_page() {
    p->beginPage("Text");

    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	w.puts("Demo Text Input: <FORM METHOD=GET ACTION=/form>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.puts("<INPUT NAME=mtext><BR>");
	w.putf("<INPUT TYPE=submit NAME=multilinesubmit VALUE=SUBMIT>\n");
	w.puts("</FORM>");
    });
}


void register_config_page() {
    p->beginPage("Config");

    // This lamba function is re-run every time /Config is called
    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	//bool readingfile = true;
	char lineidx[5];
	char mapstr[14]; // "1 1 1 1 1 053" + NULL
	uint8_t display_cnt = 0;

	w.putf("<FORM METHOD=GET ACTION=/form>\n");
	w.putf("Save In Memory Demo Mapping to flash");
	w.putf("<INPUT TYPE=submit NAME=submit VALUE=SAVE>\n");
	w.putf("</FORM><BR>\n");

	w.putf("Array idx below is offset for GIF mappings<BR>\n");
	w.putf("Click Commit Button at the bottom to save to RAM<BR><BR>\n");
	w.putf("<FORM METHOD=GET ACTION=/form>\n");

	for (uint16_t index = 0; index <= CFG_LAST_INDEX; index++) {
	    uint16_t targetidx = demo_mapping[index].mapping;
	    snprintf(lineidx, 5, "%04d", index);
	    snprintf(mapstr, 14, "%d %d %d %d %d %03d", 
		demo_mapping[index].enabled[0],
		demo_mapping[index].enabled[1],
		demo_mapping[index].enabled[2],
		demo_mapping[index].enabled[3],
		demo_mapping[index].enabled[4],
		demo_mapping[index].mapping );
	    w.putf("%s: <INPUT NAME=%s SIZE=10 VALUE=\"%s\"> ", 
		   lineidx, lineidx, mapstr);
	    if (demo_list[demoidx(targetidx)].name != NULL) {
		w.putf(demo_list[demoidx(targetidx)].name);
	    } else {
		w.putf("undefined");
	    }
	    w.putf(" (%d)<BR>", demoidx(targetidx));
	    if (display_cnt && display_cnt % 15 == 0) {
		w.putf("<INPUT TYPE=submit NAME=save VALUE=SUBMIT>\n");
		w.putf("</FORM>\n");
		w.putf("<FORM METHOD=GET ACTION=/form>\n");
	    }
	    display_cnt++;
	    // We used to read the file, but now we render the in memory table
	    //readingfile = PutFileLine(w, "/demo_map.txt");
	}
	w.putf("<INPUT TYPE=submit NAME=save VALUE=SUBMIT>\n");
	w.putf("</FORM>\n");
	// empty the rest of the file, but there shouldn't be anything:
	// this is required so that the file gets closed, and next caller
	// gets to open a file from scratch.
	// Ok, we used to read the file, but now we render the in memory table
	//while ( PutFileLine(w, "/demo_map.txt") ) { 1; };
    });
}

void register_FS_page() {
    p->beginPage("FS");

    // This lamba function is re-run every time /FS is called
    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	w.putf("Total space: %d", FSO.totalBytes());
	#ifdef ESP32FATFS
	w.putf("<BR>Free space:  %d", FFat.freeBytes());
	#endif
	w.putf("<BR><BR>\n");
	w.putf("<FORM METHOD=GET ACTION=/form>\n");
	w.putf("<INPUT TYPE=submit NAME=REBOOT VALUE=REBOOT\n>");
	w.putf("</FORM><BR>\n");

	File dir = FSO.open("/");
	while (File file = dir.openNextFile()) {
	    if (file.isDirectory()) continue;
	    w.putf("<FORM METHOD=GET ACTION=/form>\n");
	    w.putf("<INPUT NAME=\"%s\" VALUE=\"%s\">", file.name(), file.name());
	    w.putf(" <A HREF=\"%s\">Size: %8d</A>", file.name(), file.size());
	    w.putf("  del -> <INPUT TYPE=checkbox name=\"really delete\" VALUE=delete>\n", file.name());
	    // we can't use the value of that submit button because it's sent even if submit is not clicked
	    w.putf("<INPUT TYPE=submit VALUE=delete\n>");
	    w.putf("</FORM>\n");
	    close(file);
	}
	close(dir);
    });
}


void register_Text_page() {
    p->beginPage("Text");

    // This lamba function is re-run every time /Text is called
    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	File dir = FSO.open("/");
	while (File file = dir.openNextFile()) {
	    if (file.isDirectory()) continue;
	    w.putf("<FORM METHOD=GET ACTION=/form>\n");
	    w.putf("<INPUT NAME=\"%s\" VALUE=\"%s\">", file.name(), file.name());
	    w.putf(" <A HREF=\"%s\">Size: %8d</A>", file.name(), file.size());
	    w.putf("  del -> <INPUT TYPE=checkbox name=\"really delete\" VALUE=delete>\n", file.name());
	    // we can't use the value of that submit button because it's sent even if submit is not clicked
	    w.putf("<INPUT TYPE=submit VALUE=delete\n>");
	    w.putf("</FORM>\n");
	    close(file);
	}
	close(dir);
    });
}


void setup_wifi() {
    Serial.println("Configuring access point...");

    s.setStatusCallback(connectionStatus);
    s.addWifi(WIFI_SSID, WIFI_PASSWORD);

    p = new OmWebPages();
    p->setBuildDateAndTime(__DATE__, __TIME__);

    // To get the ordering we want, we start with a dummy page
    // that gets replaced with the full page later.
    p->beginPage("Main");
    p->addHtml([] (OmXmlWriter & w, int ref1, void *ref2)
    {
	w.putf("Placeholder\n");
    });
    rebuild_advanced_page();
    register_text_page();
    register_FS_page();
    //register_Text_page();

    s.setHandler(*p);

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

    CFG_LAST_INDEX = 0;

    Serial.print("*********** Reading ");
    Serial.print(pathname);
    Serial.print(" for display ");
    Serial.print(PANELCONFNUM);
    Serial.print(" / ");
    Serial.print(panelconfnames[PANELCONFNUM]);
    Serial.println(" ***********");

    int scanret;
    #ifdef ARDUINOONPC
    FILE *file;
    if (! (file = fopen(pathname, "r"))) die ("Error opening " FS_PREFIX "demo_map.txt");
    char line[160];

    while ( fgets(line, 160, file) != NULL) {
	scanret = sscanf(line, "%d %d %d %d %d %d", &d32, &d64, &d96bm, &d96, &d192, &dmap);
    #else
    File file;

    if (! (file = FSO.open(pathname)
    // FIXME: should this say ESP8266 instead?
    #ifdef FSOSPIFFS
				    , "r"
    #endif
					    ) ) Serial.println("Error opening demo_map.txt");
    while (file.available()) {
	// 1 0 0 0 0 012
	String line = file.readStringUntil('\n');
	// input looks like this
	// 1 3 3 3 3 106 001 the last column is line number, used for resorting the file
	scanret = sscanf(line.c_str(), "%d %d %d %d %d %d", &d32, &d64, &d96bm, &d96, &d192, &dmap);
    #endif // ARDUINOONPC
	if (line[0] == '#' || scanret != 6) {
	    Serial.print("Skipping ");
	    Serial.print(line);
	    #ifndef ARDUINOONPC
		Serial.println();
	    #endif
	    continue;
	}
	// We use dmap, the original mapping defined in the config file
	// it is only mapped to a real demo in demo_list[] at runtime
	demo_mapping[index].mapping = dmap;
	demo_mapping[index].enabled[0] = d32;
	demo_mapping[index].enabled[1] = d64;
	demo_mapping[index].enabled[2] = d96bm;
	demo_mapping[index].enabled[3] = d96;
	demo_mapping[index].enabled[4] = d192;
	if (demo_list[demoidx(dmap)].name == NULL && (d32+d64+d96bm+d96+d192)) {
	    Serial.print("Error ");
	    Serial.print(index++);
	    Serial.print(" is mapped to an undefined demo ");
	    Serial.println(dmap);
	    Serial.print(" demoidx map: ");
	    Serial.println(demoidx(dmap));
	    delay((uint32_t) 100);
	    continue;
	}
    //#define DEBUG_CFG_READ
    #ifdef DEBUG_CFG_READ
	#ifdef ESP32
	    Serial.printf("%3d: %d, %d, %d, %d, %d -> %3d/%3d (ena:%d) => ", index, d32,  d64,  d96bm,  d96,  d192,
			  dmap, demoidx(dmap), demo_mapping[index].enabled[PANELCONFNUM]);
	#elif ARDUINOONPC
	           printf("%3d: %d, %d, %d, %d, %d -> %3d/%3d (ena:%d) => ", index, d32,  d64,  d96bm,  d96,  d192,
			  dmap, demoidx(dmap), demo_mapping[index].enabled[PANELCONFNUM]);
	#else
	    Serial.print(index);
	    Serial.print(" ");
	    Serial.print(dmap);
	    Serial.print(" ");
	    Serial.print(demo_mapping[index].enabled[PANELCONFNUM]);
	    Serial.print(" ");
	#endif
	    if (demo_list[demoidx(dmap)].name != NULL) {
		Serial.print(demo_list[demoidx(dmap)].name);
	    } else {
		Serial.print("undefined");
	    }
    #endif
	// reverse position mapping used in setup_wifi dropdown creation
	demo_mapping[dmap].reverse = index;
	if (demo_mapping[index].enabled[PANELCONFNUM] && demo_list[demoidx(dmap)].func == NULL) {
	    #ifdef DEBUG_CFG_READ
		Serial.println(" (func undefined)");
	    #endif
	    index++;
	    continue;
	}
    #ifdef DEBUG_CFG_READ
	Serial.println("");
    #endif
	// keep track of the last line read (0 based)
	CFG_LAST_INDEX = index;
	index++;
    }
    if (index == 0) {
	Serial.flush();
	Serial.print("\n\n\n**** ERROR: NOTHING READ FROM ");
	Serial.print(pathname);
	Serial.println(" ******\n\n");
	Serial.flush();
	delay(5000);
    }

    #ifdef ARDUINOONPC
    fclose(file);
    #else
    file.close();
    #endif
    process_config(true);
}

#ifdef WIFI
// This code is ESP32 only since the web interface only runs on ESP32
void write_config_index(OmXmlWriter w) {
    uint16_t renameidx = 0;
    char pathname[] = "/demo_map.txt";
    char renamepathname[] = "/demo_map_00.txt";

    // Yes, if you create 100 files, you will overflow the array
    while (sprintf(renamepathname+10, "%02d.txt", renameidx)) {
	if (FSO.exists(renamepathname)) {
	    Serial.printf("Cannot rename %s to %s, already exists\n", pathname, renamepathname);
	    w.putf("Cannot rename %s to %s, already exists<BR>\n", pathname, renamepathname);
	    renameidx++;
	    continue;
	}
	break;
    }
    Serial.printf("Renaming existing %s to %s\n", pathname, renamepathname);
    w.putf("Renaming existing %s to %s<BR>\n", pathname, renamepathname);
    if (!FSO.rename(pathname, renamepathname)) Serial.printf("Can't rename %s to %s\n", pathname, renamepathname);

    // See https://github.com/espressif/arduino-esp32/blob/master/libraries/FFat/examples/FFat_Test/FFat_Test.ino
    // file is a Stream, https://www.arduino.cc/reference/en/language/functions/communication/stream/
    File file;
    char mapstr[19]; // "1 1 1 1 1 053 001\n" + NULL

    if (! (file = FSO.open(pathname, FILE_WRITE)) ) {
	Serial.println("Error creating demo_map.txt");
	w.putf("Error creating demo_map.txt, go to FS tab and restore the previous file<BR>");
    }

    file.print("#       demo #\n");
    file.print("#             position in sorted file\n");
    file.print("# :5,$!sort -k7 (or -k6 to edit and then -k7)\n");
    file.print("# The last slot is not read by the code, it's only there for sorting the file\n");

    for (uint16_t index = 0; index <= CFG_LAST_INDEX; index++) { 
	snprintf(mapstr, 19, "%d %d %d %d %d %03d %03d\n", 
	    demo_mapping[index].enabled[0],
	    demo_mapping[index].enabled[1],
	    demo_mapping[index].enabled[2],
	    demo_mapping[index].enabled[3],
	    demo_mapping[index].enabled[4],
	    demo_mapping[index].mapping, index );
	if (! file.print(mapstr)) {
	    Serial.printf("Couldn't write line %03d: %s<BR>\n", index, mapstr);
	    w.putf("Couldn't write line %03d: %s\n", index, mapstr);
	}
    }
    file.close();
    w.putf("Saved config to demo_map.txt<BR>");
}
#endif

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
	static const char *serialdev = "";
	static char *ptr = ttyusbbuf;
	char s;
	int rdlen;
	struct stat stbuf;
	static uint32_t last_esp32_ping = millis();

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
	    if (ttyfd < 0) openttyUSB(&serialdev);
	}

	//Serial.print((millis() - last_esp32_ping));
	//Serial.print(" ");
	//Serial.println(esp32_connected);
	// If nothing is received after 20 seconds, it can be that the USB connection
	// died while staying up, or the ESP32 crashed
	if (((ttyfd < 0) || (millis() - last_esp32_ping) > 20000) && 
	    esp32_connected) {
	    esp32_connected = false;
	    Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>> ESP32 ping timeout, going to local timeouts");
	    MATRIX_LOOP = 1;
	    GIFLOOPSEC = 1;
	}

	if (ttyfd < 0) return;
		
	// Read up to 80 characters in the buffer and then continue the loop
	// this should cause a loop hang of up to 7ms at 115200bps.
	uint8_t readcnt = 80;
	while (readcnt-- && (rdlen = read(ttyfd, &s, 1)) > 0) {
	    if (s == '\n' ) {
		ptr[0] = s;
		ptr[1] = 0;
		printf("ESP> %s", ttyusbbuf);
		// reset pointer for next time
		ptr = ttyusbbuf;
		// attempt to do an active serial watchdog (protocol keepalive)
		// to keep things simpler, though, we'll just check for ttyfd >= 0
		// which when connected to an ESP32 USB, is close enough to being the same.
		// If we are getting serial pings, run the current demo for
		// a long time, because we expect the ESP to send us 'next'
		//if (! strncmp(ttyusbbuf, "FrameBuffer::GFX", 16) || ! strncmp(ttyusbbuf, "Done with demo", 14)) {
		    last_esp32_ping = millis();
		    if (! esp32_connected) {
			esp32_connected = true;
			Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>> ESP32 ping received, going to long timeouts");
		    }
		//}
		handle_rpi_serial_cmd();
		return;
	    }
	    if (s == '\\') s = '\n';
	    // FIXME: there is no control on the ESP32 input length. If it is too long before a newline
	    // we get a buffer overflow
	    ptr[0] = s;
	    ptr[1] = 0;
	    ptr++;
	}
	if (rdlen < 0) {
	    printf("Error from read: %d: %s\n", rdlen, strerror(errno));
	}
    #endif
}

void showip() {
#ifdef WIFI
    DISPLAYTEXT = WiFi.localIP().toString();
    Serial.print("|I:");
    Serial.println(DISPLAYTEXT);
    matrix_change(DEMO_TEXT_INPUT, false, 3);
#endif
}

void setup() {
    Serial.begin(115200);
    Serial.println("Hello World");
    Serial.println(__DATE__);
    Serial.println(__TIME__);
#ifdef ESP8266
    Serial.println("Init ESP8266");
    // Turn off Wifi
    // https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
    WiFi.forceSleepBegin();		  // turn off ESP8266 RF
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

    Serial.println("\nEnabling Neopixels strip (if any) and Configured Display.");
#ifdef NEOPIXEL_PIN
    Serial.print("Using FastLED on pin ");
    Serial.print(NEOPIXEL_PIN);
    Serial.print(" to drive LEDs: ");
    Serial.println(STRIP_NUM_LEDS);

    #ifndef LINUX_RENDERER_SDL
	FastLED.addLeds<WS2813,NEOPIXEL_PIN>(leds, STRIP_NUM_LEDS);
    #else
        FastLED.addLeds<SDL, STRIP_NUM_LEDS, 1>(leds, STRIP_NUM_LEDS);
	#pragma message "Enabling test neopixel strip on non RPI ARDUINOONPC"
    #endif
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

#ifdef HAS_FS
    Serial.println("Init Filesystem and GIF Viewer");
    sav_setup();
#endif

#ifdef NEOPIXEL_PIN
    leds[0] = CRGB::Black;
    leds[1] = CRGB::Red;
    leds[2] = CRGB::Blue;
    leds[3] = CRGB::Green;
    leds_show();
#endif // NEOPIXEL_PIN

    uint32_t i = 100;
    #ifdef WIFI
	// Bring wifi up early to allow renaming files if they cause a crash
	show_free_mem("Before Wifi");
	setup_wifi();
	show_free_mem("After Wifi/Before SPIFFS/FFat/LittleFS");

	Serial.println("Pause to run wifi before config file parsing ('w' to stay here)");
	while (i--) {
	    if (check_startup_IR_serial() == 2) {
		Serial.println("Will pause on debug screen");
		i = 6000;
	    }
	    s.tick();
	    delay((uint32_t) 10);
	}
    #endif

#ifdef NEOPIXEL_PIN
    leds[1] = CRGB::Black;
    leds[2] = CRGB::Red;
    leds[3] = CRGB::Blue;
    leds[4] = CRGB::Green;
    leds_show();
#endif // NEOPIXEL_PIN

    // This is now required, if there is no arduino FS support, you need to replace this function
    // You could feed it a hardcoded array in the code (what used to be here, go back in git history
    // for an older version)
    Serial.println("Read config file");
    read_config_index();

#ifdef NEOPIXEL_PIN
    leds[2] = CRGB::Black;
    leds[3] = CRGB::Red;
    leds[4] = CRGB::Blue;
    leds[5] = CRGB::Green;
    leds_show();
#endif // NEOPIXEL_PIN


#ifdef WIFI
    register_config_page();
#endif

#ifdef NEOPIXEL_PIN
    leds[3] = CRGB::Black;
    leds[4] = CRGB::Red;
    leds[5] = CRGB::Blue;
    leds[6] = CRGB::Green;
    leds_show();
#endif // NEOPIXEL_PIN

    Serial.print("Init Smart or FastLED Matrix. Matrix Size: ");
    Serial.print(mw);
    Serial.print(" ");
    Serial.println(mh);
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

    // Init demos here
    Serial.println("Init Aurora");
    aurora_setup();
    Serial.println("Init TwinkleFox");
    twinklefox_setup();
    Serial.println("Init Fireworks");
    fireworks_setup();
    Serial.println("Init sublime");
    sublime_setup();
#ifdef ARDUINOONPC
    Serial.println("Init Camera");
    if (v4lcapture_setup()) Serial.println("Init Camera FAILED!");
#endif

    show_free_mem("After Demos Init");

#ifdef NEOPIXEL_PIN
    leds[4] = CRGB::Black;
    leds[5] = CRGB::Red;
    leds[6] = CRGB::Blue;
    leds[7] = CRGB::Green;
    leds_show();
#endif // NEOPIXEL_PIN

    Serial.println("Enabling IRin");
#ifdef IR_RECV_PIN
    #ifndef ESP32RMTIR
	irrecv.enableIRIn(); // Start the receiver
    #else
	irrecv.start(IR_RECV_PIN);
    #endif
    Serial.print("Enabled IRin on pin ");
    Serial.println(IR_RECV_PIN);
#endif

    GifAnim(65535); // Compute how many GIFs are defined
    Serial.println("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv");
    Serial.print("Number of GIFs defined: ");
    Serial.println(GIF_CNT);

    // MATRIX_STATE is set in read_config_index()
    MATRIX_DEMO = demo_mapping[MATRIX_STATE].mapping;
    Serial.print("First playable demo: ");
    Serial.print(MATRIX_STATE);

    Serial.print(" mapped to: ");
    Serial.println(MATRIX_DEMO);

    // Turn off dithering https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
    FastLED.setDither( 0 );
    // Set brightness as appropriate for backend by sending a null change from the default
    change_brightness( 0 );
    matrix->setTextWrap(false);
    Serial.println("Matrix Test");

    // init first matrix demo
    matrix->fillScreen(matrix->Color(0xA0, 0xA0, 0xA0));
    // more bright
    //matrix->fillScreen(matrix->Color(0xFF, 0xFF, 0xFF));
    matrix_show();
    #ifdef NEOPIXEL_PIN
    i = 100;
    #else
    i = 200;
    #endif
    Serial.println("Pause to check that all the pixels work ('p' or power to stay here)");
    while (i--) {
	if (check_startup_IR_serial()) {
	    Serial.println("Will pause on debug screen");
	    i = 60000;
	}
	delay((uint32_t) 10);
    }
    Serial.println("Done with debug grey screen, display stats");

#ifdef NEOPIXEL_PIN
    leds[5] = CRGB::Black;
    leds[6] = CRGB::Red;
    leds[7] = CRGB::Blue;
    leds[8] = CRGB::Green;
    leds_show();
#endif // NEOPIXEL_PIN

    display_stats();
    #ifdef NEOPIXEL_PIN
    delay((uint32_t) 1000);
    #else
    delay((uint32_t) 2000);
    #endif
    if (DEMO_CNT == 0) {
	// Allow web server to run, create/save rename files to make sure demos exist
	Serial.println("No demos, cannot proceed. Starting web server to fix if possible, reboot when fixed");
	#ifdef WIFI
	while (1) s.tick();
	#endif
    }

    Serial.println("Matrix Libraries Test done");
    //font_test();

#ifdef NEOPIXEL_PIN
    // init first strip demo
    colorWipe(0x0000FF00, 10);
    Serial.println("Neopixel strip init done");
    Events.addHandler(Neopixel_Anim_Handler, 20);
#endif // NEOPIXEL_PIN

    // Check IR every 20ms or so (cooperative multitasking)
    Events.addHandler(IR_Serial_Handler, 20);

    // This is tricky, with FastLED output, sending all the pixels will take
    // most of the CPU time, but Aiko should ensure that other handlers get
    // run too, even if this handler never really hits its time target and
    // runs every single time aiko triggers.
    // With RGBPanels, both on ESP32 and rPI, RGBPanels are updated on a separate
    // CPU core, so the main core runs this code without being delayed by anything
    // other than the few other light threads and how long each frame takes to compute
    // Note however that you can't reasonably have any other handler
    // running faster than MX_UPD_TIME since it would be waiting on this one
    // being done.
    // It's currently set at 20mn, or up to 50 fps
    Events.addHandler(Matrix_Handler, MX_UPD_TIME);

    //Events.addHandler(ShowMHfps, 1000);

    Serial.println("|Starting loop");
    // After init is done, show fps (can be turned on and off with 'f').
    //#ifdef ESP32
    //SHOW_LAST_FPS = true;
    //#endif
    #ifndef ARDUINOONPC
    while (Serial.available()) Serial.read();
    Serial.println("Send '|' to enable serial commands");

    #if mheight == 192
        showip();
    #endif
    #endif
}

// vim:sts=4:sw=4
