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
#include "nsObjCExceptions.h"
#include "imgIContainer.h"
#include "nsEvent.h"
#include "npapi.h"

class nsIWidget;

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

// Provide a local autorelease pool for the remainder of a method's execution.
class nsAutoreleasePool {
public:
  nsAutoreleasePool()
  {
    mLocalPool = [[NSAutoreleasePool alloc] init];
  }
  ~nsAutoreleasePool()
  {
    [mLocalPool release];
  }
private:
  NSAutoreleasePool *mLocalPool;
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

// Present (and documented) on OS X 10.6 and above.  Not present before 10.6.
// This declaration needed to avoid compiler warnings when compiling on 10.5
// and below (or using the 10.5 SDK and below).
- (void)setHelpMenu:(NSMenu *)helpMenu;

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
  // anEvent may be nil -- in that case the current mouse location is returned.
  static NSPoint ScreenLocationForEvent(NSEvent* anEvent);
  
  // Determines if an event happened over a window, whether or not the event
  // is for the window. Does not take window z-order into account.
  static BOOL IsEventOverWindow(NSEvent* anEvent, NSWindow* aWindow);

  // Events are set up so that their coordinates refer to the window to which they
  // were originally sent. If we reroute the event somewhere else, we'll have
  // to get the window coordinates this way. Do not call this unless the window
  // the event was originally targeted at is still alive!
  static NSPoint EventLocationForWindow(NSEvent* anEvent, NSWindow* aWindow);

  static BOOL IsMomentumScrollEvent(NSEvent* aEvent);

  // Hides the Menu bar and the Dock. Multiple hide/show requests can be nested.
  static void HideOSChromeOnScreen(bool aShouldHide, NSScreen* aScreen);

  static nsIWidget* GetHiddenWindowWidget();

  static void PrepareForNativeAppModalDialog();
  static void CleanUpAfterNativeAppModalDialog();

  // 3 utility functions to go from a frame of imgIContainer to CGImage and then to NSImage
  // Convert imgIContainer -> CGImageRef, caller owns result
  
  /** Creates a <code>CGImageRef</code> from a frame contained in an <code>imgIContainer</code>.
      Copies the pixel data from the indicated frame of the <code>imgIContainer</code> into a new <code>CGImageRef</code>.
      The caller owns the <code>CGImageRef</code>. 
      @param aFrame the frame to convert
      @param aResult the resulting CGImageRef
      @return NS_OK if the conversion worked, NS_ERROR_FAILURE otherwise
   */
  static nsresult CreateCGImageFromSurface(gfxImageSurface *aFrame, CGImageRef *aResult);
  
  /** Creates a Cocoa <code>NSImage</code> from a <code>CGImageRef</code>.
      Copies the pixel data from the <code>CGImageRef</code> into a new <code>NSImage</code>.
      The caller owns the <code>NSImage</code>. 
      @param aInputImage the image to convert
      @param aResult the resulting NSImage
      @return NS_OK if the conversion worked, NS_ERROR_FAILURE otherwise
   */
  static nsresult CreateNSImageFromCGImage(CGImageRef aInputImage, NSImage **aResult);

  /** Creates a Cocoa <code>NSImage</code> from a frame of an <code>imgIContainer</code>.
      Combines the two methods above. The caller owns the <code>NSImage</code>.
      @param aImage the image to extract a frame from
      @param aWhichFrame the frame to extract (see imgIContainer FRAME_*)
      @param aResult the resulting NSImage
      @return NS_OK if the conversion worked, NS_ERROR_FAILURE otherwise
   */  
  static nsresult CreateNSImageFromImageContainer(imgIContainer *aImage, PRUint32 aWhichFrame, NSImage **aResult);

  /**
   * Returns nsAString for aSrc.
   */
  static void GetStringForNSString(const NSString *aSrc, nsAString& aDist);

  /**
   * Makes NSString instance for aString.
   */
  static NSString* ToNSString(const nsAString& aString);

  /**
   * Returns NSRect for aGeckoRect.
   */
  static void GeckoRectToNSRect(const nsIntRect& aGeckoRect,
                                       NSRect& aOutCocoaRect);

  /**
   * Makes NSEvent instance for aEventTytpe and aEvent.
   */
  static NSEvent* MakeNewCocoaEventWithType(NSEventType aEventType,
                                            NSEvent *aEvent);

  /**
   * Initializes aNPCocoaEvent.
   */
  static void InitNPCocoaEvent(NPCocoaEvent* aNPCocoaEvent);

  /**
   * Initializes aPluginEvent for aCocoaEvent.
   */
  static void InitPluginEvent(nsPluginEvent &aPluginEvent,
                              NPCocoaEvent &aCocoaEvent);
  /**
   * Initializes nsInputEvent for aNativeEvent or aModifiers.
   */
  static void InitInputEvent(nsInputEvent &aInputEvent,
                             NSEvent* aNativeEvent);
  static void InitInputEvent(nsInputEvent &aInputEvent,
                             NSUInteger aModifiers);
};

#endif // nsCocoaUtils_h_
