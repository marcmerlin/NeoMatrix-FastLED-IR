NeoMatrix-FastLED-IR
====================
This is a combined example code that does neopixel animations _while_ listening for IR commands (given a capable enough chip)
and while animating a neopixel matrix.
It is written/tested on ESP8266 and ESP32. It should also work with minimal changes on teensy.

This is a fork of https://github.com/marcmerlin/FastLED-IR and adds FastLED-Neomatrix / SmartMatrix::GFX code:

It uses these libraries
- https://github.com/markszabo/IRremoteESP8266/ (ESP8266 only)
- https://github.com/z3t0/Arduino-IRremote (ESP32 and others)
- https://github.com/FastLED/FastLED
- https://github.com/marcmerlin/FastLED_NeoMatrix (NeoMatrix backend only)
- https://github.com/marcmerlin/SmartMatrix_GFX + https://github.com/pixelmatix/SmartMatrix/tree/teensylc (SmartMatrix backend only)
- https://github.com/adafruit/Adafruit-GFX-Library

Photos and videos
-----------------
https://www.youtube.com/watch?v=tU_wkrrv_4A

On NeoMatrix Backend (FastLED::NeoMatrix) 24x32:
![dsc09944](https://user-images.githubusercontent.com/1369412/39416207-0b726474-4c00-11e8-9d04-fb0264b12017.jpg)

On SmartMatrix backend (SmartMatrix::GFX), 64x96:
![image](https://user-images.githubusercontent.com/1369412/55821904-60220980-5ab3-11e9-9326-8e7fdf46f3c6.png)
![image](https://user-images.githubusercontent.com/1369412/55821913-6617ea80-5ab3-11e9-9467-38400b16ab49.png)
