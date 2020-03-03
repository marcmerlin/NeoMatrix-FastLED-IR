#ifndef aurora_h
#define aurora_h

#include "neomatrix_config.h"
#include "Aurora/Effects.h"
#include "Aurora/Drawable.h"
#include "Aurora/Boid.h"
#include "Aurora/Attractor.h"
#include "Aurora/Geometry.h"

#include "Aurora/PatternAttract.h"
PatternAttract attract;
#include "Aurora/PatternBounce.h"
PatternBounce bounce;
#include "Aurora/PatternCube.h"
PatternCube cube;
#include "Aurora/PatternFlock.h"
PatternFlock Aflock;
#include "Aurora/PatternFlowField.h"
PatternFlowField flowfield;
#include "Aurora/PatternIncrementalDrift.h"
PatternIncrementalDrift incrementaldrift;
#include "Aurora/PatternIncrementalDrift2.h"
PatternIncrementalDrift2 incrementaldrift2;
#include "Aurora/PatternPendulumWave.h"
PatternPendulumWave pendulumwave;
#include "Aurora/PatternRadar.h"
PatternRadar radar;
#include "Aurora/PatternSpiral.h"
PatternSpiral spiral;
#include "Aurora/PatternSpiro.h"
PatternSpiro spiro;
#include "Aurora/PatternSwirl.h"
PatternSwirl swirl;
#include "Aurora/PatternWave.h"
PatternWave wave;

AuroraDrawable* items[] = {
    &attract,
    &bounce,
    &cube,
    &Aflock,
    &flowfield,
    &incrementaldrift,
    &incrementaldrift2,
    &pendulumwave,
    &radar,		    // 8 not great on non square
    &spiral,
    &spiro,
    &swirl,		    // 11 not great on bigger display
    &wave,
};
AuroraDrawable *drawable;

int8_t item = -1;
uint8_t numitems = sizeof(items) / sizeof(items[0]);

uint8_t aurora(uint8_t item) {
    static uint8_t delayframe = 1;
    uint8_t repeat = 1;
    static uint16_t counter = 1;
    uint16_t loops = 2000;

    // 13 demos, 0 to 12
    if (item == 0 ) { loops = 3000; } // bounce
    if (item == 5 ) { loops = 4000; } // Incremental Drift
    if (item == 6 ) { loops = 6000; } // Incremental Drift Rose
    if (item == 7 ) { loops = 5000; } // Pendulum Wave
    if (item == 10) { loops = 7000; } // Spiro

    if (matrix_reset_demo == 1) {
	counter = 0;
	drawable = items[item];
	Serial.print("Switching to drawable #");
	Serial.print(item);
	Serial.print(": ");
	Serial.println(drawable->name);
	drawable->start();
	matrix_reset_demo = 0;
	matrix->clear();
    }

    if (--delayframe) {
	matrix_show(); // make sure we still run at the same speed.
	return repeat;
    }
    delayframe = 1;

    drawable->drawFrame();
    matrix_show();

    if (counter++ < loops) return repeat;
    matrix_reset_demo = 1;
    matrix->setPassThruColor();
    return 0;
}

void aurora_setup() {
    effects.leds = matrixleds;
    effects.Setup();
}

#endif // aurora_h

// vim:sts=4:sw=4
