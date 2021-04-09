/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/Unused.h"

#include <unistd.h>
#include <math.h>

#include "nsChildView.h"
#include "nsCocoaWindow.h"

#include "mozilla/Maybe.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/WheelHandlingHelper.h"  // for WheelDeltaAdjustmentStrategy
#include "mozilla/WritingModes.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/SimpleGestureEventBinding.h"
#include "mozilla/dom/WheelEventBinding.h"

#include "nsArrayUtils.h"
#include "nsExceptionHandler.h"
#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsToolkit.h"
#include "nsCRT.h"

#include "nsFontMetrics.h"
#include "nsIRollupListener.h"
#include "nsViewManager.h"
#include "nsIFile.h"
#include "nsILocalFileMac.h"
#include "nsGfxCIID.h"
#include "nsStyleConsts.h"
#include "nsIWidgetListener.h"
#include "nsIScreen.h"

#include "nsDragService.h"
#include "nsClipboard.h"
#include "nsCursorManager.h"
#include "nsWindowMap.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsMenuUtilsX.h"
#include "nsMenuBarX.h"
#include "NativeKeyBindings.h"
#include "MacThemeGeometryType.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "gfxUtils.h"
#include "nsRegion.h"
#include "Layers.h"
#include "ClientLayerManager.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "GfxTexturesReporter.h"
#include "GLTextureImage.h"
#include "GLContextProvider.h"
#include "GLContextCGL.h"
#include "OGLShaderProgram.h"
#include "ScopedGLHelpers.h"
#include "HeapCopyOfStackArray.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/NativeLayerCA.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/widget/CompositorWidget.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BorrowedContext.h"
#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#  include "mozilla/a11y/Platform.h"
#endif

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_ui.h"

#include <dlfcn.h>

#include <ApplicationServices/ApplicationServices.h>

#include "GeckoProfiler.h"

#include "mozilla/layers/ChromeProcessController.h"
#include "nsLayoutUtils.h"
#include "InputData.h"
#include "SwipeTracker.h"
#include "VibrancyManager.h"
#include "nsNativeThemeCocoa.h"
#include "nsIDOMWindowUtils.h"
#include "Units.h"
#include "UnitTransforms.h"
#include "mozilla/UniquePtrExtensions.h"
#include "CustomCocoaEvents.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;
using namespace mozilla::widget;

using mozilla::gfx::Matrix4x4;

#undef DEBUG_UPDATE
#undef INVALIDATE_DEBUGGING  // flash areas as they are invalidated

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION 100

LazyLogModule sCocoaLog("nsCocoaWidgets");

extern "C" {
CG_EXTERN void CGContextResetCTM(CGContextRef);
CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
CG_EXTERN void CGContextResetClip(CGContextRef);

typedef CFTypeRef CGSRegionObj;
CGError CGSNewRegionWithRect(const CGRect* rect, CGSRegionObj* outRegion);
CGError CGSNewRegionWithRectList(const CGRect* rects, int rectCount, CGSRegionObj* outRegion);
}

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu;  // Application menu shared by all menubars

extern nsIArray* gDraggedTransferables;

ChildView* ChildViewMouseTracker::sLastMouseEventView = nil;
NSEvent* ChildViewMouseTracker::sLastMouseMoveEvent = nil;
NSWindow* ChildViewMouseTracker::sWindowUnderMouse = nil;
NSPoint ChildViewMouseTracker::sLastScrollEventScreenLocation = NSZeroPoint;

#ifdef INVALIDATE_DEBUGGING
static void blinkRect(Rect* r);
static void blinkRgn(RgnHandle rgn);
#endif

bool gUserCancelledDrag = false;

uint32_t nsChildView::sLastInputEventCount = 0;

static bool sIsTabletPointerActivated = false;

static uint32_t sUniqueKeyEventId = 0;

// The view that will do our drawing or host our NSOpenGLContext or Core Animation layer.
@interface PixelHostingView : NSView {
}

@end

@interface ChildView (Private)

// sets up our view, attaching it to its owning gecko view
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild;

// set up a gecko mouse event based on a cocoa mouse event
- (void)convertCocoaMouseWheelEvent:(NSEvent*)aMouseEvent
                       toGeckoEvent:(WidgetWheelEvent*)outWheelEvent;
- (void)convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(WidgetInputEvent*)outGeckoEvent;
- (void)convertCocoaTabletPointerEvent:(NSEvent*)aMouseEvent
                          toGeckoEvent:(WidgetMouseEvent*)outGeckoEvent;
- (NSMenu*)contextMenu;

- (void)markLayerForDisplay;
- (CALayer*)rootCALayer;
- (void)updateRootCALayer;

#ifdef ACCESSIBILITY
- (id<mozAccessible>)accessible;
#endif

- (LayoutDeviceIntPoint)convertWindowCoordinates:(NSPoint)aPoint;
- (LayoutDeviceIntPoint)convertWindowCoordinatesRoundDown:(NSPoint)aPoint;

- (BOOL)inactiveWindowAcceptsMouseEvent:(NSEvent*)aEvent;
- (void)updateWindowDraggableState;

- (bool)beginOrEndGestureForEventPhase:(NSEvent*)aEvent;

- (bool)shouldConsiderStartingSwipeFromEvent:(NSEvent*)aEvent;

@end

#pragma mark -

// Flips a screen coordinate from a point in the cocoa coordinate system (bottom-left rect) to a
// point that is a "flipped" cocoa coordinate system (starts in the top-left).
static inline void FlipCocoaScreenCoordinate(NSPoint& inPoint) {
  inPoint.y = nsCocoaUtils::FlippedScreenY(inPoint.y);
}

namespace mozilla {

struct SwipeEventQueue {
  SwipeEventQueue(uint32_t aAllowedDirections, uint64_t aInputBlockId)
      : allowedDirections(aAllowedDirections), inputBlockId(aInputBlockId) {}

  nsTArray<PanGestureInput> queuedEvents;
  uint32_t allowedDirections;
  uint64_t inputBlockId;
};

}  // namespace mozilla

#pragma mark -

nsChildView::nsChildView()
    : nsBaseWidget(),
      mView(nullptr),
      mParentView(nil),
      mParentWidget(nullptr),
      mCompositingLock("ChildViewCompositing"),
      mBackingScaleFactor(0.0),
      mVisible(false),
      mDrawing(false),
      mIsDispatchPaint(false),
      mCurrentPanGestureBelongsToSwipe{false} {}

nsChildView::~nsChildView() {
  if (mSwipeTracker) {
    mSwipeTracker->Destroy();
    mSwipeTracker = nullptr;
  }

  // Notify the children that we're gone.  childView->ResetParent() can change
  // our list of children while it's being iterated, so the way we iterate the
  // list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    nsChildView* childView = static_cast<nsChildView*>(kid);
    kid = kid->GetPrevSibling();
    childView->ResetParent();
  }

  NS_WARNING_ASSERTION(mOnDestroyCalled, "nsChildView object destroyed without calling Destroy()");

  if (mContentLayer) {
    mNativeLayerRoot->RemoveLayer(mContentLayer);  // safe if already removed
  }

  DestroyCompositor();

  // An nsChildView object that was in use can be destroyed without Destroy()
  // ever being called on it.  So we also need to do a quick, safe cleanup
  // here (it's too late to just call Destroy(), which can cause crashes).
  // It's particularly important to make sure widgetDestroyed is called on our
  // mView -- this method NULLs mView's mGeckoChild, and NULL checks on
  // mGeckoChild are used throughout the ChildView class to tell if it's safe
  // to use a ChildView object.
  [mView widgetDestroyed];  // Safe if mView is nil.
  mParentWidget = nil;
  TearDownView();  // Safe if called twice.
}

nsresult nsChildView::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                             const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Because the hidden window is created outside of an event loop,
  // we need to provide an autorelease pool to avoid leaking cocoa objects
  // (see bug 559075).
  nsAutoreleasePool localPool;

  mBounds = aRect;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  BaseCreate(aParent, aInitData);

  mParentView = nil;
  if (aParent) {
    // This is the popup window case. aParent is the nsCocoaWindow for the
    // popup window, and mParentView will be its content view.
    mParentView = (NSView*)aParent->GetNativeData(NS_NATIVE_WIDGET);
    mParentWidget = aParent;
  } else {
    // This is the top-level window case.
    // aNativeParent will be the contentView of our window, since that's what
    // nsCocoaWindow returns when asked for an NS_NATIVE_VIEW.
    // We do not have a direct "parent widget" association with the top level
    // window's nsCocoaWindow object.
    mParentView = reinterpret_cast<NSView*>(aNativeParent);
  }

  // create our parallel NSView and hook it up to our parent. Recall
  // that NS_NATIVE_WIDGET is the NSView.
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mParentView);
  NSRect r = nsCocoaUtils::DevPixelsToCocoaPoints(mBounds, scaleFactor);
  mView = [[ChildView alloc] initWithFrame:r geckoChild:this];

  mNativeLayerRoot = NativeLayerRootCA::CreateForCALayer([mView rootCALayer]);
  mNativeLayerRoot->SetBackingScale(scaleFactor);

  // If this view was created in a Gecko view hierarchy, the initial state
  // is hidden.  If the view is attached only to a native NSView but has
  // no Gecko parent (as in embedding), the initial state is visible.
  if (mParentWidget)
    [mView setHidden:YES];
  else
    mVisible = true;

  // Hook it up in the NSView hierarchy.
  if (mParentView) {
    [mParentView addSubview:mView];
  }

  // if this is a ChildView, make sure that our per-window data
  // is set up
  if ([mView isKindOfClass:[ChildView class]])
    [[WindowDataMap sharedWindowDataMap] ensureDataForWindow:[mView window]];

  NS_ASSERTION(!mTextInputHandler, "mTextInputHandler has already existed");
  mTextInputHandler = new TextInputHandler(this, mView);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsChildView::TearDownView() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mView) return;

  NSWindow* win = [mView window];
  NSResponder* responder = [win firstResponder];

  // We're being unhooked from the view hierarchy, don't leave our view
  // or a child view as the window first responder.
  if (responder && [responder isKindOfClass:[NSView class]] &&
      [(NSView*)responder isDescendantOf:mView]) {
    [win makeFirstResponder:[mView superview]];
  }

  // If mView is win's contentView, win (mView's NSWindow) "owns" mView --
  // win has retained mView, and will detach it from the view hierarchy and
  // release it when necessary (when win is itself destroyed (in a call to
  // [win dealloc])).  So all we need to do here is call [mView release] (to
  // match the call to [mView retain] in nsChildView::StandardCreate()).
  // Also calling [mView removeFromSuperviewWithoutNeedingDisplay] causes
  // mView to be released again and dealloced, while remaining win's
  // contentView.  So if we do that here, win will (for a short while) have
  // an invalid contentView (for the consequences see bmo bugs 381087 and
  // 374260).
  if ([mView isEqual:[win contentView]]) {
    [mView release];
  } else {
    // Stop NSView hierarchy being changed during [ChildView drawRect:]
    [mView performSelectorOnMainThread:@selector(delayedTearDown)
                            withObject:nil
                         waitUntilDone:false];
  }
  mView = nil;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsCocoaWindow* nsChildView::GetAppWindowWidget() const {
  id windowDelegate = [[mView window] delegate];
  if (windowDelegate && [windowDelegate isKindOfClass:[WindowDelegate class]]) {
    return [(WindowDelegate*)windowDelegate geckoWidget];
  }
  return nullptr;
}

void nsChildView::Destroy() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Make sure that no composition is in progress while disconnecting
  // ourselves from the view.
  MutexAutoLock lock(mCompositingLock);

  if (mOnDestroyCalled) return;
  mOnDestroyCalled = true;

  // Stuff below may delete the last ref to this
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  [mView widgetDestroyed];

  nsBaseWidget::Destroy();

  NotifyWindowDestroyed();
  mParentWidget = nil;

  TearDownView();

  nsBaseWidget::OnDestroy();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

#pragma mark -

#if 0
static void PrintViewHierarchy(NSView *view)
{
  while (view) {
    NSLog(@"  view is %x, frame %@", view, NSStringFromRect([view frame]));
    view = [view superview];
  }
}
#endif

// Return native data according to aDataType
void* nsChildView::GetNativeData(uint32_t aDataType) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  void* retVal = nullptr;

  switch (aDataType) {
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = (void*)mView;
      break;

    case NS_NATIVE_WINDOW:
      retVal = [mView window];
      break;

    case NS_NATIVE_GRAPHIC:
      NS_ERROR("Requesting NS_NATIVE_GRAPHIC on a Mac OS X child view!");
      retVal = nullptr;
      break;

    case NS_NATIVE_OFFSETX:
      retVal = 0;
      break;

    case NS_NATIVE_OFFSETY:
      retVal = 0;
      break;

    case NS_RAW_NATIVE_IME_CONTEXT:
      retVal = GetPseudoIMEContext();
      if (retVal) {
        break;
      }
      retVal = [mView inputContext];
      // If input context isn't available on this widget, we should set |this|
      // instead of nullptr since if this returns nullptr, IMEStateManager
      // cannot manage composition with TextComposition instance.  Although,
      // this case shouldn't occur.
      if (NS_WARN_IF(!retVal)) {
        retVal = this;
      }
      break;

    case NS_NATIVE_WINDOW_WEBRTC_DEVICE_ID: {
      NSWindow* win = [mView window];
      if (win) {
        retVal = (void*)[win windowNumber];
      }
      break;
    }
  }

  return retVal;

  NS_OBJC_END_TRY_BLOCK_RETURN(nullptr);
}

#pragma mark -

void nsChildView::SuppressAnimation(bool aSuppress) {
  GetAppWindowWidget()->SuppressAnimation(aSuppress);
}

bool nsChildView::IsVisible() const {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mVisible) {
    return mVisible;
  }

  if (!GetAppWindowWidget()->IsVisible()) {
    return false;
  }

  // mVisible does not accurately reflect the state of a hidden tabbed view
  // so verify that the view has a window as well
  // then check native widget hierarchy visibility
  return ([mView window] != nil) && !NSIsEmptyRect([mView visibleRect]);

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

// Some NSView methods (e.g. setFrame and setHidden) invalidate the view's
// bounds in our window. However, we don't want these invalidations because
// they are unnecessary and because they actually slow us down since we
// block on the compositor inside drawRect.
// When we actually need something invalidated, there will be an explicit call
// to Invalidate from Gecko, so turning these automatic invalidations off
// won't hurt us in the non-OMTC case.
// The invalidations inside these NSView methods happen via a call to the
// private method -[NSWindow _setNeedsDisplayInRect:]. Our BaseWindow
// implementation of that method is augmented to let us ignore those calls
// using -[BaseWindow disable/enableSetNeedsDisplay].
static void ManipulateViewWithoutNeedingDisplay(NSView* aView, void (^aCallback)()) {
  BaseWindow* win = nil;
  if ([[aView window] isKindOfClass:[BaseWindow class]]) {
    win = (BaseWindow*)[aView window];
  }
  [win disableSetNeedsDisplay];
  aCallback();
  [win enableSetNeedsDisplay];
}

// Hide or show this component
void nsChildView::Show(bool aState) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (aState != mVisible) {
    // Provide an autorelease pool because this gets called during startup
    // on the "hidden window", resulting in cocoa object leakage if there's
    // no pool in place.
    nsAutoreleasePool localPool;

    ManipulateViewWithoutNeedingDisplay(mView, ^{
      [mView setHidden:!aState];
    });

    mVisible = aState;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Change the parent of this widget
void nsChildView::SetParent(nsIWidget* aNewParent) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mOnDestroyCalled) return;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  if (mParentWidget) {
    mParentWidget->RemoveChild(this);
  }

  if (aNewParent) {
    ReparentNativeWidget(aNewParent);
  } else {
    [mView removeFromSuperview];
    mParentView = nil;
  }

  mParentWidget = aNewParent;

  if (mParentWidget) {
    mParentWidget->AddChild(this);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsChildView::ReparentNativeWidget(nsIWidget* aNewParent) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  MOZ_ASSERT(aNewParent, "null widget");

  if (mOnDestroyCalled) return;

  NSView<mozView>* newParentView = (NSView<mozView>*)aNewParent->GetNativeData(NS_NATIVE_WIDGET);
  NS_ENSURE_TRUE_VOID(newParentView);

  // we hold a ref to mView, so this is safe
  [mView removeFromSuperview];
  mParentView = newParentView;
  [mParentView addSubview:mView];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsChildView::ResetParent() {
  if (!mOnDestroyCalled) {
    if (mParentWidget) mParentWidget->RemoveChild(this);
    if (mView) [mView removeFromSuperview];
  }
  mParentWidget = nullptr;
}

nsIWidget* nsChildView::GetParent() { return mParentWidget; }

float nsChildView::GetDPI() {
  float dpi = 96.0;
  nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
  if (screen) {
    screen->GetDpi(&dpi);
  }
  return dpi;
}

void nsChildView::Enable(bool aState) {}

bool nsChildView::IsEnabled() const { return true; }

void nsChildView::SetFocus(Raise, mozilla::dom::CallerType aCallerType) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSWindow* window = [mView window];
  if (window) [window makeFirstResponder:mView];
  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Override to set the cursor on the mac
void nsChildView::SetCursor(nsCursor aDefaultCursor, imgIContainer* aImageCursor,
                            uint32_t aHotspotX, uint32_t aHotspotY) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if ([mView isDragInProgress]) return;  // Don't change the cursor during dragging.

  if (aImageCursor) {
    nsresult rv = [[nsCursorManager sharedInstance] setCursorWithImage:aImageCursor
                                                              hotSpotX:aHotspotX
                                                              hotSpotY:aHotspotY
                                                           scaleFactor:BackingScaleFactor()];
    if (NS_SUCCEEDED(rv)) {
      return;
    }
  }

  nsBaseWidget::SetCursor(aDefaultCursor, nullptr, 0, 0);
  [[nsCursorManager sharedInstance] setCursor:aDefaultCursor];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

#pragma mark -

// Get this component dimension
LayoutDeviceIntRect nsChildView::GetBounds() {
  return !mView ? mBounds : CocoaPointsToDevPixels([mView frame]);
}

LayoutDeviceIntRect nsChildView::GetClientBounds() {
  LayoutDeviceIntRect rect = GetBounds();
  if (!mParentWidget) {
    // For top level widgets we want the position on screen, not the position
    // of this view inside the window.
    rect.MoveTo(WidgetToScreenOffset());
  }
  return rect;
}

LayoutDeviceIntRect nsChildView::GetScreenBounds() {
  LayoutDeviceIntRect rect = GetBounds();
  rect.MoveTo(WidgetToScreenOffset());
  return rect;
}

double nsChildView::GetDefaultScaleInternal() { return BackingScaleFactor(); }

CGFloat nsChildView::BackingScaleFactor() const {
  if (mBackingScaleFactor > 0.0) {
    return mBackingScaleFactor;
  }
  if (!mView) {
    return 1.0;
  }
  mBackingScaleFactor = nsCocoaUtils::GetBackingScaleFactor(mView);
  return mBackingScaleFactor;
}

void nsChildView::BackingScaleFactorChanged() {
  CGFloat newScale = nsCocoaUtils::GetBackingScaleFactor(mView);

  // ignore notification if it hasn't really changed (or maybe we have
  // disabled HiDPI mode via prefs)
  if (mBackingScaleFactor == newScale) {
    return;
  }

  SuspendAsyncCATransactions();
  mBackingScaleFactor = newScale;
  NSRect frame = [mView frame];
  mBounds = nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, newScale);

  mNativeLayerRoot->SetBackingScale(mBackingScaleFactor);

  if (mWidgetListener && !mWidgetListener->GetAppWindow()) {
    if (PresShell* presShell = mWidgetListener->GetPresShell()) {
      presShell->BackingScaleFactorChanged();
    }
  }
}

int32_t nsChildView::RoundsWidgetCoordinatesTo() {
  if (BackingScaleFactor() == 2.0) {
    return 2;
  }
  return 1;
}

