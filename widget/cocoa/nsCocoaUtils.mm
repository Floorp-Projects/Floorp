/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <AVFoundation/AVFoundation.h>

#include <cmath>

#include "AppleUtils.h"
#include "gfx2DGlue.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "ImageRegion.h"
#include "nsCocoaUtils.h"
#include "nsChildView.h"
#include "nsMenuBarX.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaWindow.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppShellService.h"
#include "nsIOSPermissionRequest.h"
#include "nsIRunnable.h"
#include "nsIAppWindow.h"
#include "nsIBaseWindow.h"
#include "nsMenuUtilsX.h"
#include "nsToolkit.h"
#include "nsCRT.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SVGImageContext.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla;
using namespace mozilla::widget;

using mozilla::dom::Promise;
using mozilla::gfx::BackendType;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::DrawTarget;
using mozilla::gfx::Factory;
using mozilla::gfx::SamplingFilter;
using mozilla::gfx::IntPoint;
using mozilla::gfx::IntRect;
using mozilla::gfx::IntSize;
using mozilla::gfx::SurfaceFormat;
using mozilla::gfx::SourceSurface;
using mozilla::image::ImageRegion;
using std::ceil;

LazyLogModule gCocoaUtilsLog("nsCocoaUtils");
#undef LOG
#define LOG(...) MOZ_LOG(gCocoaUtilsLog, LogLevel::Debug, (__VA_ARGS__))

/*
 * For each audio and video capture request, we hold an owning reference
 * to a promise to be resolved when the request's async callback is invoked.
 * sVideoCapturePromises and sAudioCapturePromises are arrays of video and
 * audio promises waiting for to be resolved. Each array is protected by a
 * mutex.
 */
nsCocoaUtils::PromiseArray nsCocoaUtils::sVideoCapturePromises;
nsCocoaUtils::PromiseArray nsCocoaUtils::sAudioCapturePromises;
StaticMutex nsCocoaUtils::sMediaCaptureMutex;

static float MenuBarScreenHeight() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSArray* allScreens = [NSScreen screens];
  if ([allScreens count]) {
    return [[allScreens objectAtIndex:0] frame].size.height;
  }

  return 0.0;

  NS_OBJC_END_TRY_BLOCK_RETURN(0.0);
}

float nsCocoaUtils::FlippedScreenY(float y) { return MenuBarScreenHeight() - y; }

NSRect nsCocoaUtils::GeckoRectToCocoaRect(const DesktopIntRect& geckoRect) {
  // We only need to change the Y coordinate by starting with the primary screen
  // height and subtracting the gecko Y coordinate of the bottom of the rect.
  return NSMakeRect(geckoRect.x, MenuBarScreenHeight() - geckoRect.YMost(), geckoRect.width,
                    geckoRect.height);
}

NSRect nsCocoaUtils::GeckoRectToCocoaRectDevPix(const LayoutDeviceIntRect& aGeckoRect,
                                                CGFloat aBackingScale) {
  return NSMakeRect(aGeckoRect.x / aBackingScale,
                    MenuBarScreenHeight() - aGeckoRect.YMost() / aBackingScale,
                    aGeckoRect.width / aBackingScale, aGeckoRect.height / aBackingScale);
}

DesktopIntRect nsCocoaUtils::CocoaRectToGeckoRect(const NSRect& cocoaRect) {
  // We only need to change the Y coordinate by starting with the primary screen
  // height and subtracting both the cocoa y origin and the height of the
  // cocoa rect.
  DesktopIntRect rect;
  rect.x = NSToIntRound(cocoaRect.origin.x);
  rect.y = NSToIntRound(FlippedScreenY(cocoaRect.origin.y + cocoaRect.size.height));
  rect.width = NSToIntRound(cocoaRect.origin.x + cocoaRect.size.width) - rect.x;
  rect.height = NSToIntRound(FlippedScreenY(cocoaRect.origin.y)) - rect.y;
  return rect;
}

LayoutDeviceIntRect nsCocoaUtils::CocoaRectToGeckoRectDevPix(const NSRect& aCocoaRect,
                                                             CGFloat aBackingScale) {
  LayoutDeviceIntRect rect;
  rect.x = NSToIntRound(aCocoaRect.origin.x * aBackingScale);
  rect.y =
      NSToIntRound(FlippedScreenY(aCocoaRect.origin.y + aCocoaRect.size.height) * aBackingScale);
  rect.width = NSToIntRound((aCocoaRect.origin.x + aCocoaRect.size.width) * aBackingScale) - rect.x;
  rect.height = NSToIntRound(FlippedScreenY(aCocoaRect.origin.y) * aBackingScale) - rect.y;
  return rect;
}

NSPoint nsCocoaUtils::ScreenLocationForEvent(NSEvent* anEvent) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Don't trust mouse locations of mouse move events, see bug 443178.
  if (!anEvent || [anEvent type] == NSEventTypeMouseMoved) return [NSEvent mouseLocation];

  // Pin momentum scroll events to the location of the last user-controlled
  // scroll event.
  if (IsMomentumScrollEvent(anEvent)) return ChildViewMouseTracker::sLastScrollEventScreenLocation;

  return nsCocoaUtils::ConvertPointToScreen([anEvent window], [anEvent locationInWindow]);

  NS_OBJC_END_TRY_BLOCK_RETURN(NSMakePoint(0.0, 0.0));
}

BOOL nsCocoaUtils::IsEventOverWindow(NSEvent* anEvent, NSWindow* aWindow) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return NSPointInRect(ScreenLocationForEvent(anEvent), [aWindow frame]);

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

NSPoint nsCocoaUtils::EventLocationForWindow(NSEvent* anEvent, NSWindow* aWindow) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return nsCocoaUtils::ConvertPointFromScreen(aWindow, ScreenLocationForEvent(anEvent));

  NS_OBJC_END_TRY_BLOCK_RETURN(NSMakePoint(0.0, 0.0));
}

