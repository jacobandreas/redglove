
#include "cv.h"
#include "highgui.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

// The width of the window
#define WIDTH  640
#define HEIGHT 480

// The area around the last area where we will search for red pixels
#define SEARCHRANGE 100

// The mouse is down
#define STATE_UP   0
// The mouse is up
#define STATE_DOWN 1

// The user's hand is not changing size
#define CHANGE_NONE 0
// The user is closing his hand
#define CHANGE_SHRINKING 1
// The user is opening his hand
#define CHANGE_GROWING 2

// How much the size of the red region needs to change for that change to be
// registered
#define CHANGEFACTOR .75

// Where to start the search from
int searchx = -1;
int searchy = -1;

// How big to make the starting region
int startw = 100;
int starth = 100;

// How big the last starting region was
int lasth = 100;
int lastw = 100;

// How is the size of the user's hand currently changing
int shapechange = CHANGE_NONE;

CvPoint topleft;
CvPoint bottomright;

// What state is the mouse in now
int state;

// Needed for OpenCV
template<class T> class Image {

        private:
        IplImage *imgp;

        public:
        Image (IplImage *img=0) {imgp = img;}
        ~Image () {imgp = 0;}
        void operator=(IplImage *img) {imgp=img;}
        inline T* operator[] (const int rowIndex) {
                return ((T*)(imgp->imageData + rowIndex*imgp->widthStep));
        }
};

typedef struct {
        unsigned char b,g,r;
} RgbPixel;
typedef struct {
        float b,g,r;
} RgbPixelFloat;

typedef Image<RgbPixel> RgbImage;

// in case I want to add mouse handling to the window later
void on_mouse( int event, int x, int y, int flags, void* param )
{
}

// Fill in the red region and identify its center of gravity
void track_red (IplImage *frame, Display *dpy) {

        int xmin, xmax, ymin, ymax;

        // the boundaries of the red region
        int top = HEIGHT, bottom = -1, left = WIDTH, right = -1;

        // if we didn't see any red in the last frame, look everywhere
        if (searchx == -1 || searchy == -1) {
               xmin = 0;
               ymin = 0;
               xmax = WIDTH;
               ymax = HEIGHT;
        } 
        // otherwise only look around the region where we last saw it
        else {
                xmin = MAX (0, searchx - SEARCHRANGE);
                xmax = MIN (WIDTH, searchx + SEARCHRANGE);
                ymin = MAX (0, searchy - SEARCHRANGE);
                ymax = MIN (HEIGHT, searchy + SEARCHRANGE);
        }


        // search for red pixels
        
        RgbImage image (frame);

        int redxt = 0, redyt = 0;
        int count = 0;

        for (int i = ymin; i < ymax; i++) {
        for (int j = xmin; j < xmax; j++) {
                int r = image[i][j].r;
                int g = image[i][j].g;
                int b = image[i][j].b;
                if (g < 0x20 && b < 0x20 && r > 0x80) {
                        image[i][j].r=0xFF;
                        image[i][j].g=0x00;
                        image[i][j].b=0x00;
                        redyt += i;
                        redxt += j;
                        count++;

                        top = MIN (top, i);
                        bottom = MAX (bottom, i);
                        left = MIN (left, j);
                        right = MAX (right, j);

                }

        }}

        // if we found enough to make it worth our while, deal with the mouse

        if (count) {

                searchx = redxt / count;
                searchy = redyt / count;

                float redx = (float) redxt / count / WIDTH;
                float redy = (float) redyt / count / HEIGHT;

                int screenx = (int)(redx * 1480.0 - 100);
                int screeny = (int)(redy * 1000.0 - 100);

                // move the mouse to the red region's center of gravity
                XTestFakeMotionEvent (dpy, DefaultScreen (dpy), screenx, screeny, 0);

                topleft = cvPoint (left, top);
                bottomright = cvPoint (right, bottom);


                int thish = bottom - top;
                int thisw = right - left; 

                // detect whether the user is opening or closing his fist
                if (shapechange && abs(lasth - thish) < lasth * 1 - CHANGEFACTOR) {

                        int oldstate = state;

                        if (shapechange == CHANGE_GROWING)
                                state = STATE_UP;
                        else
                                state = STATE_DOWN;

                        // only fire events if we're transitioning between
                        // states
                        if(oldstate == STATE_UP && state == STATE_DOWN)
                        {
                            XTestFakeButtonEvent (dpy, 1, 1, 0);
                        }
                        else if(oldstate == STATE_DOWN && state == STATE_UP)
                        {
                            XTestFakeButtonEvent (dpy, 1, 0, 0);
                        }
                }

                if (thish < lasth * CHANGEFACTOR) shapechange = CHANGE_SHRINKING;
                else if (thish * CHANGEFACTOR > lasth) shapechange = CHANGE_GROWING;
                else shapechange = CHANGE_NONE;

                lasth = thish;
                lastw = thisw;

                XFlush (dpy);

        } else {
                searchx = -1;
                searchy = -1;
        }

}

int main( int argc, char** argv )
{

    Display *dpy = XOpenDisplay (0);

    CvCapture* capture = cvCaptureFromCAM(0);
    IplImage *frame = 0;
    
    // set up the camera and windows
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, WIDTH);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, HEIGHT);

    if( !capture )
    {
        fprintf(stderr,"Could not initialize capturing...\n");
        return -1;
    }

    cvNamedWindow( "CamShiftDemo", 1 );
    cvSetMouseCallback( "CamShiftDemo", on_mouse, 0 );
    
    frame = cvQueryFrame( capture );

    int step = frame->widthStep / sizeof (uchar);

    // grab frames forever
    for(;;)
    {

        // grab the image
        frame = cvQueryFrame( capture );

        // try again later if we weren't successful
        if( !frame )
            break;

        // mirror it
        cvFlip (frame, 0, 1);

        track_red (frame, dpy);

        // draw a box around the detected red region
        cvRectangle (frame, topleft, bottomright, cvScalar(255,255,255), 3-4*state, 8, 0);

        cvShowImage ("CamShiftDemo", frame);

        // close if they press escape
        char c = cvWaitKey(10);

    }

    // clean up
    cvReleaseCapture( &capture );
    cvDestroyWindow("CamShiftDemo");

    return 0;
}

