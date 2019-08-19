/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/Unused.h"

#include <unistd.h>
#include <math.h>

#include <IOSurface/IOSurface.h>

#include "nsChildView.h"
#include "nsCocoaWindow.h"

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/WheelHandlingHelper.h"  // for WheelDeltaAdjustmentStrategy
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
#include "nsIInterfaceRequestor.h"
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
#include "ComplexTextInputPanel.h"

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
#include "mozilla/layers/GLManager.h"
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
#include "mozilla/gfx/MacIOSurface.h"
#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#  include "mozilla/a11y/Platform.h"
#endif

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/StaticPrefs_gfx.h"

#include <dlfcn.h>

#include <ApplicationServices/ApplicationServices.h>

#include "GeckoProfiler.h"

#include "mozilla/layers/ChromeProcessController.h"
#include "nsLayoutUtils.h"
#include "InputData.h"
#include "RectTextureImage.h"
#include "SwipeTracker.h"
#include "VibrancyManager.h"
#include "nsNativeThemeCocoa.h"
#include "nsIDOMWindowUtils.h"
#include "Units.h"
#include "UnitTransforms.h"
#include "mozilla/UniquePtrExtensions.h"

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

static NSMutableDictionary* sNativeKeyEventsMap = [NSMutableDictionary dictionary];

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

- (BOOL)isRectObscuredBySubview:(NSRect)inRect;

- (LayoutDeviceIntRegion)nativeDirtyRegionWithBoundingRect:(NSRect)aRect;

- (BOOL)hasRoundedBottomCorners;
- (CGFloat)cornerRadius;
- (void)clearCorners;

- (void)setGLOpaque:(BOOL)aOpaque;

- (void)markLayerForDisplay;
- (CALayer*)rootCALayer;
- (void)updateRootCALayer;

// Overlay drawing functions for traditional CGContext drawing
- (void)drawTitleString;
- (void)maskTopCornersInContext:(CGContextRef)aContext;

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

@interface NSView (NSThemeFrameCornerRadius)
- (float)roundedCornerRadius;
@end

@interface NSWindow (NSWindowShouldZoomOnDoubleClick)
+ (BOOL)_shouldZoomOnDoubleClick;  // present on 10.7 and above
@end

// Starting with 10.7 the bottom corners of all windows are rounded.
// Unfortunately, the standard rounding that OS X applies to OpenGL views
// does not use anti-aliasing and looks very crude. Since we want a smooth,
// anti-aliased curve, we'll draw it ourselves.
// Additionally, we need to turn off the OS-supplied rounding because it
// eats into our corner's curve. We do that by overriding an NSSurface method.
@interface NSSurface
@end

@implementation NSSurface (DontCutOffCorners)
- (CGSRegionObj)_createRoundedBottomRegionForRect:(CGRect)rect {
  // Create a normal rect region without rounded bottom corners.
  CGSRegionObj region;
  CGSNewRegionWithRect(&rect, &region);
  return region;
}
@end

#pragma mark -

// Flips a screen coordinate from a point in the cocoa coordinate system (bottom-left rect) to a
// point that is a "flipped" cocoa coordinate system (starts in the top-left).
static inline void FlipCocoaScreenCoordinate(NSPoint& inPoint) {
  inPoint.y = nsCocoaUtils::FlippedScreenY(inPoint.y);
}

namespace {

// Used for OpenGL drawing from the compositor thread for BasicCompositor OMTC
// when StaticPrefs::gfx_core_animation_enabled_AtStartup() is false.
// This was created at a time when we didn't know how to use CoreAnimation for
// robust off-main-thread drawing.
class GLPresenter : public GLManager {
 public:
  static mozilla::UniquePtr<GLPresenter> CreateForWindow(nsIWidget* aWindow) {
    // Contrary to CompositorOGL, we allow unaccelerated OpenGL contexts to be
    // used. BasicCompositor only requires very basic GL functionality.
    bool forWebRender = false;
    RefPtr<GLContext> context =
        gl::GLContextProvider::CreateForWindow(aWindow, forWebRender, false);
    return context ? MakeUnique<GLPresenter>(context) : nullptr;
  }

  explicit GLPresenter(GLContext* aContext);
  virtual ~GLPresenter();

  virtual GLContext* gl() const override { return mGLContext; }
  virtual ShaderProgramOGL* GetProgram(GLenum aTarget, gfx::SurfaceFormat aFormat) override {
    MOZ_ASSERT(aTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    MOZ_ASSERT(aFormat == gfx::SurfaceFormat::R8G8B8A8);
    return mRGBARectProgram.get();
  }
  virtual const gfx::Matrix4x4& GetProjMatrix() const override { return mProjMatrix; }
  virtual void ActivateProgram(ShaderProgramOGL* aProg) override {
    mGLContext->fUseProgram(aProg->GetProgram());
  }
  virtual void BindAndDrawQuad(ShaderProgramOGL* aProg, const gfx::Rect& aLayerRect,
                               const gfx::Rect& aTextureRect) override;

  void BeginFrame(LayoutDeviceIntSize aRenderSize);
  void EndFrame();

  NSOpenGLContext* GetNSOpenGLContext() {
    return GLContextCGL::Cast(mGLContext)->GetNSOpenGLContext();
  }

 protected:
  RefPtr<mozilla::gl::GLContext> mGLContext;
  mozilla::UniquePtr<mozilla::layers::ShaderProgramOGL> mRGBARectProgram;
  gfx::Matrix4x4 mProjMatrix;
  GLuint mQuadVBO;
};

}  // unnamed namespace

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
      mViewTearDownLock("ChildViewTearDown"),
      mEffectsLock("WidgetEffects"),
      mHasRoundedBottomCorners(false),
      mDevPixelCornerRadius{0},
      mIsCoveringTitlebar(false),
      mIsFullscreen(false),
      mIsOpaque(false),
      mTitlebarCGContext(nullptr),
      mBackingScaleFactor(0.0),
      mVisible(false),
      mDrawing(false),
      mIsDispatchPaint(false),
      mPluginFocused{false},
      mCompositingState("nsChildView::mCompositingState"),
      mOpaqueRegion("nsChildView::mOpaqueRegion"),
      mCurrentPanGestureBelongsToSwipe{false} {}

nsChildView::~nsChildView() {
  ReleaseTitlebarCGContext();

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

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
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

void nsChildView::ReleaseTitlebarCGContext() {
  if (mTitlebarCGContext) {
    CGContextRelease(mTitlebarCGContext);
    mTitlebarCGContext = nullptr;
  }
}

nsresult nsChildView::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                             const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    mNativeLayerRoot = NativeLayerRootCA::CreateForCALayer([mView rootCALayer]);
    mNativeLayerRoot->SetBackingScale(scaleFactor);
    RefPtr<NativeLayer> contentLayer = mNativeLayerRoot->CreateLayer();
    mNativeLayerRoot->AppendLayer(contentLayer);
    mContentLayer = contentLayer->AsNativeLayerCA();
  }

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

  mPluginFocused = false;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsChildView::TearDownView() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsCocoaWindow* nsChildView::GetXULWindowWidget() const {
  id windowDelegate = [[mView window] delegate];
  if (windowDelegate && [windowDelegate isKindOfClass:[WindowDelegate class]]) {
    return [(WindowDelegate*)windowDelegate geckoWidget];
  }
  return nullptr;
}

void nsChildView::Destroy() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Make sure that no composition is in progress while disconnecting
  // ourselves from the view.
  MutexAutoLock lock(mViewTearDownLock);

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

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
  }

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

#pragma mark -

nsTransparencyMode nsChildView::GetTransparencyMode() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsCocoaWindow* windowWidget = GetXULWindowWidget();
  return windowWidget ? windowWidget->GetTransparencyMode() : eTransparencyOpaque;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(eTransparencyOpaque);
}

// This is called by nsContainerFrame on the root widget for all window types
// except popup windows (when nsCocoaWindow::SetTransparencyMode is used instead).
void nsChildView::SetTransparencyMode(nsTransparencyMode aMode) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsCocoaWindow* windowWidget = GetXULWindowWidget();
  if (windowWidget) {
    windowWidget->SetTransparencyMode(aMode);
  }

  UpdateInternalOpaqueRegion();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool nsChildView::IsVisible() const {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mVisible) {
    return mVisible;
  }

  if (!GetXULWindowWidget()->IsVisible()) {
    return false;
  }

  // mVisible does not accurately reflect the state of a hidden tabbed view
  // so verify that the view has a window as well
  // then check native widget hierarchy visibility
  return ([mView window] != nil) && !NSIsEmptyRect([mView visibleRect]);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Change the parent of this widget
void nsChildView::SetParent(nsIWidget* aNewParent) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsChildView::ReparentNativeWidget(nsIWidget* aNewParent) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  MOZ_ASSERT(aNewParent, "null widget");

  if (mOnDestroyCalled) return;

  NSView<mozView>* newParentView = (NSView<mozView>*)aNewParent->GetNativeData(NS_NATIVE_WIDGET);
  NS_ENSURE_TRUE_VOID(newParentView);

  // we hold a ref to mView, so this is safe
  [mView removeFromSuperview];
  mParentView = newParentView;
  [mParentView addSubview:mView];

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

void nsChildView::SetFocus(Raise) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindow* window = [mView window];
  if (window) [window makeFirstResponder:mView];
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Override to set the cursor on the mac
void nsChildView::SetCursor(nsCursor aDefaultCursor, imgIContainer* aImageCursor,
                            uint32_t aHotspotX, uint32_t aHotspotY) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    mNativeLayerRoot->SetBackingScale(mBackingScaleFactor);
  }

  if (mWidgetListener && !mWidgetListener->GetXULWindow()) {
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsChildView::Resize(double aWidth, double aHeight, bool aRepaint) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsChildView::Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  if (!StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    return;
  }

  if (mUnsuspendAsyncCATransactionsRunnable) {
    mUnsuspendAsyncCATransactionsRunnable->Cancel();
    mUnsuspendAsyncCATransactionsRunnable = nullptr;
  }

  // Make sure that there actually will be a CATransaction on the main thread
  // during which we get a chance to schedule unsuspension. Otherwise we might
  // accidentally stay suspended indefinitely.
  [mView markLayerForDisplay];

  auto compositingState = mCompositingState.Lock();
  compositingState->mAsyncCATransactionsSuspended = true;
}

void nsChildView::MaybeScheduleUnsuspendAsyncCATransactions() {
  auto compositingState = mCompositingState.Lock();
  if (compositingState->mAsyncCATransactionsSuspended && !mUnsuspendAsyncCATransactionsRunnable) {
    mUnsuspendAsyncCATransactionsRunnable =
        NewCancelableRunnableMethod("nsChildView::MaybeScheduleUnsuspendAsyncCATransactions", this,
                                    &nsChildView::UnsuspendAsyncCATransactions);
    NS_DispatchToMainThread(mUnsuspendAsyncCATransactionsRunnable);
  }
}

