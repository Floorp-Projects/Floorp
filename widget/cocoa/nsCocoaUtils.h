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
#include "Units.h"

// This must be the last include:
#include "nsObjCExceptions.h"

#include "mozilla/EventForwards.h"

// Declare the backingScaleFactor method that we want to call
// on NSView/Window/Screen objects, if they recognize it.
@interface NSObject (BackingScaleFactorCategory)
- (CGFloat)backingScaleFactor;
@end

#if !defined(MAC_OS_X_VERSION_10_8) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8
enum {
  NSEventPhaseMayBegin    = 0x1 << 5
};
#endif

class nsIWidget;

namespace mozilla {
class TimeStamp;
namespace gfx {
class SourceSurface;
} // namespace gfx
} // namespace mozilla

// Used to retain a Cocoa object for the remainder of a method's execution.
class nsAutoRetainCocoaObject {
public:
explicit nsAutoRetainCocoaObject(id anObject)
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

// Send an event to the current Cocoa app-modal session.  Present in all
// versions of OS X from (at least) 10.2.8 through 10.5.
- (void)_modalSession:(NSModalSession)aSession sendEvent:(NSEvent *)theEvent;

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
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::LayoutDeviceIntPoint LayoutDeviceIntPoint;
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;

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

  static LayoutDeviceIntPoint
  CocoaPointsToDevPixels(const NSPoint& aPt, CGFloat aBackingScale)
  {
    return LayoutDeviceIntPoint(NSToIntRound(aPt.x * aBackingScale),
                                NSToIntRound(aPt.y * aBackingScale));
  }

  static LayoutDeviceIntPoint
  CocoaPointsToDevPixelsRoundDown(const NSPoint& aPt, CGFloat aBackingScale)
  {
    return LayoutDeviceIntPoint(NSToIntFloor(aPt.x * aBackingScale),
                                NSToIntFloor(aPt.y * aBackingScale));
  }

  static LayoutDeviceIntRect
  CocoaPointsToDevPixels(const NSRect& aRect, CGFloat aBackingScale)
  {
    return LayoutDeviceIntRect(NSToIntRound(aRect.origin.x * aBackingScale),
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
  DevPixelsToCocoaPoints(const mozilla::LayoutDeviceIntPoint& aPt,
                         CGFloat aBackingScale)
  {
    return NSMakePoint((CGFloat)aPt.x / aBackingScale,
                       (CGFloat)aPt.y / aBackingScale);
  }

  // Implements an NSPoint equivalent of -[NSWindow convertRectFromScreen:].
  static NSPoint
  ConvertPointFromScreen(NSWindow* aWindow, const NSPoint& aPt)
  {
    return [aWindow convertRectFromScreen:NSMakeRect(aPt.x, aPt.y, 0, 0)].origin;
  }

  // Implements an NSPoint equivalent of -[NSWindow convertRectToScreen:].
  static NSPoint
  ConvertPointToScreen(NSWindow* aWindow, const NSPoint& aPt)
  {
    return [aWindow convertRectToScreen:NSMakeRect(aPt.x, aPt.y, 0, 0)].origin;
  }

  static NSRect
  DevPixelsToCocoaPoints(const LayoutDeviceIntRect& aRect,
                         CGFloat aBackingScale)
  {
    return NSMakeRect((CGFloat)aRect.X() / aBackingScale,
                      (CGFloat)aRect.Y() / aBackingScale,
                      (CGFloat)aRect.Width() / aBackingScale,
                      (CGFloat)aRect.Height() / aBackingScale);
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
  // expected to be desktop pixels, which are equal to Cocoa points
  // (by definition).
  static NSRect GeckoRectToCocoaRect(const mozilla::DesktopIntRect &geckoRect);

  // Converts aGeckoRect in dev pixels to points in Cocoa coordinates
  static NSRect
  GeckoRectToCocoaRectDevPix(const mozilla::LayoutDeviceIntRect &aGeckoRect,
                             CGFloat aBackingScale);

  // See explanation for geckoRectToCocoaRect, guess what this does...
  static mozilla::DesktopIntRect CocoaRectToGeckoRect(const NSRect &cocoaRect);

  static mozilla::LayoutDeviceIntRect CocoaRectToGeckoRectDevPix(
    const NSRect& aCocoaRect, CGFloat aBackingScale);

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

  // Compatibility wrappers for the -[NSEvent phase], -[NSEvent momentumPhase],
  // -[NSEvent hasPreciseScrollingDeltas] and -[NSEvent scrollingDeltaX/Y] APIs
  // that became availaible starting with the 10.7 SDK.
  // All of these can be removed once we drop support for 10.6.
  static NSEventPhase EventPhase(NSEvent* aEvent);
  static NSEventPhase EventMomentumPhase(NSEvent* aEvent);
  static BOOL IsMomentumScrollEvent(NSEvent* aEvent);
  static BOOL HasPreciseScrollingDeltas(NSEvent* aEvent);
  static void GetScrollingDeltas(NSEvent* aEvent, CGFloat* aOutDeltaX, CGFloat* aOutDeltaY);
  static BOOL EventHasPhaseInformation(NSEvent* aEvent);

  // Hides the Menu bar and the Dock. Multiple hide/show requests can be nested.
  static void HideOSChromeOnScreen(bool aShouldHide);

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
      @param aIsEntirelyBlack an outparam that, if non-null, will be set to a
                              bool that indicates whether the RGB values on all
                              pixels are zero
      @return NS_OK if the conversion worked, NS_ERROR_FAILURE otherwise
   */
  static nsresult CreateCGImageFromSurface(SourceSurface* aSurface,
                                           CGImageRef* aResult,
                                           bool* aIsEntirelyBlack = nullptr);
  
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
      @param scaleFactor the desired scale factor of the NSImage (2 for a retina display)
      @return NS_OK if the conversion worked, NS_ERROR_FAILURE otherwise
   */  
  static nsresult CreateNSImageFromImageContainer(imgIContainer *aImage, uint32_t aWhichFrame, NSImage **aResult, CGFloat scaleFactor);

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
   * Makes a cocoa event from a widget keyboard event.
   */
  static NSEvent* MakeNewCococaEventFromWidgetEvent(
                    const mozilla::WidgetKeyboardEvent& aKeyEvent,
                    NSInteger aWindowNumber,
                    NSGraphicsContext* aContext);

  /**
   * Initializes aNPCocoaEvent.
   */
  static void InitNPCocoaEvent(NPCocoaEvent* aNPCocoaEvent);

  /**
   * Initializes WidgetInputEvent for aNativeEvent or aModifiers.
   */
  static void InitInputEvent(mozilla::WidgetInputEvent &aInputEvent,
                             NSEvent* aNativeEvent);

  /**
   * Converts the native modifiers from aNativeEvent into WidgetMouseEvent
   * Modifiers. aNativeEvent can be null.
   */
  static mozilla::Modifiers ModifiersForEvent(NSEvent* aNativeEvent);

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

  /**
   * Convert string with font attribute to NSMutableAttributedString
   */
  static NSMutableAttributedString* GetNSMutableAttributedString(
           const nsAString& aText,
           const nsTArray<mozilla::FontRange>& aFontRanges,
           const bool aIsVertical,
           const CGFloat aBackingScaleFactor);

  /**
   * Compute TimeStamp from an event's timestamp.
   * If aEventTime is 0, this returns current timestamp.
   */
  static mozilla::TimeStamp GetEventTimeStamp(NSTimeInterval aEventTime);
};

#endif // nsCocoaUtils_h_
