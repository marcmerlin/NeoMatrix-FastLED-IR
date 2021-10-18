#include "neomatrix_config.h"
#ifndef table_mark_estes_h
#define table_mark_estes_h

#define MIDLX               (MATRIX_WIDTH/2)
#define MIDLY               (MATRIX_HEIGHT/2)
#define BIGGER              mmax(MATRIX_WIDTH, MATRIX_HEIGHT)
#define MAXBIG              BIGGER * 2

float mscale = 2.2;
float radius, xslope[MATRIX_WIDTH * 3], yslope[MATRIX_WIDTH * 3], xfire[MATRIX_WIDTH * 3], yfire[MATRIX_WIDTH * 3], cangle, sangle;
byte  xxx, yyy, dot = 3, radius2, rr, gg, bb, adjunct = 3,  fcount[MATRIX_WIDTH * 3], fcolor[MATRIX_WIDTH * 3], fcountr[MATRIX_WIDTH * 3];
unsigned int  counter, ringdelay, bringdelay, sumthum;
byte  pointyfix, fpeed[MATRIX_WIDTH * 3], targetfps = 40;
byte pointy, laps, hue, steper,  xblender, hhowmany, blender = 128, radius3, xpoffset[MATRIX_WIDTH * 3];
boolean flip = true, flip2 = false, flip3 = true, flop[12], mixit = false, rimmer[MATRIX_WIDTH * 2], xbouncer[MATRIX_WIDTH * 2], ybouncer[MATRIX_WIDTH * 2];
byte ccoolloorr, why1, why2, why3, eeks1, eeks2, eeks3, h = 0, oldpattern, howmany, xhowmany, kk;
unsigned long lasttest, lastmillis, dwell = 5000,  longhammer;
float locusx, locusy, driftx, drifty, xcen, ycen, yangle, xangle;
byte raad, lender = 128, xsizer, ysizer, xx,  yy, flipme = 1;
byte shifty = 6, pattern = 0, poffset;
byte sinewidth, mstep, faudio[MATRIX_WIDTH], inner, bfade = 6;
int directn = 1, quash = 5;

// New TME
uint16_t velo = 30, dot3 = 1;

void td_init();
void fakenoise();
void audioprocess();
void adjuster();
void td_init();
void td_loop();
void corner();
void bkringer();
void ringer();
void whitewarp();
void warp();
void spire();
void rmagictime();
void bkboxer();
void starer();
void hypnoduck2();
void boxer();
void spin2();
void bkstarer();
void starz();
void hypnoduck4();
void solid2();
void bubbles();
void homer2();
void circlearc();
void confetti();
void confetti2();
void Roundhole();
void solid();
void adjustme();

// New TME
void ADCircle(int16_t xc, int16_t yc, uint16_t r, CRGB Col);
void ADFCircle(int16_t xc, int16_t yc, uint16_t r, CRGB Col);
void ADFRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, CRGB Col);
void DCircle(int16_t xc, int16_t yc, uint16_t r, CRGB Col);
void DFCircle(int16_t xc, int16_t yc, uint16_t r, CRGB Col);
void DFRectangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, CRGB Col);
void DLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, CRGB Col);
void DALine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, CRGB Col);
void mirror();
void spire2();
void triforce();
void triangle(int16_t xloc, int16_t yloc, uint16_t bigg, uint8_t dangle, uint8_t kolor);
#endif