void nsChildView::UnsuspendAsyncCATransactions() {
  mUnsuspendAsyncCATransactionsRunnable = nullptr;

  auto compositingState = mCompositingState.Lock();
  compositingState->mAsyncCATransactionsSuspended = false;
  if (compositingState->mNativeLayerChangesPending) {
    // We need to call mNativeLayerRoot->ApplyChanges() at the next available
    // opportunity, and it needs to happen during a CoreAnimation transaction.
    // The easiest way to handle this request is to mark the layer as needing
    // display, because this will schedule a main thread CATransaction, during
    // which HandleMainThreadCATransaction will call ApplyChanges().
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
                                                 uint32_t aNativeMessage, uint32_t aModifierFlags,
                                                 nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  AutoObserverNotifier notifier(aObserver, "mouseevent");

  NSPoint pt = nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // aPoint is given with the origin on the top left, but convertScreenToBase
  // expects a point in a coordinate system that has its origin on the bottom left.
  NSPoint screenPoint = NSMakePoint(pt.x, nsCocoaUtils::FlippedScreenY(pt.y));
  NSPoint windowPoint = nsCocoaUtils::ConvertPointFromScreen([mView window], screenPoint);

  NSEvent* event = [NSEvent mouseEventWithType:(NSEventType)aNativeMessage
                                      location:windowPoint
                                 modifierFlags:aModifierFlags
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
    if (aNativeMessage == NSMouseEntered) {
      [window mouseEntered:event];
      return NS_OK;
    }
    if (aNativeMessage == NSMouseExited) {
      [window mouseExited:event];
      return NS_OK;
    }
    if (aNativeMessage == NSMouseMoved) {
      [window mouseMoved:event];
      return NS_OK;
    }
  }

  [NSApp sendEvent:event];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsChildView::SynthesizeNativeMouseScrollEvent(
    mozilla::LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX, double aDeltaY,
    double aDeltaZ, uint32_t aModifierFlags, uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  AutoObserverNotifier notifier(aObserver, "mousescrollevent");

  NSPoint pt = nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // Mostly copied from http://stackoverflow.com/a/6130349
  CGScrollEventUnit units = (aAdditionalFlags & nsIDOMWindowUtils::MOUSESCROLL_SCROLL_LINES)
                                ? kCGScrollEventUnitLine
                                : kCGScrollEventUnitPixel;
  CGEventRef cgEvent = CGEventCreateScrollWheelEvent(NULL, units, 3, aDeltaY, aDeltaX, aDeltaZ);
  if (!cgEvent) {
    return NS_ERROR_FAILURE;
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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsChildView::SynthesizeNativeTouchPoint(
    uint32_t aPointerId, TouchPointerState aPointerState, mozilla::LayoutDeviceIntPoint aPoint,
    double aPointerPressure, uint32_t aPointerOrientation, nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// First argument has to be an NSMenu representing the application's top-level
// menu bar. The returned item is *not* retained.
static NSMenuItem* NativeMenuItemWithLocation(NSMenu* menubar, NSString* locationString) {
  NSArray* indexes = [locationString componentsSeparatedByString:@"|"];
  unsigned int indexCount = [indexes count];
  if (indexCount == 0) return nil;

  NSMenu* currentSubmenu = [NSApp mainMenu];
  for (unsigned int i = 0; i < indexCount; i++) {
    int targetIndex;
    // We remove the application menu from consideration for the top-level menu
    if (i == 0)
      targetIndex = [[indexes objectAtIndex:i] intValue] + 1;
    else
      targetIndex = [[indexes objectAtIndex:i] intValue];
    int itemCount = [currentSubmenu numberOfItems];
    if (targetIndex < itemCount) {
      NSMenuItem* menuItem = [currentSubmenu itemAtIndex:targetIndex];
      // if this is the last index just return the menu item
      if (i == (indexCount - 1)) return menuItem;
      // if this is not the last index find the submenu and keep going
      if ([menuItem hasSubmenu])
        currentSubmenu = [menuItem submenu];
      else
        return nil;
    }
  }

  return nil;
}

bool nsChildView::SendEventToNativeMenuSystem(NSEvent* aEvent) {
  bool handled = false;
  nsCocoaWindow* widget = GetXULWindowWidget();
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // We always allow keyboard events to propagate to keyDown: but if they are
  // not handled we give menu items a chance to act. This allows for handling of
  // custom shortcuts. Note that existing shortcuts cannot be reassigned yet and
  // will have been handled by keyDown: before we get here.
  NSEvent* cocoaEvent = [sNativeKeyEventsMap objectForKey:@(aEvent->mUniqueId)];
  if (!cocoaEvent) {
    return;
  }

  if (SendEventToNativeMenuSystem(cocoaEvent)) {
    aEvent->PreventDefault();
  }
  [sNativeKeyEventsMap removeObjectForKey:@(aEvent->mUniqueId)];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Used for testing native menu system structure and event handling.
nsresult nsChildView::ActivateNativeMenuItemAt(const nsAString& indexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString* locationString =
      [NSString stringWithCharacters:reinterpret_cast<const unichar*>(indexString.BeginReading())
                              length:indexString.Length()];
  NSMenuItem* item = NativeMenuItemWithLocation([NSApp mainMenu], locationString);
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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Used for testing native menu system structure and event handling.
nsresult nsChildView::ForceUpdateNativeMenuAt(const nsAString& indexString) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCocoaWindow* widget = GetXULWindowWidget();
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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mView || !mVisible) return;

  NS_ASSERTION(GetLayerManager()->GetBackendType() != LayersBackend::LAYERS_CLIENT,
               "Shouldn't need to invalidate with accelerated OMTC layers!");

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    mContentLayer->InvalidateRegionThroughoutSwapchain(aRect.ToUnknownRect());
    [mView markLayerForDisplay];
  } else {
    [[mView pixelHostingView] setNeedsDisplayInRect:DevPixelsToCocoaPoints(aRect)];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool nsChildView::WidgetTypeSupportsAcceleration() {
  // We need to enable acceleration in popups which contain remote layer
  // trees, since the remote content won't be rendered at all otherwise. This
  // causes issues with transparency and drop shadows, so it should not be
  // used by default in release builds.
  if (HasRemoteContent()) {
    return true;
  }

  // Don't use OpenGL for transparent windows or for popup windows.
  return mView && [[mView window] isOpaque] && ![[mView window] isKindOfClass:[PopupWindow class]];
}

bool nsChildView::ShouldUseOffMainThreadCompositing() {
  // Don't use OMTC for transparent windows or for popup windows.
  if (!WidgetTypeSupportsAcceleration()) return false;

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

bool nsChildView::PaintWindowInContext(CGContextRef aContext, const LayoutDeviceIntRegion& aRegion,
                                       gfx::IntSize aSurfaceSize) {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  if (!mBackingSurface || mBackingSurface->GetSize() != aSurfaceSize) {
    mBackingSurface = gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
        aSurfaceSize, gfx::SurfaceFormat::B8G8R8A8);
    if (!mBackingSurface) {
      return false;
    }
  }

  bool painted = PaintWindowInDrawTarget(mBackingSurface, aRegion, aSurfaceSize);

  uint8_t* data;
  gfx::IntSize size;
  int32_t stride;
  gfx::SurfaceFormat format;

  if (!mBackingSurface->LockBits(&data, &size, &stride, &format)) {
    return false;
  }

  // Draw the backing surface onto the window.
  CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, data, stride * size.height, NULL);
  NSColorSpace* colorSpace = [[mView window] colorSpace];
  CGImageRef image =
      CGImageCreate(size.width, size.height, 8, 32, stride, [colorSpace CGColorSpace],
                    kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst, provider, NULL,
                    false, kCGRenderingIntentDefault);
  CGContextSaveGState(aContext);
  CGContextTranslateCTM(aContext, 0, size.height);
  CGContextScaleCTM(aContext, 1, -1);
  CGContextSetBlendMode(aContext, kCGBlendModeCopy);
  CGContextDrawImage(aContext, CGRectMake(0, 0, size.width, size.height), image);
  CGImageRelease(image);
  CGDataProviderRelease(provider);
  CGContextRestoreGState(aContext);

  mBackingSurface->ReleaseBits(data);

  return painted;
}

bool nsChildView::PaintWindowInIOSurface(CFTypeRefPtr<IOSurfaceRef> aSurface,
                                         const LayoutDeviceIntRegion& aInvalidRegion) {
  RefPtr<MacIOSurface> surf = new MacIOSurface(std::move(aSurface));
  surf->Lock(false);
  RefPtr<gfx::DrawTarget> dt = surf->GetAsDrawTargetLocked(gfx::BackendType::SKIA);
  bool result = PaintWindowInDrawTarget(dt, aInvalidRegion, dt->GetSize());
  surf->Unlock(false);
  return result;
}

void nsChildView::PaintWindowInContentLayer() {
  mContentLayer->SetRect(GetBounds().ToUnknownRect());
  mContentLayer->SetSurfaceIsFlipped(false);
  CFTypeRefPtr<IOSurfaceRef> surf = mContentLayer->NextSurface();
  if (!surf) {
    return;
  }

  PaintWindowInIOSurface(
      surf, LayoutDeviceIntRegion::FromUnknownRegion(mContentLayer->CurrentSurfaceInvalidRegion()));
  mContentLayer->NotifySurfaceReady();
}

void nsChildView::HandleMainThreadCATransaction() {
  MaybeScheduleUnsuspendAsyncCATransactions();
  WillPaintWindow();

  if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    // We're in BasicLayers mode, i.e. main thread software compositing.
    // Composite the window into our layer's surface.
    PaintWindowInContentLayer();
  } else {
    // Trigger a synchronous OMTC composite. This will call NextSurface and
    // NotifySurfaceReady on the compositor thread to update mContentLayer's
    // surface, and the main thread (this thread) will wait inside PaintWindow
    // during that time.
    PaintWindow(LayoutDeviceIntRegion(GetBounds()));
  }

  // Apply the changes from mContentLayer to its underlying CALayer. Now is a
  // good time to call this because we know we're currently inside a main thread
  // CATransaction.
  auto compositingState = mCompositingState.Lock();
  mNativeLayerRoot->ApplyChanges();
  compositingState->mNativeLayerChangesPending = false;
}

#pragma mark -

void nsChildView::ReportMoveEvent() { NotifyWindowMoved(mBounds.x, mBounds.y); }

void nsChildView::ReportSizeEvent() {
  if (mWidgetListener) mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
}

#pragma mark -

LayoutDeviceIntPoint nsChildView::GetClientOffset() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSPoint origin = [mView convertPoint:NSMakePoint(0, 0) toView:nil];
  origin.y = [[mView window] frame].size.height - origin.y;
  return CocoaPointsToDevPixels(origin);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

//    Return the offset between this child view and the screen.
//    @return       -- widget origin in device-pixel coords
LayoutDeviceIntPoint nsChildView::WidgetToScreenOffset() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

nsresult nsChildView::SetTitle(const nsAString& title) {
  // child views don't have titles
  return NS_OK;
}

nsresult nsChildView::GetAttention(int32_t aCycleCount) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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

nsresult nsChildView::StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                                     int32_t aPanelX, int32_t aPanelY, nsString& aCommitted) {
  NS_ENSURE_TRUE(mView, NS_ERROR_NOT_AVAILABLE);

  ComplexTextInputPanel* ctiPanel = ComplexTextInputPanel::GetSharedComplexTextInputPanel();

  ctiPanel->PlacePanel(aPanelX, aPanelY);
  // We deliberately don't use TextInputHandler::GetCurrentKeyEvent() to
  // obtain the NSEvent* we pass to InterpretKeyEvent().  This works fine in
  // non-e10s mode.  But in e10s mode TextInputHandler::HandleKeyDownEvent()
  // has already returned, so the relevant KeyEventState* (and its NSEvent*)
  // is already out of scope.  Furthermore we don't *need* to use it.
  // StartPluginIME() is only ever called to start a new IME session when none
  // currently exists.  So nested IME should never reach here, and so it should
  // be fine to use the last key-down event received by -[ChildView keyDown:]
  // (as we currently do).
  ctiPanel->InterpretKeyEvent([mView lastKeyDownEvent], aCommitted);

  return NS_OK;
}

void nsChildView::SetPluginFocused(bool& aFocused) {
  if (aFocused == mPluginFocused) {
    return;
  }
  if (!aFocused) {
    ComplexTextInputPanel* ctiPanel = ComplexTextInputPanel::GetSharedComplexTextInputPanel();
    if (ctiPanel) {
      ctiPanel->CancelComposition();
    }
  }
  mPluginFocused = aFocused;
}

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
    case IMEState::ENABLED:
    case IMEState::PLUGIN:
      mTextInputHandler->SetASCIICapableOnly(false);
      mTextInputHandler->EnableIME(true);
      if (mInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE) {
        mTextInputHandler->SetIMEOpenState(mInputContext.mIMEState.mOpen == IMEState::OPEN);
      }
      break;
    case IMEState::DISABLED:
      mTextInputHandler->SetASCIICapableOnly(false);
      mTextInputHandler->EnableIME(false);
      break;
    case IMEState::PASSWORD:
      mTextInputHandler->SetASCIICapableOnly(true);
      mTextInputHandler->EnableIME(false);
      break;
    default:
      NS_ERROR("not implemented!");
  }
}

InputContext nsChildView::GetInputContext() {
  switch (mInputContext.mIMEState.mEnabled) {
    case IMEState::ENABLED:
    case IMEState::PLUGIN:
      if (mTextInputHandler) {
        mInputContext.mIMEState.mOpen =
            mTextInputHandler->IsIMEOpened() ? IMEState::OPEN : IMEState::CLOSED;
        break;
      }
      // If mTextInputHandler is null, set CLOSED instead...
      MOZ_FALLTHROUGH;
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

void nsChildView::GetEditCommandsRemapped(NativeKeyBindingsType aType,
                                          const WidgetKeyboardEvent& aEvent,
                                          nsTArray<CommandInt>& aCommands, uint32_t aGeckoKeyCode,
                                          uint32_t aCocoaKeyCode) {
  NSEvent* originalEvent = reinterpret_cast<NSEvent*>(aEvent.mNativeKeyEvent);

  WidgetKeyboardEvent modifiedEvent(aEvent);
  modifiedEvent.mKeyCode = aGeckoKeyCode;

  unichar ch = nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(aGeckoKeyCode);
  NSString* chars = [[[NSString alloc] initWithCharacters:&ch length:1] autorelease];

  modifiedEvent.mNativeKeyEvent = [NSEvent keyEventWithType:[originalEvent type]
                                                   location:[originalEvent locationInWindow]
                                              modifierFlags:[originalEvent modifierFlags]
                                                  timestamp:[originalEvent timestamp]
                                               windowNumber:[originalEvent windowNumber]
                                                    context:[originalEvent context]
                                                 characters:chars
                                charactersIgnoringModifiers:chars
                                                  isARepeat:[originalEvent isARepeat]
                                                    keyCode:aCocoaKeyCode];

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  keyBindings->GetEditCommands(modifiedEvent, aCommands);
}

void nsChildView::GetEditCommands(NativeKeyBindingsType aType, const WidgetKeyboardEvent& aEvent,
                                  nsTArray<CommandInt>& aCommands) {
  // Validate the arguments.
  nsIWidget::GetEditCommands(aType, aEvent, aCommands);

  // If the key is a cursor-movement arrow, and the current selection has
  // vertical writing-mode, we'll remap so that the movement command
  // generated (in terms of characters/lines) will be appropriate for
  // the physical direction of the arrow.
  if (aEvent.mKeyCode >= NS_VK_LEFT && aEvent.mKeyCode <= NS_VK_DOWN) {
    // XXX This may be expensive. Should use the cache in TextInputHandler.
    WidgetQueryContentEvent query(true, eQuerySelectedText, this);
    DispatchWindowEvent(query);

    if (query.mSucceeded && query.mReply.mWritingMode.IsVertical()) {
      uint32_t geckoKey = 0;
      uint32_t cocoaKey = 0;

      switch (aEvent.mKeyCode) {
        case NS_VK_LEFT:
          if (query.mReply.mWritingMode.IsVerticalLR()) {
            geckoKey = NS_VK_UP;
            cocoaKey = kVK_UpArrow;
          } else {
            geckoKey = NS_VK_DOWN;
            cocoaKey = kVK_DownArrow;
          }
          break;

        case NS_VK_RIGHT:
          if (query.mReply.mWritingMode.IsVerticalLR()) {
            geckoKey = NS_VK_DOWN;
            cocoaKey = kVK_DownArrow;
          } else {
            geckoKey = NS_VK_UP;
            cocoaKey = kVK_UpArrow;
          }
          break;

        case NS_VK_UP:
          geckoKey = NS_VK_LEFT;
          cocoaKey = kVK_LeftArrow;
          break;

        case NS_VK_DOWN:
          geckoKey = NS_VK_RIGHT;
          cocoaKey = kVK_RightArrow;
          break;
      }

      GetEditCommandsRemapped(aType, aEvent, aCommands, geckoKey, cocoaKey);
      return;
    }
  }

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  keyBindings->GetEditCommands(aEvent, aCommands);
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
  if (queryContentState.mSucceeded && queryContentState.mReply.mFocusedWidget) {
    NSView<mozView>* view = static_cast<NSView<mozView>*>(
        queryContentState.mReply.mFocusedWidget->GetNativeData(NS_NATIVE_WIDGET));
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

LayoutDeviceIntRect nsChildView::RectContainingTitlebarControls() {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  NSRect rect = NSZeroRect;

  // If we draw the titlebar title string, set the rect to the full window
  // width times the default titlebar height. This height does not necessarily
  // include all the titlebar controls because we may have moved them further
  // down, but at least it will include the whole title text.
  BaseWindow* window = (BaseWindow*)[mView window];
  if ([window wantsTitleDrawn] && [window isKindOfClass:[ToolbarWindow class]]) {
    CGFloat defaultTitlebarHeight = [(ToolbarWindow*)window titlebarHeight];
    rect = NSMakeRect(0, 0, [mView bounds].size.width, defaultTitlebarHeight);
  }

  // Add the rects of the titlebar controls.
  for (id view in [window titlebarControls]) {
    rect = NSUnionRect(rect, [mView convertRect:[view bounds] fromView:view]);
  }
  return CocoaPointsToDevPixels(rect);
}

void nsChildView::PrepareWindowEffects() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  bool canBeOpaque;
  {
    MutexAutoLock lock(mEffectsLock);
    mHasRoundedBottomCorners = [mView hasRoundedBottomCorners];
    CGFloat cornerRadius = [mView cornerRadius];
    mDevPixelCornerRadius = cornerRadius * BackingScaleFactor();
    mIsCoveringTitlebar = [mView isCoveringTitlebar];
    NSInteger styleMask = [[mView window] styleMask];
    bool wasFullscreen = mIsFullscreen;
    nsCocoaWindow* windowWidget = GetXULWindowWidget();
    mIsFullscreen =
        (styleMask & NSFullScreenWindowMask) || (windowWidget && windowWidget->InFullScreenMode());

    canBeOpaque = mIsFullscreen && wasFullscreen;
    if (canBeOpaque && VibrancyManager::SystemSupportsVibrancy()) {
      canBeOpaque = !EnsureVibrancyManager().HasVibrantRegions();
    }
    if (mIsCoveringTitlebar && !StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
      mTitlebarRect = RectContainingTitlebarControls();
      UpdateTitlebarCGContext();
    }
  }

  // If we've just transitioned into or out of full screen then update the opacity on our GLContext.
  if (canBeOpaque != mIsOpaque) {
    mIsOpaque = canBeOpaque;
    [mView setGLOpaque:canBeOpaque];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsChildView::CleanupWindowEffects() {
  mCornerMaskImage = nullptr;
  mTitlebarImage = nullptr;
}

void nsChildView::AddWindowOverlayWebRenderCommands(layers::WebRenderBridgeChild* aWrBridge,
                                                    wr::DisplayListBuilder& aBuilder,
                                                    wr::IpcResourceUpdateQueue& aResources) {
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    return;
  }

  PrepareWindowEffects();

  if (!mIsCoveringTitlebar || mIsFullscreen || mTitlebarRect.IsEmpty()) {
    return;
  }

  bool needUpdate = mUpdatedTitlebarRegion.Intersects(mTitlebarRect);
  mUpdatedTitlebarRegion.SetEmpty();

  if (mTitlebarCGContext) {
    gfx::IntSize size(CGBitmapContextGetWidth(mTitlebarCGContext),
                      CGBitmapContextGetHeight(mTitlebarCGContext));
    size_t stride = CGBitmapContextGetBytesPerRow(mTitlebarCGContext);
    size_t titlebarCGContextDataLength = stride * size.height;
    gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
    Range<uint8_t> buffer(static_cast<uint8_t*>(CGBitmapContextGetData(mTitlebarCGContext)),
                          titlebarCGContextDataLength);

    if (!mTitlebarImageKey) {
      mTitlebarImageKey = Some(aWrBridge->GetNextImageKey());
      wr::ImageDescriptor descriptor(size, stride, format);
      aResources.AddImage(*mTitlebarImageKey, descriptor, buffer);
      mTitlebarImageSize = size;
      needUpdate = false;
    }

    if (needUpdate) {
      wr::ImageDescriptor descriptor(size, stride, format);
      aResources.UpdateImageBuffer(*mTitlebarImageKey, descriptor, buffer);
    }

    wr::LayoutRect rect = wr::ToLayoutRect(mTitlebarRect);
    aBuilder.PushImage(wr::LayoutRect{rect.origin, {float(size.width), float(size.height)}}, rect,
                       true, wr::ImageRendering::Auto, *mTitlebarImageKey);
  }
}

bool nsChildView::PreRender(WidgetRenderingContext* aContext) {
  // The lock makes sure that we don't attempt to tear down the view while
  // compositing. That would make us unable to call postRender on it when the
  // composition is done, thus keeping the GL context locked forever.
  mViewTearDownLock.Lock();

  bool canComposite = PreRenderImpl(aContext);

  if (!canComposite) {
    mViewTearDownLock.Unlock();
    return false;
  }
  return true;
}

class SurfaceRegistryWrapperAroundGLContextCGL : public layers::IOSurfaceRegistry {
 public:
  explicit SurfaceRegistryWrapperAroundGLContextCGL(gl::GLContextCGL* aContext)
      : mContext(aContext) {}
  void RegisterSurface(CFTypeRefPtr<IOSurfaceRef> aSurface) override {
    mContext->RegisterIOSurface(aSurface.get());
  }
  void UnregisterSurface(CFTypeRefPtr<IOSurfaceRef> aSurface) override {
    mContext->UnregisterIOSurface(aSurface.get());
  }
  RefPtr<gl::GLContextCGL> mContext;
};

bool nsChildView::PreRenderImpl(WidgetRenderingContext* aContext) {
  UniquePtr<GLManager> manager(GLManager::CreateGLManager(aContext->mLayerManager));
  gl::GLContext* gl = manager ? manager->gl() : aContext->mGL;

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    if (gl) {
      auto glContextCGL = GLContextCGL::Cast(gl);
      mContentLayer->SetRect(GetBounds().ToUnknownRect());
      mContentLayer->SetSurfaceIsFlipped(true);
      RefPtr<layers::IOSurfaceRegistry> currentRegistry = mContentLayer->GetSurfaceRegistry();
      if (!currentRegistry) {
        mContentLayer->SetSurfaceRegistry(
            MakeAndAddRef<SurfaceRegistryWrapperAroundGLContextCGL>(glContextCGL));
      } else {
        MOZ_RELEASE_ASSERT(
            static_cast<SurfaceRegistryWrapperAroundGLContextCGL*>(currentRegistry.get())
                ->mContext == glContextCGL);
      }
      CFTypeRefPtr<IOSurfaceRef> surf = mContentLayer->NextSurface();
      if (!surf) {
        return false;
      }
      glContextCGL->UseRegisteredIOSurfaceForDefaultFramebuffer(surf.get());
      return true;
    }
    // We're using BasicCompositor.
    MOZ_RELEASE_ASSERT(!mBasicCompositorIOSurface);
    mContentLayer->SetRect(GetBounds().ToUnknownRect());
    mContentLayer->SetSurfaceIsFlipped(false);
    CFTypeRefPtr<IOSurfaceRef> surf = mContentLayer->NextSurface();
    if (!surf) {
      return false;
    }
    mBasicCompositorIOSurface = new MacIOSurface(std::move(surf));
    return true;
  }

  if (gl) {
    return [mView preRender:GLContextCGL::Cast(gl)->GetNSOpenGLContext()];
  }

  // BasicCompositor.
  MOZ_RELEASE_ASSERT(mGLPresenter, "Should have been set up in InitCompositor");
  if (![mView preRender:mGLPresenter->GetNSOpenGLContext()]) {
    return false;
  }

  if (!mBasicCompositorImage) {
    mBasicCompositorImage = MakeUnique<RectTextureImage>();
  }
  return true;
}

void nsChildView::PostRender(WidgetRenderingContext* aContext) {
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    mContentLayer->NotifySurfaceReady();

    auto compositingState = mCompositingState.Lock();
    if (compositingState->mAsyncCATransactionsSuspended) {
      // We should not trigger a CATransactions on this thread. Instead, let the
      // main thread take care of calling ApplyChanges at an appropriate time.
      compositingState->mNativeLayerChangesPending = true;
    } else {
      // Force a CoreAnimation layer tree update from this thread.
      [CATransaction begin];
      mNativeLayerRoot->ApplyChanges();
      compositingState->mNativeLayerChangesPending = false;
      [CATransaction commit];
    }
  } else {
    UniquePtr<GLManager> manager(GLManager::CreateGLManager(aContext->mLayerManager));
    GLContext* gl = manager ? manager->gl() : aContext->mGL;
    NSOpenGLContext* glContext =
        gl ? GLContextCGL::Cast(gl)->GetNSOpenGLContext() : mGLPresenter->GetNSOpenGLContext();
    [mView postRender:glContext];
  }
  mViewTearDownLock.Unlock();
}

void nsChildView::DoCompositorCleanup() {
  if (mContentLayer) {
    mContentLayer->SetSurfaceRegistry(nullptr);
  }
}

void nsChildView::DrawWindowOverlay(WidgetRenderingContext* aContext, LayoutDeviceIntRect aRect) {
  mozilla::UniquePtr<GLManager> manager(GLManager::CreateGLManager(aContext->mLayerManager));
  if (manager) {
    DrawWindowOverlay(manager.get(), aRect);
  }
}

void nsChildView::DrawWindowOverlay(GLManager* aManager, LayoutDeviceIntRect aRect) {
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    return;
  }

  GLContext* gl = aManager->gl();
  ScopedGLState scopedScissorTestState(gl, LOCAL_GL_SCISSOR_TEST, false);

  MaybeDrawTitlebar(aManager);
  MaybeDrawRoundedCorners(aManager, aRect);
}

static void ClearRegion(gfx::DrawTarget* aDT, LayoutDeviceIntRegion aRegion) {
  gfxUtils::ClipToRegion(aDT, aRegion.ToUnknownRegion());
  aDT->ClearRect(gfx::Rect(0, 0, aDT->GetSize().width, aDT->GetSize().height));
  aDT->PopClip();
}

static CGContextRef CreateCGContext(const LayoutDeviceIntSize& aSize) {
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx = CGBitmapContextCreate(
      NULL, aSize.width, aSize.height, 8 /* bitsPerComponent */, aSize.width * 4, cs,
      kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  CGColorSpaceRelease(cs);

  CGContextTranslateCTM(ctx, 0, aSize.height);
  CGContextScaleCTM(ctx, 1, -1);
  CGContextSetInterpolationQuality(ctx, kCGInterpolationLow);

  return ctx;
}

static LayoutDeviceIntSize TextureSizeForSize(const LayoutDeviceIntSize& aSize) {
  return LayoutDeviceIntSize(RoundUpPow2(aSize.width), RoundUpPow2(aSize.height));
}

// When this method is entered, mEffectsLock is already being held.
void nsChildView::UpdateTitlebarCGContext() {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  if (mTitlebarRect.IsEmpty()) {
    ReleaseTitlebarCGContext();
    return;
  }

  NSRect titlebarRect = DevPixelsToCocoaPoints(mTitlebarRect);
  NSRect dirtyRect = [mView convertRect:[(BaseWindow*)[mView window] getAndResetNativeDirtyRect]
                               fromView:nil];
  NSRect dirtyTitlebarRect = NSIntersectionRect(titlebarRect, dirtyRect);

  LayoutDeviceIntSize texSize = TextureSizeForSize(mTitlebarRect.Size());
  if (!mTitlebarCGContext || CGBitmapContextGetWidth(mTitlebarCGContext) != size_t(texSize.width) ||
      CGBitmapContextGetHeight(mTitlebarCGContext) != size_t(texSize.height)) {
    dirtyTitlebarRect = titlebarRect;

    ReleaseTitlebarCGContext();

    mTitlebarCGContext = CreateCGContext(texSize);
  }

  if (NSIsEmptyRect(dirtyTitlebarRect)) {
    return;
  }

  CGContextRef ctx = mTitlebarCGContext;

  CGContextSaveGState(ctx);

  CGContextTranslateCTM(ctx, -mTitlebarRect.x, -mTitlebarRect.y);

  double scale = BackingScaleFactor();
  CGContextScaleCTM(ctx, scale, scale);

  CGContextClipToRect(ctx, NSRectToCGRect(dirtyTitlebarRect));
  CGContextClearRect(ctx, NSRectToCGRect(dirtyTitlebarRect));

  NSGraphicsContext* oldContext = [NSGraphicsContext currentContext];

  CGContextSaveGState(ctx);

  BaseWindow* window = (BaseWindow*)[mView window];
  NSView* frameView = [[window contentView] superview];
  if (![frameView isFlipped]) {
    CGContextTranslateCTM(ctx, 0, [frameView bounds].size.height);
    CGContextScaleCTM(ctx, 1, -1);
  }
  NSGraphicsContext* context =
      [NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:[frameView isFlipped]];
  [NSGraphicsContext setCurrentContext:context];

  // Draw the title string.
  if ([window wantsTitleDrawn] && [frameView respondsToSelector:@selector(_drawTitleBar:)]) {
    [frameView _drawTitleBar:[frameView bounds]];
  }

  // Draw the titlebar controls into the titlebar image.
  for (id view in [window titlebarControls]) {
    NSRect viewFrame = [view frame];
    NSRect viewRect = [mView convertRect:viewFrame fromView:frameView];
    if (!NSIntersectsRect(dirtyTitlebarRect, viewRect)) {
      continue;
    }
    // All of the titlebar controls we're interested in are subclasses of
    // NSButton.
    if (![view isKindOfClass:[NSButton class]]) {
      continue;
    }
    NSButton* button = (NSButton*)view;
    id cellObject = [button cell];
    if (![cellObject isKindOfClass:[NSCell class]]) {
      continue;
    }
    NSCell* cell = (NSCell*)cellObject;

    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, viewFrame.origin.x, viewFrame.origin.y);

    if ([context isFlipped] != [view isFlipped]) {
      CGContextTranslateCTM(ctx, 0, viewFrame.size.height);
      CGContextScaleCTM(ctx, 1, -1);
    }

    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx
                                                                     flipped:[view isFlipped]]];

    if ([window useBrightTitlebarForeground] && !nsCocoaFeatures::OnYosemiteOrLater() &&
        view == [window standardWindowButton:NSWindowFullScreenButton]) {
      // Make the fullscreen button visible on dark titlebar backgrounds by
      // drawing it into a new transparency layer and turning it white.
      CGRect r = NSRectToCGRect([view bounds]);
      CGContextBeginTransparencyLayerWithRect(ctx, r, nullptr);

      // Draw twice for double opacity.
      [cell drawWithFrame:[button bounds] inView:button];
      [cell drawWithFrame:[button bounds] inView:button];

      // Make it white.
      CGContextSetBlendMode(ctx, kCGBlendModeSourceIn);
      CGContextSetRGBFillColor(ctx, 1, 1, 1, 1);
      CGContextFillRect(ctx, r);
      CGContextSetBlendMode(ctx, kCGBlendModeNormal);

      CGContextEndTransparencyLayer(ctx);
    } else {
      [cell drawWithFrame:[button bounds] inView:button];
    }

    [NSGraphicsContext setCurrentContext:context];
    CGContextRestoreGState(ctx);
  }

  CGContextRestoreGState(ctx);

  [NSGraphicsContext setCurrentContext:oldContext];

  CGContextRestoreGState(ctx);

  mUpdatedTitlebarRegion.OrWith(CocoaPointsToDevPixels(dirtyTitlebarRect));
}