// Move this component, aX and aY are in the parent widget coordinate system
void nsChildView::Move(double aX, double aY) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  int32_t x = NSToIntRound(aX);
  int32_t y = NSToIntRound(aY);

  if (!mView || (mBounds.x == x && mBounds.y == y)) return;

  mBounds.x = x;
  mBounds.y = y;

  ManipulateViewWithoutNeedingDisplay(mView, ^{
    [mView setFrame:DevPixelsToCocoaPoints(mBounds)];
  });

  NotifyRollupGeometryChange();
  ReportMoveEvent();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsChildView::Resize(double aWidth, double aHeight, bool aRepaint) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);

  if (!mView || (mBounds.width == width && mBounds.height == height)) return;

  SuspendAsyncCATransactions();
  mBounds.width = width;
  mBounds.height = height;

  ManipulateViewWithoutNeedingDisplay(mView, ^{
    [mView setFrame:DevPixelsToCocoaPoints(mBounds)];
  });

  if (mVisible && aRepaint) {
    [[mView pixelHostingView] setNeedsDisplay:YES];
  }

  NotifyRollupGeometryChange();
  ReportSizeEvent();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsChildView::Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  int32_t x = NSToIntRound(aX);
  int32_t y = NSToIntRound(aY);
  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);

  BOOL isMoving = (mBounds.x != x || mBounds.y != y);
  BOOL isResizing = (mBounds.width != width || mBounds.height != height);
  if (!mView || (!isMoving && !isResizing)) return;

  if (isMoving) {
    mBounds.x = x;
    mBounds.y = y;
  }
  if (isResizing) {
    SuspendAsyncCATransactions();
    mBounds.width = width;
    mBounds.height = height;
  }

  ManipulateViewWithoutNeedingDisplay(mView, ^{
    [mView setFrame:DevPixelsToCocoaPoints(mBounds)];
  });

  if (mVisible && aRepaint) {
    [[mView pixelHostingView] setNeedsDisplay:YES];
  }

  NotifyRollupGeometryChange();
  if (isMoving) {
    ReportMoveEvent();
    if (mOnDestroyCalled) return;
  }
  if (isResizing) ReportSizeEvent();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// The following three methods are primarily an attempt to avoid glitches during
// window resizing.
// Here's some background on how these glitches come to be:
// CoreAnimation transactions are per-thread. They don't nest across threads.
// If you submit a transaction on the main thread and a transaction on a
// different thread, the two will race to the window server and show up on the
// screen in the order that they happen to arrive in at the window server.
// When the window size changes, there's another event that needs to be
// synchronized with: the window "shape" change. Cocoa has built-in synchronization
// mechanics that make sure that *main thread* window paints during window resizes
// are synchronized properly with the window shape change. But no such built-in
// synchronization exists for CATransactions that are triggered on a non-main
// thread.
// To cope with this, we define a "danger zone" during which we simply avoid
// triggering any CATransactions on a non-main thread (called "async" CATransactions
// here). This danger zone starts at the earliest opportunity at which we know
// about the size change, which is nsChildView::Resize, and ends at a point at
// which we know for sure that the paint has been handled completely, which is
// when we return to the event loop after layer display.
void nsChildView::SuspendAsyncCATransactions() {
  if (mUnsuspendAsyncCATransactionsRunnable) {
    mUnsuspendAsyncCATransactionsRunnable->Cancel();
    mUnsuspendAsyncCATransactionsRunnable = nullptr;
  }

  // Make sure that there actually will be a CATransaction on the main thread
  // during which we get a chance to schedule unsuspension. Otherwise we might
  // accidentally stay suspended indefinitely.
  [mView markLayerForDisplay];

  mNativeLayerRoot->SuspendOffMainThreadCommits();
}

void nsChildView::MaybeScheduleUnsuspendAsyncCATransactions() {
  if (mNativeLayerRoot->AreOffMainThreadCommitsSuspended() &&
      !mUnsuspendAsyncCATransactionsRunnable) {
    mUnsuspendAsyncCATransactionsRunnable =
        NewCancelableRunnableMethod("nsChildView::MaybeScheduleUnsuspendAsyncCATransactions", this,
                                    &nsChildView::UnsuspendAsyncCATransactions);
    NS_DispatchToMainThread(mUnsuspendAsyncCATransactionsRunnable);
  }
}

void nsChildView::UnsuspendAsyncCATransactions() {
  mUnsuspendAsyncCATransactionsRunnable = nullptr;

  if (mNativeLayerRoot->UnsuspendOffMainThreadCommits()) {
    // We need to call mNativeLayerRoot->CommitToScreen() at the next available
    // opportunity.
    // The easiest way to handle this request is to mark the layer as needing
    // display, because this will schedule a main thread CATransaction, during
    // which HandleMainThreadCATransaction will call CommitToScreen().
    [mView markLayerForDisplay];
  }
}

nsresult nsChildView::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                               int32_t aNativeKeyCode, uint32_t aModifierFlags,
                                               const nsAString& aCharacters,
                                               const nsAString& aUnmodifiedCharacters,
                                               nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "keyevent");
  return mTextInputHandler->SynthesizeNativeKeyEvent(
      aNativeKeyboardLayout, aNativeKeyCode, aModifierFlags, aCharacters, aUnmodifiedCharacters);
}

