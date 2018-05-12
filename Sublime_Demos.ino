// Frem Sublime for Novel Mutations Costume Controllers 2018, GPLv3
// https://github.com/Intrinsically-Sublime/esp8266-fastled-webserver
//
// Further adapted by Marc MERLIN for integration in FastLED::NeoMatrix
// standalone examples.

#include "config.h"


static uint8_t intensity = 42;


CRGB solidColor = CRGB::Blue;
CRGB solidRainColor = CRGB(60,80,90);

const CRGBPalette16 WoodFireColors_p = CRGBPalette16(CRGB::Black, CRGB::OrangeRed, CRGB::Orange, CRGB::Gold);		//* Orange
const CRGBPalette16 SodiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Orange, CRGB::Gold, CRGB::Goldenrod);		//* Yellow
const CRGBPalette16 CopperFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Green, CRGB::GreenYellow, CRGB::LimeGreen);	//* Green
const CRGBPalette16 AlcoholFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB::DeepSkyBlue, CRGB::LightSkyBlue);//* Blue
const CRGBPalette16 RubidiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Indigo, CRGB::Indigo, CRGB::DarkBlue);	//* Indigo
const CRGBPalette16 PotassiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::Indigo, CRGB::MediumPurple, CRGB::DeepPink);//* Violet
const CRGBPalette16 LithiumFireColors_p = CRGBPalette16(CRGB::Black, CRGB::FireBrick, CRGB::Pink, CRGB::DeepPink);	//* Red

uint8_t rotatingFirePaletteIndex = 0;
CRGBPalette16 RotatingFire_p(WoodFireColors_p);

const CRGBPalette16 firePalettes[] = {
	WoodFireColors_p,
	SodiumFireColors_p,
	CopperFireColors_p,
	AlcoholFireColors_p,
	RubidiumFireColors_p,
	PotassiumFireColors_p,
	LithiumFireColors_p,
	RotatingFire_p
};

const uint8_t firePaletteCount = ARRAY_SIZE(firePalettes);
uint8_t currentFirePaletteIndex = firePaletteCount-1; // Force rotating fire palette

// Array of temp cells (used by fire, theMatrix, coloredRain, stormyRain)
uint_fast16_t tempMatrix[MATRIX_WIDTH+1][MATRIX_HEIGHT+1];

#define leds matrixleds

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void fire()
{
#if MATRIX_HEIGHT/6 > 6
	#define FIRE_BASE	6
#else
	#define FIRE_BASE	MATRIX_HEIGHT/6+1
#endif
	static uint8_t currentPaletteIndex = 0;
	// COOLING: How much does the air cool as it rises?
	// Less cooling = taller flames.  More cooling = shorter flames.
	uint8_t cooling = 80;
	// SPARKING: What chance (out of 255) is there that a new spark will be lit?
	// Higher chance = more roaring fire.  Lower chance = more flickery fire.
	uint8_t sparking = 84;
	// SMOOTHING; How much blending should be done between frames
	// Lower = more blending and smoother flames. Higher = less blending and flickery flames
	const uint8_t fireSmoothing = 60;

	//FastLED.delay(1000/map8(speed,30,110));
	// Add entropy to random number generator; we use a lot of it.
	random16_add_entropy(random(256));

	CRGBPalette16 fire_p( CRGB::Black);

	if (currentFirePaletteIndex < firePaletteCount-1) {
		fire_p = firePalettes[currentFirePaletteIndex];
	} else {
		fire_p = RotatingFire_p;
	}

	// Loop for each column individually
	for (int x = 0; x < MATRIX_WIDTH; x++) {
		// Step 1.  Cool down every cell a little
		for (int i = 0; i < MATRIX_HEIGHT; i++) {
			tempMatrix[x][i] = qsub8(tempMatrix[x][i], random(0, ((cooling * 10) / MATRIX_HEIGHT) + 2));
		}

		// Step 2.  Heat from each cell drifts 'up' and diffuses a little
		for (int k = MATRIX_HEIGHT; k > 0; k--) {
			tempMatrix[x][k] = (tempMatrix[x][k - 1] + tempMatrix[x][k - 2] + tempMatrix[x][k - 2]) / 3;
		}

		// Step 3.  Randomly ignite new 'sparks' of heat near the bottom
		if (random(255) < sparking) {
			int j = random(FIRE_BASE);
			tempMatrix[x][j] = qadd8(tempMatrix[x][j], random(160, 255));
		}

		// Step 4.  Map from heat cells to LED colors
		for (int y = 0; y < MATRIX_HEIGHT; y++) {
			// Blend new data with previous frame. Average data between neighbouring pixels
			nblend(leds[XY(x,y)], ColorFromPalette(fire_p, ((tempMatrix[x][y]*0.7) + (tempMatrix[wrapX(x+1)][y]*0.3))), fireSmoothing);
		}
	}
}

void theMatrix()
{
	// ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds )
	rain(60, 200, map8(intensity,5,100), 195, CRGB::Green, false, false, false);
}

void coloredRain()
{
	// ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds )
	rain(60, 180, map8(intensity,2,60), 30, solidRainColor, true, false, false);
}

void stormyRain()
{
	// ( Depth of dots, maximum brightness, frequency of new dots, length of tails, color, splashes, clouds )
	rain(0, 90, map8(intensity,0,100)+30, 30, solidRainColor, true, true, true);
}

