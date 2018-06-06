#ifndef aurora_h
#define aurora_h

#include "config.h"
#include "Aurora/Effects.h"
#include "Aurora/Drawable.h"
#include "Aurora/Boid.h"
#include "Aurora/Attractor.h"
#include "Aurora/Geometry.h"

Effects effects;


#include "Aurora/PatternAttract.h"
PatternAttract attract;
#include "Aurora/PatternBounce.h"
PatternBounce bounce;
#include "Aurora/PatternCube.h"
PatternCube cube;
#include "Aurora/PatternFlock.h"
PatternFlock flock;
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

Drawable* items[] = {
    &attract,
    &bounce,
    &cube,
    &flock,
    &flowfield,
    &incrementaldrift,
    &incrementaldrift2,
    &pendulumwave,
    &radar,
    &spiral,
    &spiro,
    &swirl,
    &wave,
};
Drawable *drawable;

int8_t item = -1;
uint8_t numitems = sizeof(items) / sizeof(items[0]);

uint8_t aurora(uint8_t item) {
    static uint8_t delayframe = 1;
    uint8_t repeat = 1;
    static uint16_t counter = 1;
    uint16_t loops = 2000;

    if (item == 1 ) { loops = 3000; }
    if (item == 5 ) { loops = 4000; }
    if (item == 7 ) { loops = 5000; }
    if (item == 10) { loops = 5000; }

    if (matrix_reset_demo == 1) {
	counter = 0;
	drawable = items[item];
	Serial.print("Switching to drawable #");
	Serial.print(item);
	Serial.print(": ");
	Serial.println(drawable->name);
	drawable->start();
	matrix_reset_demo = 0;
	matrix_clear();
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

#endif

// vim:sts=4:sw=4
