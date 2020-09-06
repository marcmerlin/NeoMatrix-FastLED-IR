#ifndef ARDUINOONPC
#define BASICARDUINOFS
#endif
// Enable NEOMATRIX API even if neomatrix_config.h selects the SmartMatrix backend
#define NEOMATRIX

int OFFSETX = 0;
int OFFSETY = 0;
int FACTX = 0;
int FACTY = 0;

#if defined(ESP8266)
    // RAM is tight, save some here.
    const int lzwMaxBits = 11;
#else
    const int lzwMaxBits = 12;
#endif

// ---------------------------------------------------------------------
#define GIFANIM_INCLUDE
#include "GifAnim_Impl.h"
