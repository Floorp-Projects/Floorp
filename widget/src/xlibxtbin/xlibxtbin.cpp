/* widget -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2
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
  initialized = False;
  xtwindow    = None;
}

xtbin::~xtbin() {
}

void xtbin::xtbin_init() {
  mXlibRgbHandle = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE);
  xtdisplay = xxlib_rgb_get_display(mXlibRgbHandle);
  if (!xtdisplay)
    abort();
  app_context = XtDisplayToApplicationContext(xtdisplay);
  initialized = True;
}

void xtbin::xtbin_realize() {
  Arg args[2];
  int n;
  Widget top_widget;
  Widget embeded;
  XSetWindowAttributes attr;
  unsigned long mask;

  attr.bit_gravity = NorthWestGravity;
  attr.event_mask = 
    ButtonMotionMask |
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

  attr.colormap         = xxlib_rgb_get_cmap(mXlibRgbHandle);
  attr.background_pixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, 0xFFC0C0C0);
  attr.border_pixel     = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, 0xFF646464);

#ifdef DEBUG  
  printf("attr.background_pixel = %lx, attr.border_pixel = %lx, parent_window = %x\n", 
         (long)attr.background_pixel,
         (long)attr.border_pixel, (int)parent_window);
#endif /* DEBUG */
  
  mask = CWBitGravity | CWEventMask | CWBorderPixel | CWBackPixel;

  if (attr.colormap)
    mask |= CWColormap;

  window = XCreateWindow(xtdisplay, parent_window,
                         x, y, width, height, 0, 
                         xxlib_rgb_get_depth(mXlibRgbHandle),
                         InputOutput, xxlib_rgb_get_visual(mXlibRgbHandle),
                         mask, &attr);
  XSetWindowBackgroundPixmap(xtdisplay, window, None);
  XSelectInput(xtdisplay, window, ExposureMask);

  XMapWindow(xtdisplay, window);
  XFlush(xtdisplay);

  top_widget = XtAppCreateShell("drawingArea", "Wrapper",
                                applicationShellWidgetClass, xtdisplay,
                                NULL, 0);

  xtwidget = top_widget;

  n = 0;
  XtSetArg(args[n], XtNheight, height); n++;
  XtSetArg(args[n], XtNwidth,  width);  n++;
  XtSetValues(top_widget, args, n);

  embeded = XtVaCreateWidget("form", compositeWidgetClass, top_widget, NULL);

  n = 0;
  XtSetArg(args[n], XtNheight, height); n++;
  XtSetArg(args[n], XtNwidth,  width);  n++;
  XtSetValues(embeded, args, n);

  oldwindow = top_widget->core.window;
  top_widget->core.window = window;

  XtRegisterDrawable(xtdisplay, window, top_widget);

  XtRealizeWidget(embeded);
  XtRealizeWidget(top_widget);
  XtManageChild(embeded);

  /* Now fill out the xtbin info */
  xtwindow = XtWindow(embeded);

  /* Suppress background refresh flashing */
  XSetWindowBackgroundPixmap(xtdisplay, XtWindow(top_widget), None);
  XSetWindowBackgroundPixmap(xtdisplay, XtWindow(embeded),    None);

  /* Listen to all Xt events */
  XSelectInput(xtdisplay, XtWindow(top_widget), 0x0fffff);
  XSelectInput(xtdisplay, XtWindow(embeded),    0x0fffff);

  sync();
}

void xtbin::xtbin_new(Window aParent) {
  parent_window = aParent;
}

void xtbin::sync() {
  /* is this really all ? */
  XSync(xtdisplay, False);
}

void xtbin::xtbin_destroy() {
  sync();
  XtUnregisterDrawable(xtdisplay, xtwindow);
  sync();
  xtwidget->core.window = oldwindow;
  XtUnrealizeWidget(xtwidget);
  initialized = False;
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

