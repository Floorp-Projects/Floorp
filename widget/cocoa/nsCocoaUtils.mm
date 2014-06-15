/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "nsCocoaUtils.h"
#include "nsChildView.h"
#include "nsMenuBarX.h"
#include "nsCocoaWindow.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppShellService.h"
#include "nsIXULWindow.h"
#include "nsIBaseWindow.h"
#include "nsIServiceManager.h"
#include "nsMenuUtilsX.h"
#include "nsToolkit.h"
#include "nsCRT.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::widget;

using mozilla::gfx::BackendType;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::DrawTarget;
using mozilla::gfx::Factory;
using mozilla::gfx::IntPoint;
using mozilla::gfx::IntRect;
using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;
using mozilla::gfx::SourceSurface;

static float
MenuBarScreenHeight()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSArray* allScreens = [NSScreen screens];
  if ([allScreens count]) {
    return [[allScreens objectAtIndex:0] frame].size.height;
  }

  return 0.0;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0.0);
}

float
nsCocoaUtils::FlippedScreenY(float y)
{
  return MenuBarScreenHeight() - y;
}

NSRect nsCocoaUtils::GeckoRectToCocoaRect(const nsIntRect &geckoRect)
{
  // We only need to change the Y coordinate by starting with the primary screen
  // height and subtracting the gecko Y coordinate of the bottom of the rect.
  return NSMakeRect(geckoRect.x,
                    MenuBarScreenHeight() - geckoRect.YMost(),
                    geckoRect.width,
                    geckoRect.height);
}

NSRect nsCocoaUtils::GeckoRectToCocoaRectDevPix(const nsIntRect &aGeckoRect,
                                                CGFloat aBackingScale)
{
  return NSMakeRect(aGeckoRect.x / aBackingScale,
                    MenuBarScreenHeight() - aGeckoRect.YMost() / aBackingScale,
                    aGeckoRect.width / aBackingScale,
                    aGeckoRect.height / aBackingScale);
}

nsIntRect nsCocoaUtils::CocoaRectToGeckoRect(const NSRect &cocoaRect)
{
  // We only need to change the Y coordinate by starting with the primary screen
  // height and subtracting both the cocoa y origin and the height of the
  // cocoa rect.
  nsIntRect rect;
  rect.x = NSToIntRound(cocoaRect.origin.x);
  rect.y = NSToIntRound(FlippedScreenY(cocoaRect.origin.y + cocoaRect.size.height));
  rect.width = NSToIntRound(cocoaRect.origin.x + cocoaRect.size.width) - rect.x;
  rect.height = NSToIntRound(FlippedScreenY(cocoaRect.origin.y)) - rect.y;
  return rect;
}

nsIntRect nsCocoaUtils::CocoaRectToGeckoRectDevPix(const NSRect &aCocoaRect,
                                                   CGFloat aBackingScale)
{
  nsIntRect rect;
  rect.x = NSToIntRound(aCocoaRect.origin.x * aBackingScale);
  rect.y = NSToIntRound(FlippedScreenY(aCocoaRect.origin.y + aCocoaRect.size.height) * aBackingScale);
  rect.width = NSToIntRound((aCocoaRect.origin.x + aCocoaRect.size.width) * aBackingScale) - rect.x;
  rect.height = NSToIntRound(FlippedScreenY(aCocoaRect.origin.y) * aBackingScale) - rect.y;
  return rect;
}

NSPoint nsCocoaUtils::ScreenLocationForEvent(NSEvent* anEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Don't trust mouse locations of mouse move events, see bug 443178.
  if (!anEvent || [anEvent type] == NSMouseMoved)
    return [NSEvent mouseLocation];

  // Pin momentum scroll events to the location of the last user-controlled
  // scroll event.
  if (IsMomentumScrollEvent(anEvent))
    return ChildViewMouseTracker::sLastScrollEventScreenLocation;

  return [[anEvent window] convertBaseToScreen:[anEvent locationInWindow]];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakePoint(0.0, 0.0));
}

BOOL nsCocoaUtils::IsEventOverWindow(NSEvent* anEvent, NSWindow* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return NSPointInRect(ScreenLocationForEvent(anEvent), [aWindow frame]);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

NSPoint nsCocoaUtils::EventLocationForWindow(NSEvent* anEvent, NSWindow* aWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [aWindow convertScreenToBase:ScreenLocationForEvent(anEvent)];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakePoint(0.0, 0.0));
}

@interface NSEvent (ScrollPhase)
// 10.5 and 10.6
- (long long)_scrollPhase;
// 10.7 and above
- (NSEventPhase)phase;
- (NSEventPhase)momentumPhase;
@end

NSEventPhase nsCocoaUtils::EventPhase(NSEvent* aEvent)
{
  if ([aEvent respondsToSelector:@selector(phase)]) {
    return [aEvent phase];
  }
  return NSEventPhaseNone;
}