// This method draws an overlay in the top of the window which contains the
// titlebar controls (e.g. close, min, zoom, fullscreen) and the titlebar
// highlight effect.
// This is necessary because the real titlebar controls are covered by our
// OpenGL context. Note that in terms of the NSView hierarchy, our ChildView
// is actually below the titlebar controls - that's why hovering and clicking
// them works as expected - but their visual representation is only drawn into
// the normal window buffer, and the window buffer surface lies below the
// GLContext surface. In order to make the titlebar controls visible, we have
// to redraw them inside the OpenGL context surface.
void nsChildView::MaybeDrawTitlebar(GLManager* aManager) {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  MutexAutoLock lock(mEffectsLock);
  if (!mIsCoveringTitlebar || mIsFullscreen || mTitlebarRect.IsEmpty()) {
    return;
  }

  LayoutDeviceIntRegion updatedTitlebarRegion;
  updatedTitlebarRegion.And(mUpdatedTitlebarRegion, mTitlebarRect);
  updatedTitlebarRegion.MoveBy(-mTitlebarRect.TopLeft());
  mUpdatedTitlebarRegion.SetEmpty();

  if (!mTitlebarImage) {
    mTitlebarImage = MakeUnique<RectTextureImage>();
  }

  mTitlebarImage->UpdateFromCGContext(mTitlebarRect.Size(), updatedTitlebarRegion,
                                      mTitlebarCGContext);

  mTitlebarImage->Draw(aManager, mTitlebarRect.TopLeft());
}

