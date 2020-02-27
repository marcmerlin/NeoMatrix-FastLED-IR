#include "Table_Mark_Estes.h"

#define zeds ledmatrix

// -------------------------------------------------------------------------------
// Common functoions used by the animations

void lfado(byte bbc)
{
  for ( byte hhh = 0; hhh < MATRIX_WIDTH ; hhh++)
    for (byte jjj = 0; jjj < MATRIX_HEIGHT ; jjj++)
      zeds(hhh, jjj).fadeToBlackBy(bbc);//% = bbc/255
}

void redfado(byte bbc) {
  for ( byte hhh = 0; hhh < MATRIX_WIDTH ; hhh++)
    for (byte jjj = 0; jjj < MATRIX_HEIGHT ; jjj++)
      if (flip2)
        zeds(hhh, jjj) -= CRGB(random(bbc / 2), random(bbc), random(bbc));//leave more red
      else
        zeds(hhh, jjj) -= CRGB(random(bbc / 2), random(bbc / 2), random(bbc));// leave more yellow
}

void greenfado(byte bbc) {
  for ( byte hhh = 0; hhh < MATRIX_WIDTH ; hhh++)
    for (byte jjj = 0; jjj < MATRIX_HEIGHT ; jjj++)
      if (!flip3)
        zeds(hhh, jjj) -= CRGB(random(bbc ), random(bbc / 2), random(bbc));
      else
        zeds(hhh, jjj) -= CRGB(random(bbc ), random(bbc / 2), random(bbc / 2));// leave more teal
}

void bluefado(byte bbc) {
  for ( byte hhh = 0; hhh < MATRIX_WIDTH ; hhh++)
    for (byte jjj = 0; jjj < MATRIX_HEIGHT ; jjj++)
      if (flip2)
        zeds(hhh, jjj) -= CRGB(random(bbc ), random(bbc), random(bbc / 2));
      else
        zeds(hhh, jjj) -= CRGB(random(bbc / 2 ), random(bbc), random(bbc / 2)); //leave more purple

}
void fakenoise()
{
  faudio[0] = random(1, MIDLY);
  for ( uint16_t i = 1; i < MATRIX_WIDTH; i++) {
    faudio[i] = faudio[i - 1] + random(10) - 7;
    faudio[i] = constrain(faudio[i ], 1, MATRIX_HEIGHT - 2);
  }
  if (counter % 600 == 0) {
    faudio[random(MATRIX_WIDTH)] = constrain( (MATRIX_HEIGHT / - random(MIDLY >> 2, MIDLY)), 2, MATRIX_HEIGHT - 1);
  }
}

void audioprocess()
{
  fakenoise();
}

void adjuster() {  // applies the screen wide effect
  switch (adjunct) {
    case 0://no effect
      break;
    case 1://no effect
      break;
    case 2:
      zeds.HorizontalMirror();
      zeds.VerticalMirror();
      break;
    case 3:
      zeds.QuadrantBottomTriangleMirror();
      break;
    case 4:
      zeds.HorizontalMirror();
      break;
    case 5:
      zeds.VerticalMirror();
      break;
    case 6:
      zeds.QuadrantRotateMirror();
      break;
    case 7:
      zeds.TriangleTopMirror();
      break;
    case 8:
      zeds.QuadrantMirror();
      break;

    default:// no effect
      break;
  }
}



// -------------------------------------------------------------------------------
// Called from the main code 
// run this before each demo

