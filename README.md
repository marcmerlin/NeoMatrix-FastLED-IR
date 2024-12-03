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
- https://github.com/marcmerlin/SmartMatrix_GFX + https://github.com/pixelmatix/SmartMatrix/ (SmartMatrix backend only)
- https://github.com/hzeller/rpi-rgb-led-matrix + https://github.com/marcmerlin/FastLED_RPIRGBPanel_GFX + https://github.com/marcmerlin/ArduinoOnPc-FastLED-GFX-LEDMatrix (rPI only)
- https://github.com/adafruit/Adafruit-GFX-Library
- https://github.com/distrakt/OmEspHelpers

Hardware Support
----------------
This code was originally written for ESP8266 running a 24x32 Neopixel Matrix, and I later moved to ESP32 running
64x96 SmartMatrix, and eventually ported the code to Raspberry Pi where it now runs via rpi-rgb-panel at 128x192 via the
ArduinoOnPC compat layer.  
While the code is meant to work on all 3 platforms, some time after tag 20200307_last_ESP8266 
( https://github.com/marcmerlin/NeoMatrix-FastLED-IR/tree/5d5ce13e24b5d6c08fbcb5a46119bb32f51a1af8 )
I added Wifi support on ESP32, and moved the hardcoded demo config from code to a demo_map.txt read 
from the filesystem. This allows modifying the file at runtime, but not on SPIFFS. Running FatFS/LittleFS
on ESP8266 just takes too much RAM, so if you are set on using ESP8266 (but really, why? Please buy an ESP32)
then you should use the code at tag 20200307_last_ESP8266 (this is also a good place to get the code before
I added Wifi support in case Wifi is an issue for you, and Wifi can be turned off with a #define).

More generally you can find historical tags here https://github.com/marcmerlin/NeoMatrix-FastLED-IR/tags

Photos and videos
-----------------
Version 3 with 24x32 LED Matrix: https://www.youtube.com/watch?v=tU_wkrrv_4A

Version 5 with 128x192 RGBPanels: https://www.youtube.com/watch?v=OIXOEYw8r6E

Thanks to OSMhelpers, it generates its own html UI on the fly:
![thumb1024_420_20220109_LED_Pants_and_Shirt_v5_Join1](https://github.com/user-attachments/assets/5fdd8ae9-a0c7-406e-8249-3464d8feb4c2)
![thumb1024_421_20220109_LED_Pants_and_Shirt_v5_Join1](https://github.com/user-attachments/assets/fa890fc4-6b8c-4544-9cce-74fbd8ff1bbd)


On NeoMatrix Backend (FastLED::NeoMatrix) 24x32, running on ESP8266
![dsc09944](https://user-images.githubusercontent.com/1369412/39416207-0b726474-4c00-11e8-9d04-fb0264b12017.jpg)

On SmartMatrix backend (SmartMatrix::GFX), 64x96, running on ESP32
![image](https://user-images.githubusercontent.com/1369412/55821904-60220980-5ab3-11e9-9326-8e7fdf46f3c6.png)
![image](https://user-images.githubusercontent.com/1369412/55821913-6617ea80-5ab3-11e9-9467-38400b16ab49.png)

And on rPi/rpi-rgb-panel backend, 128x192, running on Raspberry Pi3
![image](https://user-images.githubusercontent.com/1369412/138024861-240def05-5470-402e-be27-01c293b74639.png)