static void DrawTopLeftCornerMask(CGContextRef aCtx, int aRadius) {
  CGContextSetRGBFillColor(aCtx, 1.0, 1.0, 1.0, 1.0);
  CGContextFillEllipseInRect(aCtx, CGRectMake(0, 0, aRadius * 2, aRadius * 2));
}

void nsChildView::MaybeDrawRoundedCorners(GLManager* aManager, const LayoutDeviceIntRect& aRect) {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  MutexAutoLock lock(mEffectsLock);

  if (!mCornerMaskImage) {
    mCornerMaskImage = MakeUnique<RectTextureImage>();
  }

  LayoutDeviceIntSize size(mDevPixelCornerRadius, mDevPixelCornerRadius);
  mCornerMaskImage->UpdateIfNeeded(
      size, LayoutDeviceIntRegion(),
      ^(gfx::DrawTarget* drawTarget, const LayoutDeviceIntRegion& updateRegion) {
        ClearRegion(drawTarget, updateRegion);
        RefPtr<gfx::PathBuilder> builder = drawTarget->CreatePathBuilder();
        builder->Arc(gfx::Point(mDevPixelCornerRadius, mDevPixelCornerRadius),
                     mDevPixelCornerRadius, 0, 2.0f * M_PI);
        RefPtr<gfx::Path> path = builder->Finish();
        drawTarget->Fill(path, gfx::ColorPattern(gfx::Color(1.0, 1.0, 1.0, 1.0)),
                         gfx::DrawOptions(1.0f, gfx::CompositionOp::OP_SOURCE));
      });

  // Use operator destination in: multiply all 4 channels with source alpha.
  aManager->gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_SRC_ALPHA, LOCAL_GL_ZERO,
                                     LOCAL_GL_SRC_ALPHA);

  Matrix4x4 flipX = Matrix4x4::Scaling(-1, 1, 1);
  Matrix4x4 flipY = Matrix4x4::Scaling(1, -1, 1);

  if (mIsCoveringTitlebar && !mIsFullscreen) {
    // Mask the top corners.
    mCornerMaskImage->Draw(aManager, aRect.TopLeft());
    mCornerMaskImage->Draw(aManager, aRect.TopRight(), flipX);
  }

  if (mHasRoundedBottomCorners && !mIsFullscreen) {
    // Mask the bottom corners.
    mCornerMaskImage->Draw(aManager, aRect.BottomLeft(), flipY);
    mCornerMaskImage->Draw(aManager, aRect.BottomRight(), flipY * flipX);
  }

  // Reset blend mode.
  aManager->gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_ONE,
                                     LOCAL_GL_ONE);
}

static int32_t FindTitlebarBottom(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                                  int32_t aWindowWidth) {
  int32_t titlebarBottom = 0;
  for (auto& g : aThemeGeometries) {
    if ((g.mType == nsNativeThemeCocoa::eThemeGeometryTypeTitlebar ||
         g.mType == nsNativeThemeCocoa::eThemeGeometryTypeVibrantTitlebarLight ||
         g.mType == nsNativeThemeCocoa::eThemeGeometryTypeVibrantTitlebarDark) &&
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
    if ((g.mType == nsNativeThemeCocoa::eThemeGeometryTypeToolbar) && g.mRect.X() <= 0 &&
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
  int32_t toolboxBottom =
      FindFirstRectOfType(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeToolbox).YMost();

  ToolbarWindow* win = (ToolbarWindow*)[mView window];
  int32_t titlebarHeight = CocoaPointsToDevPixels([win titlebarHeight]);
  int32_t devUnifiedHeight = titlebarHeight + unifiedToolbarBottom;
  [win setUnifiedToolbarHeight:DevPixelsToCocoaPoints(devUnifiedHeight)];

  int32_t sheetPositionDevPx = std::max(toolboxBottom, unifiedToolbarBottom);
  NSPoint sheetPositionView = {0, DevPixelsToCocoaPoints(sheetPositionDevPx)};
  NSPoint sheetPositionWindow = [mView convertPoint:sheetPositionView toView:nil];
  [win setSheetAttachmentPosition:sheetPositionWindow.y];

  // Update titlebar control offsets.
  LayoutDeviceIntRect windowButtonRect =
      FindFirstRectOfType(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeWindowButtons);
  [win placeWindowButtons:[mView convertRect:DevPixelsToCocoaPoints(windowButtonRect) toView:nil]];
  LayoutDeviceIntRect fullScreenButtonRect =
      FindFirstRectOfType(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeFullscreenButton);
  [win placeFullScreenButton:[mView convertRect:DevPixelsToCocoaPoints(fullScreenButtonRect)
                                         toView:nil]];
}

static Maybe<VibrancyType> ThemeGeometryTypeToVibrancyType(
    nsITheme::ThemeGeometryType aThemeGeometryType) {
  switch (aThemeGeometryType) {
    case nsNativeThemeCocoa::eThemeGeometryTypeVibrancyLight:
    case nsNativeThemeCocoa::eThemeGeometryTypeVibrantTitlebarLight:
      return Some(VibrancyType::LIGHT);
    case nsNativeThemeCocoa::eThemeGeometryTypeVibrancyDark:
    case nsNativeThemeCocoa::eThemeGeometryTypeVibrantTitlebarDark:
      return Some(VibrancyType::DARK);
    case nsNativeThemeCocoa::eThemeGeometryTypeSheet:
      return Some(VibrancyType::SHEET);
    case nsNativeThemeCocoa::eThemeGeometryTypeTooltip:
      return Some(VibrancyType::TOOLTIP);
    case nsNativeThemeCocoa::eThemeGeometryTypeMenu:
      return Some(VibrancyType::MENU);
    case nsNativeThemeCocoa::eThemeGeometryTypeHighlightedMenuItem:
      return Some(VibrancyType::HIGHLIGHTED_MENUITEM);
    case nsNativeThemeCocoa::eThemeGeometryTypeSourceList:
      return Some(VibrancyType::SOURCE_LIST);
    case nsNativeThemeCocoa::eThemeGeometryTypeSourceListSelection:
      return Some(VibrancyType::SOURCE_LIST_SELECTION);
    case nsNativeThemeCocoa::eThemeGeometryTypeActiveSourceListSelection:
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
  if (!VibrancyManager::SystemSupportsVibrancy()) {
    return;
  }

  LayoutDeviceIntRegion sheetRegion = GatherVibrantRegion(aThemeGeometries, VibrancyType::SHEET);
  LayoutDeviceIntRegion vibrantLightRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::LIGHT);
  LayoutDeviceIntRegion vibrantDarkRegion =
      GatherVibrantRegion(aThemeGeometries, VibrancyType::DARK);
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

  MakeRegionsNonOverlapping(sheetRegion, vibrantLightRegion, vibrantDarkRegion, menuRegion,
                            tooltipRegion, highlightedMenuItemRegion, sourceListRegion,
                            sourceListSelectionRegion, activeSourceListSelectionRegion);

  auto& vm = EnsureVibrancyManager();
  bool changed = false;
  changed |= vm.UpdateVibrantRegion(VibrancyType::LIGHT, vibrantLightRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::DARK, vibrantDarkRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::MENU, menuRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::TOOLTIP, tooltipRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::HIGHLIGHTED_MENUITEM, highlightedMenuItemRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::SHEET, sheetRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::SOURCE_LIST, sourceListRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::SOURCE_LIST_SELECTION, sourceListSelectionRegion);
  changed |= vm.UpdateVibrantRegion(VibrancyType::ACTIVE_SOURCE_LIST_SELECTION,
                                    activeSourceListSelectionRegion);

  UpdateInternalOpaqueRegion();

  if (changed) {
    SuspendAsyncCATransactions();
  }
}

NSColor* nsChildView::VibrancyFillColorForThemeGeometryType(
    nsITheme::ThemeGeometryType aThemeGeometryType) {
  if (VibrancyManager::SystemSupportsVibrancy()) {
    Maybe<VibrancyType> vibrancyType = ThemeGeometryTypeToVibrancyType(aThemeGeometryType);
    MOZ_RELEASE_ASSERT(vibrancyType, "should only encounter vibrant theme geometry types here");
    return EnsureVibrancyManager().VibrancyFillColorForType(*vibrancyType);
  }
  return [NSColor whiteColor];
}

mozilla::VibrancyManager& nsChildView::EnsureVibrancyManager() {
  MOZ_ASSERT(mView, "Only call this once we have a view!");
  if (!mVibrancyManager) {
    mVibrancyManager = MakeUnique<VibrancyManager>(*this, [mView vibrancyViewsContainer]);
  }
  return *mVibrancyManager;
}

void nsChildView::UpdateInternalOpaqueRegion() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "This should only be called on the main thread.");
  auto opaqueRegion = mOpaqueRegion.Lock();
  bool widgetIsOpaque = GetTransparencyMode() == eTransparencyOpaque;
  if (!widgetIsOpaque) {
    opaqueRegion->SetEmpty();
  } else if (VibrancyManager::SystemSupportsVibrancy()) {
    opaqueRegion->Sub(mBounds, EnsureVibrancyManager().GetUnionOfVibrantRegions());
  } else {
    *opaqueRegion = mBounds;
  }
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

already_AddRefed<gfx::DrawTarget> nsChildView::StartRemoteDrawingInRegion(
    LayoutDeviceIntRegion& aInvalidRegion, BufferMode* aBufferMode) {
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    MOZ_RELEASE_ASSERT(mBasicCompositorIOSurface,
                       "Should have been set up by nsChildView::PreRender");

    mContentLayer->InvalidateRegionThroughoutSwapchain(aInvalidRegion.ToUnknownRegion());
    aInvalidRegion =
        LayoutDeviceIntRegion::FromUnknownRegion(mContentLayer->CurrentSurfaceInvalidRegion());
    *aBufferMode = BufferMode::BUFFER_NONE;
    mBasicCompositorIOSurface->Lock(false);
    return mBasicCompositorIOSurface->GetAsDrawTargetLocked(gfx::BackendType::SKIA);
  }

  MOZ_RELEASE_ASSERT(mGLPresenter);

  LayoutDeviceIntRegion dirtyRegion(aInvalidRegion);
  LayoutDeviceIntSize renderSize = mBounds.Size();

  MOZ_RELEASE_ASSERT(mBasicCompositorImage, "Should have created this in PreRender.");
  RefPtr<gfx::DrawTarget> drawTarget = mBasicCompositorImage->BeginUpdate(renderSize, dirtyRegion);

  if (!drawTarget) {
    // Composite unchanged textures.
    DoRemoteComposition(mBounds);
    return nullptr;
  }

  aInvalidRegion = mBasicCompositorImage->GetUpdateRegion();
  *aBufferMode = BufferMode::BUFFER_NONE;

  return drawTarget.forget();
}

void nsChildView::EndRemoteDrawing() {
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    MOZ_RELEASE_ASSERT(mBasicCompositorIOSurface);

    // The DrawTarget we returned from StartRemoteDrawingInRegion, which
    // referred to pixels in mBasicCompositorIOSurface, is no longer in use.
    // We can unlock the surface and release our reference to it.
    mBasicCompositorIOSurface->Unlock(false);
    mBasicCompositorIOSurface = nullptr;
  } else {
    mBasicCompositorImage->EndUpdate();
    DoRemoteComposition(mBounds);
  }
}