NSEventPhase nsCocoaUtils::EventMomentumPhase(NSEvent* aEvent)
{
  if ([aEvent respondsToSelector:@selector(momentumPhase)]) {
    return [aEvent momentumPhase];
  }
  if ([aEvent respondsToSelector:@selector(_scrollPhase)]) {
    switch ([aEvent _scrollPhase]) {
      case 1: return NSEventPhaseBegan;
      case 2: return NSEventPhaseChanged;
      case 3: return NSEventPhaseEnded;
      default: return NSEventPhaseNone;
    }
  }
  return NSEventPhaseNone;
}

BOOL nsCocoaUtils::IsMomentumScrollEvent(NSEvent* aEvent)
{
  return [aEvent type] == NSScrollWheel &&
    EventMomentumPhase(aEvent) != NSEventPhaseNone;
}

@interface NSEvent (HasPreciseScrollingDeltas)
// 10.7 and above
- (BOOL)hasPreciseScrollingDeltas;
// For 10.6 and below, see the comment in nsChildView.h about _eventRef
- (EventRef)_eventRef;
@end

BOOL nsCocoaUtils::HasPreciseScrollingDeltas(NSEvent* aEvent)
{
  if ([aEvent respondsToSelector:@selector(hasPreciseScrollingDeltas)]) {
    return [aEvent hasPreciseScrollingDeltas];
  }

  // For events that don't contain pixel scrolling information, the event
  // kind of their underlaying carbon event is kEventMouseWheelMoved instead
  // of kEventMouseScroll.
  EventRef carbonEvent = [aEvent _eventRef];
  return carbonEvent && ::GetEventKind(carbonEvent) == kEventMouseScroll;
}

@interface NSEvent (ScrollingDeltas)
// 10.6 and below
- (CGFloat)deviceDeltaX;
- (CGFloat)deviceDeltaY;
// 10.7 and above
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
@end

void nsCocoaUtils::GetScrollingDeltas(NSEvent* aEvent, CGFloat* aOutDeltaX, CGFloat* aOutDeltaY)
{
  if ([aEvent respondsToSelector:@selector(scrollingDeltaX)]) {
    *aOutDeltaX = [aEvent scrollingDeltaX];
    *aOutDeltaY = [aEvent scrollingDeltaY];
    return;
  }
  if ([aEvent respondsToSelector:@selector(deviceDeltaX)] &&
      HasPreciseScrollingDeltas(aEvent)) {
    // Calling deviceDeltaX/Y on those events that do not contain pixel
    // scrolling information triggers a Cocoa assertion and an
    // Objective-C NSInternalInconsistencyException.
    *aOutDeltaX = [aEvent deviceDeltaX];
    *aOutDeltaY = [aEvent deviceDeltaY];
    return;
  }

  // This is only hit pre-10.7 when we are called on a scroll event that does
  // not contain pixel scrolling information.
  CGFloat lineDeltaPixels = 12;
  *aOutDeltaX = [aEvent deltaX] * lineDeltaPixels;
  *aOutDeltaY = [aEvent deltaY] * lineDeltaPixels;
}