nsresult nsChildView::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                 NativeMouseMessage aNativeMessage,
                                                 MouseButton aButton,
                                                 nsIWidget::Modifiers aModifierFlags,
                                                 nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  AutoObserverNotifier notifier(aObserver, "mouseevent");

  NSPoint pt = nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // aPoint is given with the origin on the top left, but convertScreenToBase
  // expects a point in a coordinate system that has its origin on the bottom left.
  NSPoint screenPoint = NSMakePoint(pt.x, nsCocoaUtils::FlippedScreenY(pt.y));
  NSPoint windowPoint = nsCocoaUtils::ConvertPointFromScreen([mView window], screenPoint);
  NSEventModifierFlags modifierFlags =
      nsCocoaUtils::ConvertWidgetModifiersToMacModifierFlags(aModifierFlags);

  if (aButton == MouseButton::eX1 || aButton == MouseButton::eX2) {
    // NSEvent has `buttonNumber` for `NSEventTypeOther*`.  However, it seems that
    // there is no way to specify it.  Therefore, we should return error for now.
    return NS_ERROR_INVALID_ARG;
  }

  NSEventType nativeEventType;
  switch (aNativeMessage) {
    case NativeMouseMessage::ButtonDown:
    case NativeMouseMessage::ButtonUp: {
      switch (aButton) {
        case MouseButton::ePrimary:
          nativeEventType = aNativeMessage == NativeMouseMessage::ButtonDown
                                ? NSEventTypeLeftMouseDown
                                : NSEventTypeLeftMouseUp;
          break;
        case MouseButton::eMiddle:
          nativeEventType = aNativeMessage == NativeMouseMessage::ButtonDown
                                ? NSEventTypeOtherMouseDown
                                : NSEventTypeOtherMouseUp;
          break;
        case MouseButton::eSecondary:
          nativeEventType = aNativeMessage == NativeMouseMessage::ButtonDown
                                ? NSEventTypeRightMouseDown
                                : NSEventTypeRightMouseUp;
          break;
        default:
          return NS_ERROR_INVALID_ARG;
      }
      break;
    }
    case NativeMouseMessage::Move:
      nativeEventType = NSEventTypeMouseMoved;
      break;
    case NativeMouseMessage::EnterWindow:
      nativeEventType = NSEventTypeMouseEntered;
      break;
    case NativeMouseMessage::LeaveWindow:
      nativeEventType = NSEventTypeMouseExited;
      break;
  }

  NSEvent* event = [NSEvent mouseEventWithType:nativeEventType
                                      location:windowPoint
                                 modifierFlags:modifierFlags
                                     timestamp:[[NSProcessInfo processInfo] systemUptime]
                                  windowNumber:[[mView window] windowNumber]
                                       context:nil
                                   eventNumber:0
                                    clickCount:1
                                      pressure:0.0];

  if (!event) return NS_ERROR_FAILURE;

  if ([[mView window] isKindOfClass:[BaseWindow class]]) {
    // Tracking area events don't end up in their tracking areas when sent
    // through [NSApp sendEvent:], so pass them directly to the right methods.
    BaseWindow* window = (BaseWindow*)[mView window];
    if (nativeEventType == NSEventTypeMouseEntered) {
      [window mouseEntered:event];
      return NS_OK;
    }
    if (nativeEventType == NSEventTypeMouseExited) {
      [window mouseExited:event];
      return NS_OK;
    }
    if (nativeEventType == NSEventTypeMouseMoved) {
      [window mouseMoved:event];
      return NS_OK;
    }
  }

  [NSApp sendEvent:event];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsChildView::SynthesizeNativeMouseScrollEvent(
    mozilla::LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX, double aDeltaY,
    double aDeltaZ, uint32_t aModifierFlags, uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  AutoObserverNotifier notifier(aObserver, "mousescrollevent");

  NSPoint pt = nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // Mostly copied from http://stackoverflow.com/a/6130349
  CGScrollEventUnit units = (aAdditionalFlags & nsIDOMWindowUtils::MOUSESCROLL_SCROLL_LINES)
                                ? kCGScrollEventUnitLine
                                : kCGScrollEventUnitPixel;
  CGEventRef cgEvent = CGEventCreateScrollWheelEvent(NULL, units, 3, (int32_t)aDeltaY,
                                                     (int32_t)aDeltaX, (int32_t)aDeltaZ);
  if (!cgEvent) {
    return NS_ERROR_FAILURE;
  }

  if (aNativeMessage) {
    CGEventSetIntegerValueField(cgEvent, kCGScrollWheelEventScrollPhase, aNativeMessage);
  }

  // On macOS 10.14 and up CGEventPost won't work because of changes in macOS
  // to improve security. This code makes an NSEvent corresponding to the
  // wheel event and dispatches it directly to the scrollWheel handler. Some
  // fiddling is needed with the coordinates in order to simulate what macOS
  // would do; this code adapted from the Chromium equivalent function at
  // https://chromium.googlesource.com/chromium/src.git/+/62.0.3178.1/ui/events/test/cocoa_test_event_utils.mm#38
  CGPoint location = CGEventGetLocation(cgEvent);
  location.y += NSMinY([[mView window] frame]);
  location.x -= NSMinX([[mView window] frame]);
  CGEventSetLocation(cgEvent, location);

  uint64_t kNanosPerSec = 1000000000L;
  CGEventSetTimestamp(cgEvent, [[NSProcessInfo processInfo] systemUptime] * kNanosPerSec);

  NSEvent* event = [NSEvent eventWithCGEvent:cgEvent];
  [event setValue:[mView window] forKey:@"_window"];
  [mView scrollWheel:event];

  CFRelease(cgEvent);
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsChildView::SynthesizeNativeTouchPoint(
    uint32_t aPointerId, TouchPointerState aPointerState, mozilla::LayoutDeviceIntPoint aPoint,
    double aPointerPressure, uint32_t aPointerOrientation, nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  AutoObserverNotifier notifier(aObserver, "touchpoint");

  MOZ_ASSERT(NS_IsMainThread());
  if (aPointerState == TOUCH_HOVER) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mSynthesizedTouchInput) {
    mSynthesizedTouchInput = MakeUnique<MultiTouchInput>();
  }

  LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
  MultiTouchInput inputToDispatch = UpdateSynthesizedTouchState(
      mSynthesizedTouchInput.get(), PR_IntervalNow(), TimeStamp::Now(), aPointerId, aPointerState,
      pointInWindow, aPointerPressure, aPointerOrientation);
  DispatchTouchInput(inputToDispatch);
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsChildView::SynthesizeNativeTouchpadDoubleTap(mozilla::LayoutDeviceIntPoint aPoint,
                                                        uint32_t aModifierFlags) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  DispatchDoubleTapGesture(TimeStamp::Now(), aPoint - WidgetToScreenOffset(),
                           static_cast<Modifiers>(aModifierFlags));

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

bool nsChildView::SendEventToNativeMenuSystem(NSEvent* aEvent) {
  bool handled = false;
  nsCocoaWindow* widget = GetAppWindowWidget();
  if (widget) {
    nsMenuBarX* mb = widget->GetMenuBar();
    if (mb) {
      // Check if main menu wants to handle the event.
      handled = mb->PerformKeyEquivalent(aEvent);
    }
  }

  if (!handled && sApplicationMenu) {
    // Check if application menu wants to handle the event.
    handled = [sApplicationMenu performKeyEquivalent:aEvent];
  }

  return handled;
}

void nsChildView::PostHandleKeyEvent(mozilla::WidgetKeyboardEvent* aEvent) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // We always allow keyboard events to propagate to keyDown: but if they are
  // not handled we give menu items a chance to act. This allows for handling of
  // custom shortcuts. Note that existing shortcuts cannot be reassigned yet and
  // will have been handled by keyDown: before we get here.
  NSMutableDictionary* nativeKeyEventsMap = [ChildView sNativeKeyEventsMap];
  NSEvent* cocoaEvent = [nativeKeyEventsMap objectForKey:@(aEvent->mUniqueId)];
  if (!cocoaEvent) {
    return;
  }

  if (SendEventToNativeMenuSystem(cocoaEvent)) {
    aEvent->PreventDefault();
  }
  [nativeKeyEventsMap removeObjectForKey:@(aEvent->mUniqueId)];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Used for testing native menu system structure and event handling.
nsresult nsChildView::ActivateNativeMenuItemAt(const nsAString& indexString) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsMenuUtilsX::CheckNativeMenuConsistency([NSApp mainMenu]);

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(indexString.BeginReading())
                              length:indexString.Length()];
  NSMenuItem* item =
      nsMenuUtilsX::NativeMenuItemWithLocation([NSApp mainMenu], locationString, true);
  // We can't perform an action on an item with a submenu, that will raise
  // an obj-c exception.
  if (item && ![item hasSubmenu]) {
    NSMenu* parent = [item menu];
    if (parent) {
      // NSLog(@"Performing action for native menu item titled: %@\n",
      //       [[currentSubmenu itemAtIndex:targetIndex] title]);
      [parent performActionForItemAtIndex:[parent indexOfItem:item]];
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// Used for testing native menu system structure and event handling.
nsresult nsChildView::ForceUpdateNativeMenuAt(const nsAString& indexString) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsCocoaWindow* widget = GetAppWindowWidget();
  if (widget) {
    nsMenuBarX* mb = widget->GetMenuBar();
    if (mb) {
      if (indexString.IsEmpty())
        mb->ForceNativeMenuReload();
      else
        mb->ForceUpdateNativeMenuAt(indexString);
    }
  }
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

#pragma mark -

#ifdef INVALIDATE_DEBUGGING

static Boolean KeyDown(const UInt8 theKey) {
  KeyMap map;
  GetKeys(map);
  return ((*((UInt8*)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}

static Boolean caps_lock() { return KeyDown(0x39); }

static void blinkRect(Rect* r) {
  StRegionFromPool oldClip;
  if (oldClip != NULL) ::GetClip(oldClip);

  ::ClipRect(r);
  ::InvertRect(r);
  UInt32 end = ::TickCount() + 5;
  while (::TickCount() < end)
    ;
  ::InvertRect(r);

  if (oldClip != NULL) ::SetClip(oldClip);
}

static void blinkRgn(RgnHandle rgn) {
  StRegionFromPool oldClip;
  if (oldClip != NULL) ::GetClip(oldClip);

  ::SetClip(rgn);
  ::InvertRgn(rgn);
  UInt32 end = ::TickCount() + 5;
  while (::TickCount() < end)
    ;
  ::InvertRgn(rgn);

  if (oldClip != NULL) ::SetClip(oldClip);
}

#endif

// Invalidate this component's visible area
void nsChildView::Invalidate(const LayoutDeviceIntRect& aRect) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mView || !mVisible) return;

  NS_ASSERTION(GetLayerManager()->GetBackendType() != LayersBackend::LAYERS_CLIENT,
               "Shouldn't need to invalidate with accelerated OMTC layers!");

  EnsureContentLayerForMainThreadPainting();
  mContentLayerInvalidRegion.OrWith(aRect.Intersect(GetBounds()));
  [mView markLayerForDisplay];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

bool nsChildView::WidgetTypeSupportsAcceleration() {
  // All widget types support acceleration.
  return true;
}

bool nsChildView::ShouldUseOffMainThreadCompositing() {
  // We need to enable OMTC in popups which contain remote layer
  // trees, since the remote content won't be rendered at all otherwise.
  if (HasRemoteContent()) {
    return true;
  }

  // Don't use OMTC for popup windows, because we do not want context menus to
  // pay the overhead of starting up a compositor. With the OpenGL compositor,
  // new windows are expensive because of shader re-compilation, and with
  // WebRender, new windows are expensive because they create their own threads
  // and texture caches.
  // Using OMTC with BasicCompositor for context menus would probably be fine
  // but isn't a well-tested configuration.
  if ([mView window] && [[mView window] isKindOfClass:[PopupWindow class]]) {
    // Use main-thread BasicLayerManager for drawing menus.
    return false;
  }

  return nsBaseWidget::ShouldUseOffMainThreadCompositing();
}

#pragma mark -

nsresult nsChildView::ConfigureChildren(const nsTArray<Configuration>& aConfigurations) {
  return NS_OK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
nsresult nsChildView::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus) {
  RefPtr<nsChildView> kungFuDeathGrip(this);

#ifdef DEBUG
  debug_DumpEvent(stdout, event->mWidget, event, "something", 0);
#endif

  if (event->mFlags.mIsSynthesizedForTests) {
    WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
    if (keyEvent) {
      nsresult rv = mTextInputHandler->AttachNativeKeyEvent(*keyEvent);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  aStatus = nsEventStatus_eIgnore;

  nsIWidgetListener* listener = mWidgetListener;

  // If the listener is NULL, check if the parent is a popup. If it is, then
  // this child is the popup content view attached to a popup. Get the
  // listener from the parent popup instead.
  nsCOMPtr<nsIWidget> parentWidget = mParentWidget;
  if (!listener && parentWidget) {
    if (parentWidget->WindowType() == eWindowType_popup) {
      // Check just in case event->mWidget isn't this widget
      if (event->mWidget) {
        listener = event->mWidget->GetWidgetListener();
      }
      if (!listener) {
        event->mWidget = parentWidget;
        listener = parentWidget->GetWidgetListener();
      }
    }
  }

  if (listener) aStatus = listener->HandleEvent(event, mUseAttachedEvents);

  return NS_OK;
}

bool nsChildView::DispatchWindowEvent(WidgetGUIEvent& event) {
  nsEventStatus status;
  DispatchEvent(&event, status);
  return ConvertStatus(status);
}

nsIWidget* nsChildView::GetWidgetForListenerEvents() {
  // If there is no listener, use the parent popup's listener if that exists.
  if (!mWidgetListener && mParentWidget && mParentWidget->WindowType() == eWindowType_popup) {
    return mParentWidget;
  }

  return this;
}

void nsChildView::WillPaintWindow() {
  nsCOMPtr<nsIWidget> widget = GetWidgetForListenerEvents();

  nsIWidgetListener* listener = widget->GetWidgetListener();
  if (listener) {
    listener->WillPaintWindow(widget);
  }
}

bool nsChildView::PaintWindow(LayoutDeviceIntRegion aRegion) {
  nsCOMPtr<nsIWidget> widget = GetWidgetForListenerEvents();

  nsIWidgetListener* listener = widget->GetWidgetListener();
  if (!listener) return false;

  bool returnValue = false;
  bool oldDispatchPaint = mIsDispatchPaint;
  mIsDispatchPaint = true;
  returnValue = listener->PaintWindow(widget, aRegion);

  listener = widget->GetWidgetListener();
  if (listener) {
    listener->DidPaintWindow();
  }

  mIsDispatchPaint = oldDispatchPaint;
  return returnValue;
}

bool nsChildView::PaintWindowInDrawTarget(gfx::DrawTarget* aDT,
                                          const LayoutDeviceIntRegion& aRegion,
                                          const gfx::IntSize& aSurfaceSize) {
  RefPtr<gfxContext> targetContext = gfxContext::CreateOrNull(aDT);
  MOZ_ASSERT(targetContext);

  // Set up the clip region and clear existing contents in the backing surface.
  targetContext->NewPath();
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect& r = iter.Get();
    targetContext->Rectangle(gfxRect(r.x, r.y, r.width, r.height));
    aDT->ClearRect(gfx::Rect(r.ToUnknownRect()));
  }
  targetContext->Clip();

  nsAutoRetainCocoaObject kungFuDeathGrip(mView);
  if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    nsBaseWidget::AutoLayerManagerSetup setupLayerManager(this, targetContext,
                                                          BufferMode::BUFFER_NONE);
    return PaintWindow(aRegion);
  }
  if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    // We only need this so that we actually get DidPaintWindow fired
    return PaintWindow(aRegion);
  }
  return false;
}

void nsChildView::EnsureContentLayerForMainThreadPainting() {
  // Ensure we have an mContentLayer of the correct size.
  // The content layer gets created on demand for BasicLayers windows. We do
  // not create it during widget creation because, for non-BasicLayers windows,
  // the compositing layer manager will create any layers it needs.
  gfx::IntSize size = GetBounds().Size().ToUnknownSize();
  if (mContentLayer && mContentLayer->GetSize() != size) {
    mNativeLayerRoot->RemoveLayer(mContentLayer);
    mContentLayer = nullptr;
  }
  if (!mContentLayer) {
    mPoolHandle = SurfacePool::Create(0)->GetHandleForGL(nullptr);
    RefPtr<NativeLayer> contentLayer = mNativeLayerRoot->CreateLayer(size, false, mPoolHandle);
    mNativeLayerRoot->AppendLayer(contentLayer);
    mContentLayer = contentLayer->AsNativeLayerCA();
    mContentLayer->SetSurfaceIsFlipped(false);
    mContentLayerInvalidRegion = GetBounds();
  }
}

void nsChildView::PaintWindowInContentLayer() {
  EnsureContentLayerForMainThreadPainting();
  mPoolHandle->OnBeginFrame();
  RefPtr<DrawTarget> dt = mContentLayer->NextSurfaceAsDrawTarget(
      gfx::IntRect({}, mContentLayer->GetSize()), mContentLayerInvalidRegion.ToUnknownRegion(),
      gfx::BackendType::SKIA);
  if (!dt) {
    return;
  }

  PaintWindowInDrawTarget(dt, mContentLayerInvalidRegion, dt->GetSize());
  mContentLayer->NotifySurfaceReady();
  mContentLayerInvalidRegion.SetEmpty();
  mPoolHandle->OnEndFrame();
}

void nsChildView::HandleMainThreadCATransaction() {
  WillPaintWindow();

  if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    // We're in BasicLayers mode, i.e. main thread software compositing.
    // Composite the window into our layer's surface.
    PaintWindowInContentLayer();
  } else {
    // Trigger a synchronous OMTC composite. This will call NextSurface and
    // NotifySurfaceReady on the compositor thread to update mNativeLayerRoot's
    // contents, and the main thread (this thread) will wait inside PaintWindow
    // during that time.
    PaintWindow(LayoutDeviceIntRegion(GetBounds()));
  }

  {
    // Apply the changes inside mNativeLayerRoot to the underlying CALayers. Now is a
    // good time to call this because we know we're currently inside a main thread
    // CATransaction, and the lock makes sure that no composition is currently in
    // progress, so we won't present half-composited state to the screen.
    MutexAutoLock lock(mCompositingLock);
    mNativeLayerRoot->CommitToScreen();
  }

  MaybeScheduleUnsuspendAsyncCATransactions();
}

#pragma mark -

void nsChildView::ReportMoveEvent() { NotifyWindowMoved(mBounds.x, mBounds.y); }

void nsChildView::ReportSizeEvent() {
  if (mWidgetListener) mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
}

#pragma mark -

LayoutDeviceIntPoint nsChildView::GetClientOffset() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSPoint origin = [mView convertPoint:NSMakePoint(0, 0) toView:nil];
  origin.y = [[mView window] frame].size.height - origin.y;
  return CocoaPointsToDevPixels(origin);

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

//    Return the offset between this child view and the screen.
//    @return       -- widget origin in device-pixel coords
LayoutDeviceIntPoint nsChildView::WidgetToScreenOffset() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSPoint origin = NSMakePoint(0, 0);

  // 1. First translate view origin point into window coords.
  // The returned point is in bottom-left coordinates.
  origin = [mView convertPoint:origin toView:nil];

  // 2. We turn the window-coord rect's origin into screen (still bottom-left) coords.
  origin = nsCocoaUtils::ConvertPointToScreen([mView window], origin);

  // 3. Since we're dealing in bottom-left coords, we need to make it top-left coords
  //    before we pass it back to Gecko.
  FlipCocoaScreenCoordinate(origin);

  // convert to device pixels
  return CocoaPointsToDevPixels(origin);

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

nsresult nsChildView::SetTitle(const nsAString& title) {
  // child views don't have titles
  return NS_OK;
}

nsresult nsChildView::GetAttention(int32_t aCycleCount) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

/* static */
bool nsChildView::DoHasPendingInputEvent() {
  return sLastInputEventCount != GetCurrentInputEventCount();
}

/* static */
uint32_t nsChildView::GetCurrentInputEventCount() {
  // Can't use kCGAnyInputEventType because that updates too rarely for us (and
  // always in increments of 30+!) and because apparently it's sort of broken
  // on Tiger.  So just go ahead and query the counters we care about.
  static const CGEventType eventTypes[] = {
      kCGEventLeftMouseDown,     kCGEventLeftMouseUp,      kCGEventRightMouseDown,
      kCGEventRightMouseUp,      kCGEventMouseMoved,       kCGEventLeftMouseDragged,
      kCGEventRightMouseDragged, kCGEventKeyDown,          kCGEventKeyUp,
      kCGEventScrollWheel,       kCGEventTabletPointer,    kCGEventOtherMouseDown,
      kCGEventOtherMouseUp,      kCGEventOtherMouseDragged};

  uint32_t eventCount = 0;
  for (uint32_t i = 0; i < ArrayLength(eventTypes); ++i) {
    eventCount +=
        CGEventSourceCounterForEventType(kCGEventSourceStateCombinedSessionState, eventTypes[i]);
  }
  return eventCount;
}

/* static */
void nsChildView::UpdateCurrentInputEventCount() {
  sLastInputEventCount = GetCurrentInputEventCount();
}

bool nsChildView::HasPendingInputEvent() { return DoHasPendingInputEvent(); }

#pragma mark -

void nsChildView::SetInputContext(const InputContext& aContext, const InputContextAction& aAction) {
  NS_ENSURE_TRUE_VOID(mTextInputHandler);

  if (mTextInputHandler->IsFocused()) {
    if (aContext.IsPasswordEditor()) {
      TextInputHandler::EnableSecureEventInput();
    } else {
      TextInputHandler::EnsureSecureEventInputDisabled();
    }
  }

  // IMEInputHandler::IsEditableContent() returns false when both
  // IsASCIICableOnly() and IsIMEEnabled() return false.  So, be careful
  // when you change the following code.  You might need to change
  // IMEInputHandler::IsEditableContent() too.
  mInputContext = aContext;
  switch (aContext.mIMEState.mEnabled) {
    case IMEEnabled::Enabled:
      mTextInputHandler->SetASCIICapableOnly(false);
      mTextInputHandler->EnableIME(true);
      if (mInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE) {
        mTextInputHandler->SetIMEOpenState(mInputContext.mIMEState.mOpen == IMEState::OPEN);
      }
      break;
    case IMEEnabled::Disabled:
      mTextInputHandler->SetASCIICapableOnly(false);
      mTextInputHandler->EnableIME(false);
      break;
    case IMEEnabled::Password:
      mTextInputHandler->SetASCIICapableOnly(true);
      mTextInputHandler->EnableIME(false);
      break;
    default:
      NS_ERROR("not implemented!");
  }
}

InputContext nsChildView::GetInputContext() {
  switch (mInputContext.mIMEState.mEnabled) {
    case IMEEnabled::Enabled:
      if (mTextInputHandler) {
        mInputContext.mIMEState.mOpen =
            mTextInputHandler->IsIMEOpened() ? IMEState::OPEN : IMEState::CLOSED;
        break;
      }
      // If mTextInputHandler is null, set CLOSED instead...
      [[fallthrough]];
    default:
      mInputContext.mIMEState.mOpen = IMEState::CLOSED;
      break;
  }
  return mInputContext;
}

TextEventDispatcherListener* nsChildView::GetNativeTextEventDispatcherListener() {
  if (NS_WARN_IF(!mTextInputHandler)) {
    return nullptr;
  }
  return mTextInputHandler;
}

nsresult nsChildView::AttachNativeKeyEvent(mozilla::WidgetKeyboardEvent& aEvent) {
  NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
  return mTextInputHandler->AttachNativeKeyEvent(aEvent);
}

bool nsChildView::GetEditCommands(NativeKeyBindingsType aType, const WidgetKeyboardEvent& aEvent,
                                  nsTArray<CommandInt>& aCommands) {
  // Validate the arguments.
  if (NS_WARN_IF(!nsIWidget::GetEditCommands(aType, aEvent, aCommands))) {
    return false;
  }

  Maybe<WritingMode> writingMode;
  if (aEvent.NeedsToRemapNavigationKey()) {
    if (RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher()) {
      writingMode = dispatcher->MaybeWritingModeAtSelection();
    }
  }

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  keyBindings->GetEditCommands(aEvent, writingMode, aCommands);
  return true;
}

NSView<mozView>* nsChildView::GetEditorView() {
  NSView<mozView>* editorView = mView;
  // We need to get editor's view. E.g., when the focus is in the bookmark
  // dialog, the view is <panel> element of the dialog.  At this time, the key
  // events are processed the parent window's view that has native focus.
  WidgetQueryContentEvent queryContentState(true, eQueryContentState, this);
  // This may be called during creating a menu popup frame due to creating
  // widget synchronously and that causes Cocoa asking current window level.
  // In this case, it's not safe to flush layout on the document and we don't
  // need any layout information right now.
  queryContentState.mNeedsToFlushLayout = false;
  DispatchWindowEvent(queryContentState);
  if (queryContentState.Succeeded() && queryContentState.mReply->mFocusedWidget) {
    NSView<mozView>* view = static_cast<NSView<mozView>*>(
        queryContentState.mReply->mFocusedWidget->GetNativeData(NS_NATIVE_WIDGET));
    if (view) editorView = view;
  }
  return editorView;
}

#pragma mark -

void nsChildView::CreateCompositor() {
  nsBaseWidget::CreateCompositor();
  if (mCompositorBridgeChild) {
    [mView setUsingOMTCompositor:true];
  }
}

void nsChildView::ConfigureAPZCTreeManager() { nsBaseWidget::ConfigureAPZCTreeManager(); }

void nsChildView::ConfigureAPZControllerThread() { nsBaseWidget::ConfigureAPZControllerThread(); }

bool nsChildView::PreRender(WidgetRenderingContext* aContext) {
  // The lock makes sure that we don't attempt to tear down the view while
  // compositing. That would make us unable to call postRender on it when the
  // composition is done, thus keeping the GL context locked forever.
  mCompositingLock.Lock();

  if (aContext->mGL && gfxPlatform::CanMigrateMacGPUs()) {
    GLContextCGL::Cast(aContext->mGL)->MigrateToActiveGPU();
  }

  return true;
}

void nsChildView::PostRender(WidgetRenderingContext* aContext) { mCompositingLock.Unlock(); }

RefPtr<layers::NativeLayerRoot> nsChildView::GetNativeLayerRoot() { return mNativeLayerRoot; }

static int32_t FindTitlebarBottom(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                                  int32_t aWindowWidth) {
  int32_t titlebarBottom = 0;
  for (auto& g : aThemeGeometries) {
    if ((g.mType == eThemeGeometryTypeTitlebar ||
         g.mType == eThemeGeometryTypeVibrantTitlebarLight ||
         g.mType == eThemeGeometryTypeVibrantTitlebarDark) &&
        g.mRect.X() <= 0 && g.mRect.XMost() >= aWindowWidth && g.mRect.Y() <= 0) {
      titlebarBottom = std::max(titlebarBottom, g.mRect.YMost());
    }
  }
  return titlebarBottom;
}

static int32_t FindUnifiedToolbarBottom(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                                        int32_t aWindowWidth, int32_t aTitlebarBottom) {
  int32_t unifiedToolbarBottom = aTitlebarBottom;
  for (uint32_t i = 0; i < aThemeGeometries.Length(); ++i) {
    const nsIWidget::ThemeGeometry& g = aThemeGeometries[i];
    if ((g.mType == eThemeGeometryTypeToolbar) && g.mRect.X() <= 0 &&
        g.mRect.XMost() >= aWindowWidth && g.mRect.Y() <= aTitlebarBottom) {
      unifiedToolbarBottom = std::max(unifiedToolbarBottom, g.mRect.YMost());
    }
  }
  return unifiedToolbarBottom;
}

static LayoutDeviceIntRect FindFirstRectOfType(
    const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
    nsITheme::ThemeGeometryType aThemeGeometryType) {
  for (uint32_t i = 0; i < aThemeGeometries.Length(); ++i) {
    const nsIWidget::ThemeGeometry& g = aThemeGeometries[i];
    if (g.mType == aThemeGeometryType) {
      return g.mRect;
    }
  }
  return LayoutDeviceIntRect();
}

void nsChildView::UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) {
  if (![mView window]) return;

  UpdateVibrancy(aThemeGeometries);

  if (![[mView window] isKindOfClass:[ToolbarWindow class]]) return;

  // Update unified toolbar height and sheet attachment position.
  int32_t windowWidth = mBounds.width;
  int32_t titlebarBottom = FindTitlebarBottom(aThemeGeometries, windowWidth);
  int32_t unifiedToolbarBottom =
      FindUnifiedToolbarBottom(aThemeGeometries, windowWidth, titlebarBottom);
  int32_t toolboxBottom = FindFirstRectOfType(aThemeGeometries, eThemeGeometryTypeToolbox).YMost();

  ToolbarWindow* win = (ToolbarWindow*)[mView window];
  int32_t titlebarHeight =
      [win drawsContentsIntoWindowFrame] ? 0 : CocoaPointsToDevPixels([win titlebarHeight]);
  int32_t devUnifiedHeight = titlebarHeight + unifiedToolbarBottom;
  [win setUnifiedToolbarHeight:DevPixelsToCocoaPoints(devUnifiedHeight)];

  int32_t sheetPositionDevPx = std::max(toolboxBottom, unifiedToolbarBottom);
  NSPoint sheetPositionView = {0, DevPixelsToCocoaPoints(sheetPositionDevPx)};
  NSPoint sheetPositionWindow = [mView convertPoint:sheetPositionView toView:nil];
  [win setSheetAttachmentPosition:sheetPositionWindow.y];

  // Update titlebar control offsets.
  LayoutDeviceIntRect windowButtonRect =
      FindFirstRectOfType(aThemeGeometries, eThemeGeometryTypeWindowButtons);
  [win placeWindowButtons:[mView convertRect:DevPixelsToCocoaPoints(windowButtonRect) toView:nil]];
}

static Maybe<VibrancyType> ThemeGeometryTypeToVibrancyType(
    nsITheme::ThemeGeometryType aThemeGeometryType) {
  switch (aThemeGeometryType) {
    case eThemeGeometryTypeVibrantTitlebarLight:
      return Some(VibrancyType::TITLEBAR_LIGHT);
    case eThemeGeometryTypeVibrantTitlebarDark:
      return Some(VibrancyType::TITLEBAR_DARK);
    case eThemeGeometryTypeTooltip:
      return Some(VibrancyType::TOOLTIP);
    case eThemeGeometryTypeMenu:
      return Some(VibrancyType::MENU);
    case eThemeGeometryTypeHighlightedMenuItem:
      return Some(VibrancyType::HIGHLIGHTED_MENUITEM);
    case eThemeGeometryTypeSourceList:
      return Some(VibrancyType::SOURCE_LIST);
    case eThemeGeometryTypeSourceListSelection:
      return Some(VibrancyType::SOURCE_LIST_SELECTION);
    case eThemeGeometryTypeActiveSourceListSelection:
      return Some(VibrancyType::ACTIVE_SOURCE_LIST_SELECTION);
    default:
      return Nothing();
  }
}

static LayoutDeviceIntRegion GatherVibrantRegion(
    const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries, VibrancyType aVibrancyType) {
  LayoutDeviceIntRegion region;
  for (auto& geometry : aThemeGeometries) {
    if (ThemeGeometryTypeToVibrancyType(geometry.mType) == Some(aVibrancyType)) {
      region.OrWith(geometry.mRect);
    }
  }
  return region;
}

template <typename Region>
static void MakeRegionsNonOverlappingImpl(Region& aOutUnion) {}

template <typename Region, typename... Regions>
static void MakeRegionsNonOverlappingImpl(Region& aOutUnion, Region& aFirst, Regions&... aRest) {
  MakeRegionsNonOverlappingImpl(aOutUnion, aRest...);
  aFirst.SubOut(aOutUnion);
  aOutUnion.OrWith(aFirst);
}

// Subtracts parts from regions in such a way that they don't have any overlap.
// Each region in the argument list will have the union of all the regions
// *following* it subtracted from itself. In other words, the arguments are
// sorted low priority to high priority.
template <typename Region, typename... Regions>
static void MakeRegionsNonOverlapping(Region& aFirst, Regions&... aRest) {
  Region unionOfAll;
  MakeRegionsNonOverlappingImpl(unionOfAll, aFirst, aRest...);
}

void nsChildView::UpdateVibrancy(const nsTArray<ThemeGeometry>& aThemeGeometries) {
  LayoutDeviceIntRegion vibrantTitlebarLightRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::TITLEBAR_LIGHT);
  LayoutDeviceIntRegion vibrantTitlebarDarkRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::TITLEBAR_DARK);
  LayoutDeviceIntRegion menuRegion = GatherVibrantRegion(aThemeGeometries, VibrancyType::MENU);
  LayoutDeviceIntRegion tooltipRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::TOOLTIP);
  LayoutDeviceIntRegion highlightedMenuItemRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::HIGHLIGHTED_MENUITEM);
  LayoutDeviceIntRegion sourceListRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::SOURCE_LIST);
  LayoutDeviceIntRegion sourceListSelectionRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::SOURCE_LIST_SELECTION);
  LayoutDeviceIntRegion activeSourceListSelectionRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::ACTIVE_SOURCE_LIST_SELECTION);

  MakeRegionsNonOverlapping(vibrantTitlebarLightRegion, vibrantTitlebarDarkRegion, menuRegion,
                            tooltipRegion, highlightedMenuItemRegion, sourceListRegion,
                            sourceListSelectionRegion, activeSourceListSelectionRegion);

  auto& vm = EnsureVibrancyManager();
  bool changed = false;
  changed |= vm.UpdateVibrantRegion(VibrancyType::TITLEBAR_LIGHT, vibrantTitlebarLightRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::TITLEBAR_DARK, vibrantTitlebarDarkRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::MENU, menuRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::TOOLTIP, tooltipRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::HIGHLIGHTED_MENUITEM, highlightedMenuItemRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::SOURCE_LIST, sourceListRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::SOURCE_LIST_SELECTION, sourceListSelectionRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::ACTIVE_SOURCE_LIST_SELECTION,
                                    activeSourceListSelectionRegion);

  if (changed) {
    SuspendAsyncCATransactions();
  }
}

mozilla::VibrancyManager& nsChildView::EnsureVibrancyManager() {
  MOZ_ASSERT(mView, "Only call this once we have a view!");
  if (!mVibrancyManager) {
    mVibrancyManager = MakeUnique<VibrancyManager>(*this, [mView vibrancyViewsContainer]);
  }
  return *mVibrancyManager;
}

nsChildView::SwipeInfo nsChildView::SendMayStartSwipe(
    const mozilla::PanGestureInput& aSwipeStartEvent) {
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  uint32_t direction = (aSwipeStartEvent.mPanDisplacement.x > 0.0)
                           ? (uint32_t)dom::SimpleGestureEvent_Binding::DIRECTION_RIGHT
                           : (uint32_t)dom::SimpleGestureEvent_Binding::DIRECTION_LEFT;

  // We're ready to start the animation. Tell Gecko about it, and at the same
  // time ask it if it really wants to start an animation for this event.
  // This event also reports back the directions that we can swipe in.
  LayoutDeviceIntPoint position =
      RoundedToInt(aSwipeStartEvent.mPanStartPoint * ScreenToLayoutDeviceScale(1));
  WidgetSimpleGestureEvent geckoEvent = SwipeTracker::CreateSwipeGestureEvent(
      eSwipeGestureMayStart, this, position, aSwipeStartEvent.mTimeStamp);
  geckoEvent.mDirection = direction;
  geckoEvent.mDelta = 0.0;
  geckoEvent.mAllowedDirections = 0;
  bool shouldStartSwipe = DispatchWindowEvent(geckoEvent);  // event cancelled == swipe should start

  SwipeInfo result = {shouldStartSwipe, geckoEvent.mAllowedDirections};
  return result;
}

