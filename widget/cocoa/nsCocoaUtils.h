/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaUtils_h_
#define nsCocoaUtils_h_

#import <Cocoa/Cocoa.h>

#include "nsRect.h"
#include "imgIContainer.h"
#include "npapi.h"
#include "nsTArray.h"

// This must be the last include:
#include "nsObjCExceptions.h"

#include "mozilla/EventForwards.h"

// Declare the backingScaleFactor method that we want to call
// on NSView/Window/Screen objects, if they recognize it.
@interface NSObject (BackingScaleFactorCategory)
- (CGFloat)backingScaleFactor;
@end

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

struct KeyBindingsCommand
{
  SEL selector;
  id data;
};

@interface NativeKeyBindingsRecorder : NSResponder
{
@private
  nsTArray<KeyBindingsCommand>* mCommands;
}

- (void)startRecording:(nsTArray<KeyBindingsCommand>&)aCommands;

- (void)doCommandBySelector:(SEL)aSelector;

- (void)insertText:(id)aString;

@end // NativeKeyBindingsRecorder

class nsCocoaUtils
{
  public:

  // Get the backing scale factor from an object that supports this selector
  // (NSView/Window/Screen, on 10.7 or later), returning 1.0 if not supported
  static CGFloat
  GetBackingScaleFactor(id aObject)
  {
    if (HiDPIEnabled() &&
        [aObject respondsToSelector:@selector(backingScaleFactor)]) {
      return [aObject backingScaleFactor];
    }
    return 1.0;
  }

  // Conversions between Cocoa points and device pixels, given the backing
  // scale factor from a view/window/screen.
  static int32_t
  CocoaPointsToDevPixels(CGFloat aPts, CGFloat aBackingScale)
  {
    return NSToIntRound(aPts * aBackingScale);
  }

  static nsIntPoint
  CocoaPointsToDevPixels(const NSPoint& aPt, CGFloat aBackingScale)
  {
    return nsIntPoint(NSToIntRound(aPt.x * aBackingScale),
                      NSToIntRound(aPt.y * aBackingScale));
  }

  static nsIntRect
  CocoaPointsToDevPixels(const NSRect& aRect, CGFloat aBackingScale)
  {
    return nsIntRect(NSToIntRound(aRect.origin.x * aBackingScale),
                     NSToIntRound(aRect.origin.y * aBackingScale),
                     NSToIntRound(aRect.size.width * aBackingScale),
                     NSToIntRound(aRect.size.height * aBackingScale));
  }

  static CGFloat
  DevPixelsToCocoaPoints(int32_t aPixels, CGFloat aBackingScale)
  {
    return (CGFloat)aPixels / aBackingScale;
  }

  static NSPoint
  DevPixelsToCocoaPoints(const nsIntPoint& aPt, CGFloat aBackingScale)
  {
    return NSMakePoint((CGFloat)aPt.x / aBackingScale,
                       (CGFloat)aPt.y / aBackingScale);
  }

  static NSRect
  DevPixelsToCocoaPoints(const nsIntRect& aRect, CGFloat aBackingScale)
  {
    return NSMakeRect((CGFloat)aRect.x / aBackingScale,
                      (CGFloat)aRect.y / aBackingScale,
                      (CGFloat)aRect.width / aBackingScale,
                      (CGFloat)aRect.height / aBackingScale);
  }

  // Returns the given y coordinate, which must be in screen coordinates,
  // flipped from Gecko to Cocoa or Cocoa to Gecko.
  static float FlippedScreenY(float y);

  // The following functions come in "DevPix" variants that work with
  // backing-store (device pixel) coordinates, as well as the original
  // versions that expect coordinates in Cocoa points/CSS pixels.
  // The difference becomes important in HiDPI display modes, where Cocoa
  // points and backing-store pixels are no longer 1:1.

  // Gecko rects (nsRect) contain an origin (x,y) in a coordinate
  // system with (0,0) in the top-left of the primary screen. Cocoa rects
  // (NSRect) contain an origin (x,y) in a coordinate system with (0,0)
  // in the bottom-left of the primary screen. Both nsRect and NSRect
  // contain width/height info, with no difference in their use.
  // This function does no scaling, so the Gecko coordinates are
  // expected to be CSS pixels, which we treat as equal to Cocoa points.
  static NSRect GeckoRectToCocoaRect(const nsIntRect &geckoRect);

  // Converts aGeckoRect in dev pixels to points in Cocoa coordinates
  static NSRect GeckoRectToCocoaRectDevPix(const nsIntRect &aGeckoRect,
                                           CGFloat aBackingScale);

  // See explanation for geckoRectToCocoaRect, guess what this does...
  static nsIntRect CocoaRectToGeckoRect(const NSRect &cocoaRect);

  static nsIntRect CocoaRectToGeckoRectDevPix(const NSRect &aCocoaRect,
                                              CGFloat aBackingScale);

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
  static nsresult CreateNSImageFromImageContainer(imgIContainer *aImage, uint32_t aWhichFrame, NSImage **aResult);

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
   * Just copies values between the two types; it does no coordinate-system
   * conversion, so both rects must have the same coordinate origin/direction.
   */
  static void GeckoRectToNSRect(const nsIntRect& aGeckoRect,
                                NSRect& aOutCocoaRect);

  /**
   * Returns Gecko rect for aCocoaRect.
   * Just copies values between the two types; it does no coordinate-system
   * conversion, so both rects must have the same coordinate origin/direction.
   */
  static void NSRectToGeckoRect(const NSRect& aCocoaRect,
                                nsIntRect& aOutGeckoRect);

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
  static void InitPluginEvent(mozilla::WidgetPluginEvent &aPluginEvent,
                              NPCocoaEvent &aCocoaEvent);
  /**
   * Initializes nsInputEvent for aNativeEvent or aModifiers.
   */
  static void InitInputEvent(nsInputEvent &aInputEvent,
                             NSEvent* aNativeEvent);
  static void InitInputEvent(nsInputEvent &aInputEvent,
                             NSUInteger aModifiers);

  /**
   * ConvertToCarbonModifier() returns carbon modifier flags for the cocoa
   * modifier flags.
   * NOTE: The result never includes right*Key.
   */
  static UInt32 ConvertToCarbonModifier(NSUInteger aCocoaModifier);

  /**
   * Whether to support HiDPI rendering. For testing purposes, to be removed
   * once we're comfortable with the HiDPI behavior.
   */
  static bool HiDPIEnabled();

  /**
   * Keys can optionally be bound by system or user key bindings to one or more
   * commands based on selectors. This collects any such commands in the
   * provided array.
   */
  static void GetCommandsFromKeyEvent(NSEvent* aEvent,
                                      nsTArray<KeyBindingsCommand>& aCommands);

  /**
   * Converts the string name of a Gecko key (like "VK_HOME") to the
   * corresponding Cocoa Unicode character.
   */
  static uint32_t ConvertGeckoNameToMacCharCode(const nsAString& aKeyCodeName);

  /**
   * Converts a Gecko key code (like NS_VK_HOME) to the corresponding Cocoa
   * Unicode character.
   */
  static uint32_t ConvertGeckoKeyCodeToMacCharCode(uint32_t aKeyCode);
};

#endif // nsCocoaUtils_h_
