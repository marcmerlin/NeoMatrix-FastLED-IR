#ifdef FASTLED
#include <FastLED.h>
#else
#include <Adafruit_NeoPixel.h>
#endif


#include <IRremote.h>
#include "IRcodes.h"
#ifdef __AVR__
    #include <avr/power.h>
#endif

// neopixel + IR is hard, neopixel disables interrups while IR is trying to 
// receive, and bad things happen :(
// https://github.com/z3t0/Arduino-IRremote/issues/314
// leds.show takes 1 to 3ms


#define RECV_PIN 2
#define NEOPIXEL_PIN 6
#define NUM_LEDS 60


IRrecv irrecv(RECV_PIN);

#ifdef FASTLED
CRGB leds[NUM_LEDS];
FastLED.addLeds<NEOPIXEL,NEOPIXEL_PIN>(leds, NUM_LEDS);
#else
// Parameter 1 = number of pixels in leds
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif

typedef enum {
    f_nothing = 0,
    f_colorWipe = 1,
    f_rainbow = 2,
    f_rainbowCycle = 3,
    f_theaterChase = 4,
    f_theaterChaseRainbow = 5,
} StripDemo;

StripDemo nextdemo = f_colorWipe;
int32_t nextdemo_color = 0x00FF00; // Green
int8_t nextdemo_wait = 50;


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
    
    Serial.print("Changing brightness ");
    Serial.print(change);
    Serial.print(" to level ");
    Serial.print(brightness);
    Serial.print(" value ");
    Serial.println(bright_value);
    leds.setBrightness(bright_value);
    leds_show_time();
}


// The IR ISR does not work correctly when leds.show() is
// running itself (as it does disable interrupts to get precise
// timing), so we sample IR at the same time we run delay
bool handle_IR(uint32_t delay_time) {
    decode_results IR_result;

    delay(delay_time);
    
    if (irrecv.decode(&IR_result)) {
    	irrecv.resume(); // Receive the next value
	switch (IR_result.value) {
	case IR_RGBZONE_POWER:
	    nextdemo = f_colorWipe;
	    nextdemo_color = 0x000000;
	    nextdemo_wait = 1;
	    Serial.println("Got IR: Power");
	    return 1;

	case IR_RGBZONE_BRIGHT:
	    change_brightness(+1);
	    Serial.println("Got IR: Bright");
	    return 0;

	case IR_RGBZONE_DIM:
	    change_brightness(-1);
	    Serial.println("Got IR: Dim");
	    return 0;

	case IR_RGBZONE_RED:
	    nextdemo = f_colorWipe;
	    nextdemo_color = 0xFF0000; // Red
	    nextdemo_wait = 50;
	    Serial.println("Got IR: Red");
	    return 1;

	case IR_RGBZONE_GREEN:
	    nextdemo = f_colorWipe;
	    nextdemo_color = 0x00FF00; // Green
	    nextdemo_wait = 50;
	    Serial.println("Got IR: Green");
	    return 1;

	case IR_RGBZONE_BLUE:
	    nextdemo = f_colorWipe;
	    nextdemo_color = 0x0000FF; // Blue
	    nextdemo_wait = 50;
	    Serial.println("Got IR: Blue");
	    return 1;

	case IR_RGBZONE_AUTO:
	    nextdemo = f_rainbow;
	    nextdemo_wait = 50;
	    Serial.println("Got IR: Auto");
	    return 1;

	case IR_RGBZONE_FLASH:
	    nextdemo = f_theaterChaseRainbow;
	    nextdemo_wait = 50;
	    Serial.println("Got IR: Flash");
	    return 1;

	case IR_JUNK:
	    return 0;

	default:
	    Serial.print("Got unknown IR value: ");
	    Serial.println(IR_result.value, HEX);
	    return 0;
	}
    }
    return 0;
}

void leds_show_time() {
    uint32_t before=millis();
#ifdef FASTLED
    FastLED.show();
#else
    leds.show();
#endif
    //Serial.print("leds.show took ");
    //Serial.println(millis() - before);
}

void leds_setcolor(uint16_t i, uint32_t c) {
#ifdef FASTLED
    leds[i] = c;
#else
    leds.setPixelColor(i, c);
#endif
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
	return leds.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if(WheelPos < 170) {
	WheelPos -= 85;
	return leds.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return leds.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
    for(uint16_t i=0; i<NUM_LEDS; i++) {
	leds_setcolor(i, c);
	leds_show_time();
	if (handle_IR(wait)) return;
    }
}

void rainbow(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256; j++) {
	for(i=0; i<NUM_LEDS; i++) {
	    leds_setcolor(i, Wheel((i+j) & 255));
	}
	leds_show_time();
	if (handle_IR(wait)) return;
    }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
	for(i=0; i< NUM_LEDS; i++) {
	    leds_setcolor(i, Wheel(((i * 256 / NUM_LEDS) + j) & 255));
	}
	leds_show_time();
	if (handle_IR(wait)) return;
    }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
    for (int j=0; j<10; j++) {  //do 10 cycles of chasing
	for (int q=0; q < 3; q++) {
	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
		leds_setcolor(i+q, c);    //turn every third pixel on
	    }
	    leds_show_time();

	    if (handle_IR(wait)) return;

	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
		leds_setcolor(i+q, 0);        //turn every third pixel off
	    }
	}
    }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
    for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
	for (int q=0; q < 3; q++) {
	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
		leds_setcolor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
	    }
	    leds_show_time();

	    if (handle_IR(wait)) return;

	    for (uint16_t i=0; i < NUM_LEDS; i=i+3) {
		leds_setcolor(i+q, 0);        //turn every third pixel off
	    }
	}
    }
}

void loop() {
    if ((uint8_t) nextdemo > 0) {
	Serial.print("Running demo: ");
	Serial.println((uint8_t) nextdemo);
    }
    switch (nextdemo) {
    case f_colorWipe:
	colorWipe(nextdemo_color, nextdemo_wait);
	break;
    case f_rainbow:
	rainbow(nextdemo_wait);
	break;
    case f_rainbowCycle:
	rainbowCycle(nextdemo_wait);
	break;
    case f_theaterChase:
	theaterChase(nextdemo_color, nextdemo_wait);
	break;
    case f_theaterChaseRainbow:
	theaterChaseRainbow(nextdemo_wait);
	break;
    default:
	break;
    }

    Serial.println("Loop done, listening for IR and restarting demo");
    // delay 80ms may work rarely
    // delay 200ms works 60-90% of the time
    // delay 500ms works no more reliably.
    if (handle_IR(50)) return;
    Serial.println("No IR");
}

void setup() {
    Serial.begin(115200);
    Serial.println("Enabling IRin");
    irrecv.enableIRIn(); // Start the receiver
    irrecv.blink13(true); // Start the receiver
    Serial.println("Enabled IRin, turn on LEDs");

#ifndef FASTLED
    leds.begin();
#endif
    leds_show_time(); // Initialize all pixels to 'off'
    Serial.println("LEDs on");
    colorWipe(leds.Color(255, 255, 255), 10);
    // Brightness compatible with both libraries.
    leds.setBrightness(15);
}

// vim;sts=4:sw=4