void nsChildView::TrackScrollEventAsSwipe(const mozilla::PanGestureInput& aSwipeStartEvent,
                                          uint32_t aAllowedDirections) {
  // If a swipe is currently being tracked kill it -- it's been interrupted
  // by another gesture event.
  if (mSwipeTracker) {
    mSwipeTracker->CancelSwipe(aSwipeStartEvent.mTimeStamp);
    mSwipeTracker->Destroy();
    mSwipeTracker = nullptr;
  }

  uint32_t direction = (aSwipeStartEvent.mPanDisplacement.x > 0.0)
                           ? (uint32_t)dom::SimpleGestureEvent_Binding::DIRECTION_RIGHT
                           : (uint32_t)dom::SimpleGestureEvent_Binding::DIRECTION_LEFT;

  mSwipeTracker = new SwipeTracker(*this, aSwipeStartEvent, aAllowedDirections, direction);

  if (!mAPZC) {
    mCurrentPanGestureBelongsToSwipe = true;
  }
}

void nsChildView::SwipeFinished() { mSwipeTracker = nullptr; }

void nsChildView::UpdateBoundsFromView() {
  auto oldSize = mBounds.Size();
  mBounds = CocoaPointsToDevPixels([mView frame]);
  if (mBounds.Size() != oldSize) {
    SuspendAsyncCATransactions();
  }
}

@interface NonDraggableView : NSView
@end

@implementation NonDraggableView
- (BOOL)mouseDownCanMoveWindow {
  return NO;
}
- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}
- (NSRect)_opaqueRectForWindowMoveWhenInTitlebar {
  // In NSWindows that use NSWindowStyleMaskFullSizeContentView, NSViews which
  // overlap the titlebar do not disable window dragging in the overlapping
  // areas even if they return NO from mouseDownCanMoveWindow. This can have
  // unfortunate effects: For example, dragging tabs in a browser window would
  // move the window if those tabs are in the titlebar.
  // macOS does not seem to offer a documented way to opt-out of the forced
  // window dragging in the titlebar.
  // Overriding _opaqueRectForWindowMoveWhenInTitlebar is an undocumented way
  // of opting out of this behavior. This method was added in 10.11 and is used
  // by some NSControl subclasses to prevent window dragging in the titlebar.
  // The function which assembles the draggable area of the window calls
  // _opaqueRect for the content area and _opaqueRectForWindowMoveWhenInTitlebar
  // for the titlebar area, on all visible NSViews. The default implementation
  // of _opaqueRect returns [self visibleRect], and the default implementation
  // of _opaqueRectForWindowMoveWhenInTitlebar returns NSZeroRect unless it's
  // overridden.
  return [self visibleRect];
}
@end

void nsChildView::UpdateWindowDraggingRegion(const LayoutDeviceIntRegion& aRegion) {
  // mView returns YES from mouseDownCanMoveWindow, so we need to put NSViews
  // that return NO from mouseDownCanMoveWindow in the places that shouldn't
  // be draggable. We can't do it the other way round because returning
  // YES from mouseDownCanMoveWindow doesn't have any effect if there's a
  // superview that returns NO.
  LayoutDeviceIntRegion nonDraggable;
  nonDraggable.Sub(LayoutDeviceIntRect(0, 0, mBounds.width, mBounds.height), aRegion);

  __block bool changed = false;

  // Suppress calls to setNeedsDisplay during NSView geometry changes.
  ManipulateViewWithoutNeedingDisplay(mView, ^() {
    changed = mNonDraggableRegion.UpdateRegion(
        nonDraggable, *this, [mView nonDraggableViewsContainer], ^() {
          return [[NonDraggableView alloc] initWithFrame:NSZeroRect];
        });
  });

  if (changed) {
    // Trigger an update to the window server. This will call
    // mouseDownCanMoveWindow.
    // Doing this manually is only necessary because we're suppressing
    // setNeedsDisplay calls above.
    [[mView window] setMovableByWindowBackground:NO];
    [[mView window] setMovableByWindowBackground:YES];
  }
}

void nsChildView::ReportSwipeStarted(uint64_t aInputBlockId, bool aStartSwipe) {
  if (mSwipeEventQueue && mSwipeEventQueue->inputBlockId == aInputBlockId) {
    if (aStartSwipe) {
      PanGestureInput& startEvent = mSwipeEventQueue->queuedEvents[0];
      TrackScrollEventAsSwipe(startEvent, mSwipeEventQueue->allowedDirections);
      for (size_t i = 1; i < mSwipeEventQueue->queuedEvents.Length(); i++) {
        mSwipeTracker->ProcessEvent(mSwipeEventQueue->queuedEvents[i]);
      }
    }
    mSwipeEventQueue = nullptr;
  }
}

nsEventStatus nsChildView::DispatchAPZInputEvent(InputData& aEvent) {
  APZEventResult result;

  if (mAPZC) {
    result = mAPZC->InputBridge()->ReceiveInputEvent(aEvent);
  }

  if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
    return result.GetStatus();
  }

  if (aEvent.mInputType == PINCHGESTURE_INPUT) {
    PinchGestureInput& pinchEvent = aEvent.AsPinchGestureInput();
    WidgetWheelEvent wheelEvent = pinchEvent.ToWidgetEvent(this);
    ProcessUntransformedAPZEvent(&wheelEvent, result);
  } else if (aEvent.mInputType == TAPGESTURE_INPUT) {
    TapGestureInput& tapEvent = aEvent.AsTapGestureInput();
    WidgetSimpleGestureEvent gestureEvent = tapEvent.ToWidgetEvent(this);
    ProcessUntransformedAPZEvent(&gestureEvent, result);
  } else {
    MOZ_ASSERT_UNREACHABLE();
  }

  return result.GetStatus();
}

void nsChildView::DispatchAPZWheelInputEvent(InputData& aEvent, bool aCanTriggerSwipe) {
  if (mSwipeTracker && aEvent.mInputType == PANGESTURE_INPUT) {
    // Give the swipe tracker a first pass at the event. If a new pan gesture
    // has been started since the beginning of the swipe, the swipe tracker
    // will know to ignore the event.
    nsEventStatus status = mSwipeTracker->ProcessEvent(aEvent.AsPanGestureInput());
    if (status == nsEventStatus_eConsumeNoDefault) {
      return;
    }
  }

  WidgetWheelEvent event(true, eWheel, this);

  if (mAPZC) {
    APZEventResult result;

    switch (aEvent.mInputType) {
      case PANGESTURE_INPUT: {
        result = mAPZC->InputBridge()->ReceiveInputEvent(aEvent);
        if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
          return;
        }

        PanGestureInput& panInput = aEvent.AsPanGestureInput();

        event = panInput.ToWidgetEvent(this);
        if (aCanTriggerSwipe && panInput.mOverscrollBehaviorAllowsSwipe) {
          SwipeInfo swipeInfo = SendMayStartSwipe(panInput);
          event.mCanTriggerSwipe = swipeInfo.wantsSwipe;
          if (swipeInfo.wantsSwipe) {
            if (result.GetStatus() == nsEventStatus_eIgnore) {
              // APZ has determined and that scrolling horizontally in the
              // requested direction is impossible, so it didn't do any
              // scrolling for the event.
              // We know now that MayStartSwipe wants a swipe, so we can start
              // the swipe now.
              TrackScrollEventAsSwipe(panInput, swipeInfo.allowedDirections);
            } else {
              // We don't know whether this event can start a swipe, so we need
              // to queue up events and wait for a call to ReportSwipeStarted.
              // APZ might already have started scrolling in response to the
              // event if it knew that it's the right thing to do. In that case
              // we'll still get a call to ReportSwipeStarted, and we will
              // discard the queued events at that point.
              mSwipeEventQueue =
                  MakeUnique<SwipeEventQueue>(swipeInfo.allowedDirections, result.mInputBlockId);
            }
          }
        }

        if (mSwipeEventQueue && mSwipeEventQueue->inputBlockId == result.mInputBlockId) {
          mSwipeEventQueue->queuedEvents.AppendElement(panInput);
        }
        break;
      }
      case SCROLLWHEEL_INPUT: {
        // For wheel events on OS X, send it to APZ using the WidgetInputEvent
        // variant of ReceiveInputEvent, because the APZInputBridge version of
        // that function has special handling (for delta multipliers etc.) that
        // we need to run. Using the InputData variant would bypass that and
        // go straight to the APZCTreeManager subclass.
        event = aEvent.AsScrollWheelInput().ToWidgetEvent(this);
        result = mAPZC->InputBridge()->ReceiveInputEvent(event);
        if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
          return;
        }
        break;
      };
      default:
        MOZ_CRASH("unsupported event type");
        return;
    }
    if (event.mMessage == eWheel && (event.mDeltaX != 0 || event.mDeltaY != 0)) {
      ProcessUntransformedAPZEvent(&event, result);
    }
    return;
  }

  nsEventStatus status;
  switch (aEvent.mInputType) {
    case PANGESTURE_INPUT: {
      PanGestureInput panInput = aEvent.AsPanGestureInput();
      if (panInput.mType == PanGestureInput::PANGESTURE_MAYSTART ||
          panInput.mType == PanGestureInput::PANGESTURE_START) {
        mCurrentPanGestureBelongsToSwipe = false;
      }
      if (mCurrentPanGestureBelongsToSwipe) {
        // Ignore this event. It's a momentum event from a scroll gesture
        // that was processed as a swipe, and the swipe animation has
        // already finished (so mSwipeTracker is already null).
        MOZ_ASSERT(panInput.IsMomentum(),
                   "If the fingers are still on the touchpad, we should still have a SwipeTracker, "
                   "and it should have consumed this event.");
        return;
      }

      event = panInput.ToWidgetEvent(this);
      if (aCanTriggerSwipe) {
        SwipeInfo swipeInfo = SendMayStartSwipe(panInput);

        // We're in the non-APZ case here, but we still want to know whether
        // the event was routed to a child process, so we use InputAPZContext
        // to get that piece of information.
        ScrollableLayerGuid guid;
        InputAPZContext context(guid, 0, nsEventStatus_eIgnore);

        event.mCanTriggerSwipe = swipeInfo.wantsSwipe;
        DispatchEvent(&event, status);
        if (swipeInfo.wantsSwipe) {
          if (context.WasRoutedToChildProcess()) {
            // We don't know whether this event can start a swipe, so we need
            // to queue up events and wait for a call to ReportSwipeStarted.
            mSwipeEventQueue = MakeUnique<SwipeEventQueue>(swipeInfo.allowedDirections, 0);
          } else if (event.TriggersSwipe()) {
            TrackScrollEventAsSwipe(panInput, swipeInfo.allowedDirections);
          }
        }

        if (mSwipeEventQueue && mSwipeEventQueue->inputBlockId == 0) {
          mSwipeEventQueue->queuedEvents.AppendElement(panInput);
        }
        return;
      }
      break;
    }
    case SCROLLWHEEL_INPUT: {
      event = aEvent.AsScrollWheelInput().ToWidgetEvent(this);
      break;
    }
    default:
      MOZ_CRASH("unexpected event type");
      return;
  }
  if (event.mMessage == eWheel && (event.mDeltaX != 0 || event.mDeltaY != 0)) {
    DispatchEvent(&event, status);
  }
}

void nsChildView::DispatchDoubleTapGesture(TimeStamp aEventTimeStamp,
                                           LayoutDeviceIntPoint aScreenPosition,
                                           mozilla::Modifiers aModifiers) {
  if (StaticPrefs::apz_mac_enable_double_tap_zoom_touchpad_gesture()) {
    PRIntervalTime eventIntervalTime = PR_IntervalNow();

    TapGestureInput event{
        TapGestureInput::TAPGESTURE_DOUBLE, eventIntervalTime, aEventTimeStamp,
        ViewAs<ScreenPixel>(aScreenPosition,
                            PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent),
        aModifiers};

    DispatchAPZInputEvent(event);
  } else {
    // Setup the "double tap" event.
    WidgetSimpleGestureEvent geckoEvent(true, eTapGesture, this);
    // do what convertCocoaMouseEvent does basically.
    geckoEvent.mRefPoint = aScreenPosition;
    geckoEvent.mModifiers = aModifiers;
    geckoEvent.mTime = PR_IntervalNow();
    geckoEvent.mTimeStamp = aEventTimeStamp;
    geckoEvent.mClickCount = 1;

    // Send the event.
    DispatchWindowEvent(geckoEvent);
  }
}

void nsChildView::LookUpDictionary(const nsAString& aText,
                                   const nsTArray<mozilla::FontRange>& aFontRangeArray,
                                   const bool aIsVertical, const LayoutDeviceIntPoint& aPoint) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSMutableAttributedString* attrStr = nsCocoaUtils::GetNSMutableAttributedString(
      aText, aFontRangeArray, aIsVertical, BackingScaleFactor());
  NSPoint pt = nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());
  NSDictionary* attributes = [attrStr attributesAtIndex:0 effectiveRange:nil];
  NSFont* font = [attributes objectForKey:NSFontAttributeName];
  if (font) {
    if (aIsVertical) {
      pt.x -= [font descender];
    } else {
      pt.y += [font ascender];
    }
  }

  [mView showDefinitionForAttributedString:attrStr atPoint:pt];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

#ifdef ACCESSIBILITY
already_AddRefed<a11y::LocalAccessible> nsChildView::GetDocumentAccessible() {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return nullptr;

  // mAccessible might be dead if accessibility was previously disabled and is
  // now being enabled again.
  if (mAccessible && mAccessible->IsAlive()) {
    RefPtr<a11y::LocalAccessible> ret;
    CallQueryReferent(mAccessible.get(), static_cast<a11y::LocalAccessible**>(getter_AddRefs(ret)));
    return ret.forget();
  }

  // need to fetch the accessible anew, because it has gone away.
  // cache the accessible in our weak ptr
  RefPtr<a11y::LocalAccessible> acc = GetRootAccessible();
  mAccessible = do_GetWeakReference(acc.get());

  return acc.forget();
}
#endif

class WidgetsReleaserRunnable final : public mozilla::Runnable {
 public:
  explicit WidgetsReleaserRunnable(nsTArray<nsCOMPtr<nsIWidget>>&& aWidgetArray)
      : mozilla::Runnable("WidgetsReleaserRunnable"), mWidgetArray(std::move(aWidgetArray)) {}

  // Do nothing; all this runnable does is hold a reference the widgets in
  // mWidgetArray, and those references will be dropped when this runnable
  // is destroyed.

 private:
  nsTArray<nsCOMPtr<nsIWidget>> mWidgetArray;
};

#pragma mark -

// ViewRegionContainerView is a view class for certain subviews of ChildView
// which contain the NSViews created for ViewRegions (see ViewRegion.h).
// It doesn't do anything interesting, it only acts as a container so that it's
// easier for ChildView to control the z order of its children.
@interface ViewRegionContainerView : NSView {
}
@end

@implementation ViewRegionContainerView

- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;  // Be transparent to mouse events.
}

- (BOOL)isFlipped {
  return [[self superview] isFlipped];
}

- (BOOL)mouseDownCanMoveWindow {
  return [[self superview] mouseDownCanMoveWindow];
}

@end

@implementation ChildView

// globalDragPboard is non-null during native drag sessions that did not originate
// in our native NSView (it is set in |draggingEntered:|). It is unset when the
// drag session ends for this view, either with the mouse exiting or when a drop
// occurs in this view.
NSPasteboard* globalDragPboard = nil;

// gLastDragView and gLastDragMouseDownEvent are used to communicate information
// to the drag service during drag invocation (starting a drag in from the view).
// gLastDragView is only non-null while a mouse button is pressed, so between
// mouseDown and mouseUp.
NSView* gLastDragView = nil;             // [weak]
NSEvent* gLastDragMouseDownEvent = nil;  // [strong]

+ (void)initialize {
  static BOOL initialized = NO;

  if (!initialized) {
    // Inform the OS about the types of services (from the "Services" menu)
    // that we can handle.
    NSArray* types = @[
      [UTIHelper stringFromPboardType:NSPasteboardTypeString],
      [UTIHelper stringFromPboardType:NSPasteboardTypeHTML]
    ];
    [NSApp registerServicesMenuSendTypes:types returnTypes:types];
    initialized = YES;
  }
}

+ (void)registerViewForDraggedTypes:(NSView*)aView {
  [aView
      registerForDraggedTypes:
          [NSArray
              arrayWithObjects:[UTIHelper stringFromPboardType:NSFilenamesPboardType],
                               [UTIHelper stringFromPboardType:kMozFileUrlsPboardType],
                               [UTIHelper stringFromPboardType:NSPasteboardTypeString],
                               [UTIHelper stringFromPboardType:NSPasteboardTypeHTML],
                               [UTIHelper
                                   stringFromPboardType:(NSString*)kPasteboardTypeFileURLPromise],
                               [UTIHelper stringFromPboardType:kMozWildcardPboardType],
                               [UTIHelper stringFromPboardType:kPublicUrlPboardType],
                               [UTIHelper stringFromPboardType:kPublicUrlNamePboardType],
                               [UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType], nil]];
}

