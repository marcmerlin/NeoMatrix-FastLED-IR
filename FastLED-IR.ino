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

#define NUM_LEDS 48

#ifdef ESP8266
#include <FastLED.h>
#define NEOPIXEL_PIN D1 // GPIO5


#include <IRremoteESP8266.h>
// D4 is also the system LED, causing it to blink on IR receive, which is great.
#define RECV_PIN D4     // GPIO2

// Turn off Wifi in setup()
// https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
//
#include "ESP8266WiFi.h"
extern "C" {
#include "user_interface.h"
}
#endif

// This file contains codes I captured and mapped myself
// using IRremote's examples/IRrecvDemo
#include "IRcodes.h"

// From Adafruit lib
#ifdef __AVR__
    #include <avr/power.h>
#endif

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


// ---------------------------------------------------------------------------

void change_brightness(int8_t change) {
    static uint8_t brightness = 4;
    static uint32_t last_brightness_change = 0 ;
    uint8_t bright_value;

    if (millis() - last_brightness_change < 300) {
	Serial.print("Too soon... Ignoring brightness change from ");
	Serial.println(brightness);
	return;
    }
    last_brightness_change = millis();
    brightness = constrain(brightness + change, 1, 8);
    bright_value = (1 << brightness) - 1;

    FastLED.setBrightness(bright_value);

    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" value ");
    Serial.println(bright_value);
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
    // Get proper dithering for non full brightness
    // https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
    //delay(delay_time);
    FastLED.delay(delay_time);
    #ifdef ESP8266
    // reset watchdog timer often enough to avoid reboots
    // https://www.hackster.io/rayburne/esp8266-turn-off-wifi-reduce-current-big-time-1df8ae
    wdt_reset();
    #endif

    if (irrecv.decode(&IR_result)) {
    	irrecv.resume(); // Receive the next value
	switch (IR_result.value) {
	case IR_RGBZONE_POWER:
	    nextdemo = f_colorWipe;
	    demo_color = 0x000000;
	    // Changing the speed value will
	    speed = 1;
	    Serial.println("Got IR: Power");
	    return 1;

	case IR_RGBZONE_BRIGHT:
	    change_brightness(+1);
	    Serial.println("Got IR: Bright");
	    return 1;

	case IR_RGBZONE_DIM:
	    change_brightness(-1);
	    Serial.println("Got IR: Dim");
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
	    delay(1000);
	    return 0;
	}
    }
    return 0;
}

void leds_show() {
    FastLED.show();
    // This causes flickering
    //FastLED[0].showLeds();
}

void leds_setcolor(uint16_t i, uint32_t c) {
    leds[i] = c;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
	//return leds.Color(255 - WheelPos * 3, 0, WheelPos * 3);
	return ((((uint32_t)(255 - WheelPos * 3)) << 16) + (WheelPos * 3));
    }
    if(WheelPos < 170) {
	WheelPos -= 85;
	//return leds.Color(0, WheelPos * 3, 255 - WheelPos * 3);
	return ((((uint32_t)(WheelPos * 3)) << 8) + (255 - WheelPos * 3));
    }
    WheelPos -= 170;
    //return leds.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    return ((((uint32_t)(WheelPos * 3)) << 16) + (((uint32_t)(255 - WheelPos * 3)) << 8));
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
    if ((uint8_t) nextdemo > 0) {
	Serial.print("Running demo: ");
	Serial.println((uint8_t) nextdemo);
    }
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


    Serial.print("Loop done, listening for IR and restarting demo at speed ");
    Serial.println(speed);
    // delay 80ms may work rarely
    // delay 200ms works 60-90% of the time
    // delay 500ms works no more reliably.
    if (handle_IR(1)) return;
    Serial.println("No IR");
}



void setup() {
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
    delay(1);                                // give RF section time to shutdown
    #define FREQUENCY    160                  // valid 80, 160
    // this breaks FastLED, so skip that step.
    // system_update_cpu_freq(FREQUENCY);
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
    FastLED.setBrightness(32);
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

    // init first demo
    colorWipe(0x00FFFFFF, 10);
}

// vim:sts=4:sw=4
