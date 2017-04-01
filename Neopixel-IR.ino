#include <Adafruit_NeoPixel.h>
#include <IRremote.h>
#include "IRcodes.h"
#ifdef __AVR__
    #include <avr/power.h>
#endif

#define RECV_PIN 2
#define NEOPIXEL_PIN 6

IRrecv irrecv(RECV_PIN);

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);



// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
    for(uint16_t i=0; i<strip.numPixels(); i++) {
	strip.setPixelColor(i, c);
	strip.show();
	delay(wait);
    }
}

void rainbow(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256; j++) {
	for(i=0; i<strip.numPixels(); i++) {
	    strip.setPixelColor(i, Wheel((i+j) & 255));
	}
	strip.show();
	delay(wait);
    }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
    uint16_t i, j;

    for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
	for(i=0; i< strip.numPixels(); i++) {
	    strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
	}
	strip.show();
	delay(wait);
    }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
    for (int j=0; j<10; j++) {  //do 10 cycles of chasing
	for (int q=0; q < 3; q++) {
	    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
		strip.setPixelColor(i+q, c);    //turn every third pixel on
	    }
	    strip.show();

	    delay(wait);

	    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
		strip.setPixelColor(i+q, 0);        //turn every third pixel off
	    }
	}
    }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
    for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
	for (int q=0; q < 3; q++) {
	    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
		strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
	    }
	    strip.show();

	    delay(wait);

	    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
		strip.setPixelColor(i+q, 0);        //turn every third pixel off
	    }
	}
    }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if(WheelPos < 85) {
	return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if(WheelPos < 170) {
	WheelPos -= 85;
	return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void change_brightness(int8_t change) {
    static uint8_t brightness = 8;
    static uint32_t last_brightness_change = 0 ;
    uint8_t bright_value;

    if (millis() - last_brightness_change < 3000) {
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
    strip.setBrightness(bright_value);
    strip.show();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Enabling IRin");
    irrecv.enableIRIn(); // Start the receiver
    Serial.println("Enabled IRin, turn on LEDs");
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    Serial.println("LEDs on");
    colorWipe(strip.Color(255, 255, 255), 10);
}

void loop() {
    decode_results IR_result;

    if (irrecv.decode(&IR_result)) {
	irrecv.resume(); // Receive the next value
	switch (IR_result.value) {
	case IR_RGBZONE_POWER:
	    colorWipe(strip.Color(0, 0, 0), 0);
	    break;

	case IR_RGBZONE_BRIGHT:
	    change_brightness(+1);
	    break;

	case IR_RGBZONE_DIM:
	    change_brightness(-1);
	    break;

	case IR_RGBZONE_RED:
	    colorWipe(strip.Color(255, 0, 0), 50); // Red
	    break;

	case IR_RGBZONE_BLUE:
	    colorWipe(strip.Color(0, 0, 255), 50); // Blue
	    break;

	case IR_RGBZONE_GREEN:
	    colorWipe(strip.Color(0, 255, 0), 50); // Green
	    break;

	case IR_JUNK:
	    break;

	default:
	    Serial.print("Got unknown IR value: ");
	    Serial.println(IR_result.value, HEX);
	    #if 0
	    theaterChase(strip.Color(127, 127, 127), 50); // White
	    theaterChase(strip.Color(127, 0, 0), 50); // Red
	    theaterChase(strip.Color(0, 0, 127), 50); // Blue

	    rainbow(20);
	    rainbowCycle(20);
	    theaterChaseRainbow(50);
	    #endif
	}
    }
    delay(10);
}
// vim;sts=4:sw=4