// initWithFrame:geckoChild:
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((self = [super initWithFrame:inFrame])) {
    mGeckoChild = inChild;
    mBlockedLastMouseDown = NO;
    mExpectingWheelStop = NO;

    mLastMouseDownEvent = nil;
    mLastKeyDownEvent = nil;
    mClickThroughMouseDownEvent = nil;
    mDragService = nullptr;

    mGestureState = eGestureState_None;
    mCumulativeRotation = 0.0;

    mIsUpdatingLayer = NO;

    [self setFocusRingType:NSFocusRingTypeNone];

#ifdef __LP64__
    mCancelSwipeAnimation = nil;
#endif

    mNonDraggableViewsContainer = [[ViewRegionContainerView alloc] initWithFrame:[self bounds]];
    mVibrancyViewsContainer = [[ViewRegionContainerView alloc] initWithFrame:[self bounds]];

    [mNonDraggableViewsContainer setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [mVibrancyViewsContainer setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    [self addSubview:mNonDraggableViewsContainer];
    [self addSubview:mVibrancyViewsContainer];

    mPixelHostingView = [[PixelHostingView alloc] initWithFrame:[self bounds]];
    [mPixelHostingView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    [self addSubview:mPixelHostingView];

    mRootCALayer = [[CALayer layer] retain];
    mRootCALayer.position = NSZeroPoint;
    mRootCALayer.bounds = NSZeroRect;
    mRootCALayer.anchorPoint = NSZeroPoint;
    mRootCALayer.contentsGravity = kCAGravityTopLeft;
    [[mPixelHostingView layer] addSublayer:mRootCALayer];

    mLastPressureStage = 0;
  }

  // register for things we'll take from other applications
  [ChildView registerViewForDraggedTypes:self];

  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (NSTextInputContext*)inputContext {
  if (!mGeckoChild) {
    // -[ChildView widgetDestroyed] has been called, but
    // -[ChildView delayedTearDown] has not yet completed.  Accessing
    // [super inputContext] now would uselessly recreate a text input context
    // for us, under which -[ChildView validAttributesForMarkedText] would
    // be called and the assertion checking for mTextInputHandler would fail.
    // We return nil to avoid that.
    return nil;
  }
  return [super inputContext];
}

- (void)installTextInputHandler:(TextInputHandler*)aHandler {
  mTextInputHandler = aHandler;
}

- (void)uninstallTextInputHandler {
  mTextInputHandler = nullptr;
}

- (NSView*)vibrancyViewsContainer {
  return mVibrancyViewsContainer;
}

- (NSView*)nonDraggableViewsContainer {
  return mNonDraggableViewsContainer;
}

- (NSView*)pixelHostingView {
  return mPixelHostingView;
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mLastMouseDownEvent release];
  [mLastKeyDownEvent release];
  [mClickThroughMouseDownEvent release];
  ChildViewMouseTracker::OnDestroyView(self);

  [mVibrancyViewsContainer removeFromSuperview];
  [mVibrancyViewsContainer release];
  [mNonDraggableViewsContainer removeFromSuperview];
  [mNonDraggableViewsContainer release];
  [mPixelHostingView removeFromSuperview];
  [mPixelHostingView release];
  [mRootCALayer release];

  if (gLastDragView == self) {
    gLastDragView = nil;
  }

  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)widgetDestroyed {
  if (mTextInputHandler) {
    mTextInputHandler->OnDestroyWidget(mGeckoChild);
    mTextInputHandler = nullptr;
  }
  mGeckoChild = nullptr;

  // Just in case we're destroyed abruptly and missed the draggingExited
  // or performDragOperation message.
  NS_IF_RELEASE(mDragService);
}

// mozView method, return our gecko child view widget. Note this does not AddRef.
- (nsIWidget*)widget {
  return static_cast<nsIWidget*>(mGeckoChild);
}

- (NSString*)description {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return [NSString stringWithFormat:@"ChildView %p, gecko child %p, frame %@", self, mGeckoChild,
                                    NSStringFromRect([self frame])];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

// Make the origin of this view the topLeft corner (gecko origin) rather
// than the bottomLeft corner (standard cocoa origin).
- (BOOL)isFlipped {
  return YES;
}

// We accept key and mouse events, so don't keep passing them up the chain. Allow
// this to be a 'focused' widget for event dispatch.
- (BOOL)acceptsFirstResponder {
  return YES;
}

// Accept mouse down events on background windows
- (BOOL)acceptsFirstMouse:(NSEvent*)aEvent {
  if (![[self window] isKindOfClass:[PopupWindow class]]) {
    // We rely on this function to tell us that the mousedown was on a
    // background window. Inside mouseDown we can't tell whether we were
    // inactive because at that point we've already been made active.
    // Unfortunately, acceptsFirstMouse is called for PopupWindows even when
    // their parent window is active, so ignore this on them for now.
    mClickThroughMouseDownEvent = [aEvent retain];
  }
  return YES;
}

- (BOOL)mouseDownCanMoveWindow {
  // Return YES so that parts of this view can be draggable. The non-draggable
  // parts will be covered by NSViews that return NO from
  // mouseDownCanMoveWindow and thus override draggability from the inside.
  // These views are assembled in nsChildView::UpdateWindowDraggingRegion.
  return YES;
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  if (mGeckoChild) {
    // actually, it could be the color space that's changed,
    // but we can't tell the difference here except by retrieving
    // the backing scale factor and comparing to the old value
    mGeckoChild->BackingScaleFactorChanged();
  }
}

- (BOOL)isCoveringTitlebar {
  return [[self window] isKindOfClass:[BaseWindow class]] &&
         [(BaseWindow*)[self window] mainChildView] == self &&
         [(BaseWindow*)[self window] drawsContentsIntoWindowFrame];
}

- (void)viewWillStartLiveResize {
  nsCocoaWindow* windowWidget = mGeckoChild ? mGeckoChild->GetAppWindowWidget() : nullptr;
  if (windowWidget) {
    windowWidget->NotifyLiveResizeStarted();
  }
}

- (void)viewDidEndLiveResize {
  // mGeckoChild may legitimately be null here. It should also have been null
  // in viewWillStartLiveResize, so there's no problem. However if we run into
  // cases where the windowWidget was non-null in viewWillStartLiveResize but
  // is null here, that might be problematic because we might get stuck with
  // a content process that has the displayport suppressed. If that scenario
  // arises (I'm not sure that it does) we will need to handle it gracefully.
  nsCocoaWindow* windowWidget = mGeckoChild ? mGeckoChild->GetAppWindowWidget() : nullptr;
  if (windowWidget) {
    windowWidget->NotifyLiveResizeStopped();
  }
}

- (void)markLayerForDisplay {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!mIsUpdatingLayer) {
    // This call will cause updateRootCALayer to be called during the upcoming
    // main thread CoreAnimation transaction. It will also trigger a transaction
    // if no transaction is currently pending.
    [[mPixelHostingView layer] setNeedsDisplay];
  }
}

- (void)ensureNextCompositeIsAtomicWithMainThreadPaint {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mGeckoChild) {
    mGeckoChild->SuspendAsyncCATransactions();
  }
}

- (void)updateRootCALayer {
  if (NS_IsMainThread() && mGeckoChild) {
    MOZ_RELEASE_ASSERT(!mIsUpdatingLayer, "Re-entrant layer display?");
    mIsUpdatingLayer = YES;
    mGeckoChild->HandleMainThreadCATransaction();
    mIsUpdatingLayer = NO;
  }
}

- (CALayer*)rootCALayer {
  return mRootCALayer;
}

// If we've just created a non-native context menu, we need to mark it as
// such and let the OS (and other programs) know when it opens and closes
// (this is how the OS knows to close other programs' context menus when
// ours open).  We send the initial notification here, but others are sent
// in nsCocoaWindow::Show().
- (void)maybeInitContextMenuTracking {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

#ifdef MOZ_USE_NATIVE_POPUP_WINDOWS
  return;
#endif /* MOZ_USE_NATIVE_POPUP_WINDOWS */

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE_VOID(rollupListener);
  nsCOMPtr<nsIWidget> widget = rollupListener->GetRollupWidget();
  NS_ENSURE_TRUE_VOID(widget);

  NSWindow* popupWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
  if (!popupWindow || ![popupWindow isKindOfClass:[PopupWindow class]]) return;

  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                    object:@"org.mozilla.gecko.PopupWindow"];
  [(PopupWindow*)popupWindow setIsContextMenu:YES];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Returns true if the event should no longer be processed, false otherwise.
// This does not return whether or not anything was rolled up.
- (BOOL)maybeRollup:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  BOOL consumeEvent = NO;

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE(rollupListener, false);

  if (rollupListener->RollupNativeMenu()) {
    // A native menu was rolled up.
    // Don't consume this event; if the menu wanted to consume this event it would already have done
    // so and we wouldn't even get here. For example, we won't get here for left clicks that close
    // native menus (because the native menu consumes it), but we will get here for right clicks
    // that close native menus, and we do not want to consume those right clicks.
    return NO;
  }

  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if (rollupWidget) {
    NSWindow* currentPopup = static_cast<NSWindow*>(rollupWidget->GetNativeData(NS_NATIVE_WINDOW));
    if (!nsCocoaUtils::IsEventOverWindow(theEvent, currentPopup)) {
      // event is not over the rollup window, default is to roll up
      bool shouldRollup = true;

      // check to see if scroll/zoom events should roll up the popup
      if ([theEvent type] == NSEventTypeScrollWheel || [theEvent type] == NSEventTypeMagnify ||
          [theEvent type] == NSEventTypeSmartMagnify) {
        shouldRollup = rollupListener->ShouldRollupOnMouseWheelEvent();
        // consume scroll events that aren't over the popup
        // unless the popup is an arrow panel
        consumeEvent = rollupListener->ShouldConsumeOnMouseWheelEvent();
      }

      // if we're dealing with menus, we probably have submenus and
      // we don't want to rollup if the click is in a parent menu of
      // the current submenu
      uint32_t popupsToRollup = UINT32_MAX;
      AutoTArray<nsIWidget*, 5> widgetChain;
      uint32_t sameTypeCount = rollupListener->GetSubmenuWidgetChain(&widgetChain);
      for (uint32_t i = 0; i < widgetChain.Length(); i++) {
        nsIWidget* widget = widgetChain[i];
        NSWindow* currWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
        if (nsCocoaUtils::IsEventOverWindow(theEvent, currWindow)) {
          // don't roll up if the mouse event occurred within a menu of the
          // same type. If the mouse event occurred in a menu higher than
          // that, roll up, but pass the number of popups to Rollup so
          // that only those of the same type close up.
          if (i < sameTypeCount) {
            shouldRollup = false;
          } else {
            popupsToRollup = sameTypeCount;
          }
          break;
        }
      }

      if (shouldRollup) {
        if ([theEvent type] == NSEventTypeLeftMouseDown) {
          NSPoint point = [NSEvent mouseLocation];
          FlipCocoaScreenCoordinate(point);
          LayoutDeviceIntPoint devPoint = mGeckoChild->CocoaPointsToDevPixels(point);
          gfx::IntPoint pos = devPoint.ToUnknownPoint();
          consumeEvent = (BOOL)rollupListener->Rollup(popupsToRollup, true, &pos, nullptr);
        } else {
          consumeEvent = (BOOL)rollupListener->Rollup(popupsToRollup, true, nullptr, nullptr);
        }
      }
    }
  }

  return consumeEvent;

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

- (void)swipeWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!anEvent || !mGeckoChild) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float deltaX = [anEvent deltaX];  // left=1.0, right=-1.0
  float deltaY = [anEvent deltaY];  // up=1.0, down=-1.0

  // Setup the "swipe" event.
  WidgetSimpleGestureEvent geckoEvent(true, eSwipeGesture, mGeckoChild);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

  // Record the left/right direction.
  if (deltaX > 0.0)
    geckoEvent.mDirection |= dom::SimpleGestureEvent_Binding::DIRECTION_LEFT;
  else if (deltaX < 0.0)
    geckoEvent.mDirection |= dom::SimpleGestureEvent_Binding::DIRECTION_RIGHT;

  // Record the up/down direction.
  if (deltaY > 0.0)
    geckoEvent.mDirection |= dom::SimpleGestureEvent_Binding::DIRECTION_UP;
  else if (deltaY < 0.0)
    geckoEvent.mDirection |= dom::SimpleGestureEvent_Binding::DIRECTION_DOWN;

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Pinch zoom gesture.
- (void)magnifyWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if ([self maybeRollup:anEvent]) {
    return;
  }

  if (!mGeckoChild) {
    return;
  }

  // Instead of calling beginOrEndGestureForEventPhase we basically inline
  // the effects of it here, because that function doesn't play too well with
  // how we create PinchGestureInput events below. The main point of that
  // function is to avoid flip-flopping between rotation/magnify gestures, which
  // we can do by checking and setting mGestureState appropriately. A secondary
  // result of that function is to send the final eMagnifyGesture event when
  // the gesture ends, but APZ takes care of that for us.
  if (mGestureState == eGestureState_RotateGesture && [anEvent phase] != NSEventPhaseBegan) {
    // If we're already in a rotation and not "starting" a magnify, abort.
    return;
  }
  mGestureState = eGestureState_MagnifyGesture;

  NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(anEvent, [self window]);
  ScreenPoint position =
      ViewAs<ScreenPixel>([self convertWindowCoordinatesRoundDown:locationInWindow],
                          PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);
  ExternalPoint screenOffset =
      ViewAs<ExternalPixel>(mGeckoChild->WidgetToScreenOffset(),
                            PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);

  PRIntervalTime eventIntervalTime = PR_IntervalNow();
  TimeStamp eventTimeStamp = nsCocoaUtils::GetEventTimeStamp([anEvent timestamp]);
  NSEventPhase eventPhase = [anEvent phase];
  PinchGestureInput::PinchGestureType pinchGestureType;

  switch (eventPhase) {
    case NSEventPhaseBegan: {
      pinchGestureType = PinchGestureInput::PINCHGESTURE_START;
      break;
    }
    case NSEventPhaseChanged: {
      pinchGestureType = PinchGestureInput::PINCHGESTURE_SCALE;
      break;
    }
    case NSEventPhaseEnded: {
      pinchGestureType = PinchGestureInput::PINCHGESTURE_END;
      mGestureState = eGestureState_None;
      break;
    }
    default: {
      NS_WARNING("Unexpected phase for pinch gesture event.");
      return;
    }
  }

  PinchGestureInput event{pinchGestureType,
                          PinchGestureInput::TRACKPAD,
                          eventIntervalTime,
                          eventTimeStamp,
                          screenOffset,
                          position,
                          100.0,
                          100.0 * (1.0 - [anEvent magnification]),
                          nsCocoaUtils::ModifiersForEvent(anEvent)};

  mGeckoChild->DispatchAPZInputEvent(event);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Smart zoom gesture, i.e. two-finger double tap on trackpads.
- (void)smartMagnifyWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!anEvent || !mGeckoChild || [self beginOrEndGestureForEventPhase:anEvent]) {
    return;
  }

  if ([self maybeRollup:anEvent]) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if (StaticPrefs::apz_mac_enable_double_tap_zoom_touchpad_gesture()) {
    TimeStamp eventTimeStamp = nsCocoaUtils::GetEventTimeStamp([anEvent timestamp]);
    NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(anEvent, [self window]);
    LayoutDevicePoint position = [self convertWindowCoordinatesRoundDown:locationInWindow];

    mGeckoChild->DispatchDoubleTapGesture(eventTimeStamp, RoundedToInt(position),
                                          nsCocoaUtils::ModifiersForEvent(anEvent));
  } else {
    // Setup the "double tap" event.
    WidgetSimpleGestureEvent geckoEvent(true, eTapGesture, mGeckoChild);
    [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
    geckoEvent.mClickCount = 1;

    // Send the event.
    mGeckoChild->DispatchWindowEvent(geckoEvent);
  }

  // Clear the gesture state
  mGestureState = eGestureState_None;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)rotateWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!anEvent || !mGeckoChild || [self beginOrEndGestureForEventPhase:anEvent]) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float rotation = [anEvent rotation];

  EventMessage msg;
  switch (mGestureState) {
    case eGestureState_StartGesture:
      msg = eRotateGestureStart;
      mGestureState = eGestureState_RotateGesture;
      break;

    case eGestureState_RotateGesture:
      msg = eRotateGestureUpdate;
      break;

    case eGestureState_None:
    case eGestureState_MagnifyGesture:
    default:
      return;
  }

  // Setup the event.
  WidgetSimpleGestureEvent geckoEvent(true, msg, mGeckoChild);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mDelta = -rotation;
  if (rotation > 0.0) {
    geckoEvent.mDirection = dom::SimpleGestureEvent_Binding::ROTATION_COUNTERCLOCKWISE;
  } else {
    geckoEvent.mDirection = dom::SimpleGestureEvent_Binding::ROTATION_CLOCKWISE;
  }

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Keep track of the cumulative rotation for the final "rotate" event.
  mCumulativeRotation += rotation;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// `beginGestureWithEvent` and `endGestureWithEvent` are not called for
// applications that link against the macOS 10.11 or later SDK when we're
// running on macOS 10.11 or later. For compatibility with all supported macOS
// versions, we have to call {begin,end}GestureWithEvent ourselves based on
// the event phase when we're handling gestures.
- (bool)beginOrEndGestureForEventPhase:(NSEvent*)aEvent {
  if (!aEvent) {
    return false;
  }

  if (aEvent.phase == NSEventPhaseBegan) {
    [self beginGestureWithEvent:aEvent];
    return true;
  }

  if (aEvent.phase == NSEventPhaseEnded || aEvent.phase == NSEventPhaseCancelled) {
    [self endGestureWithEvent:aEvent];
    return true;
  }

  return false;
}

- (void)beginGestureWithEvent:(NSEvent*)aEvent {
  if (!aEvent) {
    return;
  }

  mGestureState = eGestureState_StartGesture;
  mCumulativeRotation = 0.0;
}

- (void)endGestureWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!anEvent || !mGeckoChild) {
    // Clear the gestures state if we cannot send an event.
    mGestureState = eGestureState_None;
    mCumulativeRotation = 0.0;
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  switch (mGestureState) {
    case eGestureState_RotateGesture: {
      // Setup the "rotate" event.
      WidgetSimpleGestureEvent geckoEvent(true, eRotateGesture, mGeckoChild);
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
      geckoEvent.mDelta = -mCumulativeRotation;
      if (mCumulativeRotation > 0.0) {
        geckoEvent.mDirection = dom::SimpleGestureEvent_Binding::ROTATION_COUNTERCLOCKWISE;
      } else {
        geckoEvent.mDirection = dom::SimpleGestureEvent_Binding::ROTATION_CLOCKWISE;
      }

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    } break;

    case eGestureState_MagnifyGesture:  // APZ handles sending the widget events
    case eGestureState_None:
    case eGestureState_StartGesture:
    default:
      break;
  }

  // Clear the gestures state.
  mGestureState = eGestureState_None;
  mCumulativeRotation = 0.0;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (bool)shouldConsiderStartingSwipeFromEvent:(NSEvent*)anEvent {
  // This method checks whether the AppleEnableSwipeNavigateWithScrolls global
  // preference is set.  If it isn't, fluid swipe tracking is disabled, and a
  // horizontal two-finger gesture is always a scroll (even in Safari).  This
  // preference can't (currently) be set from the Preferences UI -- only using
  // 'defaults write'.
  if (![NSEvent isSwipeTrackingFromScrollEventsEnabled]) {
    return false;
  }

  // Only initiate horizontal tracking for gestures that have just begun --
  // otherwise a scroll to one side of the page can have a swipe tacked on
  // to it.
  NSEventPhase eventPhase = [anEvent phase];
  if ([anEvent type] != NSEventTypeScrollWheel || eventPhase != NSEventPhaseBegan ||
      ![anEvent hasPreciseScrollingDeltas]) {
    return false;
  }

  // Only initiate horizontal tracking for events whose horizontal element is
  // at least eight times larger than its vertical element. This minimizes
  // performance problems with vertical scrolls (by minimizing the possibility
  // that they'll be misinterpreted as horizontal swipes), while still
  // tolerating a small vertical element to a true horizontal swipe.  The number
  // '8' was arrived at by trial and error.
  CGFloat deltaX = [anEvent scrollingDeltaX];
  CGFloat deltaY = [anEvent scrollingDeltaY];
  return std::abs(deltaX) > std::abs(deltaY) * 8;
}

- (void)setUsingOMTCompositor:(BOOL)aUseOMTC {
  mUsingOMTCompositor = aUseOMTC;
}

// Returning NO from this method only disallows ordering on mousedown - in order
// to prevent it for mouseup too, we need to call [NSApp preventWindowOrdering]
// when handling the mousedown event.
- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)aEvent {
  // Always using system-provided window ordering for normal windows.
  if (![[self window] isKindOfClass:[PopupWindow class]]) return NO;

  // Don't reorder when we don't have a parent window, like when we're a
  // context menu or a tooltip.
  return ![[self window] parentWindow];
}

