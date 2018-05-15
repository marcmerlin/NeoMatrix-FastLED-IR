#include "config.h"
#ifndef table_mark_estes_h
#define table_mark_estes_h

#define MIDLX               (MATRIX_WIDTH/2)
#define MIDLY               (MATRIX_HEIGHT/2)

float mscale = 2.2;
float radius, xslope[MATRIX_WIDTH * 3], yslope[MATRIX_WIDTH * 3], xfire[MATRIX_WIDTH * 3], yfire[MATRIX_WIDTH * 3], cangle, sangle;
byte  xxx, yyy, dot = 3, radius2, rr, gg, bb, adjunct = 3,  fcount[MATRIX_WIDTH * 3], fcolor[MATRIX_WIDTH * 3], fcountr[MATRIX_WIDTH * 3];
unsigned int  counter, ringdelay, bringdelay, sumthum;
byte  pointyfix, fpeed[MATRIX_WIDTH * 3], targetfps = 40;
byte pointy, laps, hue, steper,  xblender, hhowmany, blender = 128, radius3, xpoffset[MATRIX_WIDTH * 3];
boolean flip = true, flip2 = false, flip3 = true, mixit = false, rimmer[MATRIX_WIDTH * 2], xbouncer[MATRIX_WIDTH * 2], ybouncer[MATRIX_WIDTH * 2];
byte ccoolloorr, why1, why2, why3, eeks1, eeks2, eeks3, h = 0, oldpattern, howmany, xhowmany, kk;
unsigned long lasttest, lastmillis, dwell = 5000,  longhammer;
float locusx, locusy, driftx, drifty, xcen, ycen, yangle, xangle;
byte raad, lender = 128, xsizer, ysizer, xx,  yy, flipme = 1;
byte shifty = 6, pattern = 0, poffset;
byte sinewidth, mstep, faudio[64], inner, bfade = 6;
int directn = 1, quash = 5;


#endif