void td_init() {
  // File-wise init
  driftx = random8(4, MATRIX_WIDTH - 4);//set an initial location for the animation center
  drifty = random8(4, MATRIX_HEIGHT - 4);// set an initial location for the animation center
  mstep = byte( 256 / (MATRIX_WIDTH - 1)); //mstep is the step size to distribute 256 over an array the width of the matrix
 steper = random8(2, 8);// steper is used to modify h to generate a color step on each move
  lastmillis = millis();
  lasttest = millis();
  //randomSeed(analogRead(0) - analogRead(3) + analogRead(5));
  hue = random8();//get a starting point for the color progressions
  mscale = 2.2;
  fakenoise();
  cangle = (sin8(random(25, 220)) - 128.0) / 128.0;//angle of movement for the center of animation gives a float value between -1 and 1
  sangle = (sin8(random(25, 220)) - 128.0) / 128.0;//angle of movement for the center of animation in the y direction gives a float value between -1 and 1

  // Per pattern init
  targetfps = random(20, 30);
  bfade = random(1, 8);
  dot = random(2, 6);// controls the size of a circle in many animations
  if (max(MATRIX_HEIGHT, MATRIX_WIDTH) >  64) dot += 4;
  if (max(MATRIX_HEIGHT, MATRIX_WIDTH) > 128) dot += 4;
  adjunct = (random(3, 11));//controls which screen wide effect is used if any

  dwell = 1000 * (random(20, 40));//set how long the pattern will play
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

void td_loop() {
  hue += 1;//increment the color basis
  h = hue;  //set h to the color basis
  if (counter % shifty == 0) driftx =  driftx + cangle;//move the x center every so often
  if (driftx > (MATRIX_WIDTH - MIDLX / 2))//change directin of drift if you get near the right 1/4 of the screen
    cangle = 0 - abs(cangle);
  if (driftx < MIDLX / 2)//change directin of drift if you get near the right 1/4 of the screen
    cangle = abs(cangle);
  if ((counter + shifty / 2) % shifty == 0) drifty =  drifty + sangle;//move the y center every so often
  if (drifty > ( MATRIX_HEIGHT - MIDLY / 2))// if y gets too big, reverse
    sangle = 0 - abs(sangle);
  if (drifty < MIDLY / 2) {// if y gets too small reverse
    sangle = abs(sangle);
  }

  driftx = constrain(driftx, MIDLX - MIDLX / 3, MIDLX + MIDLX / 3);//constrain the center, probably never gets evoked any more but was useful at one time to keep the graphics on the screen....
  drifty = constrain(drifty, MIDLY - MIDLY / 3, MIDLY - MIDLY / 3);
  // test for frame rate,  every 15 frames

  // this calls the pre frame fade,based on a byte "bfade" which is mostly random, but sometimes assignes for a specific effect
  switch (bfade)
  {
    case 0:
      lfado(3);//almost none  3/256 or 1.17%
      break;
    //cases 5,6,and 7 cause the fade to be non symetric, favoring one color over others, this adds a bit of fun to the ghosts
    case 5:
      bluefado(8 + (random8() >> 2));
      break;
    case 6:
      redfado(8 + (random8() >> 2));
      break;
    case 7:
      greenfado(8 + (random8() >> 2));
      break;

    default:
      lfado(bfade << 4);// 1 = 16,  4 = 64, this sets the amont of fade as a fraction over 256  range is 16/256 (6.25%) i.e. not much  to 64/256 (25%)or considerable
      break;
  }
  counter++;//increment the counter which is used for many things
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
      //if (flip || flip2)
        zeds.DrawCircle(driftx, drifty , counter - ringdelay, CHSV( h + 85, 255, 255));
      //else
      //  zeds.DrawCircle(driftx, drifty , counter - ringdelay, CRGB::White);
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

    //if (!flip2)
    //  zeds(xfire[i], yfire[i]) = CRGB::White;
    //else
      zeds(xfire[i], yfire[i]) = CHSV(fcolor[i] , 255, 255); //one shade of color

    if (xfire[i] < 0 || yfire[i] < 0 || xfire[i] > MATRIX_WIDTH || yfire[i] > MATRIX_HEIGHT) {
      xfire[i] = driftx;
      yfire[i] = drifty;
      fcount[i] = random8(); //angle
      fcolor[i] = random8();;
      fpeed[i] = random8(2, 12);
    }
  }
  //if (!flip2)
    zeds(xfire[howmany - 1], yfire[howmany - 1]) = CHSV(fcolor[howmany - 1] , 255, 255);//many color
  //else
   // zeds(xfire[howmany - 1], yfire[howmany - 1]) = CRGB::White;
}

void warp() {
  if (counter == 0 )
  {
    howmany = random (MIDLX * 7 / 5, MATRIX_WIDTH * 7 / 5);

    for (int i = 0; i < howmany; i++) {
      fcount[i] = random8(); //angle
      fcolor[i] = blender + random(45);//color
      fpeed[i] = random(2, 12);
      xfire[i] = driftx;
      yfire[i] = drifty;

    }
  }

  for (int i = 0; i < howmany; i++)
  {
    xfire[i] = xfire[i] + (fpeed[i] / 4.0) * (sin8(fcount[i] + h ) - 128.0) / 128.0;
    yfire[i] = yfire[i] + ( fpeed[i] / 4.0) * (cos8(fcount[i] + h ) - 128.0) / 128.0;
    if (!flip)
      zeds(xfire[i], yfire[i]) = CHSV(fcolor[i], 255, 255);//random colors
    else if (!flip2)
      zeds(xfire[i], yfire[i]) = CRGB::White;
    else
      zeds(xfire[i], yfire[i]) = CHSV(blender*64/howmany, 255, 255); //one shade of color

    if (xfire[i] < 0 || yfire[i] < 0 || xfire[i] > MATRIX_WIDTH || yfire[i] > MATRIX_HEIGHT) {
      xfire[i] = driftx;
      yfire[i] = drifty;
      fcount[i] = random8(); //angle
      fcolor[i] = random8();;
      fpeed[i] = random8(2, 8);
    }
  }
  if (!flip2)
    zeds(xfire[howmany - 1], yfire[howmany - 1]) = CHSV(blender*64/howmany , 255, 255);
  else
    zeds(xfire[howmany - 1], yfire[howmany - 1]) = CRGB::White;
}

