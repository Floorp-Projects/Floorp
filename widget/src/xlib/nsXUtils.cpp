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

#include "nsXUtils.h"

#include <unistd.h>
#include <string.h>

//////////////////////////////////////////////////////////////////
/* static */ void
nsXUtils::FlashWindow(Display *     display,
                      Window        window,
                      unsigned int  times,    /* Number of times to flash */
                      unsigned long interval) /* Interval between flashes */
{
	Window       root_window;
	Window       child_window;
	GC           gc;
	int          x;
	int          y;
	unsigned int width;
	unsigned int height;
	unsigned int border_width;
	unsigned int depth;
	int          root_x;
	int          root_y;
	unsigned int i;

	XGCValues    gcv;

	XGetGeometry(display,
				 window,
				 &root_window,
				 &x,
				 &y,
				 &width,
				 &height,
				 &border_width,
				 &depth);

	XTranslateCoordinates(display, 
						  window,
						  root_window, 
						  0, 
						  0,
						  &root_x,
						  &root_y,
						  &child_window);

    memset(&gcv, 0, sizeof(XGCValues));

	gcv.function = GXxor;
	gcv.foreground = WhitePixel(display, DefaultScreen(display));
	gcv.subwindow_mode = IncludeInferiors;

	if (gcv.foreground == 0)
		gcv.foreground = 1;

	gc = XCreateGC(display,
				   root_window,
				   GCFunction | GCForeground | GCSubwindowMode, 
				   &gcv);

	XGrabServer(display);

	for (i = 0; i < times; i++)
	{
		XFillRectangle(display,
					   root_window,
					   gc,
					   root_x,
					   root_y,
					   width,
					   height);
		
		XSync(display, False);

		usleep(interval);
	}
							
		
	XFreeGC(display, gc);  

	XUngrabServer(display);
}
/*****************************************************************/