- (void)mouseDown:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if ([self shouldDelayWindowOrderingForEvent:theEvent]) {
    [NSApp preventWindowOrdering];
  }

  // If we've already seen this event due to direct dispatch from menuForEvent:
  // just bail; if not, remember it.
  if (mLastMouseDownEvent == theEvent) {
    [mLastMouseDownEvent release];
    mLastMouseDownEvent = nil;
    return;
  } else {
    [mLastMouseDownEvent release];
    mLastMouseDownEvent = [theEvent retain];
  }

  [gLastDragMouseDownEvent release];
  gLastDragMouseDownEvent = [theEvent retain];
  gLastDragView = self;

  // We need isClickThrough because at this point the window we're in might
  // already have become main, so the check for isMainWindow in
  // WindowAcceptsEvent isn't enough. It also has to check isClickThrough.
  BOOL isClickThrough = (theEvent == mClickThroughMouseDownEvent);
  [mClickThroughMouseDownEvent release];
  mClickThroughMouseDownEvent = nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if ([self maybeRollup:theEvent] ||
      !ChildViewMouseTracker::WindowAcceptsEvent([self window], theEvent, self, isClickThrough)) {
    // Remember blocking because that means we want to block mouseup as well.
    mBlockedLastMouseDown = YES;
    return;
  }

  // in order to send gecko events we'll need a gecko widget
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  NSInteger clickCount = [theEvent clickCount];
  if (mBlockedLastMouseDown && clickCount > 1) {
    // Don't send a double click if the first click of the double click was
    // blocked.
    clickCount--;
  }
  geckoEvent.mClickCount = clickCount;

  if (!StaticPrefs::dom_event_treat_ctrl_click_as_right_click_disabled() &&
      geckoEvent.IsControl()) {
    geckoEvent.mButton = MouseButton::eSecondary;
  } else {
    geckoEvent.mButton = MouseButton::ePrimary;
    // Don't send a click if ctrl key is pressed.
    geckoEvent.mClickEventPrevented = geckoEvent.IsControl();
  }

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  mBlockedLastMouseDown = NO;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)mouseUp:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  gLastDragView = nil;

  if (!mGeckoChild || mBlockedLastMouseDown) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  if (!StaticPrefs::dom_event_treat_ctrl_click_as_right_click_disabled() &&
      ([theEvent modifierFlags] & NSEventModifierFlagControl)) {
    geckoEvent.mButton = MouseButton::eSecondary;
  } else {
    geckoEvent.mButton = MouseButton::ePrimary;
  }

  // Remember the event's position before calling DispatchInputEvent, because
  // that call can mutate it and convert it into a different coordinate space.
  LayoutDeviceIntPoint pos = geckoEvent.mRefPoint;

  // This might destroy our widget (and null out mGeckoChild).
  bool defaultPrevented =
      (mGeckoChild->DispatchInputEvent(&geckoEvent) == nsEventStatus_eConsumeNoDefault);

  if (!mGeckoChild) {
    return;
  }

  // Check to see if we are double-clicking in draggable parts of the window.
  if (!defaultPrevented && [theEvent clickCount] == 2 &&
      !mGeckoChild->GetNonDraggableRegion().Contains(pos.x, pos.y)) {
    if (nsCocoaUtils::ShouldZoomOnTitlebarDoubleClick()) {
      [[self window] performZoom:nil];
    } else if (nsCocoaUtils::ShouldMinimizeOnTitlebarDoubleClick()) {
      [[self window] performMiniaturize:nil];
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                         exitFrom:(WidgetMouseEvent::ExitFrom)aExitFrom {
  if (!mGeckoChild) return;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, [self window]);
  NSPoint localEventLocation = [self convertPoint:windowEventLocation fromView:nil];

  EventMessage msg = aEnter ? eMouseEnterIntoWidget : eMouseExitFromWidget;
  WidgetMouseEvent event(true, msg, mGeckoChild, WidgetMouseEvent::eReal);
  event.mRefPoint = mGeckoChild->CocoaPointsToDevPixels(localEventLocation);
  if (event.mMessage == eMouseExitFromWidget) {
    event.mExitFrom = Some(aExitFrom);
  }
  nsEventStatus status;  // ignored
  mGeckoChild->DispatchEvent(&event, status);
}

- (void)handleMouseMoved:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)mouseDragged:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  // Note, sending the above event might have destroyed our widget since we didn't retain.
  // Fine so long as we don't access any local variables from here on.

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)rightMouseDown:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  // The right mouse went down, fire off a right mouse down event to gecko
  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eSecondary;
  geckoEvent.mClickCount = [theEvent clickCount];

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return;

  if (!StaticPrefs::ui_context_menus_after_mouseup()) {
    // Let the superclass do the context menu stuff.
    [super rightMouseDown:theEvent];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)rightMouseUp:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eSecondary;
  geckoEvent.mClickCount = [theEvent clickCount];

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return;

  if (StaticPrefs::ui_context_menus_after_mouseup()) {
    // Let the superclass do the context menu stuff, but pretend it's rightMouseDown.
    NSEvent* dupeEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                            location:theEvent.locationInWindow
                                       modifierFlags:theEvent.modifierFlags
                                           timestamp:theEvent.timestamp
                                        windowNumber:theEvent.windowNumber
                                             context:nil
                                         eventNumber:theEvent.eventNumber
                                          clickCount:theEvent.clickCount
                                            pressure:theEvent.pressure];

    [super rightMouseDown:dupeEvent];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)rightMouseDragged:(NSEvent*)theEvent {
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eSecondary;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)otherMouseDown:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if ([self maybeRollup:theEvent] ||
      !ChildViewMouseTracker::WindowAcceptsEvent([self window], theEvent, self))
    return;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eMiddle;
  geckoEvent.mClickCount = [theEvent clickCount];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)otherMouseUp:(NSEvent*)theEvent {
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eMiddle;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)otherMouseDragged:(NSEvent*)theEvent {
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eMiddle;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)sendWheelStartOrStop:(EventMessage)msg forEvent:(NSEvent*)theEvent {
  WidgetWheelEvent wheelEvent(true, msg, mGeckoChild);
  [self convertCocoaMouseWheelEvent:theEvent toGeckoEvent:&wheelEvent];
  mExpectingWheelStop = (msg == eWheelOperationStart);
  mGeckoChild->DispatchInputEvent(wheelEvent.AsInputEvent());
}

- (void)sendWheelCondition:(BOOL)condition
                     first:(EventMessage)first
                    second:(EventMessage)second
                  forEvent:(NSEvent*)theEvent {
  if (mExpectingWheelStop == condition) {
    [self sendWheelStartOrStop:first forEvent:theEvent];
  }
  [self sendWheelStartOrStop:second forEvent:theEvent];
}

static PanGestureInput::PanGestureType PanGestureTypeForEvent(NSEvent* aEvent) {
  switch ([aEvent phase]) {
    case NSEventPhaseMayBegin:
      return PanGestureInput::PANGESTURE_MAYSTART;
    case NSEventPhaseCancelled:
      return PanGestureInput::PANGESTURE_CANCELLED;
    case NSEventPhaseBegan:
      return PanGestureInput::PANGESTURE_START;
    case NSEventPhaseChanged:
      return PanGestureInput::PANGESTURE_PAN;
    case NSEventPhaseEnded:
      return PanGestureInput::PANGESTURE_END;
    case NSEventPhaseNone:
      switch ([aEvent momentumPhase]) {
        case NSEventPhaseBegan:
          return PanGestureInput::PANGESTURE_MOMENTUMSTART;
        case NSEventPhaseChanged:
          return PanGestureInput::PANGESTURE_MOMENTUMPAN;
        case NSEventPhaseEnded:
          return PanGestureInput::PANGESTURE_MOMENTUMEND;
        default:
          NS_ERROR("unexpected event phase");
          return PanGestureInput::PANGESTURE_PAN;
      }
    default:
      NS_ERROR("unexpected event phase");
      return PanGestureInput::PANGESTURE_PAN;
  }
}

static int32_t RoundUp(double aDouble) {
  return aDouble < 0 ? static_cast<int32_t>(floor(aDouble)) : static_cast<int32_t>(ceil(aDouble));
}

static gfx::IntPoint GetIntegerDeltaForEvent(NSEvent* aEvent) {
  if ([aEvent hasPreciseScrollingDeltas]) {
    // Pixel scroll events (events with hasPreciseScrollingDeltas == YES)
    // carry pixel deltas in the scrollingDeltaX/Y fields and line scroll
    // information in the deltaX/Y fields.
    // Prior to 10.12, these line scroll fields would be zero for most pixel
    // scroll events and non-zero for some, whenever at least a full line
    // worth of pixel scrolling had accumulated. That's the behavior we want.
    // Starting with 10.12 however, pixel scroll events no longer accumulate
    // deltaX and deltaY; they just report floating point values for every
    // single event. So we need to do our own accumulation.
    return PanGestureInput::GetIntegerDeltaForEvent([aEvent phase] == NSEventPhaseBegan,
                                                    [aEvent deltaX], [aEvent deltaY]);
  }

  // For line scrolls, or pre-10.12, just use the rounded up value of deltaX / deltaY.
  return gfx::IntPoint(RoundUp([aEvent deltaX]), RoundUp([aEvent deltaY]));
}

- (void)scrollWheel:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  ChildViewMouseTracker::MouseScrolled(theEvent);

  if ([self maybeRollup:theEvent]) {
    return;
  }

  if (!mGeckoChild) {
    return;
  }

  NSEventPhase phase = [theEvent phase];
  // Fire eWheelOperationStart/End events when 2 fingers touch/release the
  // touchpad.
  if (phase & NSEventPhaseMayBegin) {
    [self sendWheelCondition:YES
                       first:eWheelOperationEnd
                      second:eWheelOperationStart
                    forEvent:theEvent];
  } else if (phase & (NSEventPhaseEnded | NSEventPhaseCancelled)) {
    [self sendWheelCondition:NO
                       first:eWheelOperationStart
                      second:eWheelOperationEnd
                    forEvent:theEvent];
  }

  if (!mGeckoChild) {
    return;
  }
  RefPtr<nsChildView> geckoChildDeathGrip(mGeckoChild);

  NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(theEvent, [self window]);

  // Use convertWindowCoordinatesRoundDown when converting the position to
  // integer screen pixels in order to ensure that coordinates which are just
  // inside the right / bottom edges of the window don't end up outside of the
  // window after rounding.
  ScreenPoint position =
      ViewAs<ScreenPixel>([self convertWindowCoordinatesRoundDown:locationInWindow],
                          PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);

  bool usePreciseDeltas = [theEvent hasPreciseScrollingDeltas] &&
                          Preferences::GetBool("mousewheel.enable_pixel_scrolling", true);
  bool hasPhaseInformation = nsCocoaUtils::EventHasPhaseInformation(theEvent);

  gfx::IntPoint lineOrPageDelta = -GetIntegerDeltaForEvent(theEvent);

  Modifiers modifiers = nsCocoaUtils::ModifiersForEvent(theEvent);

  PRIntervalTime eventIntervalTime = PR_IntervalNow();
  TimeStamp eventTimeStamp = nsCocoaUtils::GetEventTimeStamp([theEvent timestamp]);

  ScreenPoint preciseDelta;
  if (usePreciseDeltas) {
    CGFloat pixelDeltaX = [theEvent scrollingDeltaX];
    CGFloat pixelDeltaY = [theEvent scrollingDeltaY];
    double scale = geckoChildDeathGrip->BackingScaleFactor();
    preciseDelta = ScreenPoint(-pixelDeltaX * scale, -pixelDeltaY * scale);
  }

  if (usePreciseDeltas && hasPhaseInformation) {
    PanGestureInput panEvent(PanGestureTypeForEvent(theEvent), eventIntervalTime, eventTimeStamp,
                             position, preciseDelta, modifiers);
    panEvent.mLineOrPageDeltaX = lineOrPageDelta.x;
    panEvent.mLineOrPageDeltaY = lineOrPageDelta.y;

    if (panEvent.mType == PanGestureInput::PANGESTURE_END) {
      // Check if there's a momentum start event in the event queue, so that we
      // can annotate this event.
      NSEvent* nextWheelEvent = [NSApp nextEventMatchingMask:NSEventMaskScrollWheel
                                                   untilDate:[NSDate distantPast]
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:NO];
      if (nextWheelEvent &&
          PanGestureTypeForEvent(nextWheelEvent) == PanGestureInput::PANGESTURE_MOMENTUMSTART) {
        panEvent.mFollowedByMomentum = true;
      }
    }

    bool canTriggerSwipe = [self shouldConsiderStartingSwipeFromEvent:theEvent];
    panEvent.mRequiresContentResponseIfCannotScrollHorizontallyInStartDirection = canTriggerSwipe;
    geckoChildDeathGrip->DispatchAPZWheelInputEvent(panEvent, canTriggerSwipe);
  } else if (usePreciseDeltas) {
    // This is on 10.6 or old touchpads that don't have any phase information.
    ScrollWheelInput wheelEvent(
        eventIntervalTime, eventTimeStamp, modifiers, ScrollWheelInput::SCROLLMODE_INSTANT,
        ScrollWheelInput::SCROLLDELTA_PIXEL, position, preciseDelta.x, preciseDelta.y, false,
        // This parameter is used for wheel delta
        // adjustment, such as auto-dir scrolling,
        // but we do't need to do anything special here
        // since this wheel event is sent to
        // DispatchAPZWheelInputEvent, which turns this
        // ScrollWheelInput back into a WidgetWheelEvent
        // and then it goes through the regular handling
        // in APZInputBridge. So passing |eNone| won't
        // pass up the necessary wheel delta adjustment.
        WheelDeltaAdjustmentStrategy::eNone);
    wheelEvent.mLineOrPageDeltaX = lineOrPageDelta.x;
    wheelEvent.mLineOrPageDeltaY = lineOrPageDelta.y;
    wheelEvent.mIsMomentum = nsCocoaUtils::IsMomentumScrollEvent(theEvent);
    geckoChildDeathGrip->DispatchAPZWheelInputEvent(wheelEvent, false);
  } else {
    ScrollWheelInput::ScrollMode scrollMode = ScrollWheelInput::SCROLLMODE_INSTANT;
    if (StaticPrefs::general_smoothScroll() && StaticPrefs::general_smoothScroll_mouseWheel()) {
      scrollMode = ScrollWheelInput::SCROLLMODE_SMOOTH;
    }
    ScrollWheelInput wheelEvent(eventIntervalTime, eventTimeStamp, modifiers, scrollMode,
                                ScrollWheelInput::SCROLLDELTA_LINE, position, lineOrPageDelta.x,
                                lineOrPageDelta.y, false,
                                // This parameter is used for wheel delta
                                // adjustment, such as auto-dir scrolling,
                                // but we do't need to do anything special here
                                // since this wheel event is sent to
                                // DispatchAPZWheelInputEvent, which turns this
                                // ScrollWheelInput back into a WidgetWheelEvent
                                // and then it goes through the regular handling
                                // in APZInputBridge. So passing |eNone| won't
                                // pass up the necessary wheel delta adjustment.
                                WheelDeltaAdjustmentStrategy::eNone);
    wheelEvent.mLineOrPageDeltaX = lineOrPageDelta.x;
    wheelEvent.mLineOrPageDeltaY = lineOrPageDelta.y;
    geckoChildDeathGrip->DispatchAPZWheelInputEvent(wheelEvent, false);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (NSMenu*)menuForEvent:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mGeckoChild) return nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild) return nil;

  // Cocoa doesn't always dispatch a mouseDown: for a control-click event,
  // depends on what we return from menuForEvent:. Gecko always expects one
  // and expects the mouse down event before the context menu event, so
  // get that event sent first if this is a left mouse click.
  if ([theEvent type] == NSEventTypeLeftMouseDown) {
    [self mouseDown:theEvent];
    if (!mGeckoChild) return nil;
  }

  WidgetMouseEvent geckoEvent(true, eContextMenu, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  if (StaticPrefs::dom_event_treat_ctrl_click_as_right_click_disabled() &&
      [theEvent type] == NSEventTypeLeftMouseDown) {
    geckoEvent.mContextMenuTrigger = WidgetMouseEvent::eControlClick;
    geckoEvent.mButton = MouseButton::ePrimary;
  } else {
    geckoEvent.mButton = MouseButton::eSecondary;
  }

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return nil;

  [self maybeInitContextMenuTracking];

  // We never return an actual NSMenu* for the context menu. Gecko might have
  // responded to the eContextMenu event by putting up a fake context menu.
  return nil;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)convertCocoaMouseWheelEvent:(NSEvent*)aMouseEvent
                       toGeckoEvent:(WidgetWheelEvent*)outWheelEvent {
  [self convertCocoaMouseEvent:aMouseEvent toGeckoEvent:outWheelEvent];

  bool usePreciseDeltas = [aMouseEvent hasPreciseScrollingDeltas] &&
                          Preferences::GetBool("mousewheel.enable_pixel_scrolling", true);

  outWheelEvent->mDeltaMode = usePreciseDeltas ? dom::WheelEvent_Binding::DOM_DELTA_PIXEL
                                               : dom::WheelEvent_Binding::DOM_DELTA_LINE;
  outWheelEvent->mIsMomentum = nsCocoaUtils::IsMomentumScrollEvent(aMouseEvent);
}

- (void)convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(WidgetInputEvent*)outGeckoEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NS_ASSERTION(outGeckoEvent,
               "convertCocoaMouseEvent:toGeckoEvent: requires non-null aoutGeckoEvent");
  if (!outGeckoEvent) return;

  nsCocoaUtils::InitInputEvent(*outGeckoEvent, aMouseEvent);

  // convert point to view coordinate system
  NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(aMouseEvent, [self window]);

  outGeckoEvent->mRefPoint = [self convertWindowCoordinates:locationInWindow];

  WidgetMouseEventBase* mouseEvent = outGeckoEvent->AsMouseEventBase();
  mouseEvent->mButtons = 0;
  NSUInteger mouseButtons = [NSEvent pressedMouseButtons];

  if (mouseButtons & 0x01) {
    mouseEvent->mButtons |= MouseButtonsFlag::ePrimaryFlag;
  }
  if (mouseButtons & 0x02) {
    mouseEvent->mButtons |= MouseButtonsFlag::eSecondaryFlag;
  }
  if (mouseButtons & 0x04) {
    mouseEvent->mButtons |= MouseButtonsFlag::eMiddleFlag;
  }
  if (mouseButtons & 0x08) {
    mouseEvent->mButtons |= MouseButtonsFlag::e4thFlag;
  }
  if (mouseButtons & 0x10) {
    mouseEvent->mButtons |= MouseButtonsFlag::e5thFlag;
  }

  switch ([aMouseEvent type]) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved:
      if ([aMouseEvent subtype] == NSEventSubtypeTabletPoint) {
        [self convertCocoaTabletPointerEvent:aMouseEvent toGeckoEvent:mouseEvent->AsMouseEvent()];
      }
      break;

    default:
      // Don't check other NSEvents for pressure.
      break;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)convertCocoaTabletPointerEvent:(NSEvent*)aPointerEvent
                          toGeckoEvent:(WidgetMouseEvent*)aOutGeckoEvent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN
  if (!aOutGeckoEvent || !sIsTabletPointerActivated) {
    return;
  }
  if ([aPointerEvent type] != NSEventTypeMouseMoved) {
    aOutGeckoEvent->mPressure = [aPointerEvent pressure];
    MOZ_ASSERT(aOutGeckoEvent->mPressure >= 0.0 && aOutGeckoEvent->mPressure <= 1.0);
  }
  aOutGeckoEvent->mInputSource = dom::MouseEvent_Binding::MOZ_SOURCE_PEN;
  aOutGeckoEvent->tiltX = lround([aPointerEvent tilt].x * 90);
  aOutGeckoEvent->tiltY = lround([aPointerEvent tilt].y * 90);
  aOutGeckoEvent->tangentialPressure = [aPointerEvent tangentialPressure];
  // Make sure the twist value is in the range of 0-359.
  int32_t twist = fmod([aPointerEvent rotation], 360);
  aOutGeckoEvent->twist = twist >= 0 ? twist : twist + 360;
  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)tabletProximity:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN
  sIsTabletPointerActivated = [theEvent isEnteringProximity];
  NS_OBJC_END_TRY_IGNORE_BLOCK
}

#pragma mark -
// NSTextInputClient implementation

- (NSRange)markedRange {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->MarkedRange();

  NS_OBJC_END_TRY_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRange)selectedRange {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->SelectedRange();

  NS_OBJC_END_TRY_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex {
  NS_ENSURE_TRUE(mTextInputHandler, NO);
  if (charIndex == NSNotFound) {
    return NO;
  }
  return mTextInputHandler->DrawsVerticallyForCharacterAtIndex(charIndex);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  NS_ENSURE_TRUE(mTextInputHandler, 0);
  return mTextInputHandler->CharacterIndexForPoint(thePoint);
}

- (NSArray*)validAttributesForMarkedText {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, [NSArray array]);
  return mTextInputHandler->GetValidAttributesForMarkedText();

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NS_ENSURE_TRUE_VOID(mGeckoChild);

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  NSAttributedString* attrStr;
  if ([aString isKindOfClass:[NSAttributedString class]]) {
    attrStr = static_cast<NSAttributedString*>(aString);
  } else {
    attrStr = [[[NSAttributedString alloc] initWithString:aString] autorelease];
  }

  mTextInputHandler->InsertText(attrStr, &replacementRange);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)doCommandBySelector:(SEL)aSelector {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mGeckoChild || !mTextInputHandler) {
    return;
  }

  const char* sel = reinterpret_cast<const char*>(aSelector);
  if (!mTextInputHandler->DoCommandBySelector(sel)) {
    [super doCommandBySelector:aSelector];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)unmarkText {
  NS_ENSURE_TRUE_VOID(mTextInputHandler);
  mTextInputHandler->CommitIMEComposition();
}

- (BOOL)hasMarkedText {
  NS_ENSURE_TRUE(mTextInputHandler, NO);
  return mTextInputHandler->HasMarkedText();
}

- (void)setMarkedText:(id)aString
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NS_ENSURE_TRUE_VOID(mTextInputHandler);

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  NSAttributedString* attrStr;
  if ([aString isKindOfClass:[NSAttributedString class]]) {
    attrStr = static_cast<NSAttributedString*>(aString);
  } else {
    attrStr = [[[NSAttributedString alloc] initWithString:aString] autorelease];
  }

  mTextInputHandler->SetMarkedText(attrStr, selectedRange, &replacementRange);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)aRange
                                               actualRange:(NSRangePointer)actualRange {
  NS_ENSURE_TRUE(mTextInputHandler, nil);
  return mTextInputHandler->GetAttributedSubstringFromRange(aRange, actualRange);
}

- (NSRect)firstRectForCharacterRange:(NSRange)aRange actualRange:(NSRangePointer)actualRange {
  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRect(0.0, 0.0, 0.0, 0.0));
  return mTextInputHandler->FirstRectForCharacterRange(aRange, actualRange);
}

- (void)quickLookWithEvent:(NSEvent*)event {
  // Show dictionary by current point
  WidgetContentCommandEvent contentCommandEvent(true, eContentCommandLookUpDictionary, mGeckoChild);
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  contentCommandEvent.mRefPoint = mGeckoChild->CocoaPointsToDevPixels(point);
  mGeckoChild->DispatchWindowEvent(contentCommandEvent);
  // The widget might have been destroyed.
}