void nsChildView::CleanupRemoteDrawing() {
  mBasicCompositorImage = nullptr;
  mCornerMaskImage = nullptr;
  mTitlebarImage = nullptr;
  mGLPresenter = nullptr;
}

bool nsChildView::InitCompositor(Compositor* aCompositor) {
  if (!StaticPrefs::gfx_core_animation_enabled_AtStartup() &&
      aCompositor->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    if (!mGLPresenter) {
      mGLPresenter = GLPresenter::CreateForWindow(this);
    }

    return !!mGLPresenter;
  }
  return true;
}

void nsChildView::DoRemoteComposition(const LayoutDeviceIntRect& aRenderRect) {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  mGLPresenter->BeginFrame(aRenderRect.Size());

  // Draw the result from the basic compositor.
  mBasicCompositorImage->Draw(mGLPresenter.get(), LayoutDeviceIntPoint(0, 0));

  // DrawWindowOverlay doesn't do anything for non-GL, so it didn't paint
  // anything during the basic compositor transaction. Draw the overlay now.
  DrawWindowOverlay(mGLPresenter.get(), aRenderRect);

  mGLPresenter->EndFrame();
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
  if (mAPZC) {
    uint64_t inputBlockId = 0;
    ScrollableLayerGuid guid;
    return mAPZC->InputBridge()->ReceiveInputEvent(aEvent, &guid, &inputBlockId);
  }
  return nsEventStatus_eIgnore;
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
    uint64_t inputBlockId = 0;
    ScrollableLayerGuid guid;
    nsEventStatus result = nsEventStatus_eIgnore;

    switch (aEvent.mInputType) {
      case PANGESTURE_INPUT: {
        result = mAPZC->InputBridge()->ReceiveInputEvent(aEvent, &guid, &inputBlockId);
        if (result == nsEventStatus_eConsumeNoDefault) {
          return;
        }

        PanGestureInput& panInput = aEvent.AsPanGestureInput();

        event = panInput.ToWidgetWheelEvent(this);
        if (aCanTriggerSwipe && panInput.mOverscrollBehaviorAllowsSwipe) {
          SwipeInfo swipeInfo = SendMayStartSwipe(panInput);
          event.mCanTriggerSwipe = swipeInfo.wantsSwipe;
          if (swipeInfo.wantsSwipe) {
            if (result == nsEventStatus_eIgnore) {
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
                  MakeUnique<SwipeEventQueue>(swipeInfo.allowedDirections, inputBlockId);
            }
          }
        }

        if (mSwipeEventQueue && mSwipeEventQueue->inputBlockId == inputBlockId) {
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
        event = aEvent.AsScrollWheelInput().ToWidgetWheelEvent(this);
        result = mAPZC->InputBridge()->ReceiveInputEvent(event, &guid, &inputBlockId);
        if (result == nsEventStatus_eConsumeNoDefault) {
          return;
        }
        break;
      };
      default:
        MOZ_CRASH("unsupported event type");
        return;
    }
    if (event.mMessage == eWheel && (event.mDeltaX != 0 || event.mDeltaY != 0)) {
      ProcessUntransformedAPZEvent(&event, guid, inputBlockId, result);
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

      event = panInput.ToWidgetWheelEvent(this);
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
      event = aEvent.AsScrollWheelInput().ToWidgetWheelEvent(this);
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

void nsChildView::LookUpDictionary(const nsAString& aText,
                                   const nsTArray<mozilla::FontRange>& aFontRangeArray,
                                   const bool aIsVertical, const LayoutDeviceIntPoint& aPoint) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsChildView::SetPrefersReducedMotionOverrideForTest(bool aValue) {
  // Tell that the cache value we are going to set isn't cleared via
  // nsPresContext::ThemeChangedInternal which is called right before
  // we queue the media feature value change for this prefers-reduced-motion
  // change.
  LookAndFeel::SetShouldRetainCacheForTest(true);

  LookAndFeelInt prefersReducedMotion;
  prefersReducedMotion.id = LookAndFeel::eIntID_PrefersReducedMotion;
  prefersReducedMotion.value = aValue ? 1 : 0;

  AutoTArray<LookAndFeelInt, 1> lookAndFeelCache;
  lookAndFeelCache.AppendElement(prefersReducedMotion);

  // If we could have a way to modify
  // NSWorkspace.accessibilityDisplayShouldReduceMotion, we could use it, but
  // unfortunately there is no way, so we change the cache value instead as if
  // it's set in the parent process.
  LookAndFeel::SetIntCache(lookAndFeelCache);

  if (nsCocoaFeatures::OnMojaveOrLater() &&
      NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification) {
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                      object:nil];
  } else if (nsCocoaFeatures::OnYosemiteOrLater() &&
             NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                      object:nil];
  } else {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult nsChildView::ResetPrefersReducedMotionOverrideForTest() {
  LookAndFeel::SetShouldRetainCacheForTest(false);
  return NS_OK;
}

#ifdef ACCESSIBILITY
already_AddRefed<a11y::Accessible> nsChildView::GetDocumentAccessible() {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return nullptr;

  // mAccessible might be dead if accessibility was previously disabled and is
  // now being enabled again.
  if (mAccessible && mAccessible->IsAlive()) {
    RefPtr<a11y::Accessible> ret;
    CallQueryReferent(mAccessible.get(), static_cast<a11y::Accessible**>(getter_AddRefs(ret)));
    return ret.forget();
  }

  // need to fetch the accessible anew, because it has gone away.
  // cache the accessible in our weak ptr
  RefPtr<a11y::Accessible> acc = GetRootAccessible();
  mAccessible = do_GetWeakReference(acc.get());

  return acc.forget();
}
#endif

// GLPresenter implementation

GLPresenter::GLPresenter(GLContext* aContext) : mGLContext(aContext), mQuadVBO{0} {
  mGLContext->MakeCurrent();
  ShaderConfigOGL config;
  config.SetTextureTarget(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
  mRGBARectProgram =
      MakeUnique<ShaderProgramOGL>(mGLContext, ProgramProfileOGL::GetProfileFor(config));

  // Create mQuadVBO.
  mGLContext->fGenBuffers(1, &mQuadVBO);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

  // 1 quad, with the number of the quad (vertexID) encoded in w.
  GLfloat vertices[] = {
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
  };
  HeapCopyOfStackArray<GLfloat> verticesOnHeap(vertices);
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER, verticesOnHeap.ByteLength(), verticesOnHeap.Data(),
                          LOCAL_GL_STATIC_DRAW);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

GLPresenter::~GLPresenter() {
  if (mQuadVBO) {
    mGLContext->MakeCurrent();
    mGLContext->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }
}

void GLPresenter::BindAndDrawQuad(ShaderProgramOGL* aProgram, const gfx::Rect& aLayerRect,
                                  const gfx::Rect& aTextureRect) {
  mGLContext->MakeCurrent();

  gfx::Rect layerRects[4];
  gfx::Rect textureRects[4];

  layerRects[0] = aLayerRect;
  textureRects[0] = aTextureRect;

  aProgram->SetLayerRects(layerRects);
  aProgram->SetTextureRects(textureRects);

  const GLuint coordAttribIndex = 0;

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  mGLContext->fVertexAttribPointer(coordAttribIndex, 4, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   (GLvoid*)0);
  mGLContext->fEnableVertexAttribArray(coordAttribIndex);
  mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, 6);
  mGLContext->fDisableVertexAttribArray(coordAttribIndex);
}

void GLPresenter::BeginFrame(LayoutDeviceIntSize aRenderSize) {
  mGLContext->MakeCurrent();

  mGLContext->fViewport(0, 0, aRenderSize.width, aRenderSize.height);

  // Matrix to transform (0, 0, width, height) to viewport space (-1.0, 1.0,
  // 2, 2) and flip the contents.
  gfx::Matrix viewMatrix = gfx::Matrix::Translation(-1.0, 1.0);
  viewMatrix.PreScale(2.0f / float(aRenderSize.width), 2.0f / float(aRenderSize.height));
  viewMatrix.PreScale(1.0f, -1.0f);

  gfx::Matrix4x4 matrix3d = gfx::Matrix4x4::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  // set the projection matrix for the next time the program is activated
  mProjMatrix = matrix3d;

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA, LOCAL_GL_ONE,
                                 LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  mGLContext->fEnable(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
}

void GLPresenter::EndFrame() {
  mGLContext->SwapBuffers();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

class WidgetsReleaserRunnable final : public mozilla::Runnable {
 public:
  explicit WidgetsReleaserRunnable(nsTArray<nsCOMPtr<nsIWidget>>&& aWidgetArray)
      : mozilla::Runnable("WidgetsReleaserRunnable"), mWidgetArray(aWidgetArray) {}

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
// gLastDragView is only non-null while mouseDragged is on the call stack.
NSView* gLastDragView = nil;
NSEvent* gLastDragMouseDownEvent = nil;

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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super initWithFrame:inFrame])) {
    mGeckoChild = inChild;
    mBlockedLastMouseDown = NO;
    mExpectingWheelStop = NO;

    mLastMouseDownEvent = nil;
    mLastKeyDownEvent = nil;
    mClickThroughMouseDownEvent = nil;
    mDragService = nullptr;

    mGestureState = eGestureState_None;
    mCumulativeMagnification = 0.0;
    mCumulativeRotation = 0.0;

    mNeedsGLUpdate = NO;

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

    mTopLeftCornerMask = NULL;
    mLastPressureStage = 0;
  }

  // register for things we'll take from other applications
  [ChildView registerViewForDraggedTypes:self];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(systemMetricsChanged)
                                               name:NSControlTintDidChangeNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(systemMetricsChanged)
                                               name:NSSystemColorsDidChangeNotification
                                             object:nil];

  if (nsCocoaFeatures::OnMojaveOrLater() &&
      NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification) {
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(systemMetricsChanged)
               name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
             object:nil];
  } else if (nsCocoaFeatures::OnYosemiteOrLater() &&
             NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(systemMetricsChanged)
               name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
             object:nil];
  }

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(scrollbarSystemMetricChanged)
                                               name:NSPreferredScrollerStyleDidChangeNotification
                                             object:nil];
  [[NSDistributedNotificationCenter defaultCenter]
             addObserver:self
                selector:@selector(systemMetricsChanged)
                    name:@"AppleAquaScrollBarVariantChanged"
                  object:nil
      suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

  if (!StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_surfaceNeedsUpdate:)
                                                 name:NSViewGlobalFrameDidChangeNotification
                                               object:mPixelHostingView];
  }

  [[NSDistributedNotificationCenter defaultCenter]
             addObserver:self
                selector:@selector(systemMetricsChanged)
                    name:@"AppleInterfaceThemeChangedNotification"
                  object:nil
      suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// ComplexTextInputPanel's interpretKeyEvent hack won't work without this.
// It makes calls to +[NSTextInputContext currentContext], deep in system
// code, return the appropriate context.
- (NSTextInputContext*)inputContext {
  NSTextInputContext* pluginContext = NULL;
  if (mGeckoChild && mGeckoChild->IsPluginFocused()) {
    ComplexTextInputPanel* ctiPanel = ComplexTextInputPanel::GetSharedComplexTextInputPanel();
    if (ctiPanel) {
      pluginContext = (NSTextInputContext*)ctiPanel->GetInputContext();
    }
  }
  if (pluginContext) {
    return pluginContext;
  } else {
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
}

- (void)installTextInputHandler:(TextInputHandler*)aHandler {
  mTextInputHandler = aHandler;
}

- (void)uninstallTextInputHandler {
  mTextInputHandler = nullptr;
}

- (bool)preRender:(NSOpenGLContext*)aGLContext {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  if (![self window] || ([[self window] isKindOfClass:[BaseWindow class]] &&
                         ![(BaseWindow*)[self window] isVisibleOrBeingShown] &&
                         ![(BaseWindow*)[self window] isMiniaturized])) {
    // Before the window is shown, our GL context's front FBO is not
    // framebuffer complete, so we refuse to render.
    return false;
  }

  CGLLockContext((CGLContextObj)[aGLContext CGLContextObj]);

  if (!mGLContext) {
    mGLContext = aGLContext;
    [mGLContext retain];
    mNeedsGLUpdate = YES;
  }

  if (mNeedsGLUpdate) {
    [self updateGLContext];
    mNeedsGLUpdate = NO;
  }

  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

- (void)postRender:(NSOpenGLContext*)aGLContext {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  CGLUnlockContext((CGLContextObj)[aGLContext CGLContextObj]);

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mGLContext release];
  [mLastMouseDownEvent release];
  [mLastKeyDownEvent release];
  [mClickThroughMouseDownEvent release];
  CGImageRelease(mTopLeftCornerMask);
  ChildViewMouseTracker::OnDestroyView(self);

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  [mVibrancyViewsContainer removeFromSuperview];
  [mVibrancyViewsContainer release];
  [mNonDraggableViewsContainer removeFromSuperview];
  [mNonDraggableViewsContainer release];
  [mPixelHostingView removeFromSuperview];
  [mPixelHostingView release];

  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

- (void)systemMetricsChanged {
  if (mGeckoChild) mGeckoChild->NotifyThemeChanged();
}

- (void)scrollbarSystemMetricChanged {
  [self systemMetricsChanged];

  if (mGeckoChild) {
    if (nsIWidgetListener* listener = mGeckoChild->GetWidgetListener()) {
      if (PresShell* presShell = listener->GetPresShell()) {
        presShell->ReconstructFrames();
      }
    }
  }
}

- (NSString*)description {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSString stringWithFormat:@"ChildView %p, gecko child %p, frame %@", self, mGeckoChild,
                                    NSStringFromRect([self frame])];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
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

// Only called if StaticPrefs::gfx_core_animation_enabled_AtStartup() is false.
- (void)updateGLContext {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  [mGLContext setView:mPixelHostingView];
  [mGLContext update];
}

- (void)_surfaceNeedsUpdate:(NSNotification*)notification {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  if (mGLContext) {
    CGLLockContext((CGLContextObj)[mGLContext CGLContextObj]);
    mNeedsGLUpdate = YES;
    CGLUnlockContext((CGLContextObj)[mGLContext CGLContextObj]);
  }
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
  nsCocoaWindow* windowWidget = mGeckoChild ? mGeckoChild->GetXULWindowWidget() : nullptr;
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
  nsCocoaWindow* windowWidget = mGeckoChild ? mGeckoChild->GetXULWindowWidget() : nullptr;
  if (windowWidget) {
    windowWidget->NotifyLiveResizeStopped();
  }
}

- (NSColor*)vibrancyFillColorForThemeGeometryType:(nsITheme::ThemeGeometryType)aThemeGeometryType {
  if (!mGeckoChild) {
    return [NSColor whiteColor];
  }
  return mGeckoChild->VibrancyFillColorForThemeGeometryType(aThemeGeometryType);
}

- (LayoutDeviceIntRegion)nativeDirtyRegionWithBoundingRect:(NSRect)aRect {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  LayoutDeviceIntRect boundingRect = mGeckoChild->CocoaPointsToDevPixels(aRect);
  const NSRect* rects;
  NSInteger count;
  [mPixelHostingView getRectsBeingDrawn:&rects count:&count];

  if (count > MAX_RECTS_IN_REGION) {
    return boundingRect;
  }

  LayoutDeviceIntRegion region;
  for (NSInteger i = 0; i < count; ++i) {
    region.Or(region, mGeckoChild->CocoaPointsToDevPixels(rects[i]));
  }
  region.And(region, boundingRect);
  return region;
}

// The display system has told us that a portion of our view is dirty. Tell
// gecko to paint it
// This method is called from mPixelHostingView's drawRect handler.
// Only called when StaticPrefs::gfx_core_animation_enabled_AtStartup() is false.
- (void)doDrawRect:(NSRect)aRect {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  if (!NS_IsMainThread()) {
    // In the presence of CoreAnimation, this method can sometimes be called on
    // a non-main thread. Ignore those calls because Gecko can only react to
    // them on the main thread.
    return;
  }

  if (!mGeckoChild || !mGeckoChild->IsVisible()) return;
  CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

  if (mUsingOMTCompositor) {
    // Make sure the window's "drawRect" buffer does not interfere with our
    // OpenGL drawing's rounded corners.
    [self clearCorners];
    // Force a sync OMTC composite into the OpenGL context and return.
    LayoutDeviceIntRect geckoBounds = mGeckoChild->GetBounds();
    LayoutDeviceIntRegion region(geckoBounds);
    mGeckoChild->PaintWindow(region);
    return;
  }

  AUTO_PROFILER_LABEL("ChildView::drawRect", OTHER);

  // The CGContext that drawRect supplies us with comes with a transform that
  // scales one user space unit to one Cocoa point, which can consist of
  // multiple dev pixels. But Gecko expects its supplied context to be scaled
  // to device pixels, so we need to reverse the scaling.
  double scale = mGeckoChild->BackingScaleFactor();
  CGContextSaveGState(cgContext);
  CGContextScaleCTM(cgContext, 1.0 / scale, 1.0 / scale);

  NSSize viewSize = [self bounds].size;
  gfx::IntSize backingSize =
      gfx::IntSize::Truncate(viewSize.width * scale, viewSize.height * scale);
  LayoutDeviceIntRegion region = [self nativeDirtyRegionWithBoundingRect:aRect];

  bool painted = mGeckoChild->PaintWindowInContext(cgContext, region, backingSize);

  // Undo the scale transform so that from now on the context is in
  // CocoaPoints again.
  CGContextRestoreGState(cgContext);

  if (!painted && [mPixelHostingView isOpaque]) {
    // Gecko refused to draw, but we've claimed to be opaque, so we have to
    // draw something--fill with white.
    CGContextSetRGBFillColor(cgContext, 1, 1, 1, 1);
    CGContextFillRect(cgContext, NSRectToCGRect(aRect));
  }

  if ([self isCoveringTitlebar]) {
    [self drawTitleString];
    [self maskTopCornersInContext:cgContext];
  }
}

- (BOOL)hasRoundedBottomCorners {
  return [[self window] respondsToSelector:@selector(bottomCornerRounded)] &&
         [[self window] bottomCornerRounded];
}

- (CGFloat)cornerRadius {
  NSView* frameView = [[[self window] contentView] superview];
  if (!frameView || ![frameView respondsToSelector:@selector(roundedCornerRadius)]) return 4.0f;
  return [frameView roundedCornerRadius];
}

- (void)setGLOpaque:(BOOL)aOpaque {
  if (mGLContext) {
    MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());
    CGLLockContext((CGLContextObj)[mGLContext CGLContextObj]);
    // Make the context opaque for fullscreen (since it performs better), and transparent
    // for windowed (since we need it for rounded corners), but allow overriding
    // it to opaque for testing purposes, even if that breaks the rounded corners.
    GLint opaque = aOpaque || StaticPrefs::gfx_compositor_glcontext_opaque();
    [mGLContext setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];
    CGLUnlockContext((CGLContextObj)[mGLContext CGLContextObj]);
  }
}

// If StaticPrefs::gfx_core_animation_enabled_AtStartup() is false, our "accelerated" windows are
// NSWindows which are not CoreAnimation-backed but contain an NSView with
// an attached NSOpenGLContext.
// This means such windows have two WindowServer-level "surfaces" (NSSurface):
//  (1) The window's "drawRect" contents (a main-memory backed buffer) in the
//      back and
//  (2) the OpenGL drawing in the front.
// These two surfaces are composited by the window manager against our window's
// backdrop, i.e. everything on the screen behind our window.
// When our window has rounded corners, our OpenGL drawing respects those
// rounded corners and will leave transparent pixels in the corners. In these
// places the contents of the window's "drawRect" buffer can show through. So
// we need to make sure that this buffer is transparent in the corners so that
// the rounded corner anti-aliasing in the OpenGL context will blend directly
// against the backdrop of the window.
// We don't bother clearing parts of the window that are covered by opaque
// pixels from the OpenGL context.
- (void)clearCorners {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  CGFloat radius = [self cornerRadius];
  CGFloat w = [self bounds].size.width, h = [self bounds].size.height;
  [[NSColor clearColor] set];

  if ([self isCoveringTitlebar]) {
    NSRectFill(NSMakeRect(0, 0, radius, radius));
    NSRectFill(NSMakeRect(w - radius, 0, radius, radius));
  }

  if ([self hasRoundedBottomCorners]) {
    NSRectFill(NSMakeRect(0, h - radius, radius, radius));
    NSRectFill(NSMakeRect(w - radius, h - radius, radius, radius));
  }
}

// This is the analog of nsChildView::MaybeDrawRoundedCorners for CGContexts.
// We only need to mask the top corners here because Cocoa does the masking
// for the window's bottom corners automatically.
- (void)maskTopCornersInContext:(CGContextRef)aContext {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  CGFloat radius = [self cornerRadius];
  int32_t devPixelCornerRadius = mGeckoChild->CocoaPointsToDevPixels(radius);

  // First make sure that mTopLeftCornerMask is set up.
  if (!mTopLeftCornerMask || int32_t(CGImageGetWidth(mTopLeftCornerMask)) != devPixelCornerRadius) {
    CGImageRelease(mTopLeftCornerMask);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef imgCtx =
        CGBitmapContextCreate(NULL, devPixelCornerRadius, devPixelCornerRadius, 8,
                              devPixelCornerRadius * 4, rgb, kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(rgb);
    DrawTopLeftCornerMask(imgCtx, devPixelCornerRadius);
    mTopLeftCornerMask = CGBitmapContextCreateImage(imgCtx);
    CGContextRelease(imgCtx);
  }

  // kCGBlendModeDestinationIn is the secret sauce which allows us to erase
  // already painted pixels. It's defined as R = D * Sa: multiply all channels
  // of the destination pixel with the alpha of the source pixel. In our case,
  // the source is mTopLeftCornerMask.
  CGContextSaveGState(aContext);
  CGContextSetBlendMode(aContext, kCGBlendModeDestinationIn);

  CGRect destRect = CGRectMake(0, 0, radius, radius);

  // Erase the top left corner...
  CGContextDrawImage(aContext, destRect, mTopLeftCornerMask);

  // ... and the top right corner.
  CGContextTranslateCTM(aContext, [self bounds].size.width, 0);
  CGContextScaleCTM(aContext, -1, 1);
  CGContextDrawImage(aContext, destRect, mTopLeftCornerMask);

  CGContextRestoreGState(aContext);
}

- (void)drawTitleString {
  MOZ_RELEASE_ASSERT(!StaticPrefs::gfx_core_animation_enabled_AtStartup());

  BaseWindow* window = (BaseWindow*)[self window];
  if (![window wantsTitleDrawn]) {
    return;
  }

  NSView* frameView = [[window contentView] superview];
  if (![frameView respondsToSelector:@selector(_drawTitleBar:)]) {
    return;
  }

  NSGraphicsContext* oldContext = [NSGraphicsContext currentContext];
  CGContextRef ctx = (CGContextRef)[oldContext graphicsPort];
  CGContextSaveGState(ctx);
  if ([oldContext isFlipped] != [frameView isFlipped]) {
    CGContextTranslateCTM(ctx, 0, [self bounds].size.height);
    CGContextScaleCTM(ctx, 1, -1);
  }
  [NSGraphicsContext
      setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx
                                                                   flipped:[frameView isFlipped]]];
  [frameView _drawTitleBar:[frameView bounds]];
  CGContextRestoreGState(ctx);
  [NSGraphicsContext setCurrentContext:oldContext];
}

- (void)viewWillDraw {
  if (!NS_IsMainThread()) {
    // In the presence of CoreAnimation, this method can sometimes be called on
    // a non-main thread. Ignore those calls because Gecko can only react to
    // them on the main thread.
    return;
  }

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    // If we use CALayers for display, we will call WillPaintWindow during
    // nsChildView::HandleMainThreadCATransaction, and not here.
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if (mGeckoChild) {
    // The OS normally *will* draw our NSWindow, no matter what we do here.
    // But Gecko can delete our parent widget(s) (along with mGeckoChild)
    // while processing a paint request, which closes our NSWindow and
    // makes the OS throw an NSInternalInconsistencyException assertion when
    // it tries to draw it.  Sometimes the OS also aborts the browser process.
    // So we need to retain our parent(s) here and not release it/them until
    // the next time through the main thread's run loop.  When we do this we
    // also need to retain and release mGeckoChild, which holds a strong
    // reference to us.  See bug 550392.
    nsIWidget* parent = mGeckoChild->GetParent();
    if (parent) {
      nsTArray<nsCOMPtr<nsIWidget>> widgetArray;
      while (parent) {
        widgetArray.AppendElement(parent);
        parent = parent->GetParent();
      }
      widgetArray.AppendElement(mGeckoChild);
      nsCOMPtr<nsIRunnable> releaserRunnable = new WidgetsReleaserRunnable(std::move(widgetArray));
      NS_DispatchToMainThread(releaserRunnable);
    }

    if (mUsingOMTCompositor) {
      if (ShadowLayerForwarder* slf = mGeckoChild->GetLayerManager()->AsShadowForwarder()) {
        slf->WindowOverlayChanged();
      } else if (WebRenderLayerManager* wrlm =
                     mGeckoChild->GetLayerManager()->AsWebRenderLayerManager()) {
        wrlm->WindowOverlayChanged();
      }
    }

    mGeckoChild->WillPaintWindow();
  }
  [super viewWillDraw];
}

- (void)markLayerForDisplay {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
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
    mGeckoChild->HandleMainThreadCATransaction();
  }
}

- (CALayer*)rootCALayer {
  return [mPixelHostingView layer];
}

// If we've just created a non-native context menu, we need to mark it as
// such and let the OS (and other programs) know when it opens and closes
// (this is how the OS knows to close other programs' context menus when
// ours open).  We send the initial notification here, but others are sent
// in nsCocoaWindow::Show().
- (void)maybeInitContextMenuTracking {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Returns true if the event should no longer be processed, false otherwise.
// This does not return whether or not anything was rolled up.
- (BOOL)maybeRollup:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  BOOL consumeEvent = NO;

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE(rollupListener, false);
  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if (rollupWidget) {
    NSWindow* currentPopup = static_cast<NSWindow*>(rollupWidget->GetNativeData(NS_NATIVE_WINDOW));
    if (!nsCocoaUtils::IsEventOverWindow(theEvent, currentPopup)) {
      // event is not over the rollup window, default is to roll up
      bool shouldRollup = true;

      // check to see if scroll events should roll up the popup
      if ([theEvent type] == NSScrollWheel) {
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
        if ([theEvent type] == NSLeftMouseDown) {
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

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)swipeWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Pinch zoom gesture.
- (void)magnifyWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild) {
    return;
  }

  // FIXME: bug 1525793 -- this may need to handle zooming or not on a per-document basis.
  if (StaticPrefs::apz_allow_zooming()) {
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
        break;
      }
      default: {
        NS_WARNING("Unexpected phase for pinch gesture event.");
        return;
      }
    }

    PinchGestureInput event{pinchGestureType,
                            eventIntervalTime,
                            eventTimeStamp,
                            screenOffset,
                            position,
                            100.0,
                            100.0 * (1.0 - [anEvent magnification]),
                            nsCocoaUtils::ModifiersForEvent(anEvent)};

    if (pinchGestureType == PinchGestureInput::PINCHGESTURE_END) {
      event.mFocusPoint = PinchGestureInput::BothFingersLifted<ScreenPixel>();
    }

    mGeckoChild->DispatchAPZInputEvent(event);
  } else {
    if (!anEvent || [self beginOrEndGestureForEventPhase:anEvent]) {
      return;
    }

    nsAutoRetainCocoaObject kungFuDeathGrip(self);

    float deltaZ = [anEvent deltaZ];

    EventMessage msg;
    switch (mGestureState) {
      case eGestureState_StartGesture:
        msg = eMagnifyGestureStart;
        mGestureState = eGestureState_MagnifyGesture;
        break;

      case eGestureState_MagnifyGesture:
        msg = eMagnifyGestureUpdate;
        break;

      case eGestureState_None:
      case eGestureState_RotateGesture:
      default:
        return;
    }

    // This sends the pinch gesture value as a fake wheel event that has the
    // control key pressed so that pages can implement custom pinch gesture
    // handling. It may seem strange that this doesn't use a wheel event with
    // the deltaZ property set, but this matches Chrome's behavior as described
    // at https://code.google.com/p/chromium/issues/detail?id=289887
    //
    // The intent of the formula below is to produce numbers similar to Chrome's
    // implementation of this feature. Chrome implements deltaY using the formula
    // "-100 * log(1 + [event magnification])" which is unfortunately incorrect.
    // All deltas for a single pinch gesture should sum to 0 if the start and end
    // of a pinch gesture end up in the same place. This doesn't happen in Chrome
    // because they followed Apple's misleading documentation, which implies that
    // "1 + [event magnification]" is the scale factor. The scale factor is
    // instead "pow(ratio, [event magnification])" so "[event magnification]" is
    // already in log space.
    //
    // The multiplication by the backing scale factor below counteracts the
    // division by the backing scale factor in WheelEvent.
    WidgetWheelEvent geckoWheelEvent(true, EventMessage::eWheel, mGeckoChild);
    [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoWheelEvent];
    double backingScale = mGeckoChild->BackingScaleFactor();
    geckoWheelEvent.mDeltaY = -100.0 * [anEvent magnification] * backingScale;
    geckoWheelEvent.mModifiers |= MODIFIER_CONTROL;
    mGeckoChild->DispatchWindowEvent(geckoWheelEvent);

    // If the fake wheel event wasn't stopped, then send a normal magnify event.
    if (!geckoWheelEvent.mFlags.mDefaultPrevented) {
      WidgetSimpleGestureEvent geckoEvent(true, msg, mGeckoChild);
      geckoEvent.mDelta = deltaZ;
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
      mGeckoChild->DispatchWindowEvent(geckoEvent);

      // Keep track of the cumulative magnification for the final "magnify" event.
      mCumulativeMagnification += deltaZ;
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Smart zoom gesture, i.e. two-finger double tap on trackpads.
- (void)smartMagnifyWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild || [self beginOrEndGestureForEventPhase:anEvent]) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // Setup the "double tap" event.
  WidgetSimpleGestureEvent geckoEvent(true, eTapGesture, mGeckoChild);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mClickCount = 1;

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Clear the gesture state
  mGestureState = eGestureState_None;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rotateWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

  bool usingElCapitanOrLaterSDK = true;
#if !defined(MAC_OS_X_VERSION_10_11) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
  usingElCapitanOrLaterSDK = false;
#endif

  if (nsCocoaFeatures::OnElCapitanOrLater() && usingElCapitanOrLaterSDK) {
    if (aEvent.phase == NSEventPhaseBegan) {
      [self beginGestureWithEvent:aEvent];
      return true;
    }

    if (aEvent.phase == NSEventPhaseEnded || aEvent.phase == NSEventPhaseCancelled) {
      [self endGestureWithEvent:aEvent];
      return true;
    }
  }

  return false;
}

- (void)beginGestureWithEvent:(NSEvent*)aEvent {
  if (!aEvent) {
    return;
  }

  mGestureState = eGestureState_StartGesture;
  mCumulativeMagnification = 0;
  mCumulativeRotation = 0.0;
}

- (void)endGestureWithEvent:(NSEvent*)anEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild) {
    // Clear the gestures state if we cannot send an event.
    mGestureState = eGestureState_None;
    mCumulativeMagnification = 0.0;
    mCumulativeRotation = 0.0;
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  switch (mGestureState) {
    case eGestureState_MagnifyGesture: {
      // Setup the "magnify" event.
      WidgetSimpleGestureEvent geckoEvent(true, eMagnifyGesture, mGeckoChild);
      geckoEvent.mDelta = mCumulativeMagnification;
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    } break;

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

    case eGestureState_None:
    case eGestureState_StartGesture:
    default:
      break;
  }

  // Clear the gestures state.
  mGestureState = eGestureState_None;
  mCumulativeMagnification = 0.0;
  mCumulativeRotation = 0.0;

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NSEventPhase eventPhase = nsCocoaUtils::EventPhase(anEvent);
  if ([anEvent type] != NSScrollWheel || eventPhase != NSEventPhaseBegan ||
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NSUInteger modifierFlags = [theEvent modifierFlags];

  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  NSInteger clickCount = [theEvent clickCount];
  if (mBlockedLastMouseDown && clickCount > 1) {
    // Don't send a double click if the first click of the double click was
    // blocked.
    clickCount--;
  }
  geckoEvent.mClickCount = clickCount;

  if (modifierFlags & NSControlKeyMask)
    geckoEvent.mButton = MouseButton::eRight;
  else
    geckoEvent.mButton = MouseButton::eLeft;

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  mBlockedLastMouseDown = NO;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseUp:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild || mBlockedLastMouseDown) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  if ([theEvent modifierFlags] & NSControlKeyMask)
    geckoEvent.mButton = MouseButton::eRight;
  else
    geckoEvent.mButton = MouseButton::eLeft;

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
    if ([self shouldZoomOnDoubleClick]) {
      [[self window] performZoom:nil];
    } else if ([self shouldMinimizeOnTitlebarDoubleClick]) {
      NSButton* minimizeButton = [[self window] standardWindowButton:NSWindowMiniaturizeButton];
      [minimizeButton performClick:self];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

  event.mExitFrom = aExitFrom;

  nsEventStatus status;  // ignored
  mGeckoChild->DispatchEvent(&event, status);
}

- (void)handleMouseMoved:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseDragged:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  gLastDragView = self;

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  // Note, sending the above event might have destroyed our widget since we didn't retain.
  // Fine so long as we don't access any local variables from here on.
  gLastDragView = nil;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDown:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  // The right mouse went down, fire off a right mouse down event to gecko
  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eRight;
  geckoEvent.mClickCount = [theEvent clickCount];

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return;

  if (!nsBaseWidget::ShowContextMenuAfterMouseUp()) {
    // Let the superclass do the context menu stuff.
    [super rightMouseDown:theEvent];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseUp:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eRight;
  geckoEvent.mClickCount = [theEvent clickCount];

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return;

  if (nsBaseWidget::ShowContextMenuAfterMouseUp()) {
    // Let the superclass do the context menu stuff, but pretend it's rightMouseDown.
    NSEvent* dupeEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                            location:theEvent.locationInWindow
                                       modifierFlags:theEvent.modifierFlags
                                           timestamp:theEvent.timestamp
                                        windowNumber:theEvent.windowNumber
                                             context:theEvent.context
                                         eventNumber:theEvent.eventNumber
                                          clickCount:theEvent.clickCount
                                            pressure:theEvent.pressure];

    [super rightMouseDown:dupeEvent];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDragged:(NSEvent*)theEvent {
  if (!mGeckoChild) return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eRight;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)otherMouseDown:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  switch (nsCocoaUtils::EventPhase(aEvent)) {
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
      switch (nsCocoaUtils::EventMomentumPhase(aEvent)) {
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

static int32_t TakeLargestInt(gfx::Float* aFloat) {
  int32_t result(*aFloat);  // truncate towards zero
  *aFloat -= result;
  return result;
}

static gfx::IntPoint AccumulateIntegerDelta(NSEvent* aEvent) {
  static gfx::Point sAccumulator(0.0f, 0.0f);
  if (nsCocoaUtils::EventPhase(aEvent) == NSEventPhaseBegan) {
    sAccumulator = gfx::Point(0.0f, 0.0f);
  }
  sAccumulator.x += [aEvent deltaX];
  sAccumulator.y += [aEvent deltaY];
  return gfx::IntPoint(TakeLargestInt(&sAccumulator.x), TakeLargestInt(&sAccumulator.y));
}

static gfx::IntPoint GetIntegerDeltaForEvent(NSEvent* aEvent) {
  if (nsCocoaFeatures::OnSierraOrLater() && [aEvent hasPreciseScrollingDeltas]) {
    // Pixel scroll events (events with hasPreciseScrollingDeltas == YES)
    // carry pixel deltas in the scrollingDeltaX/Y fields and line scroll
    // information in the deltaX/Y fields.
    // Prior to 10.12, these line scroll fields would be zero for most pixel
    // scroll events and non-zero for some, whenever at least a full line
    // worth of pixel scrolling had accumulated. That's the behavior we want.
    // Starting with 10.12 however, pixel scroll events no longer accumulate
    // deltaX and deltaY; they just report floating point values for every
    // single event. So we need to do our own accumulation.
    return AccumulateIntegerDelta(aEvent);
  }

  // For line scrolls, or pre-10.12, just use the rounded up value of deltaX / deltaY.
  return gfx::IntPoint(RoundUp([aEvent deltaX]), RoundUp([aEvent deltaY]));
}

- (void)scrollWheel:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  ChildViewMouseTracker::MouseScrolled(theEvent);

  if ([self maybeRollup:theEvent]) {
    return;
  }

  if (!mGeckoChild) {
    return;
  }

  NSEventPhase phase = nsCocoaUtils::EventPhase(theEvent);
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

  bool usePreciseDeltas = nsCocoaUtils::HasPreciseScrollingDeltas(theEvent) &&
                          Preferences::GetBool("mousewheel.enable_pixel_scrolling", true);
  bool hasPhaseInformation = nsCocoaUtils::EventHasPhaseInformation(theEvent);

  gfx::IntPoint lineOrPageDelta = -GetIntegerDeltaForEvent(theEvent);

  Modifiers modifiers = nsCocoaUtils::ModifiersForEvent(theEvent);

  NSTimeInterval beforeNow = [[NSProcessInfo processInfo] systemUptime] - [theEvent timestamp];
  PRIntervalTime eventIntervalTime = PR_IntervalNow() - PR_MillisecondsToInterval(beforeNow * 1000);
  TimeStamp eventTimeStamp = TimeStamp::Now() - TimeDuration::FromSeconds(beforeNow);

  ScreenPoint preciseDelta;
  if (usePreciseDeltas) {
    CGFloat pixelDeltaX = 0, pixelDeltaY = 0;
    nsCocoaUtils::GetScrollingDeltas(theEvent, &pixelDeltaX, &pixelDeltaY);
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
      NSEvent* nextWheelEvent = [NSApp nextEventMatchingMask:NSScrollWheelMask
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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSMenu*)menuForEvent:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mGeckoChild) return nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild) return nil;

  // Cocoa doesn't always dispatch a mouseDown: for a control-click event,
  // depends on what we return from menuForEvent:. Gecko always expects one
  // and expects the mouse down event before the context menu event, so
  // get that event sent first if this is a left mouse click.
  if ([theEvent type] == NSLeftMouseDown) {
    [self mouseDown:theEvent];
    if (!mGeckoChild) return nil;
  }

  WidgetMouseEvent geckoEvent(true, eContextMenu, mGeckoChild, WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.mButton = MouseButton::eRight;
  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild) return nil;

  [self maybeInitContextMenuTracking];

  // We never return an actual NSMenu* for the context menu. Gecko might have
  // responded to the eContextMenu event by putting up a fake context menu.
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)convertCocoaMouseWheelEvent:(NSEvent*)aMouseEvent
                       toGeckoEvent:(WidgetWheelEvent*)outWheelEvent {
  [self convertCocoaMouseEvent:aMouseEvent toGeckoEvent:outWheelEvent];

  bool usePreciseDeltas = nsCocoaUtils::HasPreciseScrollingDeltas(aMouseEvent) &&
                          Preferences::GetBool("mousewheel.enable_pixel_scrolling", true);

  outWheelEvent->mDeltaMode = usePreciseDeltas ? dom::WheelEvent_Binding::DOM_DELTA_PIXEL
                                               : dom::WheelEvent_Binding::DOM_DELTA_LINE;
  outWheelEvent->mIsMomentum = nsCocoaUtils::IsMomentumScrollEvent(aMouseEvent);
}

- (void)convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(WidgetInputEvent*)outGeckoEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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
    mouseEvent->mButtons |= MouseButtonsFlag::eLeftFlag;
  }
  if (mouseButtons & 0x02) {
    mouseEvent->mButtons |= MouseButtonsFlag::eRightFlag;
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
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSLeftMouseDragged:
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSRightMouseDragged:
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSOtherMouseDragged:
    case NSMouseMoved:
      if ([aMouseEvent subtype] == NSTabletPointEventSubtype) {
        [self convertCocoaTabletPointerEvent:aMouseEvent toGeckoEvent:mouseEvent->AsMouseEvent()];
      }
      break;

    default:
      // Don't check other NSEvents for pressure.
      break;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)convertCocoaTabletPointerEvent:(NSEvent*)aPointerEvent
                          toGeckoEvent:(WidgetMouseEvent*)aOutGeckoEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN
  if (!aOutGeckoEvent || !sIsTabletPointerActivated) {
    return;
  }
  if ([aPointerEvent type] != NSMouseMoved) {
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
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)tabletProximity:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN
  sIsTabletPointerActivated = [theEvent isEnteringProximity];
  NS_OBJC_END_TRY_ABORT_BLOCK
}

- (BOOL)shouldZoomOnDoubleClick {
  if ([NSWindow respondsToSelector:@selector(_shouldZoomOnDoubleClick)]) {
    return [NSWindow _shouldZoomOnDoubleClick];
  }
  return nsCocoaFeatures::OnYosemiteOrLater();
}

- (BOOL)shouldMinimizeOnTitlebarDoubleClick {
  NSString* MDAppleMiniaturizeOnDoubleClickKey = @"AppleMiniaturizeOnDoubleClick";
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  bool shouldMinimize = [[userDefaults objectForKey:MDAppleMiniaturizeOnDoubleClickKey] boolValue];

  return shouldMinimize;
}

#pragma mark -
// NSTextInputClient implementation

- (NSRange)markedRange {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->MarkedRange();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRange)selectedRange {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->SelectedRange();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NS_ENSURE_TRUE(mTextInputHandler, [NSArray array]);
  return mTextInputHandler->GetValidAttributesForMarkedText();

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE_VOID(mGeckoChild);

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  NSAttributedString* attrStr;
  if ([aString isKindOfClass:[NSAttributedString class]]) {
    attrStr = static_cast<NSAttributedString*>(aString);
  } else {
    attrStr = [[[NSAttributedString alloc] initWithString:aString] autorelease];
  }

  mTextInputHandler->InsertText(attrStr, &replacementRange);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)doCommandBySelector:(SEL)aSelector {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild || !mTextInputHandler) {
    return;
  }

  const char* sel = reinterpret_cast<const char*>(aSelector);
  if (!mTextInputHandler->DoCommandBySelector(sel)) {
    [super doCommandBySelector:aSelector];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE_VOID(mTextInputHandler);

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  NSAttributedString* attrStr;
  if ([aString isKindOfClass:[NSAttributedString class]]) {
    attrStr = static_cast<NSAttributedString*>(aString);
  } else {
    attrStr = [[[NSAttributedString alloc] initWithString:aString] autorelease];
  }

  mTextInputHandler->SetMarkedText(attrStr, selectedRange, &replacementRange);

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, [[self window] level]);
  return mTextInputHandler->GetWindowLevel();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSNormalWindowLevel);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

      CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("\nBug 893973: ") +
                                                 NS_LITERAL_CSTRING(CRASH_MESSAGE));
      CrashReporter::AppendAppNotesToCrashReport(additionalInfo);

      MOZ_CRASH(CRASH_MESSAGE);