void spire() {

  if (counter == 0)
  {
    radius =  MATRIX_HEIGHT / 2 - 3;
    flip = true;
    radius2 =  5;
    flip2 = false;
    dot = dot + 3 + random(5);
  }

  if (h % 16 == 0)
  {
    if (radius < 5)
      flip = 1 - flip;
    if (radius > MATRIX_HEIGHT / 2)
      flip = 1 - flip;
    if (flip)
      radius --;
    else
      radius++;

    if (radius2 < 5)
      flip2 = 1 - flip2;
    if (radius2 > MATRIX_HEIGHT / 2)
      flip2 = 1 - flip2;
    if (flip2)
      radius2 --;
    else
      radius2++;
  }

  float xer = driftx + radius * (cos8(2 * h) - 128.0) / 128.0;
  float yer = drifty + radius * (sin8(2 * h) - 128.0) / 128.0;
  zeds.DrawCircle(xer, yer, dot, CHSV(h, 255, 255));
  xer = driftx - radius * (cos8(2 * h) - 128.0) / 128.0;
  yer = drifty - radius * (sin8(2 * h) - 128.0) / 128.0;
  zeds.DrawCircle(xer, yer, dot, CHSV(h + 128, 255, 255));

  xer = driftx + radius * (cos8(2 * h + 87) - 128.0) / 128.0;
  yer = drifty + radius * (sin8(2 * h + 87) - 128.0) / 128.0;
  zeds.DrawCircle(xer, yer, dot, CHSV(h + 85, 255, 255));
  xer = driftx - radius * (cos8(2 * h + 87) - 128.0) / 128.0;
  yer = drifty - radius * (sin8(2 * h + 87) - 128.0) / 128.0;
  zeds.DrawCircle(xer, yer, dot, CHSV(h - 85, 255, 255));

  xer = driftx + radius * (cos8(2 * h + 43) - 128.0) / 128.0;
  yer = drifty + radius * (sin8(2 * h + 43) - 128.0) / 128.0;
  zeds.DrawCircle(xer, yer, dot, CHSV(h + 43, 255, 255));
  xer = driftx - radius * (cos8(2 * h + 43) - 128.0) / 128.0;
  yer = drifty - radius * (sin8(2 * h + 43) - 128.0) / 128.0;
  zeds.DrawCircle(xer, yer, dot, CHSV(h - 43, 255, 255));
}

void rmagictime()
{

  if (counter == 0)
  {
    locusx = driftx;
    locusy = drifty;
    raad = 1;
    ringdelay = random(30, 60);
    // ccoolloorr = random8();
  }

  if (raad < MATRIX_WIDTH - dot)
  {
    zeds.DrawCircle(driftx, drifty , raad, CHSV(ccoolloorr + h , 255, 255));
    zeds.DrawCircle(driftx, drifty , raad + 1, CHSV(ccoolloorr + h , 255, 255));
    zeds.DrawCircle(driftx, drifty , raad - 1, CHSV(ccoolloorr + h , 255, 255));
    raad++;
  }

  if (raad == MATRIX_WIDTH - dot) {
    ringdelay--;
    //ringdelay = constrain(ringdelay, 0, 20);
  }

  if (ringdelay == 0)
  {
    raad = 1;
    locusx = driftx;
    locusy = drifty;
    ringdelay = random(50, 70);
    ccoolloorr = random(8);
  }
}

void bkboxer() {

  if (counter >= bringdelay)
  {
    if (counter - bringdelay <= MATRIX_WIDTH + 13)
    {
      zeds.DrawRectangle(driftx - 12 - (counter - bringdelay) , drifty - 12 - (counter - bringdelay) , driftx + 12 + (counter - bringdelay), drifty + 12 + (counter - bringdelay), CHSV(h, 255, 255));
      zeds.DrawRectangle(driftx - 8 - (counter - bringdelay) , drifty - 8 - (counter - bringdelay) , driftx + 8 + (counter - bringdelay), drifty + 8 + (counter - bringdelay), CHSV(h +   steper * 3, 255, 255));
      zeds.DrawRectangle(driftx - 4 - (counter - bringdelay) , drifty - 4 - (counter - bringdelay) , driftx + 4 + (counter - bringdelay), drifty + 4 + (counter - bringdelay), CHSV(h +  steper * 6, 255, 255));
      zeds.DrawRectangle(driftx - (counter - bringdelay) , drifty - (counter - bringdelay) , driftx + (counter - bringdelay), drifty + (counter - bringdelay), CHSV(h + steper * 9, 255, 255));
    }
    else {
      bringdelay = counter + random(60, 980);

    }
  }
}