- (NSInteger)windowLevel {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, [[self window] level]);
  return mTextInputHandler->GetWindowLevel();

  NS_OBJC_END_TRY_BLOCK_RETURN(NSNormalWindowLevel);
}

#pragma mark -

// This is a private API that Cocoa uses.
// Cocoa will call this after the menu system returns "NO" for "performKeyEquivalent:".
// We want all they key events we can get so just return YES. In particular, this fixes
// ctrl-tab - we don't get a "keyDown:" call for that without this.
- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  return YES;
}

- (NSEvent*)lastKeyDownEvent {
  return mLastKeyDownEvent;
}

- (void)keyDown:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mLastKeyDownEvent release];
  mLastKeyDownEvent = [theEvent retain];

  // Weird things can happen on keyboard input if the key window isn't in the
  // current space.  For example see bug 1056251.  To get around this, always
  // make sure that, if our window is key, it's also made frontmost.  Doing
  // this automatically switches to whatever space our window is in.  Safari
  // does something similar.  Our window should normally always be key --
  // otherwise why is the OS sending us a key down event?  But it's just
  // possible we're in Gecko's hidden window, so we check first.
  NSWindow* viewWindow = [self window];
  if (viewWindow && [viewWindow isKeyWindow]) {
    [viewWindow orderWindow:NSWindowAbove relativeTo:0];
  }

#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
  if (!Preferences::GetBool("intl.allow-insecure-text-input", false) && mGeckoChild &&
      mTextInputHandler && mTextInputHandler->IsFocused()) {
    NSWindow* window = [self window];
    NSString* info = [NSString
        stringWithFormat:
            @"\nview [%@], window [%@], window is key %i, is fullscreen %i, app is active %i", self,
            window, [window isKeyWindow], ([window styleMask] & (1 << 14)) != 0, [NSApp isActive]];
    nsAutoCString additionalInfo([info UTF8String]);

    if (mGeckoChild->GetInputContext().IsPasswordEditor() &&
        !TextInputHandler::IsSecureEventInputEnabled()) {
#  define CRASH_MESSAGE "A password editor has focus, but not in secure input mode"

      CrashReporter::AppendAppNotesToCrashReport("\nBug 893973: "_ns +
                                                 nsLiteralCString(CRASH_MESSAGE));
      CrashReporter::AppendAppNotesToCrashReport(additionalInfo);

      MOZ_CRASH(CRASH_MESSAGE);
#  undef CRASH_MESSAGE
    } else if (!mGeckoChild->GetInputContext().IsPasswordEditor() &&
               TextInputHandler::IsSecureEventInputEnabled()) {
#  define CRASH_MESSAGE "A non-password editor has focus, but in secure input mode"

      CrashReporter::AppendAppNotesToCrashReport("\nBug 893973: "_ns +
                                                 nsLiteralCString(CRASH_MESSAGE));
      CrashReporter::AppendAppNotesToCrashReport(additionalInfo);

      MOZ_CRASH(CRASH_MESSAGE);
#  undef CRASH_MESSAGE
    }
  }
#endif  // #if !defined(RELEASE_OR_BETA) || defined(DEBUG)

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  if (mGeckoChild) {
    if (mTextInputHandler) {
      sUniqueKeyEventId++;
      NSMutableDictionary* nativeKeyEventsMap = [ChildView sNativeKeyEventsMap];
      [nativeKeyEventsMap setObject:theEvent forKey:@(sUniqueKeyEventId)];
      // Purge old native events, in case we're still holding on to them. We
      // keep at most 10 references to 10 different native events.
      [nativeKeyEventsMap removeObjectForKey:@(sUniqueKeyEventId - 10)];
      mTextInputHandler->HandleKeyDownEvent(theEvent, sUniqueKeyEventId);
    } else {
      // There was no text input handler. Offer the event to the native menu
      // system to check if there are any registered custom shortcuts for this
      // event.
      mGeckoChild->SendEventToNativeMenuSystem(theEvent);
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)keyUp:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  mTextInputHandler->HandleKeyUpEvent(theEvent);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)insertNewline:(id)sender {
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::InsertParagraph);
  }
}

- (void)insertLineBreak:(id)sender {
  // Ctrl + Enter in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::InsertLineBreak);
  }
}

- (void)deleteBackward:(id)sender {
  // Backspace in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::DeleteCharBackward);
  }
}

- (void)deleteBackwardByDecomposingPreviousCharacter:(id)sender {
  // Ctrl + Backspace in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::DeleteCharBackward);
  }
}

- (void)deleteWordBackward:(id)sender {
  // Alt + Backspace in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::DeleteWordBackward);
  }
}

- (void)deleteToBeginningOfBackward:(id)sender {
  // Command + Backspace in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::DeleteToBeginningOfLine);
  }
}

- (void)deleteForward:(id)sender {
  // Delete in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::DeleteCharForward);
  }
}

- (void)deleteWordForward:(id)sender {
  // Alt + Delete in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::DeleteWordForward);
  }
}

- (void)insertTab:(id)sender {
  // Tab in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::InsertTab);
  }
}

- (void)insertBacktab:(id)sender {
  // Shift + Tab in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::InsertBacktab);
  }
}

- (void)moveRight:(id)sender {
  // RightArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::CharNext);
  }
}

- (void)moveRightAndModifySelection:(id)sender {
  // Shift + RightArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectCharNext);
  }
}

- (void)moveWordRight:(id)sender {
  // Alt + RightArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::WordNext);
  }
}

- (void)moveWordRightAndModifySelection:(id)sender {
  // Alt + Shift + RightArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectWordNext);
  }
}

- (void)moveToRightEndOfLine:(id)sender {
  // Command + RightArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::EndLine);
  }
}

- (void)moveToRightEndOfLineAndModifySelection:(id)sender {
  // Command + Shift + RightArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectEndLine);
  }
}

- (void)moveLeft:(id)sender {
  // LeftArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::CharPrevious);
  }
}

- (void)moveLeftAndModifySelection:(id)sender {
  // Shift + LeftArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectCharPrevious);
  }
}

- (void)moveWordLeft:(id)sender {
  // Alt + LeftArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::WordPrevious);
  }
}

- (void)moveWordLeftAndModifySelection:(id)sender {
  // Alt + Shift + LeftArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectWordPrevious);
  }
}

- (void)moveToLeftEndOfLine:(id)sender {
  // Command + LeftArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::BeginLine);
  }
}

- (void)moveToLeftEndOfLineAndModifySelection:(id)sender {
  // Command + Shift + LeftArrow in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectBeginLine);
  }
}

- (void)moveUp:(id)sender {
  // ArrowUp in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::LinePrevious);
  }
}

- (void)moveUpAndModifySelection:(id)sender {
  // Shift + ArrowUp in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectLinePrevious);
  }
}

- (void)moveToBeginningOfDocument:(id)sender {
  // Command + ArrowUp in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::MoveTop);
  }
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender {
  // Command + Shift + ArrowUp or Shift + Home in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectTop);
  }
}

- (void)moveDown:(id)sender {
  // ArrowDown in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::LineNext);
  }
}

- (void)moveDownAndModifySelection:(id)sender {
  // Shift + ArrowDown in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectLineNext);
  }
}

- (void)moveToEndOfDocument:(id)sender {
  // Command + ArrowDown in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::MoveBottom);
  }
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender {
  // Command + Shift + ArrowDown or Shift + End in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectBottom);
  }
}

- (void)scrollPageUp:(id)sender {
  // PageUp in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::ScrollPageUp);
  }
}

- (void)pageUpAndModifySelection:(id)sender {
  // Shift + PageUp in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectPageUp);
  }
}

- (void)scrollPageDown:(id)sender {
  // PageDown in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::ScrollPageDown);
  }
}

- (void)pageDownAndModifySelection:(id)sender {
  // Shift + PageDown in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::SelectPageDown);
  }
}

- (void)scrollToEndOfDocument:(id)sender {
  // End in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::ScrollBottom);
  }
}

- (void)scrollToBeginningOfDocument:(id)sender {
  // Home in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::ScrollTop);
  }
}

// XXX Don't decleare nor implement calcelOperation: because it
//     causes not calling keyDown: for Command + Period.
//     We need to handle it from doCommandBySelector:.

- (void)complete:(id)sender {
  // Alt + Escape or Alt + Shift + Escape in the default settings.
  if (mTextInputHandler) {
    mTextInputHandler->HandleCommand(Command::Complete);
  }
}

- (void)flagsChanged:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mTextInputHandler->HandleFlagsChanged(theEvent);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (BOOL)isFirstResponder {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSResponder* resp = [[self window] firstResponder];
  return (resp == (NSResponder*)self);

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

- (BOOL)isDragInProgress {
  if (!mDragService) return NO;

  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  return dragSession != nullptr;
}

- (BOOL)inactiveWindowAcceptsMouseEvent:(NSEvent*)aEvent {
  // If we're being destroyed assume the default -- return YES.
  if (!mGeckoChild) return YES;

  WidgetMouseEvent geckoEvent(true, eMouseActivate, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:aEvent toGeckoEvent:&geckoEvent];
  return (mGeckoChild->DispatchInputEvent(&geckoEvent) != nsEventStatus_eConsumeNoDefault);
}

// We must always call through to our superclass, even when mGeckoChild is
// nil -- otherwise the keyboard focus can end up in the wrong NSView.
- (BOOL)becomeFirstResponder {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return [super becomeFirstResponder];

  NS_OBJC_END_TRY_BLOCK_RETURN(YES);
}

- (void)viewsWindowDidBecomeKey {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mGeckoChild) return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // check to see if the window implements the mozWindow protocol. This
  // allows embedders to avoid re-entrant calls to -makeKeyAndOrderFront,
  // which can happen because these activate calls propagate out
  // to the embedder via nsIEmbeddingSiteWindow::SetFocus().
  BOOL isMozWindow = [[self window] respondsToSelector:@selector(setSuppressMakeKeyFront:)];
  if (isMozWindow) [[self window] setSuppressMakeKeyFront:YES];

  nsIWidgetListener* listener = mGeckoChild->GetWidgetListener();
  if (listener) listener->WindowActivated();

  if (isMozWindow) [[self window] setSuppressMakeKeyFront:NO];

  if (mGeckoChild->GetInputContext().IsPasswordEditor()) {
    TextInputHandler::EnableSecureEventInput();
  } else {
    TextInputHandler::EnsureSecureEventInputDisabled();
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)viewsWindowDidResignKey {
  if (!mGeckoChild) return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  nsIWidgetListener* listener = mGeckoChild->GetWidgetListener();
  if (listener) listener->WindowDeactivated();

  TextInputHandler::EnsureSecureEventInputDisabled();
}

// If the call to removeFromSuperview isn't delayed from nsChildView::
// TearDownView(), the NSView hierarchy might get changed during calls to
// [ChildView drawRect:], which leads to "beyond bounds" exceptions in
// NSCFArray.  For more info see bmo bug 373122.  Apple's docs claim that
// removeFromSuperviewWithoutNeedingDisplay "can be safely invoked during
// display" (whatever "display" means).  But it's _not_ true that it can be
// safely invoked during calls to [NSView drawRect:].  We use
// removeFromSuperview here because there's no longer any danger of being
// "invoked during display", and because doing do clears up bmo bug 384343.
- (void)delayedTearDown {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [self removeFromSuperview];
  [self release];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

#pragma mark -

// drag'n'drop stuff
#define kDragServiceContractID "@mozilla.org/widget/dragservice;1"

- (NSDragOperation)dragOperationFromDragAction:(int32_t)aDragAction {
  if (nsIDragService::DRAGDROP_ACTION_LINK & aDragAction) return NSDragOperationLink;
  if (nsIDragService::DRAGDROP_ACTION_COPY & aDragAction) return NSDragOperationCopy;
  if (nsIDragService::DRAGDROP_ACTION_MOVE & aDragAction) return NSDragOperationGeneric;
  return NSDragOperationNone;
}

- (LayoutDeviceIntPoint)convertWindowCoordinates:(NSPoint)aPoint {
  if (!mGeckoChild) {
    return LayoutDeviceIntPoint(0, 0);
  }

  NSPoint localPoint = [self convertPoint:aPoint fromView:nil];
  return mGeckoChild->CocoaPointsToDevPixels(localPoint);
}

- (LayoutDeviceIntPoint)convertWindowCoordinatesRoundDown:(NSPoint)aPoint {
  if (!mGeckoChild) {
    return LayoutDeviceIntPoint(0, 0);
  }

  NSPoint localPoint = [self convertPoint:aPoint fromView:nil];
  return mGeckoChild->CocoaPointsToDevPixelsRoundDown(localPoint);
}

// This is a utility function used by NSView drag event methods
// to send events. It contains all of the logic needed for Gecko
// dragging to work. Returns the appropriate cocoa drag operation code.
- (NSDragOperation)doDragAction:(EventMessage)aMessage sender:(id)aSender {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mGeckoChild) return NSDragOperationNone;

  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView doDragAction: entered\n"));

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
    if (!mDragService) return NSDragOperationNone;
  }

  if (aMessage == eDragEnter) {
    mDragService->StartDragSession();
  }

  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession) {
    if (aMessage == eDragOver) {
      // fire the drag event at the source. Just ignore whether it was
      // cancelled or not as there isn't actually a means to stop the drag
      nsCOMPtr<nsIDragService> dragService = mDragService;
      dragService->FireDragEventAtSource(eDrag,
                                         nsCocoaUtils::ModifiersForEvent([NSApp currentEvent]));
      dragSession->SetCanDrop(false);
    } else if (aMessage == eDrop) {
      // We make the assumption that the dragOver handlers have correctly set
      // the |canDrop| property of the Drag Session.
      bool canDrop = false;
      if (!NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)) || !canDrop) {
        [self doDragAction:eDragExit sender:aSender];

        nsCOMPtr<nsINode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          nsCOMPtr<nsIDragService> dragService = mDragService;
          dragService->EndDragSession(false, nsCocoaUtils::ModifiersForEvent([NSApp currentEvent]));
        }
        return NSDragOperationNone;
      }
    }

    unsigned int modifierFlags = [[NSApp currentEvent] modifierFlags];
    uint32_t action = nsIDragService::DRAGDROP_ACTION_MOVE;
    // force copy = option, alias = cmd-option, default is move
    if (modifierFlags & NSEventModifierFlagOption) {
      if (modifierFlags & NSEventModifierFlagCommand)
        action = nsIDragService::DRAGDROP_ACTION_LINK;
      else
        action = nsIDragService::DRAGDROP_ACTION_COPY;
    }
    dragSession->SetDragAction(action);
  }

  // set up gecko event
  WidgetDragEvent geckoEvent(true, aMessage, mGeckoChild);
  nsCocoaUtils::InitInputEvent(geckoEvent, [NSApp currentEvent]);

  // Use our own coordinates in the gecko event.
  // Convert event from gecko global coords to gecko view coords.
  NSPoint draggingLoc = [aSender draggingLocation];

  geckoEvent.mRefPoint = [self convertWindowCoordinates:draggingLoc];

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return NSDragOperationNone;

  if (dragSession) {
    switch (aMessage) {
      case eDragEnter:
      case eDragOver: {
        uint32_t dragAction;
        dragSession->GetDragAction(&dragAction);

        // If TakeChildProcessDragAction returns something other than
        // DRAGDROP_ACTION_UNINITIALIZED, it means that the last event was sent
        // to the child process and this event is also being sent to the child
        // process. In this case, use the last event's action instead.
        nsDragService* dragService = static_cast<nsDragService*>(mDragService);
        int32_t childDragAction = dragService->TakeChildProcessDragAction();
        if (childDragAction != nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
          dragAction = childDragAction;
        }

        return [self dragOperationFromDragAction:dragAction];
      }
      case eDragExit:
      case eDrop: {
        nsCOMPtr<nsINode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          // We're leaving a window while doing a drag that was
          // initiated in a different app. End the drag session,
          // since we're done with it for now (until the user
          // drags back into mozilla).
          nsCOMPtr<nsIDragService> dragService = mDragService;
          dragService->EndDragSession(false, nsCocoaUtils::ModifiersForEvent([NSApp currentEvent]));
        }
      }
      default:
        break;
    }
  }

  return NSDragOperationGeneric;

  NS_OBJC_END_TRY_BLOCK_RETURN(NSDragOperationNone);
}

// NSDraggingDestination
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView draggingEntered: entered\n"));

  // there should never be a globalDragPboard when "draggingEntered:" is
  // called, but just in case we'll take care of it here.
  [globalDragPboard release];

  // Set the global drag pasteboard that will be used for this drag session.
  // This will be set back to nil when the drag session ends (mouse exits
  // the view or a drop happens within the view).
  globalDragPboard = [[sender draggingPasteboard] retain];

  return [self doDragAction:eDragEnter sender:sender];

  NS_OBJC_END_TRY_BLOCK_RETURN(NSDragOperationNone);
}

// NSDraggingDestination
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView draggingUpdated: entered\n"));
  return [self doDragAction:eDragOver sender:sender];
}

// NSDraggingDestination
- (void)draggingExited:(id<NSDraggingInfo>)sender {
  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView draggingExited: entered\n"));

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  [self doDragAction:eDragExit sender:sender];
  NS_IF_RELEASE(mDragService);
}

// NSDraggingDestination
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  BOOL handled = [self doDragAction:eDrop sender:sender] != NSDragOperationNone;
  NS_IF_RELEASE(mDragService);
  return handled;
}

// NSDraggingSource
// This is just implemented so we comply with the NSDraggingSource protocol.
- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return UINT_MAX;
}

