/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Hack to compensate for the old (<1.5) Xrandr development headers on
// Mozilla's build boxes.

#ifndef _XRANDR_H_WRAPPER_HACK_
#define _XRANDR_H_WRAPPER_HACK_

#include_next <X11/extensions/Xrandr.h>

#if RANDR_MAJOR == 1 && RANDR_MINOR < 5 // defined in randr.h
typedef struct _XRRMonitorInfo {
    Atom name;
    Bool primary;
    Bool automatic;
    int noutput;
    int x;
    int y;
    int width;
    int height;
    int mwidth;
    int mheight;
    RROutput *outputs;
} XRRMonitorInfo;
#endif

#endif // _XRANDR_H_WRAPPER_HACK_