void drawstar(byte xlocl, byte ylocl, byte biggy, byte little, byte points, byte dangle, byte koler)// random multipoint star
{
  if (counter == 0) {
    shifty = 3;//move quick
  }
  radius2 = 255 / points;
  for (int i = 0; i < points; i++)
  {
    zeds.DrawLine(xlocl + ((little * (sin8(i * radius2 + radius2 / 2 - dangle) - 128.0)) / 128), ylocl + ((little * (cos8(i * radius2 + radius2 / 2 - dangle) - 128.0)) / 128), xlocl + ((biggy * (sin8(i * radius2 - dangle) - 128.0)) / 128), ylocl + ((biggy * (cos8(i * radius2 - dangle) - 128.0)) / 128), CHSV(koler , 255, 255));
    zeds.DrawLine(xlocl + ((little * (sin8(i * radius2 - radius2 / 2 - dangle) - 128.0)) / 128), ylocl + ((little * (cos8(i * radius2 - radius2 / 2 - dangle) - 128.0)) / 128), xlocl + ((biggy * (sin8(i * radius2 - dangle) - 128.0)) / 128), ylocl + ((biggy * (cos8(i * radius2 - dangle) - 128.0)) / 128), CHSV(koler , 255, 255));
  }
}

void starer() {
  if (counter == 0)
    pointy = 7;
  if (counter >= ringdelay)
  {
    if (counter - ringdelay <= MATRIX_WIDTH + 5)
      drawstar(driftx  , drifty, 2 * (counter - ringdelay), (counter - ringdelay), pointy, blender + h, h * 2 + 85);
    else {
      ringdelay = counter + random(50, 99);
      pointy = random(3, 9);
    }
  }
}

void hypnoduck2()
{
  // MM FLAGS
  flip = 0; // force whitewarp
  //flip2 controls white or color
  flip3 = 0; // force whitewarp
  if (counter == 0)
    dot = random(2, 4);
  if (!flip2)
    quash = 12 + dot;
  else
    quash = -12 - dot;

  if (flip3 && flip2)
    zeds.DrawFilledRectangle(0, 0, MATRIX_WIDTH  - 1, MATRIX_HEIGHT  - 1, CHSV(h, 255, 150));
  else {
    if (flip)
      zeds.DrawFilledRectangle(0, 0, MATRIX_WIDTH  - 1, MATRIX_HEIGHT  - 1, CRGB::Black);
    else
      whitewarp();
  }

  for (uint32_t jj = 50; jj < 100 + (counter % 400); jj += 5)
  {
    xangle =  (sin8(jj + quash * h) - 128.0) / 128.0;
    yangle =  (cos8(jj + quash * h) - 128.0) / 128.0;

    zeds.DrawFilledCircle( driftx + xangle * (jj /  (19 - dot)) , drifty + yangle * (jj / (19 - dot)), dot, CHSV(h - 85, 255, 255));


    xangle =  (sin8(jj + quash * h + 128) - 128.0) / 128.0;
    yangle =  (cos8(jj + quash * h + 128) - 128.0) / 128.0;
    zeds.DrawFilledCircle( driftx + xangle * (jj /  (19 - dot)) ,  drifty + yangle * (jj / (19 - dot)), dot, CHSV(h , 255, 255));
  }
}

void boxer() {
  if (counter >= ringdelay)
  {
    if (counter - ringdelay <= MATRIX_WIDTH)
    {
      zeds.DrawRectangle(driftx - (counter - ringdelay) , drifty - (counter - ringdelay) , driftx + (counter - ringdelay), drifty + (counter - ringdelay), CHSV(h * 2 + 128, 255, 255));
    }
    else
      ringdelay = counter + random(40, 70);
  }
}

void spin2()  // 4 meteriorw moving in ovals
{
  driftx = MIDLX;
  drifty = MIDLY;
  xangle =  (sin8(2 * (h )) - 128.0) / 128.0;
  yangle =  (cos8(2 * (h )) - 128.0) / 128.0;

  zeds.DrawFilledCircle(MIDLX + xangle * (MIDLX - 9), MIDLY + yangle * (MIDLY - 1) , dot, CHSV(h + 128, 255, 255));
  xangle =  (sin8(2 * (h ) + 64) - 128.0) / 128.0;
  yangle =  (cos8(2 * (h ) + 64) - 128.0) / 128.0;
  zeds.DrawFilledCircle(MIDLX + xangle * (MIDLX - 1), MIDLY + yangle * (MIDLY - 9) , dot, CHSV(h , 255, 255));
  xangle =  (sin8(2 * (h ) + 128) - 128.0) / 128.0;
  yangle =  (cos8(2 * (h ) + 128) - 128.0) / 128.0;
  zeds.DrawFilledCircle(MIDLX + xangle * (MIDLX - 9), MIDLY + yangle * (MIDLY - 1) , dot, CHSV(h + 64, 255, 255));
  xangle =  (sin8(2 * (h ) - 64) - 128.0) / 128.0;
  yangle =  (cos8(2 * (h ) - 64) - 128.0) / 128.0;
  zeds.DrawFilledCircle(MIDLX + xangle * (MIDLX - 1), MIDLY + yangle * (MIDLY - 9) , dot, CHSV(h - 64, 255, 255));
}

