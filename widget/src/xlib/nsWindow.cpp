/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsWindow.h"



nsWindow::nsWindow() : nsWidget()
{
  NS_INIT_REFCNT();
  name = "nsWindow";
}

nsWindow::~nsWindow()
{
}

void
nsWindow::DestroyNative(void)
{
  if (mGC)
    XFreeGC(gDisplay, mGC);
  if (mBaseWindow) {
    XDestroyWindow(gDisplay, mBaseWindow);
    DeleteWindowCallback(mBaseWindow);
  }
}


void
nsWindow::CreateNative(Window aParent, nsRect aRect)
{
  XSetWindowAttributes attr;
  unsigned long        attr_mask;
  int width;
  int height;
  // on a window resize, we don't want to window contents to
  // be discarded...
  attr.bit_gravity = NorthWestGravity;
  // make sure that we listen for events
  attr.event_mask = SubstructureNotifyMask | StructureNotifyMask | ExposureMask;
  // set the default background color and border to that awful gray
  attr.background_pixel = bg_pixel;
  attr.border_pixel = border_pixel;
  // set the colormap
  attr.colormap = xlib_rgb_get_cmap();
  // here's what's in the struct
  attr_mask = CWBitGravity | CWEventMask | CWBackPixel | CWBorderPixel;
  // check to see if there was actually a colormap.
  if (attr.colormap)
    attr_mask |= CWColormap;

  printf("Creating XWindow: x %d y %d w %d h %d\n",
         aRect.x, aRect.y, aRect.width, aRect.height);
  if (aRect.width == 0) {
    printf("*** Fixing width...\n");
    width = 1;
  }
  else {
    width = aRect.width;
  }
  if (aRect.height == 0) {
    printf("*** Fixing height...\n");
    height = 1;
  }
  else {
    height = aRect.height;
  }
  
  mBaseWindow = XCreateWindow(gDisplay,
                              aParent,
                              aRect.x, aRect.y,
                              width, height,
                              0, // border width
                              gDepth,
                              InputOutput,    // class
                              gVisual,        // get the visual from xlibrgb
                              attr_mask,
                              &attr);
  // add the callback for this
  AddWindowCallback(mBaseWindow, this);
  // map this window and flush the connection.  we want to see this
  // thing now.
  XMapWindow(gDisplay,
             mBaseWindow);
  XSync(gDisplay, False);

}

ChildWindow::ChildWindow(): nsWindow()
{
  name = "nsChildWindow";
}
