/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Sylvain Pasche <sylvain.pasche@gmail.com>
 *   Stuart Morgan <stuart.morgan@alumni.case.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCocoaUtils_h_
#define nsCocoaUtils_h_

#import <Cocoa/Cocoa.h>

#include "nsRect.h"

class nsCocoaUtils
{
  public:
  // Returns the height of the primary screen (the one with the menu bar, which
  // is documented to be the first in the |screens| array).
  static float MenuBarScreenHeight();

  // Returns the given y coordinate, which must be in screen coordinates,
  // flipped from Gecko to Cocoa or Cocoa to Gecko.
  static float FlippedScreenY(float y);
  
  // Gecko rects (nsRect) contain an origin (x,y) in a coordinate
  // system with (0,0) in the top-left of the primary screen. Cocoa rects
  // (NSRect) contain an origin (x,y) in a coordinate system with (0,0)
  // in the bottom-left of the primary screen. Both nsRect and NSRect
  // contain width/height info, with no difference in their use.
  static NSRect GeckoRectToCocoaRect(const nsRect &geckoRect);
  
  // See explanation for geckoRectToCocoaRect, guess what this does...
  static nsRect CocoaRectToGeckoRect(const NSRect &cocoaRect);
  
  // Gives the location for the event in screen coordinates. Do not call this
  // unless the window the event was originally targeted at is still alive!
  static NSPoint ScreenLocationForEvent(NSEvent* anEvent);
  
  // Determines if an event happened over a window, whether or not the event
  // is for the window. Does not take window z-order into account.
  static BOOL IsEventOverWindow(NSEvent* anEvent, NSWindow* aWindow);
  
  // Events are set up so that their coordinates refer to the window to which they
  // were originally sent. If we reroute the event somewhere else, we'll have
  // to get the window coordinates this way. Do not call this unless the window
  // the event was originally targeted at is still alive!
  static NSPoint EventLocationForWindow(NSEvent* anEvent, NSWindow* aWindow);
  
  // Finds the foremost window that is under the mouse for the current application.
  static NSWindow* FindWindowUnderPoint(NSPoint aPoint);  
};

#endif // nsCocoaUtils_h_