void bkstarer() {
  if (counter == 0)
    pointyfix = 5;
  if (counter >= bringdelay)
  {
    if (counter - bringdelay <= MATRIX_WIDTH + 20)
    {
      //void drawstar(byte xlocl, byte ylocl, byte biggy, byte little, byte points, byte dangle, byte koler)// random multipoint star
      drawstar(driftx, drifty, 2 * (MATRIX_WIDTH - (counter - bringdelay) + 15), 15 + MATRIX_WIDTH - (counter - bringdelay), pointyfix, blender - h, h  - 60);
      drawstar(driftx, drifty, 2 * (MATRIX_WIDTH - (counter - bringdelay) + 10), 10 + MATRIX_WIDTH - (counter - bringdelay), pointyfix, blender - h, h  - 50);
      drawstar(driftx, drifty, 2 * (MATRIX_WIDTH - (counter - bringdelay) + 5), 5 + MATRIX_WIDTH - (counter - bringdelay), pointyfix, blender - h, h  - 40);
      drawstar(driftx, drifty, 2 * (MATRIX_WIDTH - (counter - bringdelay)), MATRIX_WIDTH - (counter - bringdelay), pointyfix, blender - h, h - 30  );
    }
    else
    {
      bringdelay = counter + random(69, 126);
      pointy = random(4, 9);
    }
  }
}

void starz()// 3 stars spin in a circle
{
  if (counter == 0) {
    howmany = random (3, 9);
    inner = random(MIDLY / 5, MIDLY / 2);
    radius2 = 255 / howmany;
  }

  xcen = MIDLX * (sin8(-2 * h - 85) - 128.0) / 128;
  ycen = MIDLY  * (cos8(-2 * h - 85 ) - 128.0) / 128;

  if (h % 16 == 0) {
    inner = inner + flipme;
    if (inner > MIDLY / 2 || inner < MIDLY / 5)
      flipme = -flipme;
  }

  for (int i = 0; i < howmany; i++)
  {
    zeds.DrawLine(driftx + xcen + ((inner * (sin8(i * radius2 + radius2 / 2 +  h) - 128.0)) / 128), drifty + ycen + ((inner * (cos8(i * radius2 + radius2 / 2 +  h) - 128.0)) / 128), driftx + xcen + ((MIDLX * (sin8(i * radius2 +  h) - 128.0)) / 128), drifty + ycen + ((MIDLY * (cos8(i * radius2 +  h) - 128.0)) / 128), CHSV(h , 255, 255));
    zeds.DrawLine(driftx + xcen + ((inner * (sin8(i * radius2 - radius2 / 2 +  h) - 128.0)) / 128), drifty + ycen + ((inner * (cos8(i * radius2 - radius2 / 2 +  h) - 128.0)) / 128), driftx + xcen + ((MIDLX * (sin8(i * radius2 +  h) - 128.0)) / 128), drifty + ycen + ((MIDLY * (cos8(i * radius2 +  h) - 128.0)) / 128), CHSV(h , 255, 255));
  }

  xcen = MIDLX  * (sin8(-2 * h + 85) - 128.0) / 128;
  ycen = MIDLY  * (cos8(-2 * h + 85) - 128.0) / 128;
  for (int i = 0; i < howmany; i++)
  {
    zeds.DrawLine(driftx + xcen + ((inner * (sin8(i * radius2 + radius2 / 2 +  h) - 128.0)) / 128), drifty + ycen + ((inner * (cos8(i * radius2 + radius2 / 2 +  h) - 128.0)) / 128), driftx + xcen + ((MIDLX * (sin8(i * radius2 +  h) - 128.0)) / 128), drifty + ycen + ((MIDLY * (cos8(i * radius2 +  h) - 128.0)) / 128), CHSV(h + 85, 255, 255));
    zeds.DrawLine(driftx + xcen + ((inner * (sin8(i * radius2 - radius2 / 2 +  h) - 128.0)) / 128), drifty + ycen + ((inner * (cos8(i * radius2 - radius2 / 2 +  h) - 128.0)) / 128), driftx + xcen + ((MIDLX * (sin8(i * radius2 +  h) - 128.0)) / 128), drifty + ycen + ((MIDLY * (cos8(i * radius2 +  h) - 128.0)) / 128), CHSV(h + 85, 255, 255));
  }

  xcen = MIDLX  * (sin8(-2 * h) - 128.0) / 128;
  ycen = MIDLY  * (cos8(-2 * h) - 128.0) / 128;
  for (int i = 0; i < howmany; i++)
  {
    zeds.DrawLine(driftx + xcen + ((inner * (sin8(i * radius2 + radius2 / 2 +  h) - 128.0)) / 128), drifty + ycen + ((inner * (cos8(i * radius2 + radius2 / 2 +  h) - 128.0)) / 128), driftx + xcen + ((MIDLX * (sin8(i * radius2 +  h) - 128.0)) / 128), drifty + ycen + ((MIDLY * (cos8(i * radius2 +  h) - 128.0)) / 128), CHSV(h  - 85, 255, 255));
    zeds.DrawLine(driftx + xcen + ((inner * (sin8(i * radius2 - radius2 / 2 +  h) - 128.0)) / 128), drifty + ycen + ((inner * (cos8(i * radius2 - radius2 / 2 +  h) - 128.0)) / 128), driftx + xcen + ((MIDLX * (sin8(i * radius2 +  h) - 128.0)) / 128), drifty + ycen + ((MIDLY * (cos8(i * radius2 +  h) - 128.0)) / 128), CHSV(h  - 85, 255, 255));
  }
}

