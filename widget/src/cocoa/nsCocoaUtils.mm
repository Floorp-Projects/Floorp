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

#include "nsCocoaUtils.h"


float nsCocoaUtils::MenuBarScreenHeight()
{
  NSArray* allScreens = [NSScreen screens];
  if ([allScreens count])
    return [[allScreens objectAtIndex:0] frame].size.height;
  else
    return 0; // If there are no screens, there's not much we can say.
}


float nsCocoaUtils::FlippedScreenY(float y)
{
  return MenuBarScreenHeight() - y;
}


NSRect nsCocoaUtils::GeckoRectToCocoaRect(const nsRect &geckoRect)
{
  // We only need to change the Y coordinate by starting with the primary screen
  // height, subtracting the gecko Y coordinate, and subtracting the height.
  return NSMakeRect(geckoRect.x,
                    MenuBarScreenHeight() - (geckoRect.y + geckoRect.height),
                    geckoRect.width,
                    geckoRect.height);
}


nsRect nsCocoaUtils::CocoaRectToGeckoRect(const NSRect &cocoaRect)
{
  // We only need to change the Y coordinate by starting with the primary screen
  // height and subtracting both the cocoa y origin and the height of the
  // cocoa rect.
  return nsRect((nscoord)cocoaRect.origin.x,
                (nscoord)(MenuBarScreenHeight() - (cocoaRect.origin.y + cocoaRect.size.height)),
                (nscoord)cocoaRect.size.width,
                (nscoord)cocoaRect.size.height);
}


NSPoint nsCocoaUtils::ScreenLocationForEvent(NSEvent* anEvent)
{
  return [[anEvent window] convertBaseToScreen:[anEvent locationInWindow]];
}


BOOL nsCocoaUtils::IsEventOverWindow(NSEvent* anEvent, NSWindow* aWindow)
{
  return NSPointInRect(ScreenLocationForEvent(anEvent), [aWindow frame]);
}


NSPoint nsCocoaUtils::EventLocationForWindow(NSEvent* anEvent, NSWindow* aWindow)
{
  return [aWindow convertScreenToBase:ScreenLocationForEvent(anEvent)];
}


NSWindow* nsCocoaUtils::FindWindowUnderPoint(NSPoint aPoint)
{
  int windowCount;
  NSCountWindows(&windowCount);
  int* windowList = (int*)malloc(sizeof(int) * windowCount);
  if (!windowList)
    return nil;
  // The list we get back here is in order from front to back.
  NSWindowList(windowCount, windowList);

  for (int i = 0; i < windowCount; i++) {
    NSWindow* currentWindow = [NSApp windowWithWindowNumber:windowList[i]];
    if (currentWindow && NSPointInRect(aPoint, [currentWindow frame])) {
      free(windowList);
      return currentWindow;
    }
  }

  free(windowList);
  return nil;
}
