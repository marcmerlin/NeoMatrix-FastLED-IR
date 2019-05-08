#define BASICSPIFFS
#define NEOMATRIX

int OFFSETX = 0;
int OFFSETY = 0;
int FACTX = 0;
int FACTY = 0;

#if defined(ESP8266)
    #include <FS.h>
    #define FSO SPIFFS
    #define matrix_size 32
    // Needs trailing slash
    #define GIF_DIRECTORY "/gifs/"
    extern "C" {
        #include "user_interface.h"
    }
    const int lzwMaxBits = 11;
#elif defined(ESP32)
    #include "FFat.h"
    #define FSO FFat
    #define FSOFAT
    #define matrix_size 64
    // Do NOT add a trailing slash, or things will fail for SPIFFS
    #define GIF_DIRECTORY "/gifs64"
    const int lzwMaxBits = 12;
#endif

// If the matrix is a different size than the GIFs, allow panning through the GIF
// while displaying it, or bouncing it around if it's smaller than the display

// ---------------------------------------------------------------------
#define GIFANIM_INCLUDE
#include "GifAnim_Impl.h"