void hypnoduck4()
// spiral inward with the hyponic light
{
  // MMFLAGS
  uint16_t hd4size = 890;
  uint16_t hd4center = 166; // 166 for 64x64, decrease to 120 for 96x96
  uint8_t  hd4step = 5; // 5 is ok for 64x64, but need to decrease to 3 or 2 for 96x96

  if      (max(MATRIX_HEIGHT, MATRIX_WIDTH) > 128) { hd4size = 2600; hd4center = 160; hd4step = 1; }
  else if (max(MATRIX_HEIGHT, MATRIX_WIDTH) > 64)  { hd4size = 1200; hd4center = 166; hd4step = 3; };

  if (flip2) quash = 8; else quash = -8;

  if (flip3)
    zeds.DrawFilledRectangle(0, 0, MATRIX_WIDTH  - 1, MATRIX_HEIGHT  - 1, CRGB::Black);
  else
  { //if (flip)
      zeds.DrawFilledRectangle(0, 0, MATRIX_WIDTH  - 1, MATRIX_HEIGHT  - 1, CHSV(h + 35, 255, 155));
    //else
    //  zeds.DrawFilledRectangle(0, 0, MATRIX_WIDTH  - 1, MATRIX_HEIGHT  - 1, CRGB::White);
  }
  for (uint32_t jj = hd4size; jj > hd4center - (counter % 160); jj -= hd4step)
  {
    xangle =  (sin8(jj + quash * h) - 128.0) / 128.0;
    yangle =  (cos8(jj + quash * h) - 128.0) / 128.0;
    zeds.DrawFilledCircle(driftx + xangle * (jj /  17) , drifty + yangle * (jj / 17), 2, CHSV(h, 255, 255));

    xangle =  (sin8(jj + quash * h + 128) - 128.0) / 128.0;
    yangle =  (cos8(jj + quash * h + 128) - 128.0) / 128.0;
    zeds.DrawFilledCircle(driftx + xangle * (jj /  17) , drifty + yangle * (jj / 17), 2, CHSV(h - 115, 255, 255));
  }
}

void solid2()
{
  if (counter == 0)
    rr = random8();
  zeds.DrawFilledRectangle(0 , 0,  MATRIX_WIDTH, MATRIX_HEIGHT, CHSV(rr -  h, 255, 90));
}

void bubbles() {
  if (counter == 0)
  {
    howmany = random (MIDLX >> 1, MIDLX );

    for (byte u = 0; u < howmany; u++) {
      xfire[u] = random(MATRIX_WIDTH);
      yfire[u]  = random(MATRIX_HEIGHT);
      fcolor[u] = random8(); //color
      fpeed[u] = random(1, 7); //speed

      fcount[u] = random(3, MIDLX >> 1); //radius
      if (random8() > 128)
        rimmer[u] = true;
      else
        rimmer[u] = false;
    }
  }

  for (byte u = 0; u < howmany; u++) {
    zeds.DrawFilledCircle(xfire[u],  yfire[u] - fcount[u] / 2, fcount[u], CHSV(fcolor[u], 255, 255));
    if (rimmer[u])
      zeds.DrawCircle(xfire[u],  yfire[u] - fcount[u] / 2, fcount[u], CHSV(fcolor[u] + 87, 255, 255));
    else
      zeds.DrawCircle(xfire[u],  yfire[u] - fcount[u] / 2, fcount[u], CHSV(fcolor[u] - 87, 255, 255));

    if (counter % fpeed[u] == 0)
    {
      if (u % 2 == 0)
        yfire[u]++;
      else
        xfire[u]++;
    }
    if (yfire[u] > MATRIX_HEIGHT + fcount[u] + 2  || xfire[u] > MATRIX_WIDTH + fcount[u] + 2  )
    {
      if (u % 2 == 0)
      {
        xfire[u] = random(3, MATRIX_WIDTH - 3);
        yfire[u] =   0;
      }
      else
      {
        yfire[u] = random(3, MATRIX_HEIGHT - 3);
        xfire[u] =   0;
      }
      fcolor[u] = random8(); //color
      fpeed[u] = random(1, 7); //speed
      fcount[u] = random(2, MIDLX >> 1); //radius

      if (random8() > 128)
        rimmer[u] = true;
      else
        rimmer[u] = false;
    }
  }
}

