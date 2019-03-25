#define BASICSPIFFS
#define NEOMATRIX
#define SPI_FFS

#if defined(ESP8266)
    #include <FS.h>
    #define SPI_FFS
    #if matrix_size == 64
        #define GIF_DIRECTORY "/gifs64/"
    #else
        #define GIF_DIRECTORY "/gifs/"
    #endif
    extern "C" {
        #include "user_interface.h"
    }
    const int lzwMaxBits = 11;
#elif defined(ESP32)
    #include <SPIFFS.h>
    #define SPI_FFS
    // Do NOT add a trailing slash, or things will fail
    #if matrix_size == 64
        #define GIF_DIRECTORY "/gifs64"
    #else
        #define GIF_DIRECTORY "/gifs"
    #endif
    const int lzwMaxBits = 12;
#endif

// If the matrix is a different size than the GIFs, allow panning through the GIF
// while displaying it, or bouncing it around if it's smaller than the display
int OFFSETX = 0;
int OFFSETY = 0;

// ---------------------------------------------------------------------
#define GIFANIM_INCLUDE
#include "GifAnim_Impl.h"