void rain(byte backgroundDepth, byte maxBrightness, byte spawnFreq, byte tailLength, CRGB rainColor, bool splashes, bool clouds, bool storm)
{
	static uint16_t noiseX = random16();
	static uint16_t noiseY = random16();
	static uint16_t noiseZ = random16();

	//FastLED.delay(1000/map8(speed,16,32));
	// Add entropy to random number generator; we use a lot of it.

	CRGB lightningColor = CRGB(72,72,80);
	CRGBPalette16 rain_p( CRGB::Black, rainColor );
	CRGBPalette16 rainClouds_p( CRGB::Black, CRGB(15,24,24), CRGB(9,15,15), CRGB::Black );

	fadeToBlackBy( leds, NUMMATRIX, 255-tailLength);

	// Loop for each column individually
	for (int x = 0; x < MATRIX_WIDTH; x++) {
		// Step 1.  Move each dot down one cell
		for (int i = 0; i < MATRIX_HEIGHT; i++) {
			if (tempMatrix[x][i] >= backgroundDepth) {	// Don't move empty cells
				tempMatrix[x][i-1] = tempMatrix[x][i];
				tempMatrix[x][i] = 0;
			}
		}

		// Step 2.  Randomly spawn new dots at top
		if (random(255) < spawnFreq) {
			tempMatrix[x][MATRIX_HEIGHT-1] = random(backgroundDepth, maxBrightness);
		}

		// Step 3. Map from tempMatrix cells to LED colors
		for (int y = 0; y < MATRIX_HEIGHT; y++) {
			if (tempMatrix[x][y] >= backgroundDepth) {	// Don't write out empty cells
				leds[XY(x,y)] = ColorFromPalette(rain_p, tempMatrix[x][y]);
			}
		}

		// Step 4. Add splash if called for
		if (splashes) {
			static uint_fast16_t splashArray[MATRIX_WIDTH];

			byte j = splashArray[x];
			byte v = tempMatrix[x][0];

			if (j >= backgroundDepth) {
				leds[XY(x-2,0,true)] = ColorFromPalette(rain_p, j/3);
				leds[XY(x+2,0,true)] = ColorFromPalette(rain_p, j/3);
				splashArray[x] = 0; 	// Reset splash
			}

			if (v >= backgroundDepth) {
				leds[XY(x-1,1,true)] = ColorFromPalette(rain_p, v/2);
				leds[XY(x+1,1,true)] = ColorFromPalette(rain_p, v/2);
				splashArray[x] = v;	// Prep splash for next frame
			}
		}

		// Step 5. Add lightning if called for
		if (storm) {
			int lightning[MATRIX_WIDTH][MATRIX_HEIGHT];

			if (random16() < 200) {		// Odds of a lightning bolt
				lightning[scale8(random8(), MATRIX_WIDTH)][MATRIX_HEIGHT-1] = 255;	// Random starting location
				for(int ly = MATRIX_HEIGHT-1; ly > 0; ly--) {
					for (int lx = 1; lx < MATRIX_WIDTH-1; lx++) {
						if (lightning[lx][ly] == 255) {
							lightning[lx][ly] = 0;
							uint8_t dir = random8(4);
							switch (dir) {
								case 0:
									leds[XY(lx+1,ly-1,true)] = lightningColor;
									lightning[wrapX(lx+1)][ly-1] = 255;	// move down and right
								break;
								case 1:
									leds[XY(lx,ly-1,true)] = CRGB(128,128,128);
									lightning[lx][ly-1] = 255;		// move down
								break;
								case 2:
									leds[XY(lx-1,ly-1,true)] = CRGB(128,128,128);
									lightning[wrapX(lx-1)][ly-1] = 255;	// move down and left
								break;
								case 3:
									leds[XY(lx-1,ly-1,true)] = CRGB(128,128,128);
									lightning[wrapX(lx-1)][ly-1] = 255;	// fork down and left
									leds[XY(lx-1,ly-1,true)] = CRGB(128,128,128);
									lightning[wrapX(lx+1)][ly-1] = 255;	// fork down and right
								break;
							}
						}
					}
				}
			}
		}

		// Step 6. Add clouds if called for
		if (clouds) {
			uint16_t noiseScale = 250;	// A value of 1 will be so zoomed in, you'll mostly see solid colors. A value of 4011 will be very zoomed out and shimmery
			const uint8_t cloudHeight = (MATRIX_HEIGHT*0.2)+1;
			// This is the array that we keep our computed noise values in
			static uint8_t noise[MATRIX_WIDTH][cloudHeight];
			int xoffset = noiseScale * x + gHue;

			for(int z = 0; z < cloudHeight; z++) {
				int yoffset = noiseScale * z - gHue;
				uint8_t dataSmoothing = 192;
				uint8_t noiseData = qsub8(inoise8(noiseX + xoffset,noiseY + yoffset,noiseZ),16);
				noiseData = qadd8(noiseData,scale8(noiseData,39));
				noise[x][z] = scale8( noise[x][z], dataSmoothing) + scale8( noiseData, 256 - dataSmoothing);
				nblend(leds[XY(x,MATRIX_HEIGHT-z-1)], ColorFromPalette(rainClouds_p, noise[x][z]), (cloudHeight-z)*(250/cloudHeight));
			}
			noiseZ ++;
		}
	}
}

void sublime_loop() {
	EVERY_N_SECONDS(5) {
		RotatingFire_p = firePalettes[ rotatingFirePaletteIndex ];
		rotatingFirePaletteIndex = addmod8( rotatingFirePaletteIndex, 1, firePaletteCount-1);
	}
}