void homer2() {// growing egg
  if (counter == 0 )
  {
    howmany = random (MIDLX + 8, 2 * MATRIX_WIDTH - 12);
    dot = random(1, 5);
    if (max(MATRIX_HEIGHT, MATRIX_WIDTH) >  64) dot += 4;
    if (max(MATRIX_HEIGHT, MATRIX_WIDTH) > 128) dot += 4;
    for (int i = 0; i < howmany; i++) {
      fcount[i] = random8(); //angle
      fcolor[i] = random8();//color
      fpeed[i] = random(1, 4);// bigger = slower

      xpoffset[i] = random8();
      ccoolloorr = random8();
    }
  }
  byte tempo = dot * 2 + 50;
  if (counter % tempo == 0) {
    dot++;
    counter = counter + tempo;

  }

  if (dot >= MIDLX) {
    dot = random(3, 7);
    if (max(MATRIX_HEIGHT, MATRIX_WIDTH) >  64) dot += 4;
    if (max(MATRIX_HEIGHT, MATRIX_WIDTH) > 128) dot += 4;
    ccoolloorr =  random8();
  }
  zeds.DrawCircle( MIDLX  , MIDLY, dot + 1, CRGB::White);
  for (int i = 0; i < howmany; i++)
  {
    if (flip)
      zeds(MIDLX +  (dot + MIDLX - ((counter + xpoffset[i]) % MIDLX) / fpeed[i]) * (sin8(fcount[i] + 2 * h ) - 128.0) / 128.0 , MIDLY +  (dot + MIDLY - ((counter + xpoffset[i]) % MIDLY) / fpeed[i]) * (cos8(fcount[i] + 2 * h ) - 128.0) / 128.0) =  CHSV(fcolor[i], 255, 255);
    else if (!flip2)
      zeds(MIDLX +  (dot + MIDLX - ((counter + xpoffset[i]) % MIDLX) / fpeed[i]) * (sin8(fcount[i] + 2 * h ) - 128.0) / 128.0 , MIDLY +  (dot + MIDLY - ((counter + xpoffset[i]) % MIDLY) / fpeed[i]) * (cos8(fcount[i] + 2 * h ) - 128.0) / 128.0) =  CRGB::Orange;
    else
      zeds(MIDLX +  (dot + MIDLX - ((counter + xpoffset[i]) % MIDLX) / fpeed[i]) * (sin8(fcount[i] + 2 * h ) - 128.0) / 128.0, MIDLY +  (dot + MIDLY - ((counter + xpoffset[i]) % MIDLY) / fpeed[i]) * (cos8(fcount[i] + 2 * h ) - 128.0) / 128.0) = CHSV(h, 255, 255);
  }

  zeds.DrawFilledCircle( MIDLX  , MIDLY, dot  , CHSV(h, 255, 55 + 100 * dot / MIDLX));
}

