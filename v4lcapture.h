#include "neomatrix_config.h"

#define V4LCAPTURE_INCLUDE
#include "v4lcapture_single.h"

void yuv2rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    int16_t R, G, B;
    R = y				    + 1.370705 * (v-128);
    G = y		- 0.337633 * (u-128)- 0.698001 * (v-128) ;
    B = y		+ 1.732446 * (u-128);

    if (R > 255) R = 255;
    if (G > 255) G = 255;
    if (B > 255) B = 255;
    if (R < 0) R = 0;
    if (G < 0) G = 0;
    if (B < 0) B = 0;
#if 0
    // bring down brightness a bit, but this adds more math (slowdown)
    *r = R * 220 / 256;
    *g = G * 220 / 256;
    *b = B * 220 / 256;
#else
    *r = R;
    *g = G;
    *b = B;
#endif
}


// The code assumes mw<=CAPTURE_W and mh<CAPTURE_H, i.e. that the
// RGBPanel display is smaller than the camera capture.
uint16_t capt_offx = (CAPTURE_W - mw)/2;
uint16_t capt_offy = (CAPTURE_H - mh)/2;

void v4l2fb(bool mirror) {
    uint8_t y1, u, v, y2;
    uint8_t r, g, b;
    uint32_t i;
    uint16_t X;

    for (uint16_t y = 0; y<mh; y++) {
	for (uint16_t x = 0; x<mw; x+=2) {
	    i = ((y+capt_offy)*CAPTURE_W + capt_offx+x)*2;

    
	    // My USB camera outputs 'YUYV' (YUYV 4:2:2) which is indeed Y U Y2 V
	    // YUY2: "Known as YUY2, YUYV, V422 or YUNV"
	    // https://stackoverflow.com/questions/36228232/yuy2-vs-yuv-422
	    y1  = (uint8_t) *(buffer_.start+i+0);
	    u  = (uint8_t) *(buffer_.start+i+1);
	    y2 = (uint8_t) *(buffer_.start+i+2);
	    v  = (uint8_t) *(buffer_.start+i+3);

	    yuv2rgb(y1, u, v, &r, &g, &b);
	    X = x;
	    if (mirror) X = mw-X-1;
	    matrix->drawPixel(X, y, (uint32_t) ((r<<16) + (g<<8) + b));
	    yuv2rgb(y2, u, v, &r, &g, &b);
	    X = x+1;
	    if (mirror) X = mw-X-1;
	    matrix->drawPixel(X, y, (uint32_t) ((r<<16) + (g<<8) + b));
	}
    }
}


bool v4lcapture_loop(bool mirror)
{
    if (v4lfailed) return 1;
    mainloop();
    v4l2fb(mirror);
    matrix->show();
    return 0;
}

bool v4lcapture_setup()
{
    dev_name = "/dev/video0";
    open_device();
    init_device();
    start_capturing();
    return v4lfailed;
}
