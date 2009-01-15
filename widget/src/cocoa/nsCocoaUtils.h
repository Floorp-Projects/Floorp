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
#include "nsIWidget.h"
#include "nsObjCExceptions.h"

// "Borrowed" in part from the QTKit framework's QTKitDefines.h.  This is
// needed when building on OS X Tiger (10.4.X) or with a 10.4 SDK.  It won't
// be used when building on Leopard (10.5.X) or higher (or with a 10.5 or
// higher SDK).
//
// These definitions for NSInteger and NSUInteger are the 32-bit ones -- since
// we assume we'll always be building 32-bit binaries when building on Tiger
// (or with a 10.4 SDK).
#ifndef NSINTEGER_DEFINED

typedef int NSInteger;
typedef unsigned int NSUInteger;

#define NSIntegerMax    LONG_MAX
#define NSIntegerMin    LONG_MIN
#define NSUIntegerMax   ULONG_MAX

#define NSINTEGER_DEFINED 1

#endif  /* NSINTEGER_DEFINED */


// Used to retain a Cocoa object for the remainder of a method's execution.
class nsAutoRetainCocoaObject {
public:
nsAutoRetainCocoaObject(id anObject)
{
  mObject = NS_OBJC_TRY_EXPR_ABORT([anObject retain]);
}
~nsAutoRetainCocoaObject()
{
  NS_OBJC_TRY_ABORT([mObject release]);
}
private:
  id mObject;  // [STRONG]
};


@interface NSApplication (Undocumented)

// Present in all versions of OS X from (at least) 10.2.8 through 10.5.
- (BOOL)_isRunningModal;
- (BOOL)_isRunningAppModal;

// It's sometimes necessary to explicitly remove a window from the "window
// cache" in order to deactivate it.  The "window cache" is an undocumented
// subsystem, all of whose methods are included in the NSWindowCache category
// of the NSApplication class (in header files generated using class-dump).
// Present in all versions of OS X from (at least) 10.2.8 through 10.5.
- (void)_removeWindowFromCache:(NSWindow *)aWindow;

// Send an event to the current Cocoa app-modal session.  Present in all
// versions of OS X from (at least) 10.2.8 through 10.5.
- (void)_modalSession:(NSModalSession)aSession sendEvent:(NSEvent *)theEvent;

@end

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
  static NSRect GeckoRectToCocoaRect(const nsIntRect &geckoRect);
  
  // See explanation for geckoRectToCocoaRect, guess what this does...
  static nsIntRect CocoaRectToGeckoRect(const NSRect &cocoaRect);
  
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

  static nsIWidget* GetHiddenWindowWidget();

  static void PrepareForNativeAppModalDialog();
  static void CleanUpAfterNativeAppModalDialog();

  // Wrap calls to [theEvent keyCode] and [theEvent modifierFlags].  Needed to
  // work around an Apple bug (on OS X 10.4.X) that causes ctrl-ESC key events
  // sent via performKeyEquivalent: to return 0 on these calls.
  static unsigned short GetCocoaEventKeyCode(NSEvent *theEvent);
  static NSUInteger GetCocoaEventModifierFlags(NSEvent *theEvent);
};

#endif // nsCocoaUtils_h_
