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

// Returns the height (from lowest 'y' to highest 'y', regardless of sign) of
// the global coordinate system that includes all NSScreen objects.
float CocoaScreenCoordsHeight()
{
  float globalLowestY = 0;
  float globalHighestY = 0;
  NSArray *allScreens = [NSScreen screens];
  for (unsigned i = 0; i < [allScreens count]; ++i) {
    NSScreen *aScreen = (NSScreen*) [allScreens objectAtIndex:i];
    NSRect screenFrame = [aScreen frame];
    float screenLowestY = screenFrame.origin.y;
    float screenHighestY = screenFrame.origin.y + screenFrame.size.height;
    if (screenLowestY < globalLowestY)
      globalLowestY = screenLowestY;
    if (screenHighestY > globalHighestY)
      globalHighestY = screenHighestY;
  }
  return globalHighestY - globalLowestY;
}


NSRect geckoRectToCocoaRect(const nsRect &geckoRect)
{
  // We only need to change the Y coordinate by starting with the screen
  // height, subtracting the gecko Y coordinate, and subtracting the height.
  return NSMakeRect(geckoRect.x,
                    CocoaScreenCoordsHeight() - geckoRect.y - geckoRect.height,
                    geckoRect.width,
                    geckoRect.height);
}


nsRect cocoaRectToGeckoRect(const NSRect &cocoaRect)
{
  // We only need to change the Y coordinate by starting with the screen
  // height and subtracting both the cocoa y origin and the height of the
  // cocoa rect.
  return nsRect((nscoord)cocoaRect.origin.x,
                (nscoord)(CocoaScreenCoordsHeight() - (cocoaRect.origin.y + cocoaRect.size.height)),
                (nscoord)cocoaRect.size.width,
                (nscoord)cocoaRect.size.height);
}
