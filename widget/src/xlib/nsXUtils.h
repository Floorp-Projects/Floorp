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

#ifndef __nsXUtils_h
#define __nsXUtils_h

#include <X11/Xlib.h>

struct nsXUtils
{
  /**
   * Flash an area within a window (or the whole window)
   *
   * @param aDisplay     The display.
   * @param aWindow      The XID of the window to flash.
   * @param aTimes       Number of times to flash the area.
   * @param aInterval    Interval between flashes in milliseconds.
   * @param aArea        The area to flash.  The whole window if NULL.
   *
   */
  static void XFlashWindow(Display *       aDisplay,
                           Window          aWindow,
                           unsigned int    aTimes,
                           unsigned long   aInterval,
                           XRectangle *    aArea);
};

#endif  // __nsXUtils_h