BOOL nsCocoaUtils::IsMomentumScrollEvent(NSEvent* aEvent) {
  return [aEvent type] == NSEventTypeScrollWheel && [aEvent momentumPhase] != NSEventPhaseNone;
}

@interface NSEvent (HasPreciseScrollingDeltas)
// 10.7 and above
- (BOOL)hasPreciseScrollingDeltas;
// For 10.6 and below, see the comment in nsChildView.h about _eventRef
- (EventRef)_eventRef;
@end

BOOL nsCocoaUtils::HasPreciseScrollingDeltas(NSEvent* aEvent) {
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

void nsCocoaUtils::GetScrollingDeltas(NSEvent* aEvent, CGFloat* aOutDeltaX, CGFloat* aOutDeltaY) {
  if ([aEvent respondsToSelector:@selector(scrollingDeltaX)]) {
    *aOutDeltaX = [aEvent scrollingDeltaX];
    *aOutDeltaY = [aEvent scrollingDeltaY];
    return;
  }
  if ([aEvent respondsToSelector:@selector(deviceDeltaX)] && HasPreciseScrollingDeltas(aEvent)) {
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

BOOL nsCocoaUtils::EventHasPhaseInformation(NSEvent* aEvent) {
  return [aEvent phase] != NSEventPhaseNone || [aEvent momentumPhase] != NSEventPhaseNone;
}

void nsCocoaUtils::HideOSChromeOnScreen(bool aShouldHide) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Keep track of how many hiding requests have been made, so that they can
  // be nested.
  static int sHiddenCount = 0;

  sHiddenCount += aShouldHide ? 1 : -1;
  NS_ASSERTION(sHiddenCount >= 0, "Unbalanced HideMenuAndDockForWindow calls");

  NSApplicationPresentationOptions options =
      sHiddenCount <= 0 ? NSApplicationPresentationDefault
                        : NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar;
  [NSApp setPresentationOptions:options];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"
nsIWidget* nsCocoaUtils::GetHiddenWindowWidget() {
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  if (!appShell) {
    NS_WARNING("Couldn't get AppShellService in order to get hidden window ref");
    return nullptr;
  }

  nsCOMPtr<nsIAppWindow> hiddenWindow;
  appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
  if (!hiddenWindow) {
    // Don't warn, this happens during shutdown, bug 358607.
    return nullptr;
  }

  nsCOMPtr<nsIBaseWindow> baseHiddenWindow;
  baseHiddenWindow = do_GetInterface(hiddenWindow);
  if (!baseHiddenWindow) {
    NS_WARNING("Couldn't get nsIBaseWindow from hidden window (nsIAppWindow)");
    return nullptr;
  }

  nsCOMPtr<nsIWidget> hiddenWindowWidget;
  if (NS_FAILED(baseHiddenWindow->GetMainWidget(getter_AddRefs(hiddenWindowWidget)))) {
    NS_WARNING("Couldn't get nsIWidget from hidden window (nsIBaseWindow)");
    return nullptr;
  }

  return hiddenWindowWidget;
}

void nsCocoaUtils::PrepareForNativeAppModalDialog() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Don't do anything if this is embedding. We'll assume that if there is no hidden
  // window we shouldn't do anything, and that should cover the embedding case.
  nsMenuBarX* hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (!hiddenWindowMenuBar) return;

  // First put up the hidden window menu bar so that app menu event handling is correct.
  hiddenWindowMenuBar->Paint();

  NSMenu* mainMenu = [NSApp mainMenu];
  NS_ASSERTION([mainMenu numberOfItems] > 0,
               "Main menu does not have any items, something is terribly wrong!");

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

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaUtils::CleanUpAfterNativeAppModalDialog() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Don't do anything if this is embedding. We'll assume that if there is no hidden
  // window we shouldn't do anything, and that should cover the embedding case.
  nsMenuBarX* hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (!hiddenWindowMenuBar) return;

  NSWindow* mainWindow = [NSApp mainWindow];
  if (!mainWindow)
    hiddenWindowMenuBar->Paint();
  else
    [WindowDelegate paintMenubarForWindow:mainWindow];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static void data_ss_release_callback(void* aDataSourceSurface, const void* data, size_t size) {
  if (aDataSourceSurface) {
    static_cast<DataSourceSurface*>(aDataSourceSurface)->Unmap();
    static_cast<DataSourceSurface*>(aDataSourceSurface)->Release();
  }
}

// This function assumes little endian byte order.
static bool ComputeIsEntirelyBlack(const DataSourceSurface::MappedSurface& aMap,
                                   const IntSize& aSize) {
  for (int32_t y = 0; y < aSize.height; y++) {
    size_t rowStart = y * aMap.mStride;
    for (int32_t x = 0; x < aSize.width; x++) {
      size_t index = rowStart + x * 4;
      if (aMap.mData[index + 0] != 0 || aMap.mData[index + 1] != 0 || aMap.mData[index + 2] != 0) {
        return false;
      }
    }
  }
  return true;
}

nsresult nsCocoaUtils::CreateCGImageFromSurface(SourceSurface* aSurface, CGImageRef* aResult,
                                                bool* aIsEntirelyBlack) {
  RefPtr<DataSourceSurface> dataSurface;

  if (aSurface->GetFormat() == SurfaceFormat::B8G8R8A8) {
    dataSurface = aSurface->GetDataSurface();
  } else {
    // CGImageCreate only supports 16- and 32-bit bit-depth
    // Convert format to SurfaceFormat::B8G8R8A8
    dataSurface =
        gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(aSurface, SurfaceFormat::B8G8R8A8);
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

  if (aIsEntirelyBlack) {
    *aIsEntirelyBlack = ComputeIsEntirelyBlack(map, dataSurface->GetSize());
  }

  // Create a CGImageRef with the bits from the image, taking into account
  // the alpha ordering and endianness of the machine so we don't have to
  // touch the bits ourselves.
  CGDataProviderRef dataProvider = ::CGDataProviderCreateWithData(
      dataSurface.forget().take(), map.mData, map.mStride * height, data_ss_release_callback);
  CGColorSpaceRef colorSpace = ::CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
  *aResult = ::CGImageCreate(width, height, 8, 32, map.mStride, colorSpace,
                             kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst,
                             dataProvider, NULL, 0, kCGRenderingIntentDefault);
  ::CGColorSpaceRelease(colorSpace);
  ::CGDataProviderRelease(dataProvider);
  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsCocoaUtils::CreateNSImageFromCGImage(CGImageRef aInputImage, NSImage** aResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

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

  NSBitmapImageRep* offscreenRep =
      [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
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

  NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithBitmapImageRep:offscreenRep];
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

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsCocoaUtils::CreateNSImageFromImageContainer(imgIContainer* aImage, uint32_t aWhichFrame,
                                                       NSImage** aResult, CGFloat scaleFactor,
                                                       bool* aIsEntirelyBlack) {
  RefPtr<SourceSurface> surface;
  int32_t width = 0, height = 0;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);

  // Render a vector image at the correct resolution on a retina display
  if (aImage->GetType() == imgIContainer::TYPE_VECTOR && scaleFactor != 1.0f) {
    IntSize scaledSize = IntSize::Ceil(width * scaleFactor, height * scaleFactor);

    RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
        scaledSize, SurfaceFormat::B8G8R8A8);
    if (!drawTarget || !drawTarget->IsValid()) {
      NS_ERROR("Failed to create valid DrawTarget");
      return NS_ERROR_FAILURE;
    }

    RefPtr<gfxContext> context = gfxContext::CreateOrNull(drawTarget);
    MOZ_ASSERT(context);

    mozilla::image::ImgDrawResult res = aImage->Draw(
        context, scaledSize, ImageRegion::Create(scaledSize), aWhichFrame, SamplingFilter::POINT,
        /* no SVGImageContext */ Nothing(), imgIContainer::FLAG_SYNC_DECODE, 1.0);

    if (res != mozilla::image::ImgDrawResult::SUCCESS) {
      return NS_ERROR_FAILURE;
    }

    surface = drawTarget->Snapshot();
  } else {
    surface = aImage->GetFrame(aWhichFrame,
                               imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  }

  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  CGImageRef imageRef = NULL;
  nsresult rv = nsCocoaUtils::CreateCGImageFromSurface(surface, &imageRef, aIsEntirelyBlack);
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

nsresult nsCocoaUtils::CreateDualRepresentationNSImageFromImageContainer(imgIContainer* aImage,
                                                                         uint32_t aWhichFrame,
                                                                         NSImage** aResult,
                                                                         bool* aIsEntirelyBlack) {
  int32_t width = 0, height = 0;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);
  NSSize size = NSMakeSize(width, height);
  *aResult = [[NSImage alloc] init];
  [*aResult setSize:size];

  NSImage* newRepresentation = nil;
  nsresult rv = nsCocoaUtils::CreateNSImageFromImageContainer(
      aImage, aWhichFrame, &newRepresentation, 1.0f, aIsEntirelyBlack);
  if (NS_FAILED(rv) || !newRepresentation) {
    return NS_ERROR_FAILURE;
  }

  [[[newRepresentation representations] objectAtIndex:0] setSize:size];
  [*aResult addRepresentation:[[newRepresentation representations] objectAtIndex:0]];
  [newRepresentation release];
  newRepresentation = nil;

  rv = nsCocoaUtils::CreateNSImageFromImageContainer(aImage, aWhichFrame, &newRepresentation, 2.0f,
                                                     aIsEntirelyBlack);
  if (NS_FAILED(rv) || !newRepresentation) {
    return NS_ERROR_FAILURE;
  }

  [[[newRepresentation representations] objectAtIndex:0] setSize:size];
  [*aResult addRepresentation:[[newRepresentation representations] objectAtIndex:0]];
  [newRepresentation release];
  return NS_OK;
}

// static
void nsCocoaUtils::GetStringForNSString(const NSString* aSrc, nsAString& aDist) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!aSrc) {
    aDist.Truncate();
    return;
  }

  aDist.SetLength([aSrc length]);
  [aSrc getCharacters:reinterpret_cast<unichar*>(aDist.BeginWriting())
                range:NSMakeRange(0, [aSrc length])];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// static
NSString* nsCocoaUtils::ToNSString(const nsAString& aString) {
  if (aString.IsEmpty()) {
    return [NSString string];
  }
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aString.BeginReading())
                                 length:aString.Length()];
}

// static
NSString* nsCocoaUtils::ToNSString(const nsACString& aCString) {
  if (aCString.IsEmpty()) {
    return [NSString string];
  }
  return [[[NSString alloc] initWithBytes:aCString.BeginReading()
                                   length:aCString.Length()
                                 encoding:NSUTF8StringEncoding] autorelease];
}

// static
void nsCocoaUtils::GeckoRectToNSRect(const nsIntRect& aGeckoRect, NSRect& aOutCocoaRect) {
  aOutCocoaRect.origin.x = aGeckoRect.x;
  aOutCocoaRect.origin.y = aGeckoRect.y;
  aOutCocoaRect.size.width = aGeckoRect.width;
  aOutCocoaRect.size.height = aGeckoRect.height;
}

// static
void nsCocoaUtils::NSRectToGeckoRect(const NSRect& aCocoaRect, nsIntRect& aOutGeckoRect) {
  aOutGeckoRect.x = NSToIntRound(aCocoaRect.origin.x);
  aOutGeckoRect.y = NSToIntRound(aCocoaRect.origin.y);
  aOutGeckoRect.width = NSToIntRound(aCocoaRect.origin.x + aCocoaRect.size.width) - aOutGeckoRect.x;
  aOutGeckoRect.height =
      NSToIntRound(aCocoaRect.origin.y + aCocoaRect.size.height) - aOutGeckoRect.y;
}

// static
NSEvent* nsCocoaUtils::MakeNewCocoaEventWithType(NSEventType aEventType, NSEvent* aEvent) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSEvent* newEvent = [NSEvent keyEventWithType:aEventType
                                       location:[aEvent locationInWindow]
                                  modifierFlags:[aEvent modifierFlags]
                                      timestamp:[aEvent timestamp]
                                   windowNumber:[aEvent windowNumber]
                                        context:nil
                                     characters:[aEvent characters]
                    charactersIgnoringModifiers:[aEvent charactersIgnoringModifiers]
                                      isARepeat:[aEvent isARepeat]
                                        keyCode:[aEvent keyCode]];
  return newEvent;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

// static
NSEvent* nsCocoaUtils::MakeNewCococaEventFromWidgetEvent(const WidgetKeyboardEvent& aKeyEvent,
                                                         NSInteger aWindowNumber,
                                                         NSGraphicsContext* aContext) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSEventType eventType;
  if (aKeyEvent.mMessage == eKeyUp) {
    eventType = NSEventTypeKeyUp;
  } else {
    eventType = NSEventTypeKeyDown;
  }

  static const uint32_t sModifierFlagMap[][2] = {{MODIFIER_SHIFT, NSEventModifierFlagShift},
                                                 {MODIFIER_CONTROL, NSEventModifierFlagControl},
                                                 {MODIFIER_ALT, NSEventModifierFlagOption},
                                                 {MODIFIER_ALTGRAPH, NSEventModifierFlagOption},
                                                 {MODIFIER_META, NSEventModifierFlagCommand},
                                                 {MODIFIER_CAPSLOCK, NSEventModifierFlagCapsLock},
                                                 {MODIFIER_NUMLOCK, NSEventModifierFlagNumericPad}};

  NSUInteger modifierFlags = 0;
  for (uint32_t i = 0; i < ArrayLength(sModifierFlagMap); ++i) {
    if (aKeyEvent.mModifiers & sModifierFlagMap[i][0]) {
      modifierFlags |= sModifierFlagMap[i][1];
    }
  }

  NSString* characters;
  if (aKeyEvent.mCharCode) {
    characters =
        [NSString stringWithCharacters:reinterpret_cast<const unichar*>(&(aKeyEvent.mCharCode))
                                length:1];
  } else {
    uint32_t cocoaCharCode = nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(aKeyEvent.mKeyCode);
    characters = [NSString stringWithCharacters:reinterpret_cast<const unichar*>(&cocoaCharCode)
                                         length:1];
  }

  return [NSEvent keyEventWithType:eventType
                          location:NSMakePoint(0, 0)
                     modifierFlags:modifierFlags
                         timestamp:0
                      windowNumber:aWindowNumber
                           context:aContext
                        characters:characters
       charactersIgnoringModifiers:characters
                         isARepeat:NO
                           keyCode:0];  // Native key code not currently needed

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

// static
void nsCocoaUtils::InitNPCocoaEvent(NPCocoaEvent* aNPCocoaEvent) {
  memset(aNPCocoaEvent, 0, sizeof(NPCocoaEvent));
}

// static
void nsCocoaUtils::InitInputEvent(WidgetInputEvent& aInputEvent, NSEvent* aNativeEvent) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  aInputEvent.mModifiers = ModifiersForEvent(aNativeEvent);
  aInputEvent.mTime = PR_IntervalNow();
  aInputEvent.mTimeStamp = GetEventTimeStamp([aNativeEvent timestamp]);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// static
Modifiers nsCocoaUtils::ModifiersForEvent(NSEvent* aNativeEvent) {
  NSUInteger modifiers = aNativeEvent ? [aNativeEvent modifierFlags] : [NSEvent modifierFlags];
  Modifiers result = 0;
  if (modifiers & NSEventModifierFlagShift) {
    result |= MODIFIER_SHIFT;
  }
  if (modifiers & NSEventModifierFlagControl) {
    result |= MODIFIER_CONTROL;
  }
  if (modifiers & NSEventModifierFlagOption) {
    result |= MODIFIER_ALT;
    // Mac's option key is similar to other platforms' AltGr key.
    // Let's set AltGr flag when option key is pressed for consistency with
    // other platforms.
    result |= MODIFIER_ALTGRAPH;
  }
  if (modifiers & NSEventModifierFlagCommand) {
    result |= MODIFIER_META;
  }

  if (modifiers & NSEventModifierFlagCapsLock) {
    result |= MODIFIER_CAPSLOCK;
  }
  // Mac doesn't have NumLock key.  We can assume that NumLock is always locked
  // if user is using a keyboard which has numpad.  Otherwise, if user is using
  // a keyboard which doesn't have numpad, e.g., MacBook's keyboard, we can
  // assume that NumLock is always unlocked.
  // Unfortunately, we cannot know whether current keyboard has numpad or not.
  // We should notify locked state only when keys in numpad are pressed.
  // By this, web applications may not be confused by unexpected numpad key's
  // key event with unlocked state.
  if (modifiers & NSEventModifierFlagNumericPad) {
    result |= MODIFIER_NUMLOCK;
  }

  // Be aware, NSEventModifierFlagFunction is included when arrow keys, home key or some
  // other keys are pressed. We cannot check whether 'fn' key is pressed or
  // not by the flag.

  return result;
}

// static
UInt32 nsCocoaUtils::ConvertToCarbonModifier(NSUInteger aCocoaModifier) {
  UInt32 carbonModifier = 0;
  if (aCocoaModifier & NSEventModifierFlagCapsLock) {
    carbonModifier |= alphaLock;
  }
  if (aCocoaModifier & NSEventModifierFlagControl) {
    carbonModifier |= controlKey;
  }
  if (aCocoaModifier & NSEventModifierFlagOption) {
    carbonModifier |= optionKey;
  }
  if (aCocoaModifier & NSEventModifierFlagShift) {
    carbonModifier |= shiftKey;
  }
  if (aCocoaModifier & NSEventModifierFlagCommand) {
    carbonModifier |= cmdKey;
  }
  if (aCocoaModifier & NSEventModifierFlagNumericPad) {
    carbonModifier |= kEventKeyModifierNumLockMask;
  }
  if (aCocoaModifier & NSEventModifierFlagFunction) {
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
bool nsCocoaUtils::HiDPIEnabled() {
  if (!sHiDPIPrefInitialized) {
    sHiDPIPrefInitialized = true;

    int prefSetting = Preferences::GetInt("gfx.hidpi.enabled", 1);
    if (prefSetting <= 0) {
      return false;
    }

    // prefSetting is at least 1, need to check attached screens...

    int scaleFactors = 0;  // used as a bitset to track the screen types found
    NSEnumerator* screenEnum = [[NSScreen screens] objectEnumerator];
    while (NSScreen* screen = [screenEnum nextObject]) {
      NSDictionary* desc = [screen deviceDescription];
      if ([desc objectForKey:NSDeviceIsScreen] == nil) {
        continue;
      }
      CGFloat scale = [screen respondsToSelector:@selector(backingScaleFactor)]
                          ? [screen backingScaleFactor]
                          : 1.0;
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

void nsCocoaUtils::GetCommandsFromKeyEvent(NSEvent* aEvent,
                                           nsTArray<KeyBindingsCommand>& aCommands) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  MOZ_ASSERT(aEvent);

  static NativeKeyBindingsRecorder* sNativeKeyBindingsRecorder;
  if (!sNativeKeyBindingsRecorder) {
    sNativeKeyBindingsRecorder = [NativeKeyBindingsRecorder new];
  }

  [sNativeKeyBindingsRecorder startRecording:aCommands];

  // This will trigger 0 - N calls to doCommandBySelector: and insertText:
  [sNativeKeyBindingsRecorder interpretKeyEvents:[NSArray arrayWithObject:aEvent]];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@implementation NativeKeyBindingsRecorder

- (void)startRecording:(nsTArray<KeyBindingsCommand>&)aCommands {
  mCommands = &aCommands;
  mCommands->Clear();
}

- (void)doCommandBySelector:(SEL)aSelector {
  KeyBindingsCommand command = {aSelector, nil};

  mCommands->AppendElement(command);
}

- (void)insertText:(id)aString {
  KeyBindingsCommand command = {@selector(insertText:), aString};

  mCommands->AppendElement(command);
}

@end  // NativeKeyBindingsRecorder

struct KeyConversionData {
  const char* str;
  size_t strLength;
  uint32_t geckoKeyCode;
  uint32_t charCode;
};

static const KeyConversionData gKeyConversions[] = {

#define KEYCODE_ENTRY(aStr, aCode) \
  { #aStr, sizeof(#aStr) - 1, NS_##aStr, aCode }

// Some keycodes may have different name in KeyboardEvent from its key name.
#define KEYCODE_ENTRY2(aStr, aNSName, aCode) \
  { #aStr, sizeof(#aStr) - 1, NS_##aNSName, aCode }

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

uint32_t nsCocoaUtils::ConvertGeckoNameToMacCharCode(const nsAString& aKeyCodeName) {
  if (aKeyCodeName.IsEmpty()) {
    return 0;
  }

  nsAutoCString keyCodeName;
  LossyCopyUTF16toASCII(aKeyCodeName, keyCodeName);
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

uint32_t nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(uint32_t aKeyCode) {
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

NSEventModifierFlags nsCocoaUtils::ConvertWidgetModifiersToMacModifierFlags(
    nsIWidget::Modifiers aNativeModifiers) {
  if (!aNativeModifiers) {
    return 0;
  }
  struct ModifierFlagMapEntry {
    nsIWidget::Modifiers mWidgetModifier;
    NSEventModifierFlags mModifierFlags;
  };
  static constexpr ModifierFlagMapEntry sModifierFlagMap[] = {
      {nsIWidget::CAPS_LOCK, NSEventModifierFlagCapsLock},
      {nsIWidget::SHIFT_L, NSEventModifierFlagShift | 0x0002},
      {nsIWidget::SHIFT_R, NSEventModifierFlagShift | 0x0004},
      {nsIWidget::CTRL_L, NSEventModifierFlagControl | 0x0001},
      {nsIWidget::CTRL_R, NSEventModifierFlagControl | 0x2000},
      {nsIWidget::ALT_L, NSEventModifierFlagOption | 0x0020},
      {nsIWidget::ALT_R, NSEventModifierFlagOption | 0x0040},
      {nsIWidget::COMMAND_L, NSEventModifierFlagCommand | 0x0008},
      {nsIWidget::COMMAND_R, NSEventModifierFlagCommand | 0x0010},
      {nsIWidget::NUMERIC_KEY_PAD, NSEventModifierFlagNumericPad},
      {nsIWidget::HELP, NSEventModifierFlagHelp},
      {nsIWidget::FUNCTION, NSEventModifierFlagFunction}};

  NSEventModifierFlags modifierFlags = 0;
  for (const ModifierFlagMapEntry& entry : sModifierFlagMap) {
    if (aNativeModifiers & entry.mWidgetModifier) {
      modifierFlags |= entry.mModifierFlags;
    }
  }
  return modifierFlags;
}

NSMutableAttributedString* nsCocoaUtils::GetNSMutableAttributedString(
    const nsAString& aText, const nsTArray<mozilla::FontRange>& aFontRanges, const bool aIsVertical,
    const CGFloat aBackingScaleFactor) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN

  NSString* nsstr = nsCocoaUtils::ToNSString(aText);
  NSMutableAttributedString* attrStr =
      [[[NSMutableAttributedString alloc] initWithString:nsstr attributes:nil] autorelease];

  int32_t lastOffset = aText.Length();
  for (auto i = aFontRanges.Length(); i > 0; --i) {
    const FontRange& fontRange = aFontRanges[i - 1];
    NSString* fontName = nsCocoaUtils::ToNSString(fontRange.mFontName);
    CGFloat fontSize = fontRange.mFontSize / aBackingScaleFactor;
    NSFont* font = [NSFont fontWithName:fontName size:fontSize];
    if (!font) {
      font = [NSFont systemFontOfSize:fontSize];
    }

    NSDictionary* attrs = @{NSFontAttributeName : font};
    NSRange range = NSMakeRange(fontRange.mStartOffset, lastOffset - fontRange.mStartOffset);
    [attrStr setAttributes:attrs range:range];
    lastOffset = fontRange.mStartOffset;
  }

  if (aIsVertical) {
    [attrStr addAttribute:NSVerticalGlyphFormAttributeName
                    value:[NSNumber numberWithInt:1]
                    range:NSMakeRange(0, [attrStr length])];
  }

  return attrStr;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil)
}

TimeStamp nsCocoaUtils::GetEventTimeStamp(NSTimeInterval aEventTime) {
  if (!aEventTime) {
    // If the event is generated by a 3rd party application, its timestamp
    // may be 0.  In this case, just return current timestamp.
    // XXX Should we cache last event time?
    return TimeStamp::Now();
  }
  // The internal value of the macOS implementation of TimeStamp is based on
  // mach_absolute_time(), which measures "ticks" since boot.
  // Event timestamps are NSTimeIntervals (seconds) since boot. So the two time
  // representations already have the same base; we only need to convert
  // seconds into ticks.
  int64_t tick = BaseTimeDurationPlatformUtils::TicksFromMilliseconds(aEventTime * 1000.0);
  return TimeStamp::FromSystemTime(tick);
}

static NSString* ActionOnDoubleClickSystemPref() {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  NSString* kAppleActionOnDoubleClickKey = @"AppleActionOnDoubleClick";
  id value = [userDefaults objectForKey:kAppleActionOnDoubleClickKey];
  if ([value isKindOfClass:[NSString class]]) {
    return value;
  }
  return nil;
}

@interface NSWindow (NSWindowShouldZoomOnDoubleClick)
+ (BOOL)_shouldZoomOnDoubleClick;  // present on 10.7 and above
@end

bool nsCocoaUtils::ShouldZoomOnTitlebarDoubleClick() {
  if ([NSWindow respondsToSelector:@selector(_shouldZoomOnDoubleClick)]) {
    return [NSWindow _shouldZoomOnDoubleClick];
  }
  return [ActionOnDoubleClickSystemPref() isEqualToString:@"Maximize"];
}

bool nsCocoaUtils::ShouldMinimizeOnTitlebarDoubleClick() {
  // Check the system preferences.
  // We could also check -[NSWindow _shouldMiniaturizeOnDoubleClick]. It's not clear to me which
  // approach would be preferable; neither is public API.
  return [ActionOnDoubleClickSystemPref() isEqualToString:@"Minimize"];
}

// AVAuthorizationStatus is not needed unless we are running on 10.14.
// However, on pre-10.14 SDK's, AVAuthorizationStatus and its enum values
// are both defined and prohibited from use by compile-time checks. We
// define a copy of AVAuthorizationStatus to allow compilation on pre-10.14
// SDK's. The enum values must match what is defined in the 10.14 SDK.
// We use ASSERTS for 10.14 SDK builds to check the enum values match.
enum GeckoAVAuthorizationStatus : NSInteger {
  GeckoAVAuthorizationStatusNotDetermined = 0,
  GeckoAVAuthorizationStatusRestricted = 1,
  GeckoAVAuthorizationStatusDenied = 2,
  GeckoAVAuthorizationStatusAuthorized = 3
};

#if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_14
// Define authorizationStatusForMediaType: as returning
// GeckoAVAuthorizationStatus instead of AVAuthorizationStatus to allow
// compilation on pre-10.14 SDK's.
@interface AVCaptureDevice (GeckoAVAuthorizationStatus)
+ (GeckoAVAuthorizationStatus)authorizationStatusForMediaType:(AVMediaType)mediaType;
@end

@interface AVCaptureDevice (WithCompletionHandler)
+ (void)requestAccessForMediaType:(AVMediaType)mediaType
                completionHandler:(void (^)(BOOL granted))handler;
@end
#endif

static const char* AVMediaTypeToString(AVMediaType aType) {
  if (aType == AVMediaTypeVideo) {
    return "video";
  }

  if (aType == AVMediaTypeAudio) {
    return "audio";
  }

  return "unexpected type";
}

static void LogAuthorizationStatus(AVMediaType aType, int aState) {
  const char* stateString;

  switch (aState) {
    case GeckoAVAuthorizationStatusAuthorized:
      stateString = "AVAuthorizationStatusAuthorized";
      break;
    case GeckoAVAuthorizationStatusDenied:
      stateString = "AVAuthorizationStatusDenied";
      break;
    case GeckoAVAuthorizationStatusNotDetermined:
      stateString = "AVAuthorizationStatusNotDetermined";
      break;
    case GeckoAVAuthorizationStatusRestricted:
      stateString = "AVAuthorizationStatusRestricted";
      break;
    default:
      stateString = "Invalid state";
  }

  LOG("%s authorization status: %s\n", AVMediaTypeToString(aType), stateString);
}

static nsresult GetPermissionState(AVMediaType aMediaType, uint16_t& aState) {
  MOZ_ASSERT(aMediaType == AVMediaTypeVideo || aMediaType == AVMediaTypeAudio);

  // Only attempt to check authorization status on 10.14+.
  if (@available(macOS 10.14, *)) {
    GeckoAVAuthorizationStatus authStatus = static_cast<GeckoAVAuthorizationStatus>(
        [AVCaptureDevice authorizationStatusForMediaType:aMediaType]);
    LogAuthorizationStatus(aMediaType, authStatus);

    // Convert GeckoAVAuthorizationStatus to nsIOSPermissionRequest const
    switch (authStatus) {
      case GeckoAVAuthorizationStatusAuthorized:
        aState = nsIOSPermissionRequest::PERMISSION_STATE_AUTHORIZED;
        return NS_OK;
      case GeckoAVAuthorizationStatusDenied:
        aState = nsIOSPermissionRequest::PERMISSION_STATE_DENIED;
        return NS_OK;
      case GeckoAVAuthorizationStatusNotDetermined:
        aState = nsIOSPermissionRequest::PERMISSION_STATE_NOTDETERMINED;
        return NS_OK;
      case GeckoAVAuthorizationStatusRestricted:
        aState = nsIOSPermissionRequest::PERMISSION_STATE_RESTRICTED;
        return NS_OK;
      default:
        MOZ_ASSERT(false, "Invalid authorization status");
        return NS_ERROR_UNEXPECTED;
    }
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCocoaUtils::GetVideoCapturePermissionState(uint16_t& aPermissionState) {
  return GetPermissionState(AVMediaTypeVideo, aPermissionState);
}

nsresult nsCocoaUtils::GetAudioCapturePermissionState(uint16_t& aPermissionState) {
  return GetPermissionState(AVMediaTypeAudio, aPermissionState);
}

// Set |aPermissionState| to PERMISSION_STATE_AUTHORIZED if this application
// has already been granted permission to record the screen in macOS Security
// and Privacy system settings. If we do not have permission (because the user
// hasn't yet been asked yet or the user previously denied the prompt), use
// PERMISSION_STATE_DENIED. Returns NS_ERROR_NOT_IMPLEMENTED on macOS 10.14
// and earlier.
nsresult nsCocoaUtils::GetScreenCapturePermissionState(uint16_t& aPermissionState) {
  aPermissionState = nsIOSPermissionRequest::PERMISSION_STATE_NOTDETERMINED;

  // Only attempt to check screen recording authorization status on 10.15+.
  // On earlier macOS versions, screen recording is allowed by default.
  if (@available(macOS 10.15, *)) {
    if (!StaticPrefs::media_macos_screenrecording_oscheck_enabled()) {
      aPermissionState = nsIOSPermissionRequest::PERMISSION_STATE_AUTHORIZED;
      LOG("screen authorization status: authorized (test disabled via pref)");
      return NS_OK;
    }

    // Unlike with camera and microphone capture, there is no support for
    // checking the screen recording permission status. Instead, an application
    // can use the presence of window names (which are privacy sensitive) in
    // the window info list as an indication. The list only includes window
    // names if the calling application has been authorized to record the
    // screen. We use the window name, window level, and owning PID as
    // heuristics to determine if we have screen recording permission.
    AutoCFRelease<CFArrayRef> windowArray =
        CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
    if (!windowArray) {
      LOG("GetScreenCapturePermissionState() ERROR: got NULL window info list");
      return NS_ERROR_UNEXPECTED;
    }

    int32_t windowLevelDock = CGWindowLevelForKey(kCGDockWindowLevelKey);
    int32_t windowLevelNormal = CGWindowLevelForKey(kCGNormalWindowLevelKey);
    LOG("GetScreenCapturePermissionState(): DockWindowLevel: %d, "
        "NormalWindowLevel: %d",
        windowLevelDock, windowLevelNormal);

    int32_t thisPid = [[NSProcessInfo processInfo] processIdentifier];

    CFIndex windowCount = CFArrayGetCount(windowArray);
    LOG("GetScreenCapturePermissionState() returned %ld windows", windowCount);
    if (windowCount == 0) {
      return NS_ERROR_UNEXPECTED;
    }

    for (CFIndex i = 0; i < windowCount; i++) {
      CFDictionaryRef windowDict =
          reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowArray, i));

      // Get the window owner's PID
      int32_t windowOwnerPid = -1;
      CFNumberRef windowPidRef =
          reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(windowDict, kCGWindowOwnerPID));
      if (!windowPidRef || !CFNumberGetValue(windowPidRef, kCFNumberIntType, &windowOwnerPid)) {
        LOG("GetScreenCapturePermissionState() ERROR: failed to get window owner");
        continue;
      }

      // Our own window names are always readable and
      // therefore not relevant to the heuristic.
      if (thisPid == windowOwnerPid) {
        continue;
      }

      CFStringRef windowName =
          reinterpret_cast<CFStringRef>(CFDictionaryGetValue(windowDict, kCGWindowName));
      if (!windowName) {
        continue;
      }

      CFNumberRef windowLayerRef =
          reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(windowDict, kCGWindowLayer));
      int32_t windowLayer;
      if (!windowLayerRef || !CFNumberGetValue(windowLayerRef, kCFNumberIntType, &windowLayer)) {
        LOG("GetScreenCapturePermissionState() ERROR: failed to get layer");
        continue;
      }

      // If we have a window name and the window is in the dock or normal window
      // level, and for another process, assume we have screen recording access.
      LOG("GetScreenCapturePermissionState(): windowLayer: %d", windowLayer);
      if (windowLayer == windowLevelDock || windowLayer == windowLevelNormal) {
        aPermissionState = nsIOSPermissionRequest::PERMISSION_STATE_AUTHORIZED;
        LOG("screen authorization status: authorized");
        return NS_OK;
      }
    }

    aPermissionState = nsIOSPermissionRequest::PERMISSION_STATE_DENIED;
    LOG("screen authorization status: not authorized");
    return NS_OK;
  }

  LOG("GetScreenCapturePermissionState(): nothing to do, not on 10.15+");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCocoaUtils::RequestVideoCapturePermission(RefPtr<Promise>& aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  return nsCocoaUtils::RequestCapturePermission(AVMediaTypeVideo, aPromise, sVideoCapturePromises,
                                                VideoCompletionHandler);
}

nsresult nsCocoaUtils::RequestAudioCapturePermission(RefPtr<Promise>& aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  return nsCocoaUtils::RequestCapturePermission(AVMediaTypeAudio, aPromise, sAudioCapturePromises,
                                                AudioCompletionHandler);
}

//
// Stores |aPromise| on |aPromiseList| and starts an asynchronous media
// capture request for the given media type |aType|. If we are already
// waiting for a capture request for this media type, don't start a new
// request. |aHandler| is invoked on an arbitrary dispatch queue when the
// request completes and must resolve any waiting Promises on the main
// thread.
//
nsresult nsCocoaUtils::RequestCapturePermission(AVMediaType aType, RefPtr<Promise>& aPromise,
                                                PromiseArray& aPromiseList,
                                                void (^aHandler)(BOOL granted)) {
  MOZ_ASSERT(aType == AVMediaTypeVideo || aType == AVMediaTypeAudio);
#if defined(MAC_OS_X_VERSION_10_14)
  // Ensure our enum constants match. We can only do this when
  // compiling on 10.14+ because AVAuthorizationStatus is
  // prohibited by preprocessor checks on earlier OS versions.
  if (@available(macOS 10.14, *)) {
    static_assert(
        (int)GeckoAVAuthorizationStatusNotDetermined == (int)AVAuthorizationStatusNotDetermined,
        "GeckoAVAuthorizationStatusNotDetermined  does not match");
    static_assert((int)GeckoAVAuthorizationStatusRestricted == (int)AVAuthorizationStatusRestricted,
                  "GeckoAVAuthorizationStatusRestricted does not match");
    static_assert((int)GeckoAVAuthorizationStatusDenied == (int)AVAuthorizationStatusDenied,
                  "GeckoAVAuthorizationStatusDenied does not match");
    static_assert((int)GeckoAVAuthorizationStatusAuthorized == (int)AVAuthorizationStatusAuthorized,
                  "GeckoAVAuthorizationStatusAuthorized does not match");
  }
#endif
  LOG("RequestCapturePermission(%s)", AVMediaTypeToString(aType));

  // Only attempt to request authorization on 10.14+.
  if (@available(macOS 10.14, *)) {
    sMediaCaptureMutex.Lock();

    // Initialize our list of promises on first invocation
    if (aPromiseList == nullptr) {
      aPromiseList = new nsTArray<RefPtr<Promise>>;
      ClearOnShutdown(&aPromiseList);
    }

    aPromiseList->AppendElement(aPromise);
    size_t nPromises = aPromiseList->Length();

    sMediaCaptureMutex.Unlock();

    LOG("RequestCapturePermission(%s): %ld promise(s) unresolved", AVMediaTypeToString(aType),
        nPromises);

    // If we had one or more more existing promises waiting to be resolved
    // by the completion handler, we don't need to start another request.
    if (nPromises > 1) {
      return NS_OK;
    }

    // Start the request
    [AVCaptureDevice requestAccessForMediaType:aType completionHandler:aHandler];
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// Audio capture request completion handler. Called from an arbitrary
// dispatch queue.
//
void (^nsCocoaUtils::AudioCompletionHandler)(BOOL) = ^void(BOOL granted) {
  nsCocoaUtils::ResolveAudioCapturePromises(granted);
};

//
// Video capture request completion handler. Called from an arbitrary
// dispatch queue.
//
void (^nsCocoaUtils::VideoCompletionHandler)(BOOL) = ^void(BOOL granted) {
  nsCocoaUtils::ResolveVideoCapturePromises(granted);
};

void nsCocoaUtils::ResolveMediaCapturePromises(bool aGranted, PromiseArray& aPromiseList) {
  StaticMutexAutoLock lock(sMediaCaptureMutex);

  // Remove each promise from the list and resolve it.
  while (aPromiseList->Length() > 0) {
    RefPtr<Promise> promise = aPromiseList->PopLastElement();

    // Resolve on main thread
    nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
        "ResolveMediaAccessPromise",
        [aGranted, aPromise = std::move(promise)]() { aPromise->MaybeResolve(aGranted); }));
    NS_DispatchToMainThread(runnable.forget());
  }
}

void nsCocoaUtils::ResolveAudioCapturePromises(bool aGranted) {
  // Resolve on main thread
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction("ResolveAudioCapturePromise", [aGranted]() {
    ResolveMediaCapturePromises(aGranted, sAudioCapturePromises);
  }));
  NS_DispatchToMainThread(runnable.forget());
}

//
// Attempt to trigger a dialog requesting permission to record the screen.
// Unlike with the camera and microphone, there is no API to request permission
// to record the screen or to receive a callback when permission is explicitly
// allowed or denied. Here we attempt to trigger the dialog by attempting to
// capture a 1x1 pixel section of the screen. The permission dialog is not
// guaranteed to be displayed because the user may have already been prompted
// in which case macOS does not display the dialog again.
//
nsresult nsCocoaUtils::MaybeRequestScreenCapturePermission() {
  LOG("MaybeRequestScreenCapturePermission()");
  AutoCFRelease<CGImageRef> image =
      CGDisplayCreateImageForRect(kCGDirectMainDisplay, CGRectMake(0, 0, 1, 1));
  return NS_OK;
}

void nsCocoaUtils::ResolveVideoCapturePromises(bool aGranted) {
  // Resolve on main thread
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction("ResolveVideoCapturePromise", [aGranted]() {
    ResolveMediaCapturePromises(aGranted, sVideoCapturePromises);
  }));
  NS_DispatchToMainThread(runnable.forget());
}
