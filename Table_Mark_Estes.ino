#include "Table_Mark_Estes.h"

#define zeds ledmatrix

// run this before each demo
void td_random() {
//  dwell = 1000 * (random(20, 40));//set how long the pattern will play

  ringdelay = random(30, 90);//sets how often one of the effects will happen
  bringdelay = random(70, 105);//sets how often one of the effects will happen

  counter = 0;// reset the counter with each new pattern
  steper = random(2, 8);//color tempo
//  shifty = random (3, 12);//how often the drifter moves

  blender = random8();
//  if (random8() > 180)
//  {
//    if (random8() > 120)
//      blackringme = true;
//    else
//      ringme = true;
//  }

  flip = false;
  flip2 = false;
  flip3 = false;
  if (random8() > 127)
    flip = true;
  if (random8() > 127)
    flip2 = true;
  if (random8() > 127)
    flip3 = true;

  hue += random(64);//picks the next color basis
  h = hue;
  // new drift factors for x and y drift
  cangle = (sin8(random(25, 220)) - 128.0) / 128.0;
  sangle = (sin8(random(25, 220)) - 128.0) / 128.0;
}

// ------------------------------------------------------------------

void corner()//pattern=10
{
  zeds.DrawFilledRectangle(0 , 0,  MATRIX_WIDTH, MATRIX_HEIGHT, CHSV(h, 255, 255));
  h += steper;

  for (byte y = 0; y <= MATRIX_WIDTH / 2 - 1 ; y++)
  {
    zeds.DrawFilledCircle(MIDLX, MIDLY, (MATRIX_WIDTH / 2 + 1 - y) / 3, CHSV(h  + y * steper, 255, 255));
    zeds.DrawFilledCircle(0, 0, MATRIX_WIDTH / 2 - y, CHSV(64 + h + y * steper, 255, 255));
    zeds.DrawFilledCircle(0, MATRIX_HEIGHT - 1, MATRIX_WIDTH / 2 - y, CHSV(h - 64 + y * steper, 255, 255));
    zeds.DrawFilledCircle(MATRIX_WIDTH  - 1, 0, MATRIX_WIDTH / 2 - y, CHSV(h - 64 + y * steper, 255, 255));
    zeds.DrawFilledCircle(MATRIX_WIDTH  - 1, MATRIX_HEIGHT - 1, MATRIX_WIDTH / 2 - y, CHSV(h + 64 + y * steper, 255, 255));
  }
}
void bkringer() {
  if (counter >= bringdelay)
  {
    if (counter - bringdelay <= MATRIX_WIDTH + 13)
    {
      zeds.DrawCircle(driftx, drifty , (MATRIX_WIDTH + 12 - counter + bringdelay), CHSV(h + 60, 255, 255));
      zeds.DrawCircle(driftx, drifty, (MATRIX_WIDTH + 8 - counter + bringdelay), CHSV(h + 70 , 255, 255));
      zeds.DrawCircle(driftx, drifty , (MATRIX_WIDTH + 4 - counter + bringdelay), CHSV(h + 80 , 255, 255));
      zeds.DrawCircle(driftx, drifty , (MATRIX_WIDTH  - counter + bringdelay), CHSV(h + 90 , 255, 255));
    }
    else {
      bringdelay = counter + random(60, 120);

    }
  }
}

void ringer() {
  if (counter >= ringdelay)
  {
    if (counter - ringdelay <= MATRIX_WIDTH)
    {
      if (flip || flip2)
        zeds.DrawCircle(driftx, drifty , counter - ringdelay, CHSV( h + 85, 255, 255));
      else
        zeds.DrawCircle(driftx, drifty , counter - ringdelay, CRGB::White);
    }
    else
      ringdelay = counter + random(30, 80);
  }
}



void whitewarp() {
  if (counter == 0 )
  {
    howmany = random (MIDLX * 3 / 2, MATRIX_WIDTH + 4 );

    for (int i = 0; i < howmany; i++) {
      fcount[i] = random8(); //angle
      fcolor[i] = random8();
      fpeed[i] = random(2, 12);
      xfire[i] = driftx;
      yfire[i] = drifty;

    }
  }

  for (int i = 0; i < howmany; i++)
  {
    xfire[i] = xfire[i] + fpeed[i] / 4.0 * (sin8(fcount[i] + h ) - 128.0) / 128.0;
    yfire[i] = yfire[i] + fpeed[i] / 4.0 * (cos8(fcount[i] + h ) - 128.0) / 128.0;

    if (!flip2)
      zeds(xfire[i], yfire[i]) = CRGB::White;
    else
      zeds(xfire[i], yfire[i]) = CHSV(fcolor[i] , 255, 255); //one shade of color

    if (xfire[i] < 0 || yfire[i] < 0 || xfire[i] > MATRIX_WIDTH || yfire[i] > MATRIX_HEIGHT) {
      xfire[i] = driftx;
      yfire[i] = drifty;
      fcount[i] = random8(); //angle
      fcolor[i] = random8();;
      fpeed[i] = random8(2, 12);
    }
  }
  if (!flip2)
    zeds(xfire[howmany - 1], yfire[howmany - 1]) = CHSV(fcolor[howmany - 1] , 255, 255);//many color
  else
    zeds(xfire[howmany - 1], yfire[howmany - 1]) = CRGB::White;
}