#  undef CRASH_MESSAGE
    } else if (!mGeckoChild->GetInputContext().IsPasswordEditor() &&
               TextInputHandler::IsSecureEventInputEnabled()) {
#  define CRASH_MESSAGE "A non-password editor has focus, but in secure input mode"

      CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("\nBug 893973: ") +
                                                 NS_LITERAL_CSTRING(CRASH_MESSAGE));
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
      [sNativeKeyEventsMap setObject:theEvent forKey:@(sUniqueKeyEventId)];
      // Purge old native events, in case we're still holding on to them. We
      // keep at most 10 references to 10 different native events.
      [sNativeKeyEventsMap removeObjectForKey:@(sUniqueKeyEventId - 10)];
      mTextInputHandler->HandleKeyDownEvent(theEvent, sUniqueKeyEventId);
    } else {
      // There was no text input handler. Offer the event to the native menu
      // system to check if there are any registered custom shortcuts for this
      // event.
      mGeckoChild->SendEventToNativeMenuSystem(theEvent);
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)keyUp:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  mTextInputHandler->HandleKeyUpEvent(theEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mTextInputHandler->HandleFlagsChanged(theEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)isFirstResponder {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSResponder* resp = [[self window] firstResponder];
  return (resp == (NSResponder*)self);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [super becomeFirstResponder];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(YES);
}

- (void)viewsWindowDidBecomeKey {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self removeFromSuperview];
  [self release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

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
    if (modifierFlags & NSAlternateKeyMask) {
      if (modifierFlags & NSCommandKeyMask)
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

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSDragOperationNone);
}

// NSDraggingDestination
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView draggingEntered: entered\n"));

  // there should never be a globalDragPboard when "draggingEntered:" is
  // called, but just in case we'll take care of it here.
  [globalDragPboard release];

  // Set the global drag pasteboard that will be used for this drag session.
  // This will be set back to nil when the drag session ends (mouse exits
  // the view or a drop happens within the view).
  globalDragPboard = [[sender draggingPasteboard] retain];

  return [self doDragAction:eDragEnter sender:sender];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSDragOperationNone);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  gDraggedTransferables = nullptr;

  NSEvent* currentEvent = [NSApp currentEvent];
  gUserCancelledDrag = ([currentEvent type] == NSKeyDown && [currentEvent keyCode] == kVK_Escape);

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession movedToPoint:(NSPoint)aPoint {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession willBeginAtPoint:(NSPoint)aPoint {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // there should never be a globalDragPboard when "willBeginAtPoint:" is
  // called, but just in case we'll take care of it here.
  [globalDragPboard release];

  // Set the global drag pasteboard that will be used for this drag session.
  // This will be set back to nil when the drag session ends (mouse exits
  // the view or a drop happens within the view).
  globalDragPboard = [[aSession draggingPasteboard] retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSPasteboardItemDataProvider
- (void)pasteboard:(NSPasteboard*)aPasteboard
                  item:(NSPasteboardItem*)aItem
    provideDataForType:(NSString*)aType {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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
        NS_NewLocalFile(EmptyString(), true, getter_AddRefs(targFile));
        nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(targFile);
        if (!macLocalFile) {
          NS_ERROR("No Mac local file");
          continue;
        }

        // get paste location from low level pasteboard
        PasteboardRef pboardRef = NULL;
        PasteboardCreate((CFStringRef)[aPasteboard name], &pboardRef);
        if (!pboardRef) {
          continue;
        }

        PasteboardSynchronize(pboardRef);
        CFURLRef urlRef = NULL;
        PasteboardCopyPasteLocation(pboardRef, &urlRef);
        if (!urlRef) {
          CFRelease(pboardRef);
          continue;
        }

        if (!NS_SUCCEEDED(macLocalFile->InitWithCFURL(urlRef))) {
          NS_ERROR("failed InitWithCFURL");
          CFRelease(urlRef);
          CFRelease(pboardRef);
          continue;
        }

        if (!gDraggedTransferables) {
          CFRelease(urlRef);
          CFRelease(pboardRef);
          continue;
        }

        uint32_t transferableCount;
        nsresult rv = gDraggedTransferables->GetLength(&transferableCount);
        if (NS_FAILED(rv)) {
          CFRelease(urlRef);
          CFRelease(pboardRef);
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
        CFRelease(urlRef);
        CFRelease(pboardRef);

        [aPasteboard setPropertyList:[pasteboardOutputDict valueForKey:curType] forType:curType];
      }
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -

// Support for the "Services" menu. We currently only support sending strings
// and HTML to system services.

- (id)validRequestorForSendType:(NSString*)sendType returnType:(NSString*)returnType {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK

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

  NS_OBJC_END_TRY_ABORT_BLOCK
}

nsresult nsChildView::GetSelectionAsPlaintext(nsAString& aResult) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
  RefPtr<a11y::Accessible> accessible = geckoChild->GetDocumentAccessible();
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

#  ifdef DEBUG
- (void)printHierarchy {
  [[self accessible] printHierarchy];
}
#  endif

#  pragma mark -

// general

- (BOOL)accessibilityIsIgnored {
  if (!mozilla::a11y::ShouldA11yBeEnabled()) return [super accessibilityIsIgnored];

  return [[self accessible] accessibilityIsIgnored];
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#endif /* ACCESSIBILITY */

+ (uint32_t)sUniqueKeyEventId {
  return sUniqueKeyEventId;
}

+ (NSMutableDictionary*)sNativeKeyEventsMap {
  return sNativeKeyEventsMap;
}

@end

@implementation PixelHostingView

- (id)initWithFrame:(NSRect)aRect {
  self = [super initWithFrame:aRect];

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
  }

  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}

- (void)drawRect:(NSRect)aRect {
  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    NS_WARNING("Unexpected call to drawRect: This view returns YES from wantsUpdateLayer, so "
               "drawRect should not be called.");
    return;
  }
  [(ChildView*)[self superview] doDrawRect:aRect];
}

- (BOOL)wantsUpdateLayer {
  return StaticPrefs::gfx_core_animation_enabled_AtStartup();
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
                                              ? WidgetMouseEvent::eChild
                                              : WidgetMouseEvent::eTopLevel;
    [oldView sendMouseEnterOrExitEvent:aEvent enter:NO exitFrom:exitFrom];
    // After the cursor exits the window set it to a visible regular arrow cursor.
    if (exitFrom == WidgetMouseEvent::eTopLevel) {
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
  if (!aWindow || [aEvent type] == NSRightMouseDown) return YES;

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
      [aEvent type] == NSOtherMouseDown ||
      (([aEvent modifierFlags] & NSCommandKeyMask) != 0 && [aEvent type] != NSMouseMoved))
    return YES;

  // If we're here then we're dealing with a left click or mouse move on an
  // inactive window or something similar. Ask Gecko what to do.
  return [aView inactiveWindowAcceptsMouseEvent:aEvent];
}

#pragma mark -