void nsCocoaUtils::HideOSChromeOnScreen(bool aShouldHide, NSScreen* aScreen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Keep track of how many hiding requests have been made, so that they can
  // be nested.
  static int sMenuBarHiddenCount = 0, sDockHiddenCount = 0;

  // Always hide the Dock, since it's not necessarily on the primary screen.
  sDockHiddenCount += aShouldHide ? 1 : -1;
  NS_ASSERTION(sMenuBarHiddenCount >= 0, "Unbalanced HideMenuAndDockForWindow calls");

  // Only hide the menu bar if the window is on the same screen.
  // The menu bar is always on the first screen in the screen list.
  if (aScreen == [[NSScreen screens] objectAtIndex:0]) {
    sMenuBarHiddenCount += aShouldHide ? 1 : -1;
    NS_ASSERTION(sDockHiddenCount >= 0, "Unbalanced HideMenuAndDockForWindow calls");
  }

  // TODO This should be upgraded to use [NSApplication setPresentationOptions:]
  // when support for 10.5 is dropped.
  if (sMenuBarHiddenCount > 0) {
    ::SetSystemUIMode(kUIModeAllHidden, 0);
  } else if (sDockHiddenCount > 0) {
    ::SetSystemUIMode(kUIModeContentHidden, 0);
  } else {
    ::SetSystemUIMode(kUIModeNormal, 0);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"
nsIWidget* nsCocoaUtils::GetHiddenWindowWidget()
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  if (!appShell) {
    NS_WARNING("Couldn't get AppShellService in order to get hidden window ref");
    return nullptr;
  }
  
  nsCOMPtr<nsIXULWindow> hiddenWindow;
  appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
  if (!hiddenWindow) {
    // Don't warn, this happens during shutdown, bug 358607.
    return nullptr;
  }
  
  nsCOMPtr<nsIBaseWindow> baseHiddenWindow;
  baseHiddenWindow = do_GetInterface(hiddenWindow);
  if (!baseHiddenWindow) {
    NS_WARNING("Couldn't get nsIBaseWindow from hidden window (nsIXULWindow)");
    return nullptr;
  }
  
  nsCOMPtr<nsIWidget> hiddenWindowWidget;
  if (NS_FAILED(baseHiddenWindow->GetMainWidget(getter_AddRefs(hiddenWindowWidget)))) {
    NS_WARNING("Couldn't get nsIWidget from hidden window (nsIBaseWindow)");
    return nullptr;
  }
  
  return hiddenWindowWidget;
}

void nsCocoaUtils::PrepareForNativeAppModalDialog()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't do anything if this is embedding. We'll assume that if there is no hidden
  // window we shouldn't do anything, and that should cover the embedding case.
  nsMenuBarX* hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (!hiddenWindowMenuBar)
    return;

  // First put up the hidden window menu bar so that app menu event handling is correct.
  hiddenWindowMenuBar->Paint();

  NSMenu* mainMenu = [NSApp mainMenu];
  NS_ASSERTION([mainMenu numberOfItems] > 0, "Main menu does not have any items, something is terribly wrong!");
  
  // Create new menu bar for use with modal dialog
  NSMenu* newMenuBar = [[NSMenu alloc] initWithTitle:@""];
  
  // Swap in our app menu. Note that the event target is whatever window is up when
  // the app modal dialog goes up.
  NSMenuItem* firstMenuItem = [[mainMenu itemAtIndex:0] retain];
  [mainMenu removeItemAtIndex:0];
  [newMenuBar insertItem:firstMenuItem atIndex:0];
  [firstMenuItem release];
  
  // Add standard edit menu
  [newMenuBar addItem:nsMenuUtilsX::GetStandardEditMenuItem()];
  
  // Show the new menu bar
  [NSApp setMainMenu:newMenuBar];
  [newMenuBar release];
  
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaUtils::CleanUpAfterNativeAppModalDialog()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't do anything if this is embedding. We'll assume that if there is no hidden
  // window we shouldn't do anything, and that should cover the embedding case.
  nsMenuBarX* hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (!hiddenWindowMenuBar)
    return;

  NSWindow* mainWindow = [NSApp mainWindow];
  if (!mainWindow)
    hiddenWindowMenuBar->Paint();
  else
    [WindowDelegate paintMenubarForWindow:mainWindow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void data_ss_release_callback(void *aDataSourceSurface,
                              const void *data,
                              size_t size)
{
  if (aDataSourceSurface) {
    static_cast<DataSourceSurface*>(aDataSourceSurface)->Unmap();
    static_cast<DataSourceSurface*>(aDataSourceSurface)->Release();
  }
}

nsresult nsCocoaUtils::CreateCGImageFromSurface(SourceSurface* aSurface,
                                                CGImageRef* aResult)
{
  RefPtr<DataSourceSurface> dataSurface;

  if (aSurface->GetFormat() ==  SurfaceFormat::B8G8R8A8) {
    dataSurface = aSurface->GetDataSurface();
  } else {
    // CGImageCreate only supports 16- and 32-bit bit-depth
    // Convert format to SurfaceFormat::B8G8R8A8
    dataSurface = gfxUtils::
      CopySurfaceToDataSourceSurfaceWithFormat(aSurface,
                                               SurfaceFormat::B8G8R8A8);
  }

  NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

  int32_t width = dataSurface->GetSize().width;
  int32_t height = dataSurface->GetSize().height;
  if (height < 1 || width < 1) {
    return NS_ERROR_FAILURE;
  }

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }
  // The Unmap() call happens in data_ss_release_callback

  // Create a CGImageRef with the bits from the image, taking into account
  // the alpha ordering and endianness of the machine so we don't have to
  // touch the bits ourselves.
  CGDataProviderRef dataProvider = ::CGDataProviderCreateWithData(dataSurface.forget().drop(),
                                                                  map.mData,
                                                                  map.mStride * height,
                                                                  data_ss_release_callback);
  CGColorSpaceRef colorSpace = ::CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
  *aResult = ::CGImageCreate(width,
                             height,
                             8,
                             32,
                             map.mStride,
                             colorSpace,
                             kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst,
                             dataProvider,
                             NULL,
                             0,
                             kCGRenderingIntentDefault);
  ::CGColorSpaceRelease(colorSpace);
  ::CGDataProviderRelease(dataProvider);
  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsCocoaUtils::CreateNSImageFromCGImage(CGImageRef aInputImage, NSImage **aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Be very careful when creating the NSImage that the backing NSImageRep is
  // exactly 1:1 with the input image. On a retina display, both [NSImage
  // lockFocus] and [NSImage initWithCGImage:size:] will create an image with a
  // 2x backing NSImageRep. This prevents NSCursor from recognizing a retina
  // cursor, which only occurs if pixelsWide and pixelsHigh are exactly 2x the
  // size of the NSImage.
  //
  // For example, if a 32x32 SVG cursor is rendered on a retina display, then
  // aInputImage will be 64x64. The resulting NSImage will be scaled back down
  // to 32x32 so it stays the correct size on the screen by changing its size
  // (resizing a NSImage only scales the image and doesn't resample the data).
  // If aInputImage is converted using [NSImage initWithCGImage:size:] then the
  // bitmap will be 128x128 and NSCursor won't recognize a retina cursor, since
  // it will expect a 64x64 bitmap.

  int32_t width = ::CGImageGetWidth(aInputImage);
  int32_t height = ::CGImageGetHeight(aInputImage);
  NSRect imageRect = ::NSMakeRect(0.0, 0.0, width, height);

  NSBitmapImageRep *offscreenRep = [[NSBitmapImageRep alloc]
    initWithBitmapDataPlanes:NULL
    pixelsWide:width
    pixelsHigh:height
    bitsPerSample:8
    samplesPerPixel:4
    hasAlpha:YES
    isPlanar:NO
    colorSpaceName:NSDeviceRGBColorSpace
    bitmapFormat:NSAlphaFirstBitmapFormat
    bytesPerRow:0
    bitsPerPixel:0];

  NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:context];

  // Get the Quartz context and draw.
  CGContextRef imageContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  ::CGContextDrawImage(imageContext, *(CGRect*)&imageRect, aInputImage);

  [NSGraphicsContext restoreGraphicsState];

  *aResult = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
  [*aResult addRepresentation:offscreenRep];
  [offscreenRep release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsCocoaUtils::CreateNSImageFromImageContainer(imgIContainer *aImage, uint32_t aWhichFrame, NSImage **aResult, CGFloat scaleFactor)
{
  RefPtr<SourceSurface> surface;
  int32_t width = 0, height = 0;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  // Render a vector image at the correct resolution on a retina display
  if (aImage->GetType() == imgIContainer::TYPE_VECTOR && scaleFactor != 1.0f) {
    int scaledWidth = (int)ceilf(width * scaleFactor);
    int scaledHeight = (int)ceilf(height * scaleFactor);

    RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->
      CreateOffscreenContentDrawTarget(IntSize(scaledWidth, scaledHeight),
                                       SurfaceFormat::B8G8R8A8);
    if (!drawTarget) {
      NS_ERROR("Failed to create DrawTarget");
      return NS_ERROR_FAILURE;
    }

    nsRefPtr<gfxContext> context = new gfxContext(drawTarget);
    if (!context) {
      NS_ERROR("Failed to create gfxContext");
      return NS_ERROR_FAILURE;
    }

    aImage->Draw(context, GraphicsFilter::FILTER_NEAREST, gfxMatrix(),
      gfxRect(0.0f, 0.0f, scaledWidth, scaledHeight),
      nsIntRect(0, 0, width, height),
      nsIntSize(scaledWidth, scaledHeight),
      nullptr, aWhichFrame, imgIContainer::FLAG_SYNC_DECODE);

    surface = drawTarget->Snapshot();
  } else {
    surface = aImage->GetFrame(aWhichFrame, imgIContainer::FLAG_SYNC_DECODE);
  }

  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  CGImageRef imageRef = NULL;
  nsresult rv = nsCocoaUtils::CreateCGImageFromSurface(surface, &imageRef);
  if (NS_FAILED(rv) || !imageRef) {
    return NS_ERROR_FAILURE;
  }

  rv = nsCocoaUtils::CreateNSImageFromCGImage(imageRef, aResult);
  if (NS_FAILED(rv) || !aResult) {
    return NS_ERROR_FAILURE;
  }
  ::CGImageRelease(imageRef);

  // Ensure the image will be rendered the correct size on a retina display
  NSSize size = NSMakeSize(width, height);
  [*aResult setSize:size];
  [[[*aResult representations] objectAtIndex:0] setSize:size];
  return NS_OK;
}

// static
void
nsCocoaUtils::GetStringForNSString(const NSString *aSrc, nsAString& aDist)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aSrc) {
    aDist.Truncate();
    return;
  }

  aDist.SetLength([aSrc length]);
  [aSrc getCharacters: reinterpret_cast<unichar*>(aDist.BeginWriting())];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// static
NSString*
nsCocoaUtils::ToNSString(const nsAString& aString)
{
  if (aString.IsEmpty()) {
    return [NSString string];
  }
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aString.BeginReading())
                                 length:aString.Length()];
}

