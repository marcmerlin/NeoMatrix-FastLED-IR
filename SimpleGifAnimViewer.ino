#include "SimpleGifAnimViewer/GifDecoder.h"

#ifdef ESP8266
#include <FS.h>
#else
#include <SPIFFS.h>
#endif
File file;

float matrix_gamma = 3.0; // higher number is darker


#ifdef M32B8X3
// 24x32 display, so offset 32x32 gif by -4, 0
#define OFFSETX -4
#define OFFSETY 0
const uint8_t kMatrixWidth = 32; 
const uint8_t kMatrixHeight = 32; 
#else // M32B8M32B8X3X3
#define OFFSETX 0
#define OFFSETY 0
const uint8_t kMatrixWidth = 64;
const uint8_t kMatrixHeight = 64; 
#endif



/* template parameters are maxGifWidth, maxGifHeight, lzwMaxBits
 * 
 * The lzwMaxBits value of 12 supports all GIFs, but uses 16kB RAM
 * lzwMaxBits can be set to 10 or 11 for small displays, 12 for large displays
 * All 32x32-pixel GIFs tested work with 11, most work with 10
 */

// Hardcoded GIFS in code, gives:
// Sketch uses 740380 bytes (70%) of program storage space. Maximum is 1044464 bytes.
// Global variables use 52676 bytes (64%) of dynamic memory, leaving 29244 bytes for local variables. Maximum is 81920 bytes.

// TODO: find out why dynamic memory use went up to this much, when it used to be
// Global variables use 32880 bytes (40%) of dynamic memory, leaving 49040 bytes for local variables

// 12 gives 
// Sketch uses 432932 bytes (41%) of program storage space. Maximum is 1044464 bytes. << much better
// Global variables use 72704 bytes (88%) of dynamic memory, leaving 9216 bytes for local variables. Maximum is 81920 bytes.
// Low memory available, stability problems may occur.

// 11 gives 
// Sketch uses 432932 bytes (41%) of program storage space. Maximum is 1044464 bytes.
// Global variables use 64512 bytes (78%) of dynamic memory, leaving 17408 bytes for local variables. Maximum is 81920 bytes.
// Low memory available, stability problems may occur.

// 10 only uses 8 extra KB instead of 20 extra KB (12KB RAM saved)
// Sketch uses 432932 bytes (41%) of program storage space. Maximum is 1044464 bytes.
// Global variables use 60432 bytes (73%) of dynamic memory, leaving 21488 bytes for local variables. Maximum is 81920 bytes.

// 10 doesn't properly decode the bottom of /gifs/triangles_in.gif, 11 is required.
GifDecoder<kMatrixWidth, kMatrixHeight, 11> decoder;

bool fileSeekCallback(unsigned long position) { return file.seek(position); }
unsigned long filePositionCallback(void) { return file.position(); }
int fileReadCallback(void) { return file.read(); }
int fileReadBlockCallback(void * buffer, int numberOfBytes) { return file.read((uint8_t*)buffer, numberOfBytes); }

void screenClearCallback(void) { matrix_clear(); }
void updateScreenCallback(void) { matrix_show(); }

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
  CRGB color = CRGB(matrix->gamma[red], matrix->gamma[green], matrix->gamma[blue]);
  // This works but does not handle out of bounds pixels well (it writes to the last pixel)
  // matrixleds[XY(x+OFFSETX,y+OFFSETY)] = color;
  matrix->setPassThruColor(color.red*65536 + color.green*256 + color.blue);
  // drawPixel ensures we don't write out of bounds
  matrix->drawPixel(x+OFFSETX, y+OFFSETY, 0);
}

// Setup method runs once, when the sketch starts
void sav_setup() {
    decoder.setScreenClearCallback(screenClearCallback);
    decoder.setUpdateScreenCallback(updateScreenCallback);
    decoder.setDrawPixelCallback(drawPixelCallback);

    decoder.setFileSeekCallback(fileSeekCallback);
    decoder.setFilePositionCallback(filePositionCallback);
    decoder.setFileReadCallback(fileReadCallback);
    decoder.setFileReadBlockCallback(fileReadBlockCallback);

    matrix->precal_gamma(matrix_gamma);

    #ifdef ESP8266
    SPIFFS.begin();
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
	String fileName = dir.fileName();
	size_t fileSize = dir.fileSize();
	Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    #endif
    Serial.printf("\n");
}

bool sav_newgif(const char *pathname) {
    SPIFFS.begin();
    if (file) file.close();
    file = SPIFFS.open(pathname, "r");
    Serial.print(pathname);
    if (!file) {
        Serial.println(": Error opening GIF file");
	return 1;
    }
    Serial.println(": Opened GIF file, start decoding");
    decoder.startDecoding();
    return 0;
}

bool sav_loop() {
    if (decoder.decodeFrame() == ERROR_WAITING) return 1;
    return 0;
}

// vim:sts=4:sw=4
