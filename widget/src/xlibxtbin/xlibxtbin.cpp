/* widget -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XlibXtBin Implementation
 * Peter Hartshorn
 * Based on GtkXtBin by Rusty Lynch - 02/27/2000
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */    

#include "xlibxtbin.h"
#include "xlibrgb.h"

#include <stdlib.h>
#include <stdio.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Shell.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

xtbin::xtbin() {
  initialized = 0;
  xtwindow = 0;
}

xtbin::~xtbin() {
}

void xtbin::xtbin_init() {
  initialized = 1;
  app_context = XtDisplayToApplicationContext(xlib_rgb_get_display());
  xtdisplay = xlib_rgb_get_display();
}

void xtbin::xtbin_realize() {
  Arg args[2];
  int n;
  Widget top_widget;
  Widget embeded;
  XSetWindowAttributes attr;
  unsigned long mask;

  attr.bit_gravity = NorthWestGravity;
  attr.event_mask =     ButtonMotionMask |
    ButtonPressMask |
    ButtonReleaseMask |
    EnterWindowMask |
    ExposureMask |
    KeyPressMask |
    KeyReleaseMask |
    LeaveWindowMask |
    PointerMotionMask |
    StructureNotifyMask |
    VisibilityChangeMask |
    FocusChangeMask;

  mask = CWBitGravity | CWEventMask;

  window = XCreateWindow(xlib_rgb_get_display(), parent_window,
                         x, y, width, height, 0, CopyFromParent,
                         CopyFromParent, CopyFromParent,
                         mask, &attr);

  XSelectInput(xlib_rgb_get_display(), window, ExposureMask);

  XMapWindow(xlib_rgb_get_display(), window);
  XFlush(xlib_rgb_get_display());

  top_widget = XtAppCreateShell("drawingArea", "Wrapper",
                                applicationShellWidgetClass, xtdisplay,
                                NULL, 0);

  xtwidget = top_widget;

  n = 0;
  XtSetArg(args[n], XtNheight, height); n++;
  XtSetArg(args[n], XtNwidth, width); n++;
  XtSetValues(top_widget, args, n);

  embeded = XtVaCreateWidget("form", compositeWidgetClass, top_widget, NULL);

  n = 0;
  XtSetArg(args[n], XtNheight, height); n++;
  XtSetArg(args[n], XtNwidth, width); n++;
  XtSetValues(embeded, args, n);

  oldwindow = top_widget->core.window;
  top_widget->core.window = window;

  XtRegisterDrawable(xtdisplay, window, top_widget);

  XtRealizeWidget(embeded);
  XtRealizeWidget(top_widget);
  XtManageChild(embeded);

  xtwindow = XtWindow(embeded);

  XSelectInput(xtdisplay, XtWindow(top_widget), 0x0fffff);
  XSelectInput(xtdisplay, XtWindow(embeded), 0x0fffff);

  XFlush(xtdisplay);

}

void xtbin::xtbin_new(Window aParent) {
  parent_window = aParent;
}

void xtbin::xtbin_destroy() {
  XtDestroyWidget(xtwidget);
  initialized = 0;
}

void xtbin::xtbin_resize(int aX, int aY, int aWidth, int aHeight) {
  x = aX;
  y = aY;
  width = aWidth;
  height = aHeight;
}

int xtbin::xtbin_initialized() {
  return initialized;
}