// NSDraggingSource
- (BOOL)ignoreModifierKeysForDraggingSession:(NSDraggingSession*)session {
  return YES;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession
           endedAtPoint:(NSPoint)aPoint
              operation:(NSDragOperation)aOperation {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  gDraggedTransferables = nullptr;

  NSEvent* currentEvent = [NSApp currentEvent];
  gUserCancelledDrag =
      ([currentEvent type] == NSEventTypeKeyDown && [currentEvent keyCode] == kVK_Escape);

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
  }

  if (mDragService) {
    // set the dragend point from the current mouse location
    RefPtr<nsDragService> dragService = static_cast<nsDragService*>(mDragService);
    FlipCocoaScreenCoordinate(aPoint);
    dragService->SetDragEndPoint(gfx::IntPoint::Round(aPoint.x, aPoint.y));

    NSPoint pnt = [NSEvent mouseLocation];
    FlipCocoaScreenCoordinate(pnt);
    dragService->SetDragEndPoint(gfx::IntPoint::Round(pnt.x, pnt.y));

    // XXX: dropEffect should be updated per |aOperation|.
    // As things stand though, |aOperation| isn't well handled within "our"
    // events, that is, when the drop happens within the window: it is set
    // either to NSDragOperationGeneric or to NSDragOperationNone.
    // For that reason, it's not yet possible to override dropEffect per the
    // given OS value, and it's also unclear what's the correct dropEffect
    // value for NSDragOperationGeneric that is passed by other applications.
    // All that said, NSDragOperationNone is still reliable.
    if (aOperation == NSDragOperationNone) {
      RefPtr<dom::DataTransfer> dataTransfer = dragService->GetDataTransfer();
      if (dataTransfer) {
        dataTransfer->SetDropEffectInt(nsIDragService::DRAGDROP_ACTION_NONE);
      }
    }

    dragService->EndDragSession(true, nsCocoaUtils::ModifiersForEvent(currentEvent));
    NS_RELEASE(mDragService);
  }

  [globalDragPboard release];
  globalDragPboard = nil;
  [gLastDragMouseDownEvent release];
  gLastDragMouseDownEvent = nil;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession movedToPoint:(NSPoint)aPoint {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Get the drag service if it isn't already cached. The drag service
  // isn't cached when dragging over a different application.
  nsCOMPtr<nsIDragService> dragService = mDragService;
  if (!dragService) {
    dragService = do_GetService(kDragServiceContractID);
  }

  if (dragService) {
    nsDragService* ds = static_cast<nsDragService*>(dragService.get());
    ds->DragMovedWithView(aSession, aPoint);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession willBeginAtPoint:(NSPoint)aPoint {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // there should never be a globalDragPboard when "willBeginAtPoint:" is
  // called, but just in case we'll take care of it here.
  [globalDragPboard release];

  // Set the global drag pasteboard that will be used for this drag session.
  // This will be set back to nil when the drag session ends (mouse exits
  // the view or a drop happens within the view).
  globalDragPboard = [[aSession draggingPasteboard] retain];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Get the paste location from the low level pasteboard.
static CFTypeRefPtr<CFURLRef> GetPasteLocation(NSPasteboard* aPasteboard) {
  PasteboardRef pboardRef = nullptr;
  PasteboardCreate((CFStringRef)[aPasteboard name], &pboardRef);
  if (!pboardRef) {
    return nullptr;
  }

  auto pasteBoard = CFTypeRefPtr<PasteboardRef>::WrapUnderCreateRule(pboardRef);
  PasteboardSynchronize(pasteBoard.get());

  CFURLRef urlRef = nullptr;
  PasteboardCopyPasteLocation(pasteBoard.get(), &urlRef);
  return CFTypeRefPtr<CFURLRef>::WrapUnderCreateRule(urlRef);
}

// NSPasteboardItemDataProvider
- (void)pasteboard:(NSPasteboard*)aPasteboard
                  item:(NSPasteboardItem*)aItem
    provideDataForType:(NSString*)aType {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!gDraggedTransferables) {
    return;
  }

  uint32_t count = 0;
  gDraggedTransferables->GetLength(&count);

  for (uint32_t j = 0; j < count; j++) {
    nsCOMPtr<nsITransferable> currentTransferable = do_QueryElementAt(gDraggedTransferables, j);
    if (!currentTransferable) {
      return;
    }

    // Transform the transferable to an NSDictionary.
    NSDictionary* pasteboardOutputDict =
        nsClipboard::PasteboardDictFromTransferable(currentTransferable);
    if (!pasteboardOutputDict) {
      return;
    }

    // Write everything out to the pasteboard.
    unsigned int typeCount = [pasteboardOutputDict count];
    NSMutableArray* types = [NSMutableArray arrayWithCapacity:typeCount + 1];
    [types addObjectsFromArray:[pasteboardOutputDict allKeys]];
    [types addObject:[UTIHelper stringFromPboardType:kMozWildcardPboardType]];
    for (unsigned int k = 0; k < typeCount; k++) {
      NSString* curType = [types objectAtIndex:k];
      if ([curType isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeString]] ||
          [curType isEqualToString:[UTIHelper stringFromPboardType:kPublicUrlPboardType]] ||
          [curType isEqualToString:[UTIHelper stringFromPboardType:kPublicUrlNamePboardType]] ||
          [curType isEqualToString:[UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL]]) {
        [aPasteboard setString:[pasteboardOutputDict valueForKey:curType] forType:curType];
      } else if ([curType
                     isEqualToString:[UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType]]) {
        [aPasteboard setPropertyList:[pasteboardOutputDict valueForKey:curType] forType:curType];
      } else if ([curType isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
        [aPasteboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(
                                   [pasteboardOutputDict valueForKey:curType]))
                       forType:curType];
      } else if ([curType isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeTIFF]] ||
                 [curType
                     isEqualToString:[UTIHelper stringFromPboardType:kMozCustomTypesPboardType]]) {
        [aPasteboard setData:[pasteboardOutputDict valueForKey:curType] forType:curType];
      } else if ([curType
                     isEqualToString:[UTIHelper stringFromPboardType:kMozFileUrlsPboardType]]) {
        [aPasteboard writeObjects:[pasteboardOutputDict valueForKey:curType]];
      } else if ([curType
                     isEqualToString:[UTIHelper
                                         stringFromPboardType:(NSString*)
                                                                  kPasteboardTypeFileURLPromise]]) {
        nsCOMPtr<nsIFile> targFile;
        NS_NewLocalFile(u""_ns, true, getter_AddRefs(targFile));
        nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(targFile);
        if (!macLocalFile) {
          NS_ERROR("No Mac local file");
          continue;
        }

        CFTypeRefPtr<CFURLRef> url = GetPasteLocation(aPasteboard);
        if (!url) {
          continue;
        }

        if (!NS_SUCCEEDED(macLocalFile->InitWithCFURL(url.get()))) {
          NS_ERROR("failed InitWithCFURL");
          continue;
        }

        if (!gDraggedTransferables) {
          continue;
        }

        uint32_t transferableCount;
        nsresult rv = gDraggedTransferables->GetLength(&transferableCount);
        if (NS_FAILED(rv)) {
          continue;
        }

        for (uint32_t i = 0; i < transferableCount; i++) {
          nsCOMPtr<nsITransferable> item = do_QueryElementAt(gDraggedTransferables, i);
          if (!item) {
            NS_ERROR("no transferable");
            continue;
          }

          item->SetTransferData(kFilePromiseDirectoryMime, macLocalFile);

          // Now request the kFilePromiseMime data, which will invoke the data
          // provider. If successful, the file will have been created.
          nsCOMPtr<nsISupports> fileDataPrimitive;
          Unused << item->GetTransferData(kFilePromiseMime, getter_AddRefs(fileDataPrimitive));
        }

        [aPasteboard setPropertyList:[pasteboardOutputDict valueForKey:curType] forType:curType];
      }
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

#pragma mark -

// Support for the "Services" menu. We currently only support sending strings
// and HTML to system services.

- (id)validRequestorForSendType:(NSString*)sendType returnType:(NSString*)returnType {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // sendType contains the type of data that the service would like this
  // application to send to it.  sendType is nil if the service is not
  // requesting any data.
  //
  // returnType contains the type of data the the service would like to
  // return to this application (e.g., to overwrite the selection).
  // returnType is nil if the service will not return any data.
  //
  // The following condition thus triggers when the service expects a string
  // or HTML from us or no data at all AND when the service will either not
  // send back any data to us or will send a string or HTML back to us.

  id result = nil;

  NSString* stringType = [UTIHelper stringFromPboardType:NSPasteboardTypeString];
  NSString* htmlType = [UTIHelper stringFromPboardType:NSPasteboardTypeHTML];
  if ((!sendType || [sendType isEqualToString:stringType] || [sendType isEqualToString:htmlType]) &&
      (!returnType || [returnType isEqualToString:stringType] ||
       [returnType isEqualToString:htmlType])) {
    if (mGeckoChild) {
      // Assume that this object will be able to handle this request.
      result = self;

      // Keep the ChildView alive during this operation.
      nsAutoRetainCocoaObject kungFuDeathGrip(self);

      if (sendType) {
        // Determine if there is a current selection (chrome/content).
        if (!nsClipboard::sSelectionCache) {
          result = nil;
        }
      }

      // Determine if we can paste (if receiving data from the service).
      if (mGeckoChild && returnType) {
        WidgetContentCommandEvent command(true, eContentCommandPasteTransferable, mGeckoChild,
                                          true);
        // This might possibly destroy our widget (and null out mGeckoChild).
        mGeckoChild->DispatchWindowEvent(command);
        if (!mGeckoChild || !command.mSucceeded || !command.mIsEnabled) result = nil;
      }
    }
  }

  // Give the superclass a chance if this object will not handle this request.
  if (!result) result = [super validRequestorForSendType:sendType returnType:returnType];

  return result;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // Make sure that the service will accept strings or HTML.
  if (![types containsObject:[UTIHelper stringFromPboardType:NSStringPboardType]] &&
      ![types containsObject:[UTIHelper stringFromPboardType:NSPasteboardTypeString]] &&
      ![types containsObject:[UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
    return NO;
  }

  // Bail out if there is no Gecko object.
  if (!mGeckoChild) return NO;

  // Transform the transferable to an NSDictionary.
  NSDictionary* pasteboardOutputDict = nullptr;

  pasteboardOutputDict = nsClipboard::PasteboardDictFromTransferable(nsClipboard::sSelectionCache);

  if (!pasteboardOutputDict) return NO;

  // Declare the pasteboard types.
  unsigned int typeCount = [pasteboardOutputDict count];
  NSMutableArray* declaredTypes = [NSMutableArray arrayWithCapacity:typeCount];
  [declaredTypes addObjectsFromArray:[pasteboardOutputDict allKeys]];
  [pboard declareTypes:declaredTypes owner:nil];

  // Write the data to the pasteboard.
  for (unsigned int i = 0; i < typeCount; i++) {
    NSString* currentKey = [declaredTypes objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];

    if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeString]] ||
        [currentKey isEqualToString:[UTIHelper stringFromPboardType:kPublicUrlPboardType]] ||
        [currentKey isEqualToString:[UTIHelper stringFromPboardType:kPublicUrlNamePboardType]]) {
      [pboard setString:currentValue forType:currentKey];
    } else if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
      [pboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue))
                forType:currentKey];
    } else if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeTIFF]]) {
      [pboard setData:currentValue forType:currentKey];
    } else if ([currentKey
                   isEqualToString:[UTIHelper
                                       stringFromPboardType:(NSString*)
                                                                kPasteboardTypeFileURLPromise]] ||
               [currentKey
                   isEqualToString:[UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType]]) {
      [pboard setPropertyList:currentValue forType:currentKey];
    }
  }
  return YES;

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

// Called if the service wants us to replace the current selection.
- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  nsresult rv;
  nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) return NO;
  trans->Init(nullptr);

  trans->AddDataFlavor(kUnicodeMime);
  trans->AddDataFlavor(kHTMLMime);

  rv = nsClipboard::TransferableFromPasteboard(trans, pboard);
  if (NS_FAILED(rv)) return NO;

  NS_ENSURE_TRUE(mGeckoChild, false);

  WidgetContentCommandEvent command(true, eContentCommandPasteTransferable, mGeckoChild);
  command.mTransferable = trans;
  mGeckoChild->DispatchWindowEvent(command);

  return command.mSucceeded && command.mIsEnabled;
}

- (void)pressureChangeWithEvent:(NSEvent*)event {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK

  NSInteger stage = [event stage];
  if (mLastPressureStage == 1 && stage == 2) {
    NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
    if ([userDefaults integerForKey:@"com.apple.trackpad.forceClick"] == 1) {
      // This is no public API to get configuration for current force click.
      // This is filed as radar 29294285.
      [self quickLookWithEvent:event];
    }
  }
  mLastPressureStage = stage;

  NS_OBJC_END_TRY_IGNORE_BLOCK
}

nsresult nsChildView::GetSelectionAsPlaintext(nsAString& aResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!nsClipboard::sSelectionCache) {
    MOZ_ASSERT(aResult.IsEmpty());
    return NS_OK;
  }

  // Get the current chrome or content selection.
  NSDictionary* pasteboardOutputDict = nullptr;
  pasteboardOutputDict = nsClipboard::PasteboardDictFromTransferable(nsClipboard::sSelectionCache);

  if (NS_WARN_IF(!pasteboardOutputDict)) {
    return NS_ERROR_FAILURE;
  }

  // Declare the pasteboard types.
  unsigned int typeCount = [pasteboardOutputDict count];
  NSMutableArray* declaredTypes = [NSMutableArray arrayWithCapacity:typeCount];
  [declaredTypes addObjectsFromArray:[pasteboardOutputDict allKeys]];
  NSString* currentKey = [declaredTypes objectAtIndex:0];
  NSString* currentValue = [pasteboardOutputDict valueForKey:currentKey];
  const char* textSelection = [currentValue UTF8String];
  aResult = NS_ConvertUTF8toUTF16(textSelection);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

#pragma mark -

#ifdef ACCESSIBILITY

/* Every ChildView has a corresponding mozDocAccessible object that is doing all
   the heavy lifting. The topmost ChildView corresponds to a mozRootAccessible
   object.

   All ChildView needs to do is to route all accessibility calls (from the NSAccessibility APIs)
   down to its object, pretending that they are the same.
*/
- (id<mozAccessible>)accessible {
  if (!mGeckoChild) return nil;

  id<mozAccessible> nativeAccessible = nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  RefPtr<nsChildView> geckoChild(mGeckoChild);
  RefPtr<a11y::LocalAccessible> accessible = geckoChild->GetDocumentAccessible();
  if (!accessible) return nil;

  accessible->GetNativeInterface((void**)&nativeAccessible);

#  ifdef DEBUG_hakan
  NSAssert(![nativeAccessible isExpired], @"native acc is expired!!!");
#  endif

  return nativeAccessible;
}

/* Implementation of formal mozAccessible formal protocol (enabling mozViews
   to talk to mozAccessible objects in the accessibility module). */

- (BOOL)hasRepresentedView {
  return YES;
}

- (id)representedView {
  return self;
}

- (BOOL)isRoot {
  return [[self accessible] isRoot];
}

#  pragma mark -

// general

- (BOOL)isAccessibilityElement {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super isAccessibilityElement];

  return [[self accessible] isAccessibilityElement];
}

- (id)accessibilityHitTest:(NSPoint)point {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityHitTest:point];

  return [[self accessible] accessibilityHitTest:point];
}

- (id)accessibilityFocusedUIElement {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityFocusedUIElement];

  return [[self accessible] accessibilityFocusedUIElement];
}

// actions

- (NSArray*)accessibilityActionNames {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityActionNames];

  return [[self accessible] accessibilityActionNames];
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityActionDescription:action];

  return [[self accessible] accessibilityActionDescription:action];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityPerformAction:action];

  return [[self accessible] accessibilityPerformAction:action];
}

// attributes

- (NSArray*)accessibilityAttributeNames {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityAttributeNames];

  return [[self accessible] accessibilityAttributeNames];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityIsAttributeSettable:attribute];

  return [[self accessible] accessibilityIsAttributeSettable:attribute];
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityAttributeValue:attribute];

  id<mozAccessible> accessible = [self accessible];

  // if we're the root (topmost) accessible, we need to return our native AXParent as we
  // traverse outside to the hierarchy of whoever embeds us. thus, fall back on NSView's
  // default implementation for this attribute.
  if ([attribute isEqualToString:NSAccessibilityParentAttribute] && [accessible isRoot]) {
    id parentAccessible = [super accessibilityAttributeValue:attribute];
    return parentAccessible;
  }

  return [accessible accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

#endif /* ACCESSIBILITY */

+ (uint32_t)sUniqueKeyEventId {
  return sUniqueKeyEventId;
}

+ (NSMutableDictionary*)sNativeKeyEventsMap {
  // This dictionary is "leaked".
  static NSMutableDictionary* sNativeKeyEventsMap = [[NSMutableDictionary alloc] init];
  return sNativeKeyEventsMap;
}

@end

@implementation PixelHostingView

- (id)initWithFrame:(NSRect)aRect {
  self = [super initWithFrame:aRect];

  self.wantsLayer = YES;
  self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;

  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}

- (void)drawRect:(NSRect)aRect {
  NS_WARNING("Unexpected call to drawRect: This view returns YES from wantsUpdateLayer, so "
             "drawRect should not be called.");
}

- (BOOL)wantsUpdateLayer {
  return YES;
}

- (void)updateLayer {
  [(ChildView*)[self superview] updateRootCALayer];
}

- (BOOL)wantsBestResolutionOpenGLSurface {
  return nsCocoaUtils::HiDPIEnabled() ? YES : NO;
}

@end

#pragma mark -

void ChildViewMouseTracker::OnDestroyView(ChildView* aView) {
  if (sLastMouseEventView == aView) {
    sLastMouseEventView = nil;
    [sLastMouseMoveEvent release];
    sLastMouseMoveEvent = nil;
  }
}

void ChildViewMouseTracker::OnDestroyWindow(NSWindow* aWindow) {
  if (sWindowUnderMouse == aWindow) {
    sWindowUnderMouse = nil;
  }
}

void ChildViewMouseTracker::MouseEnteredWindow(NSEvent* aEvent) {
  sWindowUnderMouse = [aEvent window];
  ReEvaluateMouseEnterState(aEvent);
}

void ChildViewMouseTracker::MouseExitedWindow(NSEvent* aEvent) {
  if (sWindowUnderMouse == [aEvent window]) {
    sWindowUnderMouse = nil;
    ReEvaluateMouseEnterState(aEvent);
  }
}

void ChildViewMouseTracker::ReEvaluateMouseEnterState(NSEvent* aEvent, ChildView* aOldView) {
  ChildView* oldView = aOldView ? aOldView : sLastMouseEventView;
  sLastMouseEventView = ViewForEvent(aEvent);
  if (sLastMouseEventView != oldView) {
    // Send enter and / or exit events.
    WidgetMouseEvent::ExitFrom exitFrom = [sLastMouseEventView window] == [oldView window]
                                              ? WidgetMouseEvent::ePlatformChild
                                              : WidgetMouseEvent::ePlatformTopLevel;
    [oldView sendMouseEnterOrExitEvent:aEvent enter:NO exitFrom:exitFrom];
    // After the cursor exits the window set it to a visible regular arrow cursor.
    if (exitFrom == WidgetMouseEvent::ePlatformTopLevel) {
      [[nsCursorManager sharedInstance] setCursor:eCursor_standard];
    }
    [sLastMouseEventView sendMouseEnterOrExitEvent:aEvent enter:YES exitFrom:exitFrom];
  }
}

void ChildViewMouseTracker::ResendLastMouseMoveEvent() {
  if (sLastMouseMoveEvent) {
    MouseMoved(sLastMouseMoveEvent);
  }
}

void ChildViewMouseTracker::MouseMoved(NSEvent* aEvent) {
  MouseEnteredWindow(aEvent);
  [sLastMouseEventView handleMouseMoved:aEvent];
  if (sLastMouseMoveEvent != aEvent) {
    [sLastMouseMoveEvent release];
    sLastMouseMoveEvent = [aEvent retain];
  }
}

void ChildViewMouseTracker::MouseScrolled(NSEvent* aEvent) {
  if (!nsCocoaUtils::IsMomentumScrollEvent(aEvent)) {
    // Store the position so we can pin future momentum scroll events.
    sLastScrollEventScreenLocation = nsCocoaUtils::ScreenLocationForEvent(aEvent);
  }
}

ChildView* ChildViewMouseTracker::ViewForEvent(NSEvent* aEvent) {
  NSWindow* window = sWindowUnderMouse;
  if (!window) return nil;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, window);
  NSView* view = [[[window contentView] superview] hitTest:windowEventLocation];

  if (![view isKindOfClass:[ChildView class]]) return nil;

  ChildView* childView = (ChildView*)view;
  // If childView is being destroyed return nil.
  if (![childView widget]) return nil;
  return WindowAcceptsEvent(window, aEvent, childView) ? childView : nil;
}

BOOL ChildViewMouseTracker::WindowAcceptsEvent(NSWindow* aWindow, NSEvent* aEvent, ChildView* aView,
                                               BOOL aIsClickThrough) {
  // Right mouse down events may get through to all windows, even to a top level
  // window with an open sheet.
  if (!aWindow || [aEvent type] == NSEventTypeRightMouseDown) return YES;

  id delegate = [aWindow delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]]) return YES;

  nsIWidget* windowWidget = [(WindowDelegate*)delegate geckoWidget];
  if (!windowWidget) return YES;

  NSWindow* topLevelWindow = nil;

  switch (windowWidget->WindowType()) {
    case eWindowType_popup:
      // If this is a context menu, it won't have a parent. So we'll always
      // accept mouse move events on context menus even when none of our windows
      // is active, which is the right thing to do.
      // For panels, the parent window is the XUL window that owns the panel.
      return WindowAcceptsEvent([aWindow parentWindow], aEvent, aView, aIsClickThrough);

    case eWindowType_toplevel:
    case eWindowType_dialog:
      if ([aWindow attachedSheet]) return NO;

      topLevelWindow = aWindow;
      break;
    case eWindowType_sheet: {
      nsIWidget* parentWidget = windowWidget->GetSheetWindowParent();
      if (!parentWidget) return YES;

      topLevelWindow = (NSWindow*)parentWidget->GetNativeData(NS_NATIVE_WINDOW);
      break;
    }

    default:
      return YES;
  }

  if (!topLevelWindow || ([topLevelWindow isMainWindow] && !aIsClickThrough) ||
      [aEvent type] == NSEventTypeOtherMouseDown ||
      (([aEvent modifierFlags] & NSEventModifierFlagCommand) != 0 &&
       [aEvent type] != NSEventTypeMouseMoved))
    return YES;

  // If we're here then we're dealing with a left click or mouse move on an
  // inactive window or something similar. Ask Gecko what to do.
  return [aView inactiveWindowAcceptsMouseEvent:aEvent];
}

#pragma mark -