// static
void
nsCocoaUtils::GeckoRectToNSRect(const nsIntRect& aGeckoRect,
                                NSRect& aOutCocoaRect)
{
  aOutCocoaRect.origin.x = aGeckoRect.x;
  aOutCocoaRect.origin.y = aGeckoRect.y;
  aOutCocoaRect.size.width = aGeckoRect.width;
  aOutCocoaRect.size.height = aGeckoRect.height;
}

// static
void
nsCocoaUtils::NSRectToGeckoRect(const NSRect& aCocoaRect,
                                nsIntRect& aOutGeckoRect)
{
  aOutGeckoRect.x = NSToIntRound(aCocoaRect.origin.x);
  aOutGeckoRect.y = NSToIntRound(aCocoaRect.origin.y);
  aOutGeckoRect.width = NSToIntRound(aCocoaRect.origin.x + aCocoaRect.size.width) - aOutGeckoRect.x;
  aOutGeckoRect.height = NSToIntRound(aCocoaRect.origin.y + aCocoaRect.size.height) - aOutGeckoRect.y;
}

// static
NSEvent*
nsCocoaUtils::MakeNewCocoaEventWithType(NSEventType aEventType, NSEvent *aEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSEvent* newEvent =
    [NSEvent     keyEventWithType:aEventType
                         location:[aEvent locationInWindow] 
                    modifierFlags:[aEvent modifierFlags]
                        timestamp:[aEvent timestamp]
                     windowNumber:[aEvent windowNumber]
                          context:[aEvent context]
                       characters:[aEvent characters]
      charactersIgnoringModifiers:[aEvent charactersIgnoringModifiers]
                        isARepeat:[aEvent isARepeat]
                          keyCode:[aEvent keyCode]];
  return newEvent;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// static
void
nsCocoaUtils::InitNPCocoaEvent(NPCocoaEvent* aNPCocoaEvent)
{
  memset(aNPCocoaEvent, 0, sizeof(NPCocoaEvent));
}

// static
void
nsCocoaUtils::InitPluginEvent(WidgetPluginEvent &aPluginEvent,
                              NPCocoaEvent &aCocoaEvent)
{
  aPluginEvent.time = PR_IntervalNow();
  aPluginEvent.pluginEvent = (void*)&aCocoaEvent;
  aPluginEvent.retargetToFocusedDocument = false;
}

// static
void
nsCocoaUtils::InitInputEvent(WidgetInputEvent& aInputEvent,
                             NSEvent* aNativeEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSUInteger modifiers =
    aNativeEvent ? [aNativeEvent modifierFlags] : [NSEvent modifierFlags];
  InitInputEvent(aInputEvent, modifiers);

  aInputEvent.time = PR_IntervalNow();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// static
void
nsCocoaUtils::InitInputEvent(WidgetInputEvent& aInputEvent,
                             NSUInteger aModifiers)
{
  aInputEvent.modifiers = 0;
  if (aModifiers & NSShiftKeyMask) {
    aInputEvent.modifiers |= MODIFIER_SHIFT;
  }
  if (aModifiers & NSControlKeyMask) {
    aInputEvent.modifiers |= MODIFIER_CONTROL;
  }
  if (aModifiers & NSAlternateKeyMask) {
    aInputEvent.modifiers |= MODIFIER_ALT;
    // Mac's option key is similar to other platforms' AltGr key.
    // Let's set AltGr flag when option key is pressed for consistency with
    // other platforms.
    aInputEvent.modifiers |= MODIFIER_ALTGRAPH;
  }
  if (aModifiers & NSCommandKeyMask) {
    aInputEvent.modifiers |= MODIFIER_META;
  }

  if (aModifiers & NSAlphaShiftKeyMask) {
    aInputEvent.modifiers |= MODIFIER_CAPSLOCK;
  }
  // Mac doesn't have NumLock key.  We can assume that NumLock is always locked
  // if user is using a keyboard which has numpad.  Otherwise, if user is using
  // a keyboard which doesn't have numpad, e.g., MacBook's keyboard, we can
  // assume that NumLock is always unlocked.
  // Unfortunately, we cannot know whether current keyboard has numpad or not.
  // We should notify locked state only when keys in numpad are pressed.
  // By this, web applications may not be confused by unexpected numpad key's
  // key event with unlocked state.
  if (aModifiers & NSNumericPadKeyMask) {
    aInputEvent.modifiers |= MODIFIER_NUMLOCK;
  }

  // Be aware, NSFunctionKeyMask is included when arrow keys, home key or some
  // other keys are pressed. We cannot check whether 'fn' key is pressed or
  // not by the flag.

}

// static
UInt32
nsCocoaUtils::ConvertToCarbonModifier(NSUInteger aCocoaModifier)
{
  UInt32 carbonModifier = 0;
  if (aCocoaModifier & NSAlphaShiftKeyMask) {
    carbonModifier |= alphaLock;
  }
  if (aCocoaModifier & NSControlKeyMask) {
    carbonModifier |= controlKey;
  }
  if (aCocoaModifier & NSAlternateKeyMask) {
    carbonModifier |= optionKey;
  }
  if (aCocoaModifier & NSShiftKeyMask) {
    carbonModifier |= shiftKey;
  }
  if (aCocoaModifier & NSCommandKeyMask) {
    carbonModifier |= cmdKey;
  }
  if (aCocoaModifier & NSNumericPadKeyMask) {
    carbonModifier |= kEventKeyModifierNumLockMask;
  }
  if (aCocoaModifier & NSFunctionKeyMask) {
    carbonModifier |= kEventKeyModifierFnMask;
  }
  return carbonModifier;
}

// While HiDPI support is not 100% complete and tested, we'll have a pref
// to allow it to be turned off in case of problems (or for testing purposes).

// gfx.hidpi.enabled is an integer with the meaning:
//    <= 0 : HiDPI support is disabled
//       1 : HiDPI enabled provided all screens have the same backing resolution
//     > 1 : HiDPI enabled even if there are a mixture of screen modes

// All the following code is to be removed once HiDPI work is more complete.

static bool sHiDPIEnabled = false;
static bool sHiDPIPrefInitialized = false;

// static
bool
nsCocoaUtils::HiDPIEnabled()
{
  if (!sHiDPIPrefInitialized) {
    sHiDPIPrefInitialized = true;

    int prefSetting = Preferences::GetInt("gfx.hidpi.enabled", 1);
    if (prefSetting <= 0) {
      return false;
    }

    // prefSetting is at least 1, need to check attached screens...

    int scaleFactors = 0; // used as a bitset to track the screen types found
    NSEnumerator *screenEnum = [[NSScreen screens] objectEnumerator];
    while (NSScreen *screen = [screenEnum nextObject]) {
      NSDictionary *desc = [screen deviceDescription];
      if ([desc objectForKey:NSDeviceIsScreen] == nil) {
        continue;
      }
      CGFloat scale =
        [screen respondsToSelector:@selector(backingScaleFactor)] ?
          [screen backingScaleFactor] : 1.0;
      // Currently, we only care about differentiating "1.0" and "2.0",
      // so we set one of the two low bits to record which.
      if (scale > 1.0) {
        scaleFactors |= 2;
      } else {
        scaleFactors |= 1;
      }
    }

    // Now scaleFactors will be:
    //   0 if no screens (supporting backingScaleFactor) found
    //   1 if only lo-DPI screens
    //   2 if only hi-DPI screens
    //   3 if both lo- and hi-DPI screens
    // We'll enable HiDPI support if there's only a single screen type,
    // OR if the pref setting is explicitly greater than 1.
    sHiDPIEnabled = (scaleFactors <= 2) || (prefSetting > 1);
  }

  return sHiDPIEnabled;
}

void
nsCocoaUtils::GetCommandsFromKeyEvent(NSEvent* aEvent,
                                      nsTArray<KeyBindingsCommand>& aCommands)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_ASSERT(aEvent);

  static NativeKeyBindingsRecorder* sNativeKeyBindingsRecorder;
  if (!sNativeKeyBindingsRecorder) {
    sNativeKeyBindingsRecorder = [NativeKeyBindingsRecorder new];
  }

  [sNativeKeyBindingsRecorder startRecording:aCommands];

  // This will trigger 0 - N calls to doCommandBySelector: and insertText:
  [sNativeKeyBindingsRecorder
    interpretKeyEvents:[NSArray arrayWithObject:aEvent]];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@implementation NativeKeyBindingsRecorder

- (void)startRecording:(nsTArray<KeyBindingsCommand>&)aCommands
{
  mCommands = &aCommands;
  mCommands->Clear();
}

- (void)doCommandBySelector:(SEL)aSelector
{
  KeyBindingsCommand command = {
    aSelector,
    nil
  };

  mCommands->AppendElement(command);
}

- (void)insertText:(id)aString
{
  KeyBindingsCommand command = {
    @selector(insertText:),
    aString
  };

  mCommands->AppendElement(command);
}

@end // NativeKeyBindingsRecorder

struct KeyConversionData
{
  const char* str;
  size_t strLength;
  uint32_t geckoKeyCode;
  uint32_t charCode;
};

static const KeyConversionData gKeyConversions[] = {

#define KEYCODE_ENTRY(aStr, aCode) \
  {#aStr, sizeof(#aStr) - 1, NS_##aStr, aCode}

// Some keycodes may have different name in nsIDOMKeyEvent from its key name.
#define KEYCODE_ENTRY2(aStr, aNSName, aCode) \
  {#aStr, sizeof(#aStr) - 1, NS_##aNSName, aCode}

  KEYCODE_ENTRY(VK_CANCEL, 0x001B),
  KEYCODE_ENTRY(VK_DELETE, NSDeleteFunctionKey),
  KEYCODE_ENTRY(VK_BACK, NSBackspaceCharacter),
  KEYCODE_ENTRY2(VK_BACK_SPACE, VK_BACK, NSBackspaceCharacter),
  KEYCODE_ENTRY(VK_TAB, NSTabCharacter),
  KEYCODE_ENTRY(VK_CLEAR, NSClearLineFunctionKey),
  KEYCODE_ENTRY(VK_RETURN, NSEnterCharacter),
  KEYCODE_ENTRY(VK_SHIFT, 0),
  KEYCODE_ENTRY(VK_CONTROL, 0),
  KEYCODE_ENTRY(VK_ALT, 0),
  KEYCODE_ENTRY(VK_PAUSE, NSPauseFunctionKey),
  KEYCODE_ENTRY(VK_CAPS_LOCK, 0),
  KEYCODE_ENTRY(VK_ESCAPE, 0),
  KEYCODE_ENTRY(VK_SPACE, ' '),
  KEYCODE_ENTRY(VK_PAGE_UP, NSPageUpFunctionKey),
  KEYCODE_ENTRY(VK_PAGE_DOWN, NSPageDownFunctionKey),
  KEYCODE_ENTRY(VK_END, NSEndFunctionKey),
  KEYCODE_ENTRY(VK_HOME, NSHomeFunctionKey),
  KEYCODE_ENTRY(VK_LEFT, NSLeftArrowFunctionKey),
  KEYCODE_ENTRY(VK_UP, NSUpArrowFunctionKey),
  KEYCODE_ENTRY(VK_RIGHT, NSRightArrowFunctionKey),
  KEYCODE_ENTRY(VK_DOWN, NSDownArrowFunctionKey),
  KEYCODE_ENTRY(VK_PRINTSCREEN, NSPrintScreenFunctionKey),
  KEYCODE_ENTRY(VK_INSERT, NSInsertFunctionKey),
  KEYCODE_ENTRY(VK_HELP, NSHelpFunctionKey),
  KEYCODE_ENTRY(VK_0, '0'),
  KEYCODE_ENTRY(VK_1, '1'),
  KEYCODE_ENTRY(VK_2, '2'),
  KEYCODE_ENTRY(VK_3, '3'),
  KEYCODE_ENTRY(VK_4, '4'),
  KEYCODE_ENTRY(VK_5, '5'),
  KEYCODE_ENTRY(VK_6, '6'),
  KEYCODE_ENTRY(VK_7, '7'),
  KEYCODE_ENTRY(VK_8, '8'),
  KEYCODE_ENTRY(VK_9, '9'),
  KEYCODE_ENTRY(VK_SEMICOLON, ':'),
  KEYCODE_ENTRY(VK_EQUALS, '='),
  KEYCODE_ENTRY(VK_A, 'A'),
  KEYCODE_ENTRY(VK_B, 'B'),
  KEYCODE_ENTRY(VK_C, 'C'),
  KEYCODE_ENTRY(VK_D, 'D'),
  KEYCODE_ENTRY(VK_E, 'E'),
  KEYCODE_ENTRY(VK_F, 'F'),
  KEYCODE_ENTRY(VK_G, 'G'),
  KEYCODE_ENTRY(VK_H, 'H'),
  KEYCODE_ENTRY(VK_I, 'I'),
  KEYCODE_ENTRY(VK_J, 'J'),
  KEYCODE_ENTRY(VK_K, 'K'),
  KEYCODE_ENTRY(VK_L, 'L'),
  KEYCODE_ENTRY(VK_M, 'M'),
  KEYCODE_ENTRY(VK_N, 'N'),
  KEYCODE_ENTRY(VK_O, 'O'),
  KEYCODE_ENTRY(VK_P, 'P'),
  KEYCODE_ENTRY(VK_Q, 'Q'),
  KEYCODE_ENTRY(VK_R, 'R'),
  KEYCODE_ENTRY(VK_S, 'S'),
  KEYCODE_ENTRY(VK_T, 'T'),
  KEYCODE_ENTRY(VK_U, 'U'),
  KEYCODE_ENTRY(VK_V, 'V'),
  KEYCODE_ENTRY(VK_W, 'W'),
  KEYCODE_ENTRY(VK_X, 'X'),
  KEYCODE_ENTRY(VK_Y, 'Y'),
  KEYCODE_ENTRY(VK_Z, 'Z'),
  KEYCODE_ENTRY(VK_CONTEXT_MENU, NSMenuFunctionKey),
  KEYCODE_ENTRY(VK_NUMPAD0, '0'),
  KEYCODE_ENTRY(VK_NUMPAD1, '1'),
  KEYCODE_ENTRY(VK_NUMPAD2, '2'),
  KEYCODE_ENTRY(VK_NUMPAD3, '3'),
  KEYCODE_ENTRY(VK_NUMPAD4, '4'),
  KEYCODE_ENTRY(VK_NUMPAD5, '5'),
  KEYCODE_ENTRY(VK_NUMPAD6, '6'),
  KEYCODE_ENTRY(VK_NUMPAD7, '7'),
  KEYCODE_ENTRY(VK_NUMPAD8, '8'),
  KEYCODE_ENTRY(VK_NUMPAD9, '9'),
  KEYCODE_ENTRY(VK_MULTIPLY, '*'),
  KEYCODE_ENTRY(VK_ADD, '+'),
  KEYCODE_ENTRY(VK_SEPARATOR, 0),
  KEYCODE_ENTRY(VK_SUBTRACT, '-'),
  KEYCODE_ENTRY(VK_DECIMAL, '.'),
  KEYCODE_ENTRY(VK_DIVIDE, '/'),
  KEYCODE_ENTRY(VK_F1, NSF1FunctionKey),
  KEYCODE_ENTRY(VK_F2, NSF2FunctionKey),
  KEYCODE_ENTRY(VK_F3, NSF3FunctionKey),
  KEYCODE_ENTRY(VK_F4, NSF4FunctionKey),
  KEYCODE_ENTRY(VK_F5, NSF5FunctionKey),
  KEYCODE_ENTRY(VK_F6, NSF6FunctionKey),
  KEYCODE_ENTRY(VK_F7, NSF7FunctionKey),
  KEYCODE_ENTRY(VK_F8, NSF8FunctionKey),
  KEYCODE_ENTRY(VK_F9, NSF9FunctionKey),
  KEYCODE_ENTRY(VK_F10, NSF10FunctionKey),
  KEYCODE_ENTRY(VK_F11, NSF11FunctionKey),
  KEYCODE_ENTRY(VK_F12, NSF12FunctionKey),
  KEYCODE_ENTRY(VK_F13, NSF13FunctionKey),
  KEYCODE_ENTRY(VK_F14, NSF14FunctionKey),
  KEYCODE_ENTRY(VK_F15, NSF15FunctionKey),
  KEYCODE_ENTRY(VK_F16, NSF16FunctionKey),
  KEYCODE_ENTRY(VK_F17, NSF17FunctionKey),
  KEYCODE_ENTRY(VK_F18, NSF18FunctionKey),
  KEYCODE_ENTRY(VK_F19, NSF19FunctionKey),
  KEYCODE_ENTRY(VK_F20, NSF20FunctionKey),
  KEYCODE_ENTRY(VK_F21, NSF21FunctionKey),
  KEYCODE_ENTRY(VK_F22, NSF22FunctionKey),
  KEYCODE_ENTRY(VK_F23, NSF23FunctionKey),
  KEYCODE_ENTRY(VK_F24, NSF24FunctionKey),
  KEYCODE_ENTRY(VK_NUM_LOCK, NSClearLineFunctionKey),
  KEYCODE_ENTRY(VK_SCROLL_LOCK, NSScrollLockFunctionKey),
  KEYCODE_ENTRY(VK_COMMA, ','),
  KEYCODE_ENTRY(VK_PERIOD, '.'),
  KEYCODE_ENTRY(VK_SLASH, '/'),
  KEYCODE_ENTRY(VK_BACK_QUOTE, '`'),
  KEYCODE_ENTRY(VK_OPEN_BRACKET, '['),
  KEYCODE_ENTRY(VK_BACK_SLASH, '\\'),
  KEYCODE_ENTRY(VK_CLOSE_BRACKET, ']'),
  KEYCODE_ENTRY(VK_QUOTE, '\'')

#undef KEYCODE_ENTRY

};

uint32_t
nsCocoaUtils::ConvertGeckoNameToMacCharCode(const nsAString& aKeyCodeName)
{
  if (aKeyCodeName.IsEmpty()) {
    return 0;
  }

  nsAutoCString keyCodeName;
  keyCodeName.AssignWithConversion(aKeyCodeName);
  // We want case-insensitive comparison with data stored as uppercase.
  ToUpperCase(keyCodeName);

  uint32_t keyCodeNameLength = keyCodeName.Length();
  const char* keyCodeNameStr = keyCodeName.get();
  for (uint16_t i = 0; i < ArrayLength(gKeyConversions); ++i) {
    if (keyCodeNameLength == gKeyConversions[i].strLength &&
        nsCRT::strcmp(gKeyConversions[i].str, keyCodeNameStr) == 0) {
      return gKeyConversions[i].charCode;
    }
  }

  return 0;
}

uint32_t
nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(uint32_t aKeyCode)
{
  if (!aKeyCode) {
    return 0;
  }

  for (uint16_t i = 0; i < ArrayLength(gKeyConversions); ++i) {
    if (gKeyConversions[i].geckoKeyCode == aKeyCode) {
      return gKeyConversions[i].charCode;
    }
  }

  return 0;
}
