Neopixel-IR
===========
This is a combined example code that does neopixel animations _while_ listening for IR commands (given a capable enough chip)

It supports 6 different libraries, 2 for IR and 4 for Neopixels:
- https://github.com/z3t0/Arduino-IRremote
- https://github.com/markszabo/IRremoteESP8266/
- https://github.com/adafruit/Adafruit_NeoPixel
- https://github.com/FastLED/FastLED
- https://github.com/JoDaNl/esp8266_ws2812_i2s/
- https://github.com/MartyMacGyver/ESP32-Digital-RGB-LED-Drivers
and it allows running neopixel animations on all 4 backends while using the same common code

Supported/tested chips are:
- basic AVRs like Uno/328p, Mega, Leonardo and so forth - they only allow concurrent IR if you remove interrupt disabling in the neopixel library, or you can update the neopixel strip and then listen for IR for a while.
- Teensy v3.1 - this allows talking to neopixels _while_ listening for IR
- ESP8266 - also allows both thanks to esp8266_ws2812_i2s which doesn't tie up the CPU
- ESP32 - also allows both thanks to ESP32-Digital-RGB-LED-Drivers/RMT which also doesn't tie up the CPU.

See a few screenshots and videos:


Quick video showing the problem of running Neopixels or IR: https://www.youtube.com/watch?v=62-nEJtm070

Longer Video Showing the whole story and details: https://www.youtube.com/watch?v=hgYM-R2dIWk