void circlearc()// arc of circles
{
  if (counter % 888 == 0 || counter == 0)
  {

    bfade = random (2, 6);
    howmany = random (3, 6);
    radius2 = 64 / howmany;
    radius3 = random (MATRIX_WIDTH - (MATRIX_WIDTH >> 2), MATRIX_WIDTH + (MATRIX_WIDTH >> 2));
    poffset = random(1, 6);
    inner = random(5,  MIDLX / 2);
    // MM FLAGS: I don't like pattern 3 (filled circle within bigger circle)
    if (poffset == 3) poffset = 2;
    Serial.print("poffset ");
    Serial.println(poffset);
  }
  if ( counter % 20 == 0) {
    radius3 = radius3 + directn;

    if (radius3 <  MATRIX_WIDTH - (MATRIX_WIDTH >> 2) ||  radius3 > MATRIX_WIDTH + (MATRIX_WIDTH >> 2))
      directn = 0 - directn;
  }
  switch (poffset)
  {
    case 1:  // four -all headed in different direcitons
      for (int i = 0; i < howmany * 4; i++)
      {
        zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CHSV(h, 255, 255));
        //  zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner, CHSV(h  + 128, 255, 255));

        if ( flip2) {
          zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
          //   zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
        }
      }
      for (int i = 0; i < howmany * 4; i++)
      {
        //   zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CHSV(h, 255, 255));
        zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner, CHSV(h  + 128, 255, 255));

        if ( flip2) {
          //     zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
          zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
        }
      }
      break;

    case 2:// white 4 headed in dirrerent directions
      for (int i = 0; i < howmany * 4; i++)
      {
        if (flip2) {
          zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CHSV(h, 255, 255));
          //     zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner, CHSV(h  + 128, 255, 255));
          zeds.DrawCircle( radius3 * (sin8(i * radius2 - h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CRGB::White);
          //      zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner, CRGB::White);
        }
        zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
        //    zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));

        zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CRGB::White);
        //    zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CRGB::White);

      }
      for (int i = 0; i < howmany * 4; i++)
      {
        if (flip2) {
          //      zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CHSV(h, 255, 255));
          zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner, CHSV(h  + 128, 255, 255));
          //       zeds.DrawCircle( radius3 * (sin8(i * radius2 - h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CRGB::White);
          zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner, CRGB::White);
        }
        //     zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
        zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));

        //    zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CRGB::White);
        zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CRGB::White);

      }


      break;
    case 3:// some filled circles
      for (int i = 0; i < howmany * 4; i++)
      {
        zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner / 2, CHSV(h, 255, 255));
        //  zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner / 2, CHSV(h  + 128, 255, 255));
        if (flip2) {
          zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner , CHSV(h - 32, 255, 255));
          //   zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner , CHSV(h  + 128, 255, 255));
        }
        if (flip3) {
          zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner / 2, CHSV(h + 87, 255, 255));
          //     zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner / 2, CHSV(h  - 64, 255, 255));
          if (flip2)
          {
            zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 87 - 32, 255, 255));
            //     zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
          }
        }
      }
      for (int i = 0; i < howmany * 4; i++)
      {
        //   zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner / 2, CHSV(h, 255, 255));
        zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner / 2, CHSV(h  - 87, 255, 255));
        if (flip2) {
          //     zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner , CHSV(h, 255, 255));
          zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner , CHSV(h  - 87 - 32, 255, 255));
        }
        if (flip3) {
          //     zeds.DrawFilledCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner / 2, CHSV(h -87+32, 255, 255));
          zeds.DrawFilledCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner / 2, CHSV(h  - 87 + 32, 255, 255));
          if (flip2)
          {
            //     zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h -87 + 32, 255, 255));
            zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  + -87 - 32, 255, 255));
          }
        }
      }

      break;
    case 4:// 8  opposite directions  littles
      for (int i = 0; i < howmany * 4; i++)
      {


        zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner / 2, CHSV(h, 255, 255));
        //  zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner / 2, CHSV(h  + 128, 255, 255));

        if (flip) {
          zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner / 2, CHSV(h + 64, 255, 255));
          //  zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner / 2, CHSV(h  - 64, 255, 255));
        }
        else
        {
          zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
          //      zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
        }
      }
      for (int i = 0; i < howmany * 4; i++)
      {


        //   zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner / 2, CHSV(h, 255, 255));
        zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  - h % 64 + 128) - 128.0) / 128, inner / 2, CHSV(h  + 128, 255, 255));

        if (flip) {
          //     zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner / 2, CHSV(h + 64, 255, 255));
          zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner / 2, CHSV(h  - 64, 255, 255));
        }
        else
        {
          //       zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
          zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  -  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  - h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
        }
      }
      break;
    default:// four headed together
      for (int i = 0; i < howmany * 4; i++)
      {
        // zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CHSV(h, 255, 255));
        zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  +  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  + h % 64 + 128) - 128.0) / 128, inner, CHSV(h  + 128, 255, 255));
        if (flip2)
        {
          //      zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
          zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  +  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  + h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
        }
      }
      for (int i = 0; i < howmany * 4; i++)
      {
        zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64) - 128.0) / 128,   radius3 * (cos8(i * radius2 -  h % 64) - 128.0) / 128, inner, CHSV(h, 255, 255));
        //   zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  +  h % 64 + 128) - 128.0) / 128,  MATRIX_HEIGHT - radius3 * (cos8(i * radius2  + h % 64 + 128) - 128.0) / 128, inner, CHSV(h  + 128, 255, 255));
        if (flip2)
        {
          zeds.DrawCircle( radius3 * (sin8(i * radius2 -  h % 64 - 64) - 128.0) / 128,   MATRIX_HEIGHT - radius3 * (cos8(i * radius2 -  h % 64 - 64) - 128.0) / 128, inner, CHSV(h + 64, 255, 255));
          //    zeds.DrawCircle( MATRIX_WIDTH - radius3 * (sin8(i * radius2  +  h % 64 + 64) - 128.0) / 128,  radius3 * (cos8(i * radius2  + h % 64 + 64) - 128.0) / 128, inner, CHSV(h  - 64, 255, 255));
        }
      }

      break;
  }
}

void confetti() {
  if (random8() < 224)
    zeds.DrawCircle(random(MATRIX_WIDTH), random(MATRIX_HEIGHT), random(1, 7), CHSV(random8(), 255, 255));
  else
    zeds.DrawCircle(random(MATRIX_WIDTH), random(MATRIX_HEIGHT), random(1, 7), CHSV(h + 128 , 255, 255));
}

void confetti2() {
  if (random8() > blender)
    zeds(random(MATRIX_WIDTH), random(MATRIX_HEIGHT)) += CHSV(h + random(32) + 128, 255, 255);
  else
    zeds.DrawFilledCircle(random(MATRIX_WIDTH), random(MATRIX_HEIGHT), random(1, 9), CHSV(h + random(32) + 128 , 255, 255));
}

void Roundhole()// eff4
{
  solid();
  for (uint16_t y = 0; y < byte(MATRIX_HEIGHT < 255 ? MATRIX_HEIGHT : 255) ; y += dot)
  {
    zeds.DrawCircle(driftx, drifty, MATRIX_HEIGHT - y, CHSV(h * 2 + y * steper, 255, 255));

  }
}

void solid()
{
  if (counter == 0)
    rr = random8();
  zeds.DrawFilledRectangle(0 , 0,  MATRIX_WIDTH, MATRIX_HEIGHT, CHSV(rr + h, 255, 90));
}

// vim:sts=2:sw=2
