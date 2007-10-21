/************************************************************************
 *
 * File: xinput.c
 *
 * Sample program to access input devices other than the X pointer and
 * keyboard using the Input Device extension to X.
 * This program creates a window and selects input from it.
 * To terminate this program, press button 1 on any device being accessed
 * through the extension when the X pointer is in the test window.
 *
 * To compile this program, use
 * "cc xinput.c -I/usr/include/X11R5 -L/usr/lib/X11R5
 * -lXi -lXext -lX11 -o xinput
 */
 
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include "stdio.h"
 
main()
{
    int   i, j, count, ndevices, devcnt=0, devkeyp, devbutp;
    Display  *display;
    Window  my;
    XEvent  event;
    XDeviceInfoPtr list, slist;
    XInputClassInfo *ip;
    XDeviceButtonEvent *b;
    XEventClass  class[128];
    XDevice  *dev, *opendevs[9];
    XAnyClassPtr any;
    XKeyInfoPtr  K;
 
    if ((display = XOpenDisplay ("")) == NULL)
    {
        printf ("No connection to server - Terminating.\n");
        exit(1);
    }
    my = XCreateSimpleWindow (display, RootWindow(display,0), 100, 100,
                              100, 100, 1, BlackPixel(display,0), WhitePixel(display,0));
    XMapWindow (display, my);
    XSync(display,0);
 
    slist=list=(XDeviceInfoPtr) XListInputDevices (display, &ndevices);
    for (i=0; i<ndevices; i++, list++)
    {
        any = (XAnyClassPtr) (list->inputclassinfo);
        for (j=0; j<list->num_classes; j++)
        {
            if (any->class == KeyClass)
            {
                K = (XKeyInfoPtr) any;
                printf ("device %s:\n",list->name);
                printf ("num_keys=%d min_keycode=%d max_keycode=%d\n\n",
                        K->num_keys,K->min_keycode,K->max_keycode);
            }
            else if (any->class == ButtonClass)
                printf ("device %s num_buttons=%d\n\n",list->name,
                        ((XButtonInfoPtr) any)->num_buttons);
            /*
             * Increment 'any' to point to the next item in the linked
             * list.  The length is in bytes, so 'any' must be cast to
             * a character pointer before being incremented.
             */
            any = (XAnyClassPtr) ((char *) any + any->length);
        }
        if (1) //list->use != IsXKeyboard &&list->use != IsXPointer)
        {
            dev = XOpenDevice (display, list->id);
            for (ip= dev->classes, j=0; j<dev->num_classes; j++, ip++)
                if (ip->input_class == KeyClass)
                {
                    /* This is a macro, the braces are necessary */
                    DeviceKeyPress (dev, devkeyp, class[count++]);
                }
                else if (ip->input_class == ButtonClass)
                {
                    DeviceButtonPress (dev, devbutp,class[count++]);
                }
            opendevs[devcnt++]=dev;
        }
    }
    XSelectExtensionEvent (display, my, class, count);
    for (;;)
    {
        XNextEvent (display, &event);
        if (event.type == devkeyp)
            printf ("Device key press event device=%d\n",
                    ((XDeviceKeyEvent *) &event)->deviceid);
        else if (event.type == devbutp)
        {
            b = (XDeviceButtonEvent * ) &event;
            printf ("Device button press event device=%d button=%d\n",
                    b->deviceid, b->button);
            if (b->button==1)
                break;
        }
    }
    for (i=0; i<devcnt; i++)
        XCloseDevice (display, opendevs[i]);
    XFreeDeviceList (slist);
}
