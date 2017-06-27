/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/Logging.h"

#include <unistd.h>
#include <math.h>

#include "nsChildView.h"
#include "nsCocoaWindow.h"

#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

#include "nsArrayUtils.h"
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
#include "nsIDOMSimpleGestureEvent.h"
#include "nsThemeConstants.h"
#include "nsIWidgetListener.h"
#include "nsIPresShell.h"

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
#include "ScopedGLHelpers.h"
#include "HeapCopyOfStackArray.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/GLManager.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/widget/CompositorWidget.h"
#include "gfxUtils.h"
#include "gfxPrefs.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BorrowedContext.h"
#include "mozilla/gfx/MacIOSurface.h"
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#include "mozilla/a11y/Platform.h"
#endif
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "mozilla/Preferences.h"

#include <dlfcn.h>

#include <ApplicationServices/ApplicationServices.h>

#include "GeckoProfiler.h"

#include "nsIDOMWheelEvent.h"
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
  CGError CGSNewRegionWithRect(const CGRect *rect, CGSRegionObj *outRegion);
  CGError CGSNewRegionWithRectList(const CGRect *rects, int rectCount, CGSRegionObj *outRegion);
}

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu; // Application menu shared by all menubars

static bool gChildViewMethodsSwizzled = false;

extern nsIArray *gDraggedTransferables;

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

static uint32_t gNumberOfWidgetsNeedingEventThread = 0;

static bool sIsTabletPointerActivated = false;

static uint32_t sUniqueKeyEventId = 0;

static NSMutableDictionary* sNativeKeyEventsMap =
  [NSMutableDictionary dictionary];

@interface ChildView(Private)

// sets up our view, attaching it to its owning gecko view
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild;
- (void)forceRefreshOpenGL;

// set up a gecko mouse event based on a cocoa mouse event
- (void) convertCocoaMouseWheelEvent:(NSEvent*)aMouseEvent
                        toGeckoEvent:(WidgetWheelEvent*)outWheelEvent;
- (void) convertCocoaMouseEvent:(NSEvent*)aMouseEvent
                   toGeckoEvent:(WidgetInputEvent*)outGeckoEvent;
- (void) convertCocoaTabletPointerEvent:(NSEvent*)aMouseEvent
                           toGeckoEvent:(WidgetMouseEvent*)outGeckoEvent;
- (NSMenu*)contextMenu;

- (BOOL)isRectObscuredBySubview:(NSRect)inRect;

- (void)processPendingRedraws;

- (void)drawRect:(NSRect)aRect inContext:(CGContextRef)aContext;
- (LayoutDeviceIntRegion)nativeDirtyRegionWithBoundingRect:(NSRect)aRect;
- (BOOL)isUsingMainThreadOpenGL;
- (BOOL)isUsingOpenGL;
- (void)drawUsingOpenGL;
- (void)drawUsingOpenGLCallback;

- (BOOL)hasRoundedBottomCorners;
- (CGFloat)cornerRadius;
- (void)clearCorners;

-(void)setGLOpaque:(BOOL)aOpaque;

// Overlay drawing functions for traditional CGContext drawing
- (void)drawTitleString;
- (void)drawTitlebarHighlight;
- (void)maskTopCornersInContext:(CGContextRef)aContext;

#if USE_CLICK_HOLD_CONTEXTMENU
 // called on a timer two seconds after a mouse down to see if we should display
 // a context menu (click-hold)
- (void)clickHoldCallback:(id)inEvent;
#endif

#ifdef ACCESSIBILITY
- (id<mozAccessible>)accessible;
#endif

- (LayoutDeviceIntPoint)convertWindowCoordinates:(NSPoint)aPoint;
- (LayoutDeviceIntPoint)convertWindowCoordinatesRoundDown:(NSPoint)aPoint;
- (IAPZCTreeManager*)apzctm;

- (BOOL)inactiveWindowAcceptsMouseEvent:(NSEvent*)aEvent;
- (void)updateWindowDraggableState;

- (bool)beginOrEndGestureForEventPhase:(NSEvent*)aEvent;

- (bool)shouldConsiderStartingSwipeFromEvent:(NSEvent*)aEvent;

@end

@interface EventThreadRunner : NSObject
{
  NSThread* mThread;
}
- (id)init;

+ (void)start;
+ (void)stop;

@end

@interface NSView(NSThemeFrameCornerRadius)
- (float)roundedCornerRadius;
@end

@interface NSView(DraggableRegion)
- (CGSRegionObj)_regionForOpaqueDescendants:(NSRect)aRect forMove:(BOOL)aForMove;
- (CGSRegionObj)_regionForOpaqueDescendants:(NSRect)aRect forMove:(BOOL)aForMove forUnderTitlebar:(BOOL)aForUnderTitlebar;
@end

@interface NSWindow(NSWindowShouldZoomOnDoubleClick)
+ (BOOL)_shouldZoomOnDoubleClick; // present on 10.7 and above
@end

// Starting with 10.7 the bottom corners of all windows are rounded.
// Unfortunately, the standard rounding that OS X applies to OpenGL views
// does not use anti-aliasing and looks very crude. Since we want a smooth,
// anti-aliased curve, we'll draw it ourselves.
// Additionally, we need to turn off the OS-supplied rounding because it
// eats into our corner's curve. We do that by overriding an NSSurface method.
@interface NSSurface @end

@implementation NSSurface(DontCutOffCorners)
- (CGSRegionObj)_createRoundedBottomRegionForRect:(CGRect)rect
{
  // Create a normal rect region without rounded bottom corners.
  CGSRegionObj region;
  CGSNewRegionWithRect(&rect, &region);
  return region;
}
@end

#pragma mark -

// Flips a screen coordinate from a point in the cocoa coordinate system (bottom-left rect) to a point
// that is a "flipped" cocoa coordinate system (starts in the top-left).
static inline void
FlipCocoaScreenCoordinate(NSPoint &inPoint)
{
  inPoint.y = nsCocoaUtils::FlippedScreenY(inPoint.y);
}

void EnsureLogInitialized()
{
}

namespace {

// Used for OpenGL drawing from the compositor thread for OMTC BasicLayers.
// We need to use OpenGL for this because there seems to be no other robust
// way of drawing from a secondary thread without locking, which would cause
// deadlocks in our setup. See bug 882523.
class GLPresenter : public GLManager
{
public:
  static mozilla::UniquePtr<GLPresenter> CreateForWindow(nsIWidget* aWindow)
  {
    // Contrary to CompositorOGL, we allow unaccelerated OpenGL contexts to be
    // used. BasicCompositor only requires very basic GL functionality.
    bool forWebRender = false;
    RefPtr<GLContext> context = gl::GLContextProvider::CreateForWindow(aWindow, forWebRender, false);
    return context ? MakeUnique<GLPresenter>(context) : nullptr;
  }

  explicit GLPresenter(GLContext* aContext);
  virtual ~GLPresenter();

  virtual GLContext* gl() const override { return mGLContext; }
  virtual ShaderProgramOGL* GetProgram(GLenum aTarget, gfx::SurfaceFormat aFormat) override
  {
    MOZ_ASSERT(aTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    MOZ_ASSERT(aFormat == gfx::SurfaceFormat::R8G8B8A8);
    return mRGBARectProgram.get();
  }
  virtual const gfx::Matrix4x4& GetProjMatrix() const override
  {
    return mProjMatrix;
  }
  virtual void ActivateProgram(ShaderProgramOGL *aProg) override
  {
    mGLContext->fUseProgram(aProg->GetProgram());
  }
  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg,
                               const gfx::Rect& aLayerRect,
                               const gfx::Rect& aTextureRect) override;

  void BeginFrame(LayoutDeviceIntSize aRenderSize);
  void EndFrame();

  NSOpenGLContext* GetNSOpenGLContext()
  {
    return GLContextCGL::Cast(mGLContext)->GetNSOpenGLContext();
  }

protected:
  RefPtr<mozilla::gl::GLContext> mGLContext;
  mozilla::UniquePtr<mozilla::layers::ShaderProgramOGL> mRGBARectProgram;
  gfx::Matrix4x4 mProjMatrix;
  GLuint mQuadVBO;
};

} // unnamed namespace

namespace mozilla {

struct SwipeEventQueue {
  SwipeEventQueue(uint32_t aAllowedDirections, uint64_t aInputBlockId)
    : allowedDirections(aAllowedDirections)
    , inputBlockId(aInputBlockId)
  {}

  nsTArray<PanGestureInput> queuedEvents;
  uint32_t allowedDirections;
  uint64_t inputBlockId;
};

} // namespace mozilla

#pragma mark -

nsChildView::nsChildView() : nsBaseWidget()
, mView(nullptr)
, mParentView(nullptr)
, mParentWidget(nullptr)
, mViewTearDownLock("ChildViewTearDown")
, mEffectsLock("WidgetEffects")
, mShowsResizeIndicator(false)
, mHasRoundedBottomCorners(false)
, mIsCoveringTitlebar(false)
, mIsFullscreen(false)
, mIsOpaque(false)
, mTitlebarCGContext(nullptr)
, mBackingScaleFactor(0.0)
, mVisible(false)
, mDrawing(false)
, mIsDispatchPaint(false)
{
  EnsureLogInitialized();
}

nsChildView::~nsChildView()
{
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

  NS_WARNING_ASSERTION(
    mOnDestroyCalled,
    "nsChildView object destroyed without calling Destroy()");

  DestroyCompositor();

  if (mAPZC && gfxPrefs::AsyncPanZoomSeparateEventThread()) {
    gNumberOfWidgetsNeedingEventThread--;
    if (gNumberOfWidgetsNeedingEventThread == 0) {
      [EventThreadRunner stop];
    }
  }

  // An nsChildView object that was in use can be destroyed without Destroy()
  // ever being called on it.  So we also need to do a quick, safe cleanup
  // here (it's too late to just call Destroy(), which can cause crashes).
  // It's particularly important to make sure widgetDestroyed is called on our
  // mView -- this method NULLs mView's mGeckoChild, and NULL checks on
  // mGeckoChild are used throughout the ChildView class to tell if it's safe
  // to use a ChildView object.
  [mView widgetDestroyed]; // Safe if mView is nil.
  mParentWidget = nil;
  TearDownView(); // Safe if called twice.
}

void
nsChildView::ReleaseTitlebarCGContext()
{
  if (mTitlebarCGContext) {
    CGContextRelease(mTitlebarCGContext);
    mTitlebarCGContext = nullptr;
  }
}

nsresult
nsChildView::Create(nsIWidget* aParent,
                    nsNativeWidget aNativeParent,
                    const LayoutDeviceIntRect& aRect,
                    nsWidgetInitData* aInitData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Because the hidden window is created outside of an event loop,
  // we need to provide an autorelease pool to avoid leaking cocoa objects
  // (see bug 559075).
  nsAutoreleasePool localPool;

  // See NSView (MethodSwizzling) below.
  if (!gChildViewMethodsSwizzled) {
    nsToolkit::SwizzleMethods([NSView class], @selector(mouseDownCanMoveWindow),
                              @selector(nsChildView_NSView_mouseDownCanMoveWindow));
#ifdef __LP64__
    nsToolkit::SwizzleMethods([NSEvent class], @selector(addLocalMonitorForEventsMatchingMask:handler:),
                              @selector(nsChildView_NSEvent_addLocalMonitorForEventsMatchingMask:handler:),
                              true);
    nsToolkit::SwizzleMethods([NSEvent class], @selector(removeMonitor:),
                              @selector(nsChildView_NSEvent_removeMonitor:), true);
#endif
    gChildViewMethodsSwizzled = true;
  }

  mBounds = aRect;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  BaseCreate(aParent, aInitData);

  // inherit things from the parent view and create our parallel
  // NSView in the Cocoa display system
  mParentView = nil;
  if (aParent) {
    // inherit the top-level window. NS_NATIVE_WIDGET is always a NSView
    // regardless of if we're asking a window or a view (for compatibility
    // with windows).
    mParentView = (NSView<mozView>*)aParent->GetNativeData(NS_NATIVE_WIDGET);
    mParentWidget = aParent;
  } else {
    // This is the normal case. When we're the root widget of the view hiararchy,
    // aNativeParent will be the contentView of our window, since that's what
    // nsCocoaWindow returns when asked for an NS_NATIVE_VIEW.
    mParentView = reinterpret_cast<NSView<mozView>*>(aNativeParent);
  }

  // create our parallel NSView and hook it up to our parent. Recall
  // that NS_NATIVE_WIDGET is the NSView.
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mParentView);
  NSRect r = nsCocoaUtils::DevPixelsToCocoaPoints(mBounds, scaleFactor);
  mView = [(NSView<mozView>*)CreateCocoaView(r) retain];
  if (!mView) {
    return NS_ERROR_FAILURE;
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

// Creates the appropriate child view. Override to create something other than
// our |ChildView| object. Autoreleases, so caller must retain.
NSView*
nsChildView::CreateCocoaView(NSRect inFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[[ChildView alloc] initWithFrame:inFrame geckoChild:this] autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

void nsChildView::TearDownView()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mView)
    return;

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
    [mView performSelectorOnMainThread:@selector(delayedTearDown) withObject:nil waitUntilDone:false];
  }
  mView = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsCocoaWindow*
nsChildView::GetXULWindowWidget()
{
  id windowDelegate = [[mView window] delegate];
  if (windowDelegate && [windowDelegate isKindOfClass:[WindowDelegate class]]) {
    return [(WindowDelegate *)windowDelegate geckoWidget];
  }
  return nullptr;
}

void nsChildView::Destroy()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Make sure that no composition is in progress while disconnecting
  // ourselves from the view.
  MutexAutoLock lock(mViewTearDownLock);

  if (mOnDestroyCalled)
    return;
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
void* nsChildView::GetNativeData(uint32_t aDataType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

  void* retVal = nullptr;

  switch (aDataType)
  {
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

nsTransparencyMode nsChildView::GetTransparencyMode()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsCocoaWindow* windowWidget = GetXULWindowWidget();
  return windowWidget ? windowWidget->GetTransparencyMode() : eTransparencyOpaque;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(eTransparencyOpaque);
}

// This is called by nsContainerFrame on the root widget for all window types
// except popup windows (when nsCocoaWindow::SetTransparencyMode is used instead).
void nsChildView::SetTransparencyMode(nsTransparencyMode aMode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsCocoaWindow* windowWidget = GetXULWindowWidget();
  if (windowWidget) {
    windowWidget->SetTransparencyMode(aMode);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool nsChildView::IsVisible() const
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mVisible) {
    return mVisible;
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
static void
ManipulateViewWithoutNeedingDisplay(NSView* aView, void (^aCallback)())
{
  BaseWindow* win = nil;
  if ([[aView window] isKindOfClass:[BaseWindow class]]) {
    win = (BaseWindow*)[aView window];
  }
  [win disableSetNeedsDisplay];
  aCallback();
  [win enableSetNeedsDisplay];
}

// Hide or show this component
void
nsChildView::Show(bool aState)
{
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
void
nsChildView::SetParent(nsIWidget* aNewParent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mOnDestroyCalled)
    return;

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

void
nsChildView::ReparentNativeWidget(nsIWidget* aNewParent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_PRECONDITION(aNewParent, "");

  if (mOnDestroyCalled)
    return;

  NSView<mozView>* newParentView =
   (NSView<mozView>*)aNewParent->GetNativeData(NS_NATIVE_WIDGET);
  NS_ENSURE_TRUE_VOID(newParentView);

  // we hold a ref to mView, so this is safe
  [mView removeFromSuperview];
  mParentView = newParentView;
  [mParentView addSubview:mView];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsChildView::ResetParent()
{
  if (!mOnDestroyCalled) {
    if (mParentWidget)
      mParentWidget->RemoveChild(this);
    if (mView)
      [mView removeFromSuperview];
  }
  mParentWidget = nullptr;
}

nsIWidget*
nsChildView::GetParent()
{
  return mParentWidget;
}

float
nsChildView::GetDPI()
{
  NSWindow* window = [mView window];
  if (window && [window isKindOfClass:[BaseWindow class]]) {
    return [(BaseWindow*)window getDPI];
  }

  return 96.0;
}

void
nsChildView::Enable(bool aState)
{
}

bool nsChildView::IsEnabled() const
{
  return true;
}

nsresult
nsChildView::SetFocus(bool aRaise)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindow* window = [mView window];
  if (window)
    [window makeFirstResponder:mView];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Override to set the cursor on the mac
void
nsChildView::SetCursor(nsCursor aCursor)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if ([mView isDragInProgress])
    return; // Don't change the cursor during dragging.

  nsBaseWidget::SetCursor(aCursor);
  [[nsCursorManager sharedInstance] setCursor:aCursor];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// implement to fix "hidden virtual function" warning
nsresult
nsChildView::SetCursor(imgIContainer* aCursor,
                       uint32_t aHotspotX, uint32_t aHotspotY)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsBaseWidget::SetCursor(aCursor, aHotspotX, aHotspotY);
  return [[nsCursorManager sharedInstance] setCursorWithImage:aCursor hotSpotX:aHotspotX hotSpotY:aHotspotY scaleFactor:BackingScaleFactor()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#pragma mark -

// Get this component dimension
LayoutDeviceIntRect
nsChildView::GetBounds()
{
  return !mView ? mBounds : CocoaPointsToDevPixels([mView frame]);
}

LayoutDeviceIntRect
nsChildView::GetClientBounds()
{
  LayoutDeviceIntRect rect = GetBounds();
  if (!mParentWidget) {
    // For top level widgets we want the position on screen, not the position
    // of this view inside the window.
    rect.MoveTo(WidgetToScreenOffset());
  }
  return rect;
}

LayoutDeviceIntRect
nsChildView::GetScreenBounds()
{
  LayoutDeviceIntRect rect = GetBounds();
  rect.MoveTo(WidgetToScreenOffset());
  return rect;
}

double
nsChildView::GetDefaultScaleInternal()
{
  return BackingScaleFactor();
}

CGFloat
nsChildView::BackingScaleFactor() const
{
  if (mBackingScaleFactor > 0.0) {
    return mBackingScaleFactor;
  }
  if (!mView) {
    return 1.0;
  }
  mBackingScaleFactor = nsCocoaUtils::GetBackingScaleFactor(mView);
  return mBackingScaleFactor;
}

void
nsChildView::BackingScaleFactorChanged()
{
  CGFloat newScale = nsCocoaUtils::GetBackingScaleFactor(mView);

  // ignore notification if it hasn't really changed (or maybe we have
  // disabled HiDPI mode via prefs)
  if (mBackingScaleFactor == newScale) {
    return;
  }

  mBackingScaleFactor = newScale;
  NSRect frame = [mView frame];
  mBounds = nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, newScale);

  if (mWidgetListener && !mWidgetListener->GetXULWindow()) {
    nsIPresShell* presShell = mWidgetListener->GetPresShell();
    if (presShell) {
      presShell->BackingScaleFactorChanged();
    }
  }
}

int32_t
nsChildView::RoundsWidgetCoordinatesTo()
{
  if (BackingScaleFactor() == 2.0) {
    return 2;
  }
  return 1;
}

// Move this component, aX and aY are in the parent widget coordinate system
void
nsChildView::Move(double aX, double aY)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  int32_t x = NSToIntRound(aX);
  int32_t y = NSToIntRound(aY);

  if (!mView || (mBounds.x == x && mBounds.y == y))
    return;

  mBounds.x = x;
  mBounds.y = y;

  ManipulateViewWithoutNeedingDisplay(mView, ^{
    [mView setFrame:DevPixelsToCocoaPoints(mBounds)];
  });

  NotifyRollupGeometryChange();
  ReportMoveEvent();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsChildView::Resize(double aWidth, double aHeight, bool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);

  if (!mView || (mBounds.width == width && mBounds.height == height))
    return;

  mBounds.width  = width;
  mBounds.height = height;

  ManipulateViewWithoutNeedingDisplay(mView, ^{
    [mView setFrame:DevPixelsToCocoaPoints(mBounds)];
  });

  if (mVisible && aRepaint)
    [mView setNeedsDisplay:YES];

  NotifyRollupGeometryChange();
  ReportSizeEvent();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsChildView::Resize(double aX, double aY,
                    double aWidth, double aHeight, bool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  int32_t x = NSToIntRound(aX);
  int32_t y = NSToIntRound(aY);
  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);

  BOOL isMoving = (mBounds.x != x || mBounds.y != y);
  BOOL isResizing = (mBounds.width != width || mBounds.height != height);
  if (!mView || (!isMoving && !isResizing))
    return;

  if (isMoving) {
    mBounds.x = x;
    mBounds.y = y;
  }
  if (isResizing) {
    mBounds.width  = width;
    mBounds.height = height;
  }

  ManipulateViewWithoutNeedingDisplay(mView, ^{
    [mView setFrame:DevPixelsToCocoaPoints(mBounds)];
  });

  if (mVisible && aRepaint)
    [mView setNeedsDisplay:YES];

  NotifyRollupGeometryChange();
  if (isMoving) {
    ReportMoveEvent();
    if (mOnDestroyCalled)
      return;
  }
  if (isResizing)
    ReportSizeEvent();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const int32_t resizeIndicatorWidth = 15;
static const int32_t resizeIndicatorHeight = 15;
bool nsChildView::ShowsResizeIndicator(LayoutDeviceIntRect* aResizerRect)
{
  NSView *topLevelView = mView, *superView = nil;
  while ((superView = [topLevelView superview]))
    topLevelView = superView;

  if (![[topLevelView window] showsResizeIndicator] ||
      !([[topLevelView window] styleMask] & NSResizableWindowMask))
    return false;

  if (aResizerRect) {
    NSSize bounds = [topLevelView bounds].size;
    NSPoint corner = NSMakePoint(bounds.width, [topLevelView isFlipped] ? bounds.height : 0);
    corner = [topLevelView convertPoint:corner toView:mView];
    aResizerRect->SetRect(NSToIntRound(corner.x) - resizeIndicatorWidth,
                          NSToIntRound(corner.y) - resizeIndicatorHeight,
                          resizeIndicatorWidth, resizeIndicatorHeight);
  }
  return true;
}

nsresult nsChildView::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                               int32_t aNativeKeyCode,
                                               uint32_t aModifierFlags,
                                               const nsAString& aCharacters,
                                               const nsAString& aUnmodifiedCharacters,
                                               nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "keyevent");
  return mTextInputHandler->SynthesizeNativeKeyEvent(aNativeKeyboardLayout,
                                                     aNativeKeyCode,
                                                     aModifierFlags,
                                                     aCharacters,
                                                     aUnmodifiedCharacters);
}

nsresult nsChildView::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                 uint32_t aNativeMessage,
                                                 uint32_t aModifierFlags,
                                                 nsIObserver* aObserver)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  AutoObserverNotifier notifier(aObserver, "mouseevent");

  NSPoint pt =
    nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // aPoint is given with the origin on the top left, but convertScreenToBase
  // expects a point in a coordinate system that has its origin on the bottom left.
  NSPoint screenPoint = NSMakePoint(pt.x, nsCocoaUtils::FlippedScreenY(pt.y));
  NSPoint windowPoint =
    nsCocoaUtils::ConvertPointFromScreen([mView window], screenPoint);

  NSEvent* event = [NSEvent mouseEventWithType:(NSEventType)aNativeMessage
                                      location:windowPoint
                                 modifierFlags:aModifierFlags
                                     timestamp:[[NSProcessInfo processInfo] systemUptime]
                                  windowNumber:[[mView window] windowNumber]
                                       context:nil
                                   eventNumber:0
                                    clickCount:1
                                      pressure:0.0];

  if (!event)
    return NS_ERROR_FAILURE;

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

nsresult nsChildView::SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                       uint32_t aNativeMessage,
                                                       double aDeltaX,
                                                       double aDeltaY,
                                                       double aDeltaZ,
                                                       uint32_t aModifierFlags,
                                                       uint32_t aAdditionalFlags,
                                                       nsIObserver* aObserver)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  AutoObserverNotifier notifier(aObserver, "mousescrollevent");

  NSPoint pt =
    nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // Mostly copied from http://stackoverflow.com/a/6130349
  CGScrollEventUnit units =
    (aAdditionalFlags & nsIDOMWindowUtils::MOUSESCROLL_SCROLL_LINES)
    ? kCGScrollEventUnitLine : kCGScrollEventUnitPixel;
  CGEventRef cgEvent = CGEventCreateScrollWheelEvent(NULL, units, 3, aDeltaY, aDeltaX, aDeltaZ);
  if (!cgEvent) {
    return NS_ERROR_FAILURE;
  }

  CGEventPost(kCGHIDEventTap, cgEvent);
  CFRelease(cgEvent);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsChildView::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                                 TouchPointerState aPointerState,
                                                 mozilla::LayoutDeviceIntPoint aPoint,
                                                 double aPointerPressure,
                                                 uint32_t aPointerOrientation,
                                                 nsIObserver* aObserver)
{
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
      mSynthesizedTouchInput.get(), PR_IntervalNow(), TimeStamp::Now(),
      aPointerId, aPointerState, pointInWindow, aPointerPressure,
      aPointerOrientation);
  DispatchTouchInput(inputToDispatch);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// First argument has to be an NSMenu representing the application's top-level
// menu bar. The returned item is *not* retained.
static NSMenuItem* NativeMenuItemWithLocation(NSMenu* menubar, NSString* locationString)
{
  NSArray* indexes = [locationString componentsSeparatedByString:@"|"];
  unsigned int indexCount = [indexes count];
  if (indexCount == 0)
    return nil;

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
      if (i == (indexCount - 1))
        return menuItem;
      // if this is not the last index find the submenu and keep going
      if ([menuItem hasSubmenu])
        currentSubmenu = [menuItem submenu];
      else
        return nil;
    }
  }

  return nil;
}

bool
nsChildView::SendEventToNativeMenuSystem(NSEvent* aEvent)
{
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

void
nsChildView::PostHandleKeyEvent(mozilla::WidgetKeyboardEvent* aEvent)
{
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
nsresult
nsChildView::ActivateNativeMenuItemAt(const nsAString& indexString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString* locationString = [NSString stringWithCharacters:reinterpret_cast<const unichar*>(indexString.BeginReading())
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
nsresult
nsChildView::ForceUpdateNativeMenuAt(const nsAString& indexString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCocoaWindow *widget = GetXULWindowWidget();
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

static Boolean KeyDown(const UInt8 theKey)
{
  KeyMap map;
  GetKeys(map);
  return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}

static Boolean caps_lock()
{
  return KeyDown(0x39);
}

static void blinkRect(Rect* r)
{
  StRegionFromPool oldClip;
  if (oldClip != NULL)
    ::GetClip(oldClip);

  ::ClipRect(r);
  ::InvertRect(r);
  UInt32 end = ::TickCount() + 5;
  while (::TickCount() < end) ;
  ::InvertRect(r);

  if (oldClip != NULL)
    ::SetClip(oldClip);
}

static void blinkRgn(RgnHandle rgn)
{
  StRegionFromPool oldClip;
  if (oldClip != NULL)
    ::GetClip(oldClip);

  ::SetClip(rgn);
  ::InvertRgn(rgn);
  UInt32 end = ::TickCount() + 5;
  while (::TickCount() < end) ;
  ::InvertRgn(rgn);

  if (oldClip != NULL)
    ::SetClip(oldClip);
}

#endif

// Invalidate this component's visible area
void
nsChildView::Invalidate(const LayoutDeviceIntRect& aRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mView || !mVisible)
    return;

  NS_ASSERTION(GetLayerManager()->GetBackendType() != LayersBackend::LAYERS_CLIENT,
               "Shouldn't need to invalidate with accelerated OMTC layers!");

  if ([NSView focusView]) {
    // if a view is focussed (i.e. being drawn), then postpone the invalidate so that we
    // don't lose it.
    [mView setNeedsPendingDisplayInRect:DevPixelsToCocoaPoints(aRect)];
  }
  else {
    [mView setNeedsDisplayInRect:DevPixelsToCocoaPoints(aRect)];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

bool
nsChildView::WidgetTypeSupportsAcceleration()
{
  // We need to enable acceleration in popups which contain remote layer
  // trees, since the remote content won't be rendered at all otherwise. This
  // causes issues with transparency and drop shadows, so it should not be
  // used by default in release builds.
  if (HasRemoteContent()) {
    return true;
  }

  // Don't use OpenGL for transparent windows or for popup windows.
  return mView && [[mView window] isOpaque] &&
         ![[mView window] isKindOfClass:[PopupWindow class]];
}

bool
nsChildView::ShouldUseOffMainThreadCompositing()
{
  // Don't use OMTC for transparent windows or for popup windows.
  if (!WidgetTypeSupportsAcceleration())
    return false;

  return nsBaseWidget::ShouldUseOffMainThreadCompositing();
}

inline uint16_t COLOR8TOCOLOR16(uint8_t color8)
{
  // return (color8 == 0xFF ? 0xFFFF : (color8 << 8));
  return (color8 << 8) | color8;  /* (color8 * 257) == (color8 * 0x0101) */
}

#pragma mark -

nsresult nsChildView::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  return NS_OK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
nsresult
nsChildView::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
  RefPtr<nsChildView> kungFuDeathGrip(this);

#ifdef DEBUG
  debug_DumpEvent(stdout, event->mWidget, event, "something", 0);
#endif

  NS_ASSERTION(!(mTextInputHandler && mTextInputHandler->IsIMEComposing() &&
                 event->HasKeyEventMessage()),
    "Any key events should not be fired during IME composing");

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

  if (listener)
    aStatus = listener->HandleEvent(event, mUseAttachedEvents);

  return NS_OK;
}

bool nsChildView::DispatchWindowEvent(WidgetGUIEvent& event)
{
  nsEventStatus status;
  DispatchEvent(&event, status);
  return ConvertStatus(status);
}

nsIWidget*
nsChildView::GetWidgetForListenerEvents()
{
  // If there is no listener, use the parent popup's listener if that exists.
  if (!mWidgetListener && mParentWidget &&
      mParentWidget->WindowType() == eWindowType_popup) {
    return mParentWidget;
  }

  return this;
}

void nsChildView::WillPaintWindow()
{
  nsCOMPtr<nsIWidget> widget = GetWidgetForListenerEvents();

  nsIWidgetListener* listener = widget->GetWidgetListener();
  if (listener) {
    listener->WillPaintWindow(widget);
  }
}

bool nsChildView::PaintWindow(LayoutDeviceIntRegion aRegion)
{
  nsCOMPtr<nsIWidget> widget = GetWidgetForListenerEvents();

  nsIWidgetListener* listener = widget->GetWidgetListener();
  if (!listener)
    return false;

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

bool
nsChildView::PaintWindowInContext(CGContextRef aContext, const LayoutDeviceIntRegion& aRegion, gfx::IntSize aSurfaceSize)
{
  if (!mBackingSurface || mBackingSurface->GetSize() != aSurfaceSize) {
    mBackingSurface =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(aSurfaceSize,
                                                                   gfx::SurfaceFormat::B8G8R8A8);
    if (!mBackingSurface) {
      return false;
    }
  }

  RefPtr<gfxContext> targetContext = gfxContext::CreateOrNull(mBackingSurface);
  MOZ_ASSERT(targetContext); // already checked the draw target above

  // Set up the clip region and clear existing contents in the backing surface.
  targetContext->NewPath();
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect& r = iter.Get();
    targetContext->Rectangle(gfxRect(r.x, r.y, r.width, r.height));
    mBackingSurface->ClearRect(gfx::Rect(r.ToUnknownRect()));
  }
  targetContext->Clip();

  nsAutoRetainCocoaObject kungFuDeathGrip(mView);
  bool painted = false;
  if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    nsBaseWidget::AutoLayerManagerSetup
      setupLayerManager(this, targetContext, BufferMode::BUFFER_NONE);
    painted = PaintWindow(aRegion);
  } else if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    // We only need this so that we actually get DidPaintWindow fired
    painted = PaintWindow(aRegion);
  }

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
  CGImageRef image = CGImageCreate(size.width, size.height, 8, 32, stride,
                                   [colorSpace CGColorSpace],
                                   kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst,
                                   provider, NULL, false, kCGRenderingIntentDefault);
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

#pragma mark -

void nsChildView::ReportMoveEvent()
{
   NotifyWindowMoved(mBounds.x, mBounds.y);
}

void nsChildView::ReportSizeEvent()
{
  if (mWidgetListener)
    mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
}

#pragma mark -

LayoutDeviceIntPoint nsChildView::GetClientOffset()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSPoint origin = [mView convertPoint:NSMakePoint(0, 0) toView:nil];
  origin.y = [[mView window] frame].size.height - origin.y;
  return CocoaPointsToDevPixels(origin);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

//    Return the offset between this child view and the screen.
//    @return       -- widget origin in device-pixel coords
LayoutDeviceIntPoint nsChildView::WidgetToScreenOffset()
{
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

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntPoint(0,0));
}

nsresult
nsChildView::SetTitle(const nsAString& title)
{
  // child views don't have titles
  return NS_OK;
}

nsresult
nsChildView::GetAttention(int32_t aCycleCount)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

/* static */
bool nsChildView::DoHasPendingInputEvent()
{
  return sLastInputEventCount != GetCurrentInputEventCount();
}

/* static */
uint32_t nsChildView::GetCurrentInputEventCount()
{
  // Can't use kCGAnyInputEventType because that updates too rarely for us (and
  // always in increments of 30+!) and because apparently it's sort of broken
  // on Tiger.  So just go ahead and query the counters we care about.
  static const CGEventType eventTypes[] = {
    kCGEventLeftMouseDown,
    kCGEventLeftMouseUp,
    kCGEventRightMouseDown,
    kCGEventRightMouseUp,
    kCGEventMouseMoved,
    kCGEventLeftMouseDragged,
    kCGEventRightMouseDragged,
    kCGEventKeyDown,
    kCGEventKeyUp,
    kCGEventScrollWheel,
    kCGEventTabletPointer,
    kCGEventOtherMouseDown,
    kCGEventOtherMouseUp,
    kCGEventOtherMouseDragged
  };

  uint32_t eventCount = 0;
  for (uint32_t i = 0; i < ArrayLength(eventTypes); ++i) {
    eventCount +=
      CGEventSourceCounterForEventType(kCGEventSourceStateCombinedSessionState,
                                       eventTypes[i]);
  }
  return eventCount;
}

/* static */
void nsChildView::UpdateCurrentInputEventCount()
{
  sLastInputEventCount = GetCurrentInputEventCount();
}

bool nsChildView::HasPendingInputEvent()
{
  return DoHasPendingInputEvent();
}

#pragma mark -

nsresult
nsChildView::StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                            int32_t aPanelX, int32_t aPanelY,
                            nsString& aCommitted)
{
  NS_ENSURE_TRUE(mView, NS_ERROR_NOT_AVAILABLE);

  ComplexTextInputPanel* ctiPanel =
    ComplexTextInputPanel::GetSharedComplexTextInputPanel();

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
  ctiPanel->InterpretKeyEvent([(ChildView*)mView lastKeyDownEvent], aCommitted);

  return NS_OK;
}

void
nsChildView::SetPluginFocused(bool& aFocused)
{
  if (aFocused == mPluginFocused) {
    return;
  }
  if (!aFocused) {
    ComplexTextInputPanel* ctiPanel =
      ComplexTextInputPanel::GetSharedComplexTextInputPanel();
    if (ctiPanel) {
      ctiPanel->CancelComposition();
    }
  }
  mPluginFocused = aFocused;
}

void
nsChildView::SetInputContext(const InputContext& aContext,
                             const InputContextAction& aAction)
{
  NS_ENSURE_TRUE_VOID(mTextInputHandler);

  if (mTextInputHandler->IsFocused()) {
    if (aContext.IsPasswordEditor()) {
      TextInputHandler::EnableSecureEventInput();
    } else {
      TextInputHandler::EnsureSecureEventInputDisabled();
    }
  }

  mInputContext = aContext;
  switch (aContext.mIMEState.mEnabled) {
    case IMEState::ENABLED:
    case IMEState::PLUGIN:
      mTextInputHandler->SetASCIICapableOnly(false);
      mTextInputHandler->EnableIME(true);
      if (mInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE) {
        mTextInputHandler->SetIMEOpenState(
          mInputContext.mIMEState.mOpen == IMEState::OPEN);
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

InputContext
nsChildView::GetInputContext()
{
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

TextEventDispatcherListener*
nsChildView::GetNativeTextEventDispatcherListener()
{
  if (NS_WARN_IF(!mTextInputHandler)) {
    return nullptr;
  }
  return mTextInputHandler;
}

nsresult
nsChildView::AttachNativeKeyEvent(mozilla::WidgetKeyboardEvent& aEvent)
{
  NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
  return mTextInputHandler->AttachNativeKeyEvent(aEvent);
}

void
nsChildView::GetEditCommandsRemapped(NativeKeyBindingsType aType,
                                     const WidgetKeyboardEvent& aEvent,
                                     nsTArray<CommandInt>& aCommands,
                                     uint32_t aGeckoKeyCode,
                                     uint32_t aCocoaKeyCode)
{
  NSEvent *originalEvent = reinterpret_cast<NSEvent*>(aEvent.mNativeKeyEvent);

  WidgetKeyboardEvent modifiedEvent(aEvent);
  modifiedEvent.mKeyCode = aGeckoKeyCode;

  unichar ch = nsCocoaUtils::ConvertGeckoKeyCodeToMacCharCode(aGeckoKeyCode);
  NSString *chars =
    [[[NSString alloc] initWithCharacters:&ch length:1] autorelease];

  modifiedEvent.mNativeKeyEvent =
    [NSEvent keyEventWithType:[originalEvent type]
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

void
nsChildView::GetEditCommands(NativeKeyBindingsType aType,
                             const WidgetKeyboardEvent& aEvent,
                             nsTArray<CommandInt>& aCommands)
{
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

NSView<mozView>* nsChildView::GetEditorView()
{
  NSView<mozView>* editorView = mView;
  // We need to get editor's view. E.g., when the focus is in the bookmark
  // dialog, the view is <panel> element of the dialog.  At this time, the key
  // events are processed the parent window's view that has native focus.
  WidgetQueryContentEvent textContent(true, eQueryTextContent, this);
  textContent.InitForQueryTextContent(0, 0);
  DispatchWindowEvent(textContent);
  if (textContent.mSucceeded && textContent.mReply.mFocusedWidget) {
    NSView<mozView>* view = static_cast<NSView<mozView>*>(
      textContent.mReply.mFocusedWidget->GetNativeData(NS_NATIVE_WIDGET));
    if (view)
      editorView = view;
  }
  return editorView;
}

#pragma mark -

void
nsChildView::CreateCompositor()
{
  nsBaseWidget::CreateCompositor();
  if (mCompositorBridgeChild) {
    [(ChildView *)mView setUsingOMTCompositor:true];
  }
}

void
nsChildView::ConfigureAPZCTreeManager()
{
  nsBaseWidget::ConfigureAPZCTreeManager();

  if (gfxPrefs::AsyncPanZoomSeparateEventThread()) {
    if (gNumberOfWidgetsNeedingEventThread == 0) {
      [EventThreadRunner start];
    }
    gNumberOfWidgetsNeedingEventThread++;
  }
}

void
nsChildView::ConfigureAPZControllerThread()
{
  if (gfxPrefs::AsyncPanZoomSeparateEventThread()) {
    // The EventThreadRunner is the controller thread, but it doesn't
    // have a MessageLoop.
    APZThreadUtils::SetControllerThread(nullptr);
  } else {
    nsBaseWidget::ConfigureAPZControllerThread();
  }
}

LayoutDeviceIntRect
nsChildView::RectContainingTitlebarControls()
{
  // Start with a thin strip at the top of the window for the highlight line.
  NSRect rect = NSMakeRect(0, 0, [mView bounds].size.width,
                           [(ChildView*)mView cornerRadius]);

  // If we draw the titlebar title string, increase the height to the default
  // titlebar height. This height does not necessarily include all the titlebar
  // controls because we may have moved them further down, but at least it will
  // include the whole title text.
  BaseWindow* window = (BaseWindow*)[mView window];
  if ([window wantsTitleDrawn] && [window isKindOfClass:[ToolbarWindow class]]) {
    CGFloat defaultTitlebarHeight = [(ToolbarWindow*)window titlebarHeight];
    rect.size.height = std::max(rect.size.height, defaultTitlebarHeight);
  }

  // Add the rects of the titlebar controls.
  for (id view in [window titlebarControls]) {
    rect = NSUnionRect(rect, [mView convertRect:[view bounds] fromView:view]);
  }
  return CocoaPointsToDevPixels(rect);
}

void
nsChildView::PrepareWindowEffects()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  bool canBeOpaque;
  {
    MutexAutoLock lock(mEffectsLock);
    mShowsResizeIndicator = ShowsResizeIndicator(&mResizeIndicatorRect);
    mHasRoundedBottomCorners = [(ChildView*)mView hasRoundedBottomCorners];
    CGFloat cornerRadius = [(ChildView*)mView cornerRadius];
    mDevPixelCornerRadius = cornerRadius * BackingScaleFactor();
    mIsCoveringTitlebar = [(ChildView*)mView isCoveringTitlebar];
    NSInteger styleMask = [[mView window] styleMask];
    bool wasFullscreen = mIsFullscreen;
    nsCocoaWindow* windowWidget = GetXULWindowWidget();
    mIsFullscreen = (styleMask & NSFullScreenWindowMask) ||
                    (windowWidget && windowWidget->InFullScreenMode());

    canBeOpaque = mIsFullscreen && wasFullscreen;
    if (canBeOpaque && VibrancyManager::SystemSupportsVibrancy()) {
      canBeOpaque = !EnsureVibrancyManager().HasVibrantRegions();
    }
    if (mIsCoveringTitlebar) {
      mTitlebarRect = RectContainingTitlebarControls();
      UpdateTitlebarCGContext();
    }
  }

  // If we've just transitioned into or out of full screen then update the opacity on our GLContext.
  if (canBeOpaque != mIsOpaque) {
    mIsOpaque = canBeOpaque;
    [(ChildView*)mView setGLOpaque:canBeOpaque];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsChildView::CleanupWindowEffects()
{
  mResizerImage = nullptr;
  mCornerMaskImage = nullptr;
  mTitlebarImage = nullptr;
}

void
nsChildView::AddWindowOverlayWebRenderCommands(layers::WebRenderBridgeChild* aWrBridge,
                                               wr::DisplayListBuilder& aBuilder)
{
  PrepareWindowEffects();

  LayoutDeviceIntRegion updatedTitlebarRegion;
  updatedTitlebarRegion.And(mUpdatedTitlebarRegion, mTitlebarRect);
  mUpdatedTitlebarRegion.SetEmpty();

  if (mTitlebarCGContext) {
    gfx::IntSize size(CGBitmapContextGetWidth(mTitlebarCGContext),
                      CGBitmapContextGetHeight(mTitlebarCGContext));
    size_t stride = CGBitmapContextGetBytesPerRow(mTitlebarCGContext);
    size_t titlebarCGContextDataLength = stride * size.height;
    gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
    wr::ByteBuffer buffer(
      titlebarCGContextDataLength,
      static_cast<uint8_t *>(CGBitmapContextGetData(mTitlebarCGContext)));

    if (mTitlebarImageKey &&
        mTitlebarImageSize != size) {
      // Delete wr::ImageKey. wr::ImageKey does not support size change.
      CleanupWebRenderWindowOverlay(aWrBridge);
      MOZ_ASSERT(mTitlebarImageKey.isNothing());
    }

    if (!mTitlebarImageKey) {
      mTitlebarImageKey = Some(aWrBridge->GetNextImageKey());
      aWrBridge->SendAddImage(*mTitlebarImageKey, size, stride, format, buffer);
      mTitlebarImageSize = size;
      updatedTitlebarRegion.SetEmpty();
    }

    if (!updatedTitlebarRegion.IsEmpty()) {
      aWrBridge->SendUpdateImage(*mTitlebarImageKey, size, format, buffer);
    }

    wr::WrRect rect = wr::ToWrRect(mTitlebarRect);
    aBuilder.PushImage(wr::WrRect{ 0, 0, float(size.width), float(size.height) },
                       rect, wr::ImageRendering::Auto, *mTitlebarImageKey);
  }
}

void
nsChildView::CleanupWebRenderWindowOverlay(layers::WebRenderBridgeChild* aWrBridge)
{
  if (mTitlebarImageKey) {
    aWrBridge->SendDeleteImage(*mTitlebarImageKey);
    mTitlebarImageKey = Nothing();
  }
}

bool
nsChildView::PreRender(WidgetRenderingContext* aContext)
{
  UniquePtr<GLManager> manager(GLManager::CreateGLManager(aContext->mLayerManager));
  gl::GLContext* gl = manager ? manager->gl() : aContext->mGL;
  if (!gl) {
    return true;
  }

  // The lock makes sure that we don't attempt to tear down the view while
  // compositing. That would make us unable to call postRender on it when the
  // composition is done, thus keeping the GL context locked forever.
  mViewTearDownLock.Lock();

  NSOpenGLContext *glContext = GLContextCGL::Cast(gl)->GetNSOpenGLContext();

  if (![(ChildView*)mView preRender:glContext]) {
    mViewTearDownLock.Unlock();
    return false;
  }
  return true;
}

void
nsChildView::PostRender(WidgetRenderingContext* aContext)
{
  UniquePtr<GLManager> manager(GLManager::CreateGLManager(aContext->mLayerManager));
  gl::GLContext* gl = manager ? manager->gl() : aContext->mGL;
  if (!gl) {
    return;
  }
  NSOpenGLContext *glContext = GLContextCGL::Cast(gl)->GetNSOpenGLContext();
  [(ChildView*)mView postRender:glContext];
  mViewTearDownLock.Unlock();
}

void
nsChildView::DrawWindowOverlay(WidgetRenderingContext* aContext,
                               LayoutDeviceIntRect aRect)
{
  mozilla::UniquePtr<GLManager> manager(GLManager::CreateGLManager(aContext->mLayerManager));
  if (manager) {
    DrawWindowOverlay(manager.get(), aRect);
  }
}

void
nsChildView::DrawWindowOverlay(GLManager* aManager, LayoutDeviceIntRect aRect)
{
  GLContext* gl = aManager->gl();
  ScopedGLState scopedScissorTestState(gl, LOCAL_GL_SCISSOR_TEST, false);

  MaybeDrawTitlebar(aManager);
  MaybeDrawResizeIndicator(aManager);
  MaybeDrawRoundedCorners(aManager, aRect);
}

static void
ClearRegion(gfx::DrawTarget *aDT, LayoutDeviceIntRegion aRegion)
{
  gfxUtils::ClipToRegion(aDT, aRegion.ToUnknownRegion());
  aDT->ClearRect(gfx::Rect(0, 0, aDT->GetSize().width, aDT->GetSize().height));
  aDT->PopClip();
}

static void
DrawResizer(CGContextRef aCtx)
{
  CGContextSetShouldAntialias(aCtx, false);
  CGPoint points[6];
  points[0] = CGPointMake(13.0f, 4.0f);
  points[1] = CGPointMake(3.0f, 14.0f);
  points[2] = CGPointMake(13.0f, 8.0f);
  points[3] = CGPointMake(7.0f, 14.0f);
  points[4] = CGPointMake(13.0f, 12.0f);
  points[5] = CGPointMake(11.0f, 14.0f);
  CGContextSetRGBStrokeColor(aCtx, 0.00f, 0.00f, 0.00f, 0.15f);
  CGContextStrokeLineSegments(aCtx, points, 6);

  points[0] = CGPointMake(13.0f, 5.0f);
  points[1] = CGPointMake(4.0f, 14.0f);
  points[2] = CGPointMake(13.0f, 9.0f);
  points[3] = CGPointMake(8.0f, 14.0f);
  points[4] = CGPointMake(13.0f, 13.0f);
  points[5] = CGPointMake(12.0f, 14.0f);
  CGContextSetRGBStrokeColor(aCtx, 0.13f, 0.13f, 0.13f, 0.54f);
  CGContextStrokeLineSegments(aCtx, points, 6);

  points[0] = CGPointMake(13.0f, 6.0f);
  points[1] = CGPointMake(5.0f, 14.0f);
  points[2] = CGPointMake(13.0f, 10.0f);
  points[3] = CGPointMake(9.0f, 14.0f);
  points[5] = CGPointMake(13.0f, 13.9f);
  points[4] = CGPointMake(13.0f, 14.0f);
  CGContextSetRGBStrokeColor(aCtx, 0.84f, 0.84f, 0.84f, 0.55f);
  CGContextStrokeLineSegments(aCtx, points, 6);
}

void
nsChildView::MaybeDrawResizeIndicator(GLManager* aManager)
{
  MutexAutoLock lock(mEffectsLock);
  if (!mShowsResizeIndicator) {
    return;
  }

  if (!mResizerImage) {
    mResizerImage = MakeUnique<RectTextureImage>();
  }

  LayoutDeviceIntSize size = mResizeIndicatorRect.Size();
  mResizerImage->UpdateIfNeeded(size, LayoutDeviceIntRegion(), ^(gfx::DrawTarget* drawTarget, const LayoutDeviceIntRegion& updateRegion) {
    ClearRegion(drawTarget, updateRegion);
    gfx::BorrowedCGContext borrow(drawTarget);
    DrawResizer(borrow.cg);
    borrow.Finish();
  });

  mResizerImage->Draw(aManager, mResizeIndicatorRect.TopLeft());
}

// Draw the highlight line at the top of the titlebar.
// This function draws into the current NSGraphicsContext and assumes flippedness.
static void
DrawTitlebarHighlight(NSSize aWindowSize, CGFloat aRadius, CGFloat aDevicePixelWidth)
{
  [NSGraphicsContext saveGraphicsState];

  // Set up the clip path. We start with the outer rectangle and cut out a
  // slightly smaller inner rectangle with rounded corners.
  // The outer corners of the resulting path will be square, but they will be
  // masked away in a later step.
  NSBezierPath* path = [NSBezierPath bezierPath];
  [path setWindingRule:NSEvenOddWindingRule];
  NSRect pathRect = NSMakeRect(0, 0, aWindowSize.width, aRadius + 2);
  [path appendBezierPathWithRect:pathRect];
  pathRect = NSInsetRect(pathRect, aDevicePixelWidth, aDevicePixelWidth);
  CGFloat innerRadius = aRadius - aDevicePixelWidth;
  [path appendBezierPathWithRoundedRect:pathRect xRadius:innerRadius yRadius:innerRadius];
  [path addClip];

  // Now we fill the path with a subtle highlight gradient.
  // We don't use NSGradient because it's 5x to 15x slower than the manual fill,
  // as indicated by the performance test in bug 880620.
  for (CGFloat y = 0; y < aRadius; y += aDevicePixelWidth) {
    CGFloat t = y / aRadius;
    [[NSColor colorWithDeviceWhite:1.0 alpha:0.4 * (1.0 - t)] set];
    NSRectFillUsingOperation(NSMakeRect(0, y, aWindowSize.width, aDevicePixelWidth), NSCompositeSourceOver);
  }

  [NSGraphicsContext restoreGraphicsState];
}

static CGContextRef
CreateCGContext(const LayoutDeviceIntSize& aSize)
{
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx =
    CGBitmapContextCreate(NULL,
                          aSize.width,
                          aSize.height,
                          8 /* bitsPerComponent */,
                          aSize.width * 4,
                          cs,
                          kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst);
  CGColorSpaceRelease(cs);

  CGContextTranslateCTM(ctx, 0, aSize.height);
  CGContextScaleCTM(ctx, 1, -1);
  CGContextSetInterpolationQuality(ctx, kCGInterpolationLow);

  return ctx;
}

LayoutDeviceIntSize
TextureSizeForSize(const LayoutDeviceIntSize& aSize)
{
  return LayoutDeviceIntSize(RoundUpPow2(aSize.width),
                             RoundUpPow2(aSize.height));
}

// When this method is entered, mEffectsLock is already being held.
void
nsChildView::UpdateTitlebarCGContext()
{
  if (mTitlebarRect.IsEmpty()) {
    ReleaseTitlebarCGContext();
    return;
  }

  NSRect titlebarRect = DevPixelsToCocoaPoints(mTitlebarRect);
  NSRect dirtyRect = [mView convertRect:[(BaseWindow*)[mView window] getAndResetNativeDirtyRect] fromView:nil];
  NSRect dirtyTitlebarRect = NSIntersectionRect(titlebarRect, dirtyRect);

  LayoutDeviceIntSize texSize = TextureSizeForSize(mTitlebarRect.Size());
  if (!mTitlebarCGContext ||
      CGBitmapContextGetWidth(mTitlebarCGContext) != size_t(texSize.width) ||
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
  NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:[frameView isFlipped]];
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
    NSButton *button = (NSButton *) view;
    id cellObject = [button cell];
    if (![cellObject isKindOfClass:[NSCell class]]) {
      continue;
    }
    NSCell *cell = (NSCell *) cellObject;

    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, viewFrame.origin.x, viewFrame.origin.y);

    if ([context isFlipped] != [view isFlipped]) {
      CGContextTranslateCTM(ctx, 0, viewFrame.size.height);
      CGContextScaleCTM(ctx, 1, -1);
    }

    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:[view isFlipped]]];

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

  DrawTitlebarHighlight([frameView bounds].size, [(ChildView*)mView cornerRadius],
                        DevPixelsToCocoaPoints(1));

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
void
nsChildView::MaybeDrawTitlebar(GLManager* aManager)
{
  MutexAutoLock lock(mEffectsLock);
  if (!mIsCoveringTitlebar || mIsFullscreen) {
    return;
  }

  LayoutDeviceIntRegion updatedTitlebarRegion;
  updatedTitlebarRegion.And(mUpdatedTitlebarRegion, mTitlebarRect);
  mUpdatedTitlebarRegion.SetEmpty();

  if (!mTitlebarImage) {
    mTitlebarImage = MakeUnique<RectTextureImage>();
  }

  mTitlebarImage->UpdateFromCGContext(mTitlebarRect.Size(),
                                      updatedTitlebarRegion,
                                      mTitlebarCGContext);

  mTitlebarImage->Draw(aManager, mTitlebarRect.TopLeft());
}

static void
DrawTopLeftCornerMask(CGContextRef aCtx, int aRadius)
{
  CGContextSetRGBFillColor(aCtx, 1.0, 1.0, 1.0, 1.0);
  CGContextFillEllipseInRect(aCtx, CGRectMake(0, 0, aRadius * 2, aRadius * 2));
}

void
nsChildView::MaybeDrawRoundedCorners(GLManager* aManager,
                                     const LayoutDeviceIntRect& aRect)
{
  MutexAutoLock lock(mEffectsLock);

  if (!mCornerMaskImage) {
    mCornerMaskImage = MakeUnique<RectTextureImage>();
  }

  LayoutDeviceIntSize size(mDevPixelCornerRadius, mDevPixelCornerRadius);
  mCornerMaskImage->UpdateIfNeeded(size, LayoutDeviceIntRegion(), ^(gfx::DrawTarget* drawTarget, const LayoutDeviceIntRegion& updateRegion) {
    ClearRegion(drawTarget, updateRegion);
    RefPtr<gfx::PathBuilder> builder = drawTarget->CreatePathBuilder();
    builder->Arc(gfx::Point(mDevPixelCornerRadius, mDevPixelCornerRadius), mDevPixelCornerRadius, 0, 2.0f * M_PI);
    RefPtr<gfx::Path> path = builder->Finish();
    drawTarget->Fill(path,
                     gfx::ColorPattern(gfx::Color(1.0, 1.0, 1.0, 1.0)),
                     gfx::DrawOptions(1.0f, gfx::CompositionOp::OP_SOURCE));
  });

  // Use operator destination in: multiply all 4 channels with source alpha.
  aManager->gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_SRC_ALPHA,
                                     LOCAL_GL_ZERO, LOCAL_GL_SRC_ALPHA);

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
  aManager->gl()->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                     LOCAL_GL_ONE, LOCAL_GL_ONE);
}

static int32_t
FindTitlebarBottom(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                   int32_t aWindowWidth)
{
  int32_t titlebarBottom = 0;
  for (uint32_t i = 0; i < aThemeGeometries.Length(); ++i) {
    const nsIWidget::ThemeGeometry& g = aThemeGeometries[i];
    if ((g.mType == nsNativeThemeCocoa::eThemeGeometryTypeTitlebar) &&
        g.mRect.X() <= 0 &&
        g.mRect.XMost() >= aWindowWidth &&
        g.mRect.Y() <= 0) {
      titlebarBottom = std::max(titlebarBottom, g.mRect.YMost());
    }
  }
  return titlebarBottom;
}

static int32_t
FindUnifiedToolbarBottom(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                         int32_t aWindowWidth, int32_t aTitlebarBottom)
{
  int32_t unifiedToolbarBottom = aTitlebarBottom;
  for (uint32_t i = 0; i < aThemeGeometries.Length(); ++i) {
    const nsIWidget::ThemeGeometry& g = aThemeGeometries[i];
    if ((g.mType == nsNativeThemeCocoa::eThemeGeometryTypeToolbar) &&
        g.mRect.X() <= 0 &&
        g.mRect.XMost() >= aWindowWidth &&
        g.mRect.Y() <= aTitlebarBottom) {
      unifiedToolbarBottom = std::max(unifiedToolbarBottom, g.mRect.YMost());
    }
  }
  return unifiedToolbarBottom;
}

static LayoutDeviceIntRect
FindFirstRectOfType(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                    nsITheme::ThemeGeometryType aThemeGeometryType)
{
  for (uint32_t i = 0; i < aThemeGeometries.Length(); ++i) {
    const nsIWidget::ThemeGeometry& g = aThemeGeometries[i];
    if (g.mType == aThemeGeometryType) {
      return g.mRect;
    }
  }
  return LayoutDeviceIntRect();
}

void
nsChildView::UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries)
{
  if (![mView window])
    return;

  UpdateVibrancy(aThemeGeometries);

  if (![[mView window] isKindOfClass:[ToolbarWindow class]])
    return;

  // Update unified toolbar height and sheet attachment position.
  int32_t windowWidth = mBounds.width;
  int32_t titlebarBottom = FindTitlebarBottom(aThemeGeometries, windowWidth);
  int32_t unifiedToolbarBottom =
    FindUnifiedToolbarBottom(aThemeGeometries, windowWidth, titlebarBottom);
  int32_t toolboxBottom =
    FindFirstRectOfType(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeToolbox).YMost();

  ToolbarWindow* win = (ToolbarWindow*)[mView window];
  bool drawsContentsIntoWindowFrame = [win drawsContentsIntoWindowFrame];
  int32_t titlebarHeight = CocoaPointsToDevPixels([win titlebarHeight]);
  int32_t contentOffset = drawsContentsIntoWindowFrame ? titlebarHeight : 0;
  int32_t devUnifiedHeight = titlebarHeight + unifiedToolbarBottom - contentOffset;
  [win setUnifiedToolbarHeight:DevPixelsToCocoaPoints(devUnifiedHeight)];
  int32_t devSheetPosition = titlebarHeight + std::max(toolboxBottom, unifiedToolbarBottom) - contentOffset;
  [win setSheetAttachmentPosition:DevPixelsToCocoaPoints(devSheetPosition)];

  // Update titlebar control offsets.
  LayoutDeviceIntRect windowButtonRect = FindFirstRectOfType(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeWindowButtons);
  [win placeWindowButtons:[mView convertRect:DevPixelsToCocoaPoints(windowButtonRect) toView:nil]];
  LayoutDeviceIntRect fullScreenButtonRect = FindFirstRectOfType(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeFullscreenButton);
  [win placeFullScreenButton:[mView convertRect:DevPixelsToCocoaPoints(fullScreenButtonRect) toView:nil]];
}

static LayoutDeviceIntRegion
GatherThemeGeometryRegion(const nsTArray<nsIWidget::ThemeGeometry>& aThemeGeometries,
                          nsITheme::ThemeGeometryType aThemeGeometryType)
{
  LayoutDeviceIntRegion region;
  for (size_t i = 0; i < aThemeGeometries.Length(); ++i) {
    const nsIWidget::ThemeGeometry& g = aThemeGeometries[i];
    if (g.mType == aThemeGeometryType) {
      region.OrWith(g.mRect);
    }
  }
  return region;
}

template<typename Region>
static void MakeRegionsNonOverlappingImpl(Region& aOutUnion) { }

template<typename Region, typename ... Regions>
static void MakeRegionsNonOverlappingImpl(Region& aOutUnion, Region& aFirst, Regions& ... aRest)
{
  MakeRegionsNonOverlappingImpl(aOutUnion, aRest...);
  aFirst.SubOut(aOutUnion);
  aOutUnion.OrWith(aFirst);
}

// Subtracts parts from regions in such a way that they don't have any overlap.
// Each region in the argument list will have the union of all the regions
// *following* it subtracted from itself. In other words, the arguments are
// sorted low priority to high priority.
template<typename Region, typename ... Regions>
static void MakeRegionsNonOverlapping(Region& aFirst, Regions& ... aRest)
{
  Region unionOfAll;
  MakeRegionsNonOverlappingImpl(unionOfAll, aFirst, aRest...);
}

void
nsChildView::UpdateVibrancy(const nsTArray<ThemeGeometry>& aThemeGeometries)
{
  if (!VibrancyManager::SystemSupportsVibrancy()) {
    return;
  }

  LayoutDeviceIntRegion sheetRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeSheet);
  LayoutDeviceIntRegion vibrantLightRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeVibrancyLight);
  LayoutDeviceIntRegion vibrantDarkRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeVibrancyDark);
  LayoutDeviceIntRegion menuRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeMenu);
  LayoutDeviceIntRegion tooltipRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeTooltip);
  LayoutDeviceIntRegion highlightedMenuItemRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeHighlightedMenuItem);
  LayoutDeviceIntRegion sourceListRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeSourceList);
  LayoutDeviceIntRegion sourceListSelectionRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeSourceListSelection);
  LayoutDeviceIntRegion activeSourceListSelectionRegion =
    GatherThemeGeometryRegion(aThemeGeometries, nsNativeThemeCocoa::eThemeGeometryTypeActiveSourceListSelection);

  MakeRegionsNonOverlapping(sheetRegion, vibrantLightRegion, vibrantDarkRegion,
                            menuRegion, tooltipRegion, highlightedMenuItemRegion,
                            sourceListRegion, sourceListSelectionRegion,
                            activeSourceListSelectionRegion);

  auto& vm = EnsureVibrancyManager();
  vm.UpdateVibrantRegion(VibrancyType::LIGHT, vibrantLightRegion);
  vm.UpdateVibrantRegion(VibrancyType::TOOLTIP, tooltipRegion);
  vm.UpdateVibrantRegion(VibrancyType::MENU, menuRegion);
  vm.UpdateVibrantRegion(VibrancyType::HIGHLIGHTED_MENUITEM, highlightedMenuItemRegion);
  vm.UpdateVibrantRegion(VibrancyType::SHEET, sheetRegion);
  vm.UpdateVibrantRegion(VibrancyType::SOURCE_LIST, sourceListRegion);
  vm.UpdateVibrantRegion(VibrancyType::SOURCE_LIST_SELECTION, sourceListSelectionRegion);
  vm.UpdateVibrantRegion(VibrancyType::ACTIVE_SOURCE_LIST_SELECTION, activeSourceListSelectionRegion);
  vm.UpdateVibrantRegion(VibrancyType::DARK, vibrantDarkRegion);
}

void
nsChildView::ClearVibrantAreas()
{
  if (VibrancyManager::SystemSupportsVibrancy()) {
    EnsureVibrancyManager().ClearVibrantAreas();
  }
}

static VibrancyType
ThemeGeometryTypeToVibrancyType(nsITheme::ThemeGeometryType aThemeGeometryType)
{
  switch (aThemeGeometryType) {
    case nsNativeThemeCocoa::eThemeGeometryTypeVibrancyLight:
      return VibrancyType::LIGHT;
    case nsNativeThemeCocoa::eThemeGeometryTypeVibrancyDark:
      return VibrancyType::DARK;
    case nsNativeThemeCocoa::eThemeGeometryTypeTooltip:
      return VibrancyType::TOOLTIP;
    case nsNativeThemeCocoa::eThemeGeometryTypeMenu:
      return VibrancyType::MENU;
    case nsNativeThemeCocoa::eThemeGeometryTypeHighlightedMenuItem:
      return VibrancyType::HIGHLIGHTED_MENUITEM;
    case nsNativeThemeCocoa::eThemeGeometryTypeSheet:
      return VibrancyType::SHEET;
    case nsNativeThemeCocoa::eThemeGeometryTypeSourceList:
      return VibrancyType::SOURCE_LIST;
    case nsNativeThemeCocoa::eThemeGeometryTypeSourceListSelection:
      return VibrancyType::SOURCE_LIST_SELECTION;
    case nsNativeThemeCocoa::eThemeGeometryTypeActiveSourceListSelection:
      return VibrancyType::ACTIVE_SOURCE_LIST_SELECTION;
    default:
      MOZ_CRASH();
  }
}

NSColor*
nsChildView::VibrancyFillColorForThemeGeometryType(nsITheme::ThemeGeometryType aThemeGeometryType)
{
  if (VibrancyManager::SystemSupportsVibrancy()) {
    return EnsureVibrancyManager().VibrancyFillColorForType(
      ThemeGeometryTypeToVibrancyType(aThemeGeometryType));
  }
  return [NSColor whiteColor];
}

NSColor*
nsChildView::VibrancyFontSmoothingBackgroundColorForThemeGeometryType(nsITheme::ThemeGeometryType aThemeGeometryType)
{
  if (VibrancyManager::SystemSupportsVibrancy()) {
    return EnsureVibrancyManager().VibrancyFontSmoothingBackgroundColorForType(
      ThemeGeometryTypeToVibrancyType(aThemeGeometryType));
  }
  return [NSColor clearColor];
}

mozilla::VibrancyManager&
nsChildView::EnsureVibrancyManager()
{
  MOZ_ASSERT(mView, "Only call this once we have a view!");
  if (!mVibrancyManager) {
    mVibrancyManager = MakeUnique<VibrancyManager>(*this, mView);
  }
  return *mVibrancyManager;
}

nsChildView::SwipeInfo
nsChildView::SendMayStartSwipe(const mozilla::PanGestureInput& aSwipeStartEvent)
{
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  uint32_t direction = (aSwipeStartEvent.mPanDisplacement.x > 0.0)
    ? (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_RIGHT
    : (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_LEFT;

  // We're ready to start the animation. Tell Gecko about it, and at the same
  // time ask it if it really wants to start an animation for this event.
  // This event also reports back the directions that we can swipe in.
  LayoutDeviceIntPoint position =
    RoundedToInt(aSwipeStartEvent.mPanStartPoint * ScreenToLayoutDeviceScale(1));
  WidgetSimpleGestureEvent geckoEvent =
    SwipeTracker::CreateSwipeGestureEvent(eSwipeGestureMayStart, this,
                                          position,
                                          aSwipeStartEvent.mTimeStamp);
  geckoEvent.mDirection = direction;
  geckoEvent.mDelta = 0.0;
  geckoEvent.mAllowedDirections = 0;
  bool shouldStartSwipe = DispatchWindowEvent(geckoEvent); // event cancelled == swipe should start

  SwipeInfo result = { shouldStartSwipe, geckoEvent.mAllowedDirections };
  return result;
}

void
nsChildView::TrackScrollEventAsSwipe(const mozilla::PanGestureInput& aSwipeStartEvent,
                                     uint32_t aAllowedDirections)
{
  // If a swipe is currently being tracked kill it -- it's been interrupted
  // by another gesture event.
  if (mSwipeTracker) {
    mSwipeTracker->CancelSwipe(aSwipeStartEvent.mTimeStamp);
    mSwipeTracker->Destroy();
    mSwipeTracker = nullptr;
  }

  uint32_t direction = (aSwipeStartEvent.mPanDisplacement.x > 0.0)
    ? (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_RIGHT
    : (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_LEFT;

  mSwipeTracker = new SwipeTracker(*this, aSwipeStartEvent,
                                   aAllowedDirections, direction);

  if (!mAPZC) {
    mCurrentPanGestureBelongsToSwipe = true;
  }
}

void
nsChildView::SwipeFinished()
{
  mSwipeTracker = nullptr;
}

already_AddRefed<gfx::DrawTarget>
nsChildView::StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                                        BufferMode* aBufferMode)
{
  // should have created the GLPresenter in InitCompositor.
  MOZ_ASSERT(mGLPresenter);
  if (!mGLPresenter) {
    mGLPresenter = GLPresenter::CreateForWindow(this);

    if (!mGLPresenter) {
      return nullptr;
    }
  }

  LayoutDeviceIntRegion dirtyRegion(aInvalidRegion);
  LayoutDeviceIntSize renderSize = mBounds.Size();

  if (!mBasicCompositorImage) {
    mBasicCompositorImage = MakeUnique<RectTextureImage>();
  }

  RefPtr<gfx::DrawTarget> drawTarget =
    mBasicCompositorImage->BeginUpdate(renderSize, dirtyRegion);

  if (!drawTarget) {
    // Composite unchanged textures.
    DoRemoteComposition(mBounds);
    return nullptr;
  }

  aInvalidRegion = mBasicCompositorImage->GetUpdateRegion();
  *aBufferMode = BufferMode::BUFFER_NONE;

  return drawTarget.forget();
}

void
nsChildView::EndRemoteDrawing()
{
  mBasicCompositorImage->EndUpdate();
  DoRemoteComposition(mBounds);
}

void
nsChildView::CleanupRemoteDrawing()
{
  mBasicCompositorImage = nullptr;
  mCornerMaskImage = nullptr;
  mResizerImage = nullptr;
  mTitlebarImage = nullptr;
  mGLPresenter = nullptr;
}

bool
nsChildView::InitCompositor(Compositor* aCompositor)
{
  if (aCompositor->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    if (!mGLPresenter) {
      mGLPresenter = GLPresenter::CreateForWindow(this);
    }

    return !!mGLPresenter;
  }
  return true;
}

void
nsChildView::DoRemoteComposition(const LayoutDeviceIntRect& aRenderRect)
{
  if (![(ChildView*)mView preRender:mGLPresenter->GetNSOpenGLContext()]) {
    return;
  }
  mGLPresenter->BeginFrame(aRenderRect.Size());

  // Draw the result from the basic compositor.
  mBasicCompositorImage->Draw(mGLPresenter.get(), LayoutDeviceIntPoint(0, 0));

  // DrawWindowOverlay doesn't do anything for non-GL, so it didn't paint
  // anything during the basic compositor transaction. Draw the overlay now.
  DrawWindowOverlay(mGLPresenter.get(), aRenderRect);

  mGLPresenter->EndFrame();

  [(ChildView*)mView postRender:mGLPresenter->GetNSOpenGLContext()];
}

@interface NonDraggableView : NSView
@end

@implementation NonDraggableView
- (BOOL)mouseDownCanMoveWindow { return NO; }
- (NSView*)hitTest:(NSPoint)aPoint { return nil; }
@end

void
nsChildView::UpdateWindowDraggingRegion(const LayoutDeviceIntRegion& aRegion)
{
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
    changed = mNonDraggableRegion.UpdateRegion(nonDraggable, *this, mView, ^() {
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

void
nsChildView::ReportSwipeStarted(uint64_t aInputBlockId,
                                bool aStartSwipe)
{
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

void
nsChildView::DispatchAPZWheelInputEvent(InputData& aEvent, bool aCanTriggerSwipe)
{
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
        result = mAPZC->ReceiveInputEvent(aEvent, &guid, &inputBlockId);
        if (result == nsEventStatus_eConsumeNoDefault) {
          return;
        }

        PanGestureInput& panInput = aEvent.AsPanGestureInput();

        event = panInput.ToWidgetWheelEvent(this);
        if (aCanTriggerSwipe) {
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
              mSwipeEventQueue = MakeUnique<SwipeEventQueue>(swipeInfo.allowedDirections,
                                                             inputBlockId);
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
        // variant of ReceiveInputEvent, because the IAPZCTreeManager version of
        // that function has special handling (for delta multipliers etc.) that
        // we need to run. Using the InputData variant would bypass that and
        // go straight to the APZCTreeManager subclass.
        event = aEvent.AsScrollWheelInput().ToWidgetWheelEvent(this);
        result = mAPZC->ReceiveInputEvent(event, &guid, &inputBlockId);
        if (result == nsEventStatus_eConsumeNoDefault) {
          return;
        }
        break;
      };
      default:
        MOZ_CRASH("unsupported event type");
        return;
    }
    if (event.mMessage == eWheel &&
        (event.mDeltaX != 0 || event.mDeltaY != 0)) {
      ProcessUntransformedAPZEvent(&event, guid, inputBlockId, result);
    }
    return;
  }

  nsEventStatus status;
  switch(aEvent.mInputType) {
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
          "If the fingers are still on the touchpad, we should still have a SwipeTracker, and it should have consumed this event.");
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
  if (event.mMessage == eWheel &&
      (event.mDeltaX != 0 || event.mDeltaY != 0)) {
    DispatchEvent(&event, status);
  }
}

// When using 10.11, calling showDefinitionForAttributedString causes the
// following exception on LookupViewService. (rdar://26476091)
//
// Exception: decodeObjectForKey: class "TitlebarAndBackgroundColor" not
// loaded or does not exist
//
// So we set temporary color that is NSColor before calling it.

class MOZ_RAII AutoBackgroundSetter final {
public:
  explicit AutoBackgroundSetter(NSView* aView) {
    if (nsCocoaFeatures::OnElCapitanOrLater() &&
        [[aView window] isKindOfClass:[ToolbarWindow class]]) {
      mWindow = [(ToolbarWindow*)[aView window] retain];
      [mWindow setTemporaryBackgroundColor];
    } else {
      mWindow = nullptr;
    }
  }

  ~AutoBackgroundSetter() {
    if (mWindow) {
      [mWindow restoreBackgroundColor];
      [mWindow release];
    }
  }

private:
  ToolbarWindow* mWindow; // strong
};

void
nsChildView::LookUpDictionary(
               const nsAString& aText,
               const nsTArray<mozilla::FontRange>& aFontRangeArray,
               const bool aIsVertical,
               const LayoutDeviceIntPoint& aPoint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSMutableAttributedString* attrStr =
    nsCocoaUtils::GetNSMutableAttributedString(aText, aFontRangeArray,
                                               aIsVertical,
                                               BackingScaleFactor());
  NSPoint pt =
    nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());
  NSDictionary* attributes = [attrStr attributesAtIndex:0 effectiveRange:nil];
  NSFont* font = [attributes objectForKey:NSFontAttributeName];
  if (font) {
    if (aIsVertical) {
      pt.x -= [font descender];
    } else {
      pt.y += [font ascender];
    }
  }

  AutoBackgroundSetter setter(mView);
  [mView showDefinitionForAttributedString:attrStr atPoint:pt];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#ifdef ACCESSIBILITY
already_AddRefed<a11y::Accessible>
nsChildView::GetDocumentAccessible()
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return nullptr;

  if (mAccessible) {
    RefPtr<a11y::Accessible> ret;
    CallQueryReferent(mAccessible.get(),
                      static_cast<a11y::Accessible**>(getter_AddRefs(ret)));
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

GLPresenter::GLPresenter(GLContext* aContext)
 : mGLContext(aContext)
{
  mGLContext->MakeCurrent();
  ShaderConfigOGL config;
  config.SetTextureTarget(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
  mRGBARectProgram = MakeUnique<ShaderProgramOGL>(mGLContext,
    ProgramProfileOGL::GetProfileFor(config));

  // Create mQuadVBO.
  mGLContext->fGenBuffers(1, &mQuadVBO);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

  // 1 quad, with the number of the quad (vertexID) encoded in w.
  GLfloat vertices[] = {
    0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 0.0f,
  };
  HeapCopyOfStackArray<GLfloat> verticesOnHeap(vertices);
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER,
                          verticesOnHeap.ByteLength(),
                          verticesOnHeap.Data(),
                          LOCAL_GL_STATIC_DRAW);
   mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

GLPresenter::~GLPresenter()
{
  if (mQuadVBO) {
    mGLContext->MakeCurrent();
    mGLContext->fDeleteBuffers(1, &mQuadVBO);
    mQuadVBO = 0;
  }
}

void
GLPresenter::BindAndDrawQuad(ShaderProgramOGL *aProgram,
                             const gfx::Rect& aLayerRect,
                             const gfx::Rect& aTextureRect)
{
  mGLContext->MakeCurrent();

  gfx::Rect layerRects[4];
  gfx::Rect textureRects[4];

  layerRects[0] = aLayerRect;
  textureRects[0] = aTextureRect;

  aProgram->SetLayerRects(layerRects);
  aProgram->SetTextureRects(textureRects);

  const GLuint coordAttribIndex = 0;

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  mGLContext->fVertexAttribPointer(coordAttribIndex, 4,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   (GLvoid*)0);
  mGLContext->fEnableVertexAttribArray(coordAttribIndex);
  mGLContext->fDrawArrays(LOCAL_GL_TRIANGLES, 0, 6);
  mGLContext->fDisableVertexAttribArray(coordAttribIndex);
}

void
GLPresenter::BeginFrame(LayoutDeviceIntSize aRenderSize)
{
  mGLContext->MakeCurrent();

  mGLContext->fViewport(0, 0, aRenderSize.width, aRenderSize.height);

  // Matrix to transform (0, 0, width, height) to viewport space (-1.0, 1.0,
  // 2, 2) and flip the contents.
  gfx::Matrix viewMatrix = gfx::Matrix::Translation(-1.0, 1.0);
  viewMatrix.PreScale(2.0f / float(aRenderSize.width),
                      2.0f / float(aRenderSize.height));
  viewMatrix.PreScale(1.0f, -1.0f);

  gfx::Matrix4x4 matrix3d = gfx::Matrix4x4::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  // set the projection matrix for the next time the program is activated
  mProjMatrix = matrix3d;

  // Default blend function implements "OVER"
  mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ONE_MINUS_SRC_ALPHA,
                                 LOCAL_GL_ONE, LOCAL_GL_ONE);
  mGLContext->fEnable(LOCAL_GL_BLEND);

  mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  mGLContext->fEnable(LOCAL_GL_TEXTURE_RECTANGLE_ARB);
}

void
GLPresenter::EndFrame()
{
  mGLContext->SwapBuffers();
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

class WidgetsReleaserRunnable final : public mozilla::Runnable
{
public:
  explicit WidgetsReleaserRunnable(nsTArray<nsCOMPtr<nsIWidget>>&& aWidgetArray)
    : mozilla::Runnable("WidgetsReleaserRunnable"), mWidgetArray(aWidgetArray)
  {
  }

  // Do nothing; all this runnable does is hold a reference the widgets in
  // mWidgetArray, and those references will be dropped when this runnable
  // is destroyed.

private:
  nsTArray<nsCOMPtr<nsIWidget>> mWidgetArray;
};

#pragma mark -

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

+ (void)initialize
{
  static BOOL initialized = NO;

  if (!initialized) {
    // Inform the OS about the types of services (from the "Services" menu)
    // that we can handle.
    NSArray* types = @[[UTIHelper stringFromPboardType:NSPasteboardTypeString],
                       [UTIHelper stringFromPboardType:NSPasteboardTypeHTML]];
    [NSApp registerServicesMenuSendTypes:types returnTypes:types];
    initialized = YES;
  }
}

+ (void)registerViewForDraggedTypes:(NSView*)aView
{
  [aView registerForDraggedTypes:
    [NSArray arrayWithObjects:
      [UTIHelper stringFromPboardType:NSFilenamesPboardType],
      [UTIHelper stringFromPboardType:kMozFileUrlsPboardType],
      [UTIHelper stringFromPboardType:NSPasteboardTypeString],
      [UTIHelper stringFromPboardType:NSPasteboardTypeHTML],
      [UTIHelper stringFromPboardType:(NSString*)kPasteboardTypeFileURLPromise],
      [UTIHelper stringFromPboardType:kMozWildcardPboardType],
      [UTIHelper stringFromPboardType:kPublicUrlPboardType],
      [UTIHelper stringFromPboardType:kPublicUrlNamePboardType],
      [UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType],
      nil]];
}

// initWithFrame:geckoChild:
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super initWithFrame:inFrame])) {
    mGeckoChild = inChild;
    mPendingDisplay = NO;
    mBlockedLastMouseDown = NO;
    mExpectingWheelStop = NO;

    mLastMouseDownEvent = nil;
    mLastKeyDownEvent = nil;
    mClickThroughMouseDownEvent = nil;
    mDragService = nullptr;

    mGestureState = eGestureState_None;
    mCumulativeMagnification = 0.0;
    mCumulativeRotation = 0.0;

    // We can't call forceRefreshOpenGL here because, in order to work around
    // the bug, it seems we need to have a draw already happening. Therefore,
    // we call it in drawRect:inContext:, when we know that a draw is in
    // progress.
    mDidForceRefreshOpenGL = NO;

    mNeedsGLUpdate = NO;

    [self setFocusRingType:NSFocusRingTypeNone];

#ifdef __LP64__
    mCancelSwipeAnimation = nil;
#endif

    mTopLeftCornerMask = NULL;
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
  // TODO: replace the string with the constant once we build with the 10.7 SDK
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(scrollbarSystemMetricChanged)
                                               name:@"NSPreferredScrollerStyleDidChangeNotification"
                                             object:nil];
  [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                      selector:@selector(systemMetricsChanged)
                                                          name:@"AppleAquaScrollBarVariantChanged"
                                                        object:nil
                                            suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(_surfaceNeedsUpdate:)
                                               name:NSViewGlobalFrameDidChangeNotification
                                             object:self];

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// ComplexTextInputPanel's interpretKeyEvent hack won't work without this.
// It makes calls to +[NSTextInputContext currentContext], deep in system
// code, return the appropriate context.
- (NSTextInputContext *)inputContext
{
  NSTextInputContext* pluginContext = NULL;
  if (mGeckoChild && mGeckoChild->IsPluginFocused()) {
    ComplexTextInputPanel* ctiPanel =
      ComplexTextInputPanel::GetSharedComplexTextInputPanel();
    if (ctiPanel) {
      pluginContext = (NSTextInputContext*) ctiPanel->GetInputContext();
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

- (void)installTextInputHandler:(TextInputHandler*)aHandler
{
  mTextInputHandler = aHandler;
}

- (void)uninstallTextInputHandler
{
  mTextInputHandler = nullptr;
}

// Work around bug 603134.
// OS X has a bug that causes new OpenGL windows to only paint once or twice,
// then stop painting altogether. By clearing the drawable from the GL context,
// and then resetting the view to ourselves, we convince OS X to start updating
// again.
// This can cause a flash in new windows - bug 631339 - but it's very hard to
// fix that while maintaining this workaround.
- (void)forceRefreshOpenGL
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mGLContext clearDrawable];
  CGLLockContext((CGLContextObj)[mGLContext CGLContextObj]);
  [self updateGLContext];
  CGLUnlockContext((CGLContextObj)[mGLContext CGLContextObj]);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (bool)preRender:(NSOpenGLContext *)aGLContext
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (![self window] ||
      ([[self window] isKindOfClass:[BaseWindow class]] &&
       ![(BaseWindow*)[self window] isVisibleOrBeingShown])) {
    // Before the window is shown, our GL context's front FBO is not
    // framebuffer complete, so we refuse to render.
    return false;
  }

  if (!mGLContext) {
    mGLContext = aGLContext;
    [mGLContext retain];
    mNeedsGLUpdate = true;
  }

  CGLLockContext((CGLContextObj)[aGLContext CGLContextObj]);

  if (mNeedsGLUpdate) {
    [self updateGLContext];
    mNeedsGLUpdate = NO;
  }

  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

- (void)postRender:(NSOpenGLContext *)aGLContext
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  CGLUnlockContext((CGLContextObj)[aGLContext CGLContextObj]);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mGLContext release];
  [mPendingDirtyRects release];
  [mLastMouseDownEvent release];
  [mLastKeyDownEvent release];
  [mClickThroughMouseDownEvent release];
  CGImageRelease(mTopLeftCornerMask);
  ChildViewMouseTracker::OnDestroyView(self);

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)widgetDestroyed
{
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
- (nsIWidget*) widget
{
  return static_cast<nsIWidget*>(mGeckoChild);
}

- (void)systemMetricsChanged
{
  if (mGeckoChild)
    mGeckoChild->NotifyThemeChanged();
}

- (void)scrollbarSystemMetricChanged
{
  [self systemMetricsChanged];

  if (mGeckoChild) {
    nsIWidgetListener* listener = mGeckoChild->GetWidgetListener();
    if (listener) {
      nsIPresShell* presShell = listener->GetPresShell();
      if (presShell) {
        presShell->ReconstructFrames();
      }
    }
  }
}

- (void)setNeedsPendingDisplay
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mPendingFullDisplay = YES;
  if (!mPendingDisplay) {
    [self performSelector:@selector(processPendingRedraws) withObject:nil afterDelay:0];
    mPendingDisplay = YES;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setNeedsPendingDisplayInRect:(NSRect)invalidRect
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mPendingDirtyRects)
    mPendingDirtyRects = [[NSMutableArray alloc] initWithCapacity:1];
  [mPendingDirtyRects addObject:[NSValue valueWithRect:invalidRect]];
  if (!mPendingDisplay) {
    [self performSelector:@selector(processPendingRedraws) withObject:nil afterDelay:0];
    mPendingDisplay = YES;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Clears the queue of any pending invalides
- (void)processPendingRedraws
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPendingFullDisplay) {
    [self setNeedsDisplay:YES];
  }
  else if (mPendingDirtyRects) {
    unsigned int count = [mPendingDirtyRects count];
    for (unsigned int i = 0; i < count; ++i) {
      [self setNeedsDisplayInRect:[[mPendingDirtyRects objectAtIndex:i] rectValue]];
    }
  }
  mPendingFullDisplay = NO;
  mPendingDisplay = NO;
  [mPendingDirtyRects release];
  mPendingDirtyRects = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setNeedsDisplayInRect:(NSRect)aRect
{
  if (![self isUsingOpenGL]) {
    [super setNeedsDisplayInRect:aRect];
    return;
  }

  if ([[self window] isVisible] && [self isUsingMainThreadOpenGL]) {
    // Draw without calling drawRect. This prevent us from
    // needing to access the normal window buffer surface unnecessarily, so we
    // waste less time synchronizing the two surfaces. (These synchronizations
    // show up in a profiler as CGSDeviceLock / _CGSLockWindow /
    // _CGSSynchronizeWindowBackingStore.) It also means that Cocoa doesn't
    // have any potentially expensive invalid rect management for us.
    if (!mWaitingForPaint) {
      mWaitingForPaint = YES;
      // Use NSRunLoopCommonModes instead of the default NSDefaultRunLoopMode
      // so that the timer also fires while a native menu is open.
      [self performSelector:@selector(drawUsingOpenGLCallback)
                 withObject:nil
                 afterDelay:0
                    inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
    }
  }
}

- (NSString*)description
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSString stringWithFormat:@"ChildView %p, gecko child %p, frame %@", self, mGeckoChild, NSStringFromRect([self frame])];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Make the origin of this view the topLeft corner (gecko origin) rather
// than the bottomLeft corner (standard cocoa origin).
- (BOOL)isFlipped
{
  return YES;
}

- (BOOL)isOpaque
{
  return [[self window] isOpaque];
}

// XXX Is this really used?
- (void)sendFocusEvent:(EventMessage)eventMessage
{
  if (!mGeckoChild)
    return;

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetGUIEvent focusGuiEvent(true, eventMessage, mGeckoChild);
  focusGuiEvent.mTime = PR_IntervalNow();
  focusGuiEvent.mTimeStamp = nsCocoaUtils::GetEventTimeStamp(0);
  mGeckoChild->DispatchEvent(&focusGuiEvent, status);
}

// We accept key and mouse events, so don't keep passing them up the chain. Allow
// this to be a 'focused' widget for event dispatch.
- (BOOL)acceptsFirstResponder
{
  return YES;
}

// Accept mouse down events on background windows
- (BOOL)acceptsFirstMouse:(NSEvent*)aEvent
{
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

- (void)scrollRect:(NSRect)aRect by:(NSSize)offset
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Update any pending dirty rects to reflect the new scroll position
  if (mPendingDirtyRects) {
    unsigned int count = [mPendingDirtyRects count];
    for (unsigned int i = 0; i < count; ++i) {
      NSRect oldRect = [[mPendingDirtyRects objectAtIndex:i] rectValue];
      NSRect newRect = NSOffsetRect(oldRect, offset.width, offset.height);
      [mPendingDirtyRects replaceObjectAtIndex:i
                                    withObject:[NSValue valueWithRect:newRect]];
    }
  }
  [super scrollRect:aRect by:offset];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)mouseDownCanMoveWindow
{
  // Return YES so that parts of this view can be draggable. The non-draggable
  // parts will be covered by NSViews that return NO from
  // mouseDownCanMoveWindow and thus override draggability from the inside.
  // These views are assembled in nsChildView::UpdateWindowDraggingRegion.
  return YES;
}

-(void)updateGLContext
{
  [mGLContext setView:self];
  [mGLContext update];
}

- (void)_surfaceNeedsUpdate:(NSNotification*)notification
{
  if (mGLContext) {
    CGLLockContext((CGLContextObj)[mGLContext CGLContextObj]);
    mNeedsGLUpdate = YES;
    CGLUnlockContext((CGLContextObj)[mGLContext CGLContextObj]);
  }
}

- (BOOL)wantsBestResolutionOpenGLSurface
{
  return nsCocoaUtils::HiDPIEnabled() ? YES : NO;
}

- (void)viewDidChangeBackingProperties
{
  [super viewDidChangeBackingProperties];
  if (mGeckoChild) {
    // actually, it could be the color space that's changed,
    // but we can't tell the difference here except by retrieving
    // the backing scale factor and comparing to the old value
    mGeckoChild->BackingScaleFactorChanged();
  }
}

- (BOOL)isCoveringTitlebar
{
  return [[self window] isKindOfClass:[BaseWindow class]] &&
         [(BaseWindow*)[self window] mainChildView] == self &&
         [(BaseWindow*)[self window] drawsContentsIntoWindowFrame];
}

- (void)viewWillStartLiveResize
{
  nsCocoaWindow* windowWidget = mGeckoChild ? mGeckoChild->GetXULWindowWidget() : nullptr;
  if (windowWidget) {
    windowWidget->NotifyLiveResizeStarted();
  }
}

- (void)viewDidEndLiveResize
{
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

- (NSColor*)vibrancyFillColorForThemeGeometryType:(nsITheme::ThemeGeometryType)aThemeGeometryType
{
  if (!mGeckoChild) {
    return [NSColor whiteColor];
  }
  return mGeckoChild->VibrancyFillColorForThemeGeometryType(aThemeGeometryType);
}

- (NSColor*)vibrancyFontSmoothingBackgroundColorForThemeGeometryType:(nsITheme::ThemeGeometryType)aThemeGeometryType
{
  if (!mGeckoChild) {
    return [NSColor clearColor];
  }
  return mGeckoChild->VibrancyFontSmoothingBackgroundColorForThemeGeometryType(aThemeGeometryType);
}

- (LayoutDeviceIntRegion)nativeDirtyRegionWithBoundingRect:(NSRect)aRect
{
  LayoutDeviceIntRect boundingRect = mGeckoChild->CocoaPointsToDevPixels(aRect);
  const NSRect *rects;
  NSInteger count;
  [self getRectsBeingDrawn:&rects count:&count];

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
- (void)drawRect:(NSRect)aRect
{
  CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  [self drawRect:aRect inContext:cgContext];
}

- (void)drawRect:(NSRect)aRect inContext:(CGContextRef)aContext
{
  if (!mGeckoChild || !mGeckoChild->IsVisible())
    return;

#ifdef DEBUG_UPDATE
  LayoutDeviceIntRect geckoBounds = mGeckoChild->GetBounds();

  fprintf (stderr, "---- Update[%p][%p] [%f %f %f %f] cgc: %p\n  gecko bounds: [%d %d %d %d]\n",
           self, mGeckoChild,
           aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height, aContext,
           geckoBounds.x, geckoBounds.y, geckoBounds.width, geckoBounds.height);

  CGAffineTransform xform = CGContextGetCTM(aContext);
  fprintf (stderr, "  xform in: [%f %f %f %f %f %f]\n", xform.a, xform.b, xform.c, xform.d, xform.tx, xform.ty);
#endif

  if ([self isUsingOpenGL]) {
    // For Gecko-initiated repaints in OpenGL mode, drawUsingOpenGL is
    // directly called from a delayed perform callback - without going through
    // drawRect.
    // Paints that come through here are triggered by something that Cocoa
    // controls, for example by window resizing or window focus changes.

    // Since this view is usually declared as opaque, the window's pixel
    // buffer may now contain garbage which we need to prevent from reaching
    // the screen. The only place where garbage can show is in the window
    // corners and the vibrant regions of the window - the rest of the window
    // is covered by opaque content in our OpenGL surface.
    // So we need to clear the pixel buffer contents in these areas.
    mGeckoChild->ClearVibrantAreas();
    [self clearCorners];

    // Do GL composition and return.
    [self drawUsingOpenGL];
    return;
  }

  AUTO_PROFILER_LABEL("ChildView::drawRect", GRAPHICS);

  // The CGContext that drawRect supplies us with comes with a transform that
  // scales one user space unit to one Cocoa point, which can consist of
  // multiple dev pixels. But Gecko expects its supplied context to be scaled
  // to device pixels, so we need to reverse the scaling.
  double scale = mGeckoChild->BackingScaleFactor();
  CGContextSaveGState(aContext);
  CGContextScaleCTM(aContext, 1.0 / scale, 1.0 / scale);

  NSSize viewSize = [self bounds].size;
  gfx::IntSize backingSize = gfx::IntSize::Truncate(viewSize.width * scale, viewSize.height * scale);
  LayoutDeviceIntRegion region = [self nativeDirtyRegionWithBoundingRect:aRect];

  bool painted = mGeckoChild->PaintWindowInContext(aContext, region, backingSize);

  // Undo the scale transform so that from now on the context is in
  // CocoaPoints again.
  CGContextRestoreGState(aContext);

  if (!painted && [self isOpaque]) {
    // Gecko refused to draw, but we've claimed to be opaque, so we have to
    // draw something--fill with white.
    CGContextSetRGBFillColor(aContext, 1, 1, 1, 1);
    CGContextFillRect(aContext, NSRectToCGRect(aRect));
  }

  if ([self isCoveringTitlebar]) {
    [self drawTitleString];
    [self drawTitlebarHighlight];
    [self maskTopCornersInContext:aContext];
  }

#ifdef DEBUG_UPDATE
  fprintf (stderr, "---- update done ----\n");

#if 0
  CGContextSetRGBStrokeColor (aContext,
                            ((((unsigned long)self) & 0xff)) / 255.0,
                            ((((unsigned long)self) & 0xff00) >> 8) / 255.0,
                            ((((unsigned long)self) & 0xff0000) >> 16) / 255.0,
                            0.5);
#endif
  CGContextSetRGBStrokeColor(aContext, 1, 0, 0, 0.8);
  CGContextSetLineWidth(aContext, 4.0);
  CGContextStrokeRect(aContext, NSRectToCGRect(aRect));
#endif
}

- (BOOL)isUsingMainThreadOpenGL
{
  if (!mGeckoChild || ![self window])
    return NO;

  return mGeckoChild->GetLayerManager(nullptr)->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_OPENGL;
}

- (BOOL)isUsingOpenGL
{
  if (!mGeckoChild || ![self window])
    return NO;

  return mGLContext || mUsingOMTCompositor || [self isUsingMainThreadOpenGL];
}

- (void)drawUsingOpenGL
{
  AUTO_PROFILER_LABEL("ChildView::drawUsingOpenGL", GRAPHICS);

  if (![self isUsingOpenGL] || !mGeckoChild->IsVisible())
    return;

  mWaitingForPaint = NO;

  LayoutDeviceIntRect geckoBounds = mGeckoChild->GetBounds();
  LayoutDeviceIntRegion region(geckoBounds);

  mGeckoChild->PaintWindow(region);

  // Force OpenGL to refresh the very first time we draw. This works around a
  // Mac OS X bug that stops windows updating on OS X when we use OpenGL.
  if (!mDidForceRefreshOpenGL) {
    [self performSelector:@selector(forceRefreshOpenGL) withObject:nil afterDelay:0];
    mDidForceRefreshOpenGL = YES;
  }
}

// Called asynchronously after setNeedsDisplay in order to avoid entering the
// normal drawing machinery.
- (void)drawUsingOpenGLCallback
{
  if (mWaitingForPaint) {
    [self drawUsingOpenGL];
  }
}

- (BOOL)hasRoundedBottomCorners
{
  return [[self window] respondsToSelector:@selector(bottomCornerRounded)] &&
  [[self window] bottomCornerRounded];
}

- (CGFloat)cornerRadius
{
  NSView* frameView = [[[self window] contentView] superview];
  if (!frameView || ![frameView respondsToSelector:@selector(roundedCornerRadius)])
    return 4.0f;
  return [frameView roundedCornerRadius];
}

-(void)setGLOpaque:(BOOL)aOpaque
{
  CGLLockContext((CGLContextObj)[mGLContext CGLContextObj]);
  // Make the context opaque for fullscreen (since it performs better), and transparent
  // for windowed (since we need it for rounded corners).
  GLint opaque = aOpaque ? 1 : 0;
  [mGLContext setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];
  CGLUnlockContext((CGLContextObj)[mGLContext CGLContextObj]);
}

// Accelerated windows have two NSSurfaces:
//  (1) The window's pixel buffer in the back and
//  (2) the OpenGL view in the front.
// These two surfaces are composited by the window manager. Drawing into the
// CGContext which is provided by drawRect ends up in (1).
// When our window has rounded corners, the OpenGL view has transparent pixels
// in the corners. In these places the contents of the window's pixel buffer
// can show through. So we need to make sure that the pixel buffer is
// transparent in the corners so that no garbage reaches the screen.
// The contents of the pixel buffer in the rest of the window don't matter
// because they're covered by opaque pixels of the OpenGL context.
// Making the corners transparent works even though our window is
// declared "opaque" (in the NSWindow's isOpaque method).
- (void)clearCorners
{
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
// for the window's bottom corners automatically (starting with 10.7).
- (void)maskTopCornersInContext:(CGContextRef)aContext
{
  CGFloat radius = [self cornerRadius];
  int32_t devPixelCornerRadius = mGeckoChild->CocoaPointsToDevPixels(radius);

  // First make sure that mTopLeftCornerMask is set up.
  if (!mTopLeftCornerMask ||
      int32_t(CGImageGetWidth(mTopLeftCornerMask)) != devPixelCornerRadius) {
    CGImageRelease(mTopLeftCornerMask);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef imgCtx = CGBitmapContextCreate(NULL,
                                                devPixelCornerRadius,
                                                devPixelCornerRadius,
                                                8, devPixelCornerRadius * 4,
                                                rgb, kCGImageAlphaPremultipliedFirst);
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

- (void)drawTitleString
{
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
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:[frameView isFlipped]]];
  [frameView _drawTitleBar:[frameView bounds]];
  CGContextRestoreGState(ctx);
  [NSGraphicsContext setCurrentContext:oldContext];
}

- (void)drawTitlebarHighlight
{
  DrawTitlebarHighlight([self bounds].size, [self cornerRadius],
                        mGeckoChild->DevPixelsToCocoaPoints(1));
}

- (void)viewWillDraw
{
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
      nsCOMPtr<nsIRunnable> releaserRunnable =
        new WidgetsReleaserRunnable(Move(widgetArray));
      NS_DispatchToMainThread(releaserRunnable);
    }

    if ([self isUsingOpenGL]) {
      if (ShadowLayerForwarder* slf = mGeckoChild->GetLayerManager()->AsShadowForwarder()) {
        slf->WindowOverlayChanged();
      }
    }

    mGeckoChild->WillPaintWindow();
  }
  [super viewWillDraw];
}

#if USE_CLICK_HOLD_CONTEXTMENU
//
// -clickHoldCallback:
//
// called from a timer two seconds after a mouse down to see if we should display
// a context menu (click-hold). |anEvent| is the original mouseDown event. If we're
// still in that mouseDown by this time, put up the context menu, otherwise just
// fuhgeddaboutit. |anEvent| has been retained by the OS until after this callback
// fires so we're ok there.
//
// This code currently messes in a bunch of edge cases (bugs 234751, 232964, 232314)
// so removing it until we get it straightened out.
//
- (void)clickHoldCallback:(id)theEvent;
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if( theEvent == [NSApp currentEvent] ) {
    // we're still in the middle of the same mousedown event here, activate
    // click-hold context menu by triggering the right mouseDown action.
    NSEvent* clickHoldEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                                  location:[theEvent locationInWindow]
                                             modifierFlags:[theEvent modifierFlags]
                                                 timestamp:[theEvent timestamp]
                                              windowNumber:[theEvent windowNumber]
                                                   context:[theEvent context]
                                               eventNumber:[theEvent eventNumber]
                                                clickCount:[theEvent clickCount]
                                                  pressure:[theEvent pressure]];
    [self rightMouseDown:clickHoldEvent];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
#endif

// If we've just created a non-native context menu, we need to mark it as
// such and let the OS (and other programs) know when it opens and closes
// (this is how the OS knows to close other programs' context menus when
// ours open).  We send the initial notification here, but others are sent
// in nsCocoaWindow::Show().
- (void)maybeInitContextMenuTracking
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef MOZ_USE_NATIVE_POPUP_WINDOWS
  return;
#endif /* MOZ_USE_NATIVE_POPUP_WINDOWS */

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE_VOID(rollupListener);
  nsCOMPtr<nsIWidget> widget = rollupListener->GetRollupWidget();
  NS_ENSURE_TRUE_VOID(widget);

  NSWindow *popupWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
  if (!popupWindow || ![popupWindow isKindOfClass:[PopupWindow class]])
    return;

  [[NSDistributedNotificationCenter defaultCenter]
    postNotificationName:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                  object:@"org.mozilla.gecko.PopupWindow"];
  [(PopupWindow*)popupWindow setIsContextMenu:YES];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Returns true if the event should no longer be processed, false otherwise.
// This does not return whether or not anything was rolled up.
- (BOOL)maybeRollup:(NSEvent*)theEvent
{
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
          }
          else {
            popupsToRollup = sameTypeCount;
          }
          break;
        }
      }

      if (shouldRollup) {
        if ([theEvent type] == NSLeftMouseDown) {
          NSPoint point = [NSEvent mouseLocation];
          FlipCocoaScreenCoordinate(point);
          gfx::IntPoint pos = gfx::IntPoint::Truncate(point.x, point.y);
          consumeEvent = (BOOL)rollupListener->Rollup(popupsToRollup, true, &pos, nullptr);
        }
        else {
          consumeEvent = (BOOL)rollupListener->Rollup(popupsToRollup, true, nullptr, nullptr);
        }
      }
    }
  }

  return consumeEvent;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

/*
 * In OS X Mountain Lion and above, smart zoom gestures are implemented in
 * smartMagnifyWithEvent. In OS X Lion, they are implemented in
 * magnifyWithEvent. See inline comments for more info.
 *
 * The prototypes swipeWithEvent, beginGestureWithEvent, magnifyWithEvent,
 * smartMagnifyWithEvent, rotateWithEvent, and endGestureWithEvent were
 * obtained from the following links:
 * https://developer.apple.com/library/mac/#documentation/Cocoa/Reference/ApplicationKit/Classes/NSResponder_Class/Reference/Reference.html
 * https://developer.apple.com/library/mac/#releasenotes/Cocoa/AppKit.html
 */

- (void)swipeWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild ||
      [self beginOrEndGestureForEventPhase:anEvent]) {
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
    geckoEvent.mDirection |= nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
  else if (deltaX < 0.0)
    geckoEvent.mDirection |= nsIDOMSimpleGestureEvent::DIRECTION_RIGHT;

  // Record the up/down direction.
  if (deltaY > 0.0)
    geckoEvent.mDirection |= nsIDOMSimpleGestureEvent::DIRECTION_UP;
  else if (deltaY < 0.0)
    geckoEvent.mDirection |= nsIDOMSimpleGestureEvent::DIRECTION_DOWN;

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)magnifyWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild ||
      [self beginOrEndGestureForEventPhase:anEvent]) {
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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)smartMagnifyWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild ||
      [self beginOrEndGestureForEventPhase:anEvent]) {
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

- (void)rotateWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild ||
      [self beginOrEndGestureForEventPhase:anEvent]) {
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
    geckoEvent.mDirection = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
  } else {
    geckoEvent.mDirection = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
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
- (bool)beginOrEndGestureForEventPhase:(NSEvent*)aEvent
{
  if (!aEvent) {
    return false;
  }

  bool usingElCapitanOrLaterSDK = true;
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
  usingElCapitanOrLaterSDK = false;
#endif

  if (nsCocoaFeatures::OnElCapitanOrLater() && usingElCapitanOrLaterSDK) {
    if (aEvent.phase == NSEventPhaseBegan) {
      [self beginGestureWithEvent:aEvent];
      return true;
    }

    if (aEvent.phase == NSEventPhaseEnded ||
        aEvent.phase == NSEventPhaseCancelled) {
      [self endGestureWithEvent:aEvent];
      return true;
    }
  }

  return false;
}

- (void)beginGestureWithEvent:(NSEvent*)aEvent
{
  if (!aEvent) {
    return;
  }

  mGestureState = eGestureState_StartGesture;
  mCumulativeMagnification = 0;
  mCumulativeRotation = 0.0;
}

- (void)endGestureWithEvent:(NSEvent *)anEvent
{
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
  case eGestureState_MagnifyGesture:
    {
      // Setup the "magnify" event.
      WidgetSimpleGestureEvent geckoEvent(true, eMagnifyGesture, mGeckoChild);
      geckoEvent.mDelta = mCumulativeMagnification;
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
    break;

  case eGestureState_RotateGesture:
    {
      // Setup the "rotate" event.
      WidgetSimpleGestureEvent geckoEvent(true, eRotateGesture, mGeckoChild);
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
      geckoEvent.mDelta = -mCumulativeRotation;
      if (mCumulativeRotation > 0.0) {
        geckoEvent.mDirection = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
      } else {
        geckoEvent.mDirection = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
      }

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
    break;

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

- (bool)shouldConsiderStartingSwipeFromEvent:(NSEvent*)anEvent
{
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
  if ([anEvent type] != NSScrollWheel ||
      eventPhase != NSEventPhaseBegan ||
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

- (void)setUsingOMTCompositor:(BOOL)aUseOMTC
{
  mUsingOMTCompositor = aUseOMTC;
}

// Returning NO from this method only disallows ordering on mousedown - in order
// to prevent it for mouseup too, we need to call [NSApp preventWindowOrdering]
// when handling the mousedown event.
- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent*)aEvent
{
  // Always using system-provided window ordering for normal windows.
  if (![[self window] isKindOfClass:[PopupWindow class]])
    return NO;

  // Don't reorder when we don't have a parent window, like when we're a
  // context menu or a tooltip.
  return ![[self window] parentWindow];
}

- (void)mouseDown:(NSEvent*)theEvent
{
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
  }
  else {
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

#if USE_CLICK_HOLD_CONTEXTMENU
  // fire off timer to check for click-hold after two seconds. retains |theEvent|
  [self performSelector:@selector(clickHoldCallback:) withObject:theEvent afterDelay:2.0];
#endif

  // in order to send gecko events we'll need a gecko widget
  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  NSUInteger modifierFlags = [theEvent modifierFlags];

  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  NSInteger clickCount = [theEvent clickCount];
  if (mBlockedLastMouseDown && clickCount > 1) {
    // Don't send a double click if the first click of the double click was
    // blocked.
    clickCount--;
  }
  geckoEvent.mClickCount = clickCount;

  if (modifierFlags & NSControlKeyMask)
    geckoEvent.button = WidgetMouseEvent::eRightButton;
  else
    geckoEvent.button = WidgetMouseEvent::eLeftButton;

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  mBlockedLastMouseDown = NO;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseUp:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild || mBlockedLastMouseDown)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  if ([theEvent modifierFlags] & NSControlKeyMask)
    geckoEvent.button = WidgetMouseEvent::eRightButton;
  else
    geckoEvent.button = WidgetMouseEvent::eLeftButton;

  // This might destroy our widget (and null out mGeckoChild).
  bool defaultPrevented =
    (mGeckoChild->DispatchInputEvent(&geckoEvent) == nsEventStatus_eConsumeNoDefault);

  // Check to see if we are double-clicking in the titlebar.
  CGFloat locationInTitlebar = [[self window] frame].size.height - [theEvent locationInWindow].y;
  LayoutDeviceIntPoint pos = geckoEvent.mRefPoint;
  if (!defaultPrevented && [theEvent clickCount] == 2 &&
      !mGeckoChild->GetNonDraggableRegion().Contains(pos.x, pos.y) &&
      [[self window] isKindOfClass:[ToolbarWindow class]] &&
      (locationInTitlebar < [(ToolbarWindow*)[self window] titlebarHeight] ||
       locationInTitlebar < [(ToolbarWindow*)[self window] unifiedToolbarHeight])) {
    if ([self shouldZoomOnDoubleClick]) {
      [[self window] performZoom:nil];
    } else if ([self shouldMinimizeOnTitlebarDoubleClick]) {
      NSButton *minimizeButton = [[self window] standardWindowButton:NSWindowMiniaturizeButton];
      [minimizeButton performClick:self];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                         exitFrom:(WidgetMouseEvent::ExitFrom)aExitFrom
{
  if (!mGeckoChild)
    return;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, [self window]);
  NSPoint localEventLocation = [self convertPoint:windowEventLocation fromView:nil];

  EventMessage msg = aEnter ? eMouseEnterIntoWidget : eMouseExitFromWidget;
  WidgetMouseEvent event(true, msg, mGeckoChild, WidgetMouseEvent::eReal);
  event.mRefPoint = mGeckoChild->CocoaPointsToDevPixels(localEventLocation);

  event.mExitFrom = aExitFrom;

  nsEventStatus status; // ignored
  mGeckoChild->DispatchEvent(&event, status);
}

- (void)handleMouseMoved:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  gLastDragView = self;

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  // Note, sending the above event might have destroyed our widget since we didn't retain.
  // Fine so long as we don't access any local variables from here on.
  gLastDragView = nil;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  // The right mouse went down, fire off a right mouse down event to gecko
  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eRightButton;
  geckoEvent.mClickCount = [theEvent clickCount];

  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild)
    return;

  // Let the superclass do the context menu stuff.
  [super rightMouseDown:theEvent];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eRightButton;
  geckoEvent.mClickCount = [theEvent clickCount];

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchInputEvent(&geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDragged:(NSEvent*)theEvent
{
  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eRightButton;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if ([self maybeRollup:theEvent] ||
      !ChildViewMouseTracker::WindowAcceptsEvent([self window], theEvent, self))
    return;

  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseDown, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eMiddleButton;
  geckoEvent.mClickCount = [theEvent clickCount];

  mGeckoChild->DispatchInputEvent(&geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseUp, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eMiddleButton;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)otherMouseDragged:(NSEvent*)theEvent
{
  if (!mGeckoChild)
    return;
  if (mTextInputHandler->OnHandleEvent(theEvent)) {
    return;
  }

  WidgetMouseEvent geckoEvent(true, eMouseMove, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eMiddleButton;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchInputEvent(&geckoEvent);
}

- (void)sendWheelStartOrStop:(EventMessage)msg forEvent:(NSEvent *)theEvent
{
  WidgetWheelEvent wheelEvent(true, msg, mGeckoChild);
  [self convertCocoaMouseWheelEvent:theEvent toGeckoEvent:&wheelEvent];
  mExpectingWheelStop = (msg == eWheelOperationStart);
  mGeckoChild->DispatchInputEvent(wheelEvent.AsInputEvent());
}

- (void)sendWheelCondition:(BOOL)condition
                     first:(EventMessage)first
                    second:(EventMessage)second
                  forEvent:(NSEvent *)theEvent
{
  if (mExpectingWheelStop == condition) {
    [self sendWheelStartOrStop:first forEvent:theEvent];
  }
  [self sendWheelStartOrStop:second forEvent:theEvent];
}

static PanGestureInput::PanGestureType
PanGestureTypeForEvent(NSEvent* aEvent)
{
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

static int32_t RoundUp(double aDouble)
{
  return aDouble < 0 ? static_cast<int32_t>(floor(aDouble)) :
                       static_cast<int32_t>(ceil(aDouble));
}

static int32_t
TakeLargestInt(gfx::Float* aFloat)
{
  int32_t result(*aFloat); // truncate towards zero
  *aFloat -= result;
  return result;
}

static gfx::IntPoint
AccumulateIntegerDelta(NSEvent* aEvent)
{
  static gfx::Point sAccumulator(0.0f, 0.0f);
  if (nsCocoaUtils::EventPhase(aEvent) == NSEventPhaseBegan) {
    sAccumulator = gfx::Point(0.0f, 0.0f);
  }
  sAccumulator.x += [aEvent deltaX];
  sAccumulator.y += [aEvent deltaY];
  return gfx::IntPoint(TakeLargestInt(&sAccumulator.x),
                       TakeLargestInt(&sAccumulator.y));
}

static gfx::IntPoint
GetIntegerDeltaForEvent(NSEvent* aEvent)
{
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

- (void)scrollWheel:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (gfxPrefs::AsyncPanZoomSeparateEventThread() && [self apzctm]) {
    // Disable main-thread scrolling completely when using APZ with the
    // separate event thread. This is bug 1013412.
    return;
  }

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
  ScreenPoint position = ViewAs<ScreenPixel>(
    [self convertWindowCoordinatesRoundDown:locationInWindow],
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
    PanGestureInput panEvent(PanGestureTypeForEvent(theEvent),
                             eventIntervalTime, eventTimeStamp,
                             position, preciseDelta, modifiers);
    panEvent.mLineOrPageDeltaX = lineOrPageDelta.x;
    panEvent.mLineOrPageDeltaY = lineOrPageDelta.y;

    if (panEvent.mType == PanGestureInput::PANGESTURE_END) {
      // Check if there's a momentum start event in the event queue, so that we
      // can annotate this event.
      NSEvent* nextWheelEvent =
        [NSApp nextEventMatchingMask:NSScrollWheelMask
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
    ScrollWheelInput wheelEvent(eventIntervalTime, eventTimeStamp, modifiers,
                                ScrollWheelInput::SCROLLMODE_INSTANT,
                                ScrollWheelInput::SCROLLDELTA_PIXEL,
                                position,
                                preciseDelta.x,
                                preciseDelta.y,
                                false);
    wheelEvent.mLineOrPageDeltaX = lineOrPageDelta.x;
    wheelEvent.mLineOrPageDeltaY = lineOrPageDelta.y;
    wheelEvent.mIsMomentum = nsCocoaUtils::IsMomentumScrollEvent(theEvent);
    geckoChildDeathGrip->DispatchAPZWheelInputEvent(wheelEvent, false);
  } else {
    ScrollWheelInput::ScrollMode scrollMode = ScrollWheelInput::SCROLLMODE_INSTANT;
    if (gfxPrefs::SmoothScrollEnabled() && gfxPrefs::WheelSmoothScrollEnabled()) {
      scrollMode = ScrollWheelInput::SCROLLMODE_SMOOTH;
    }
    ScrollWheelInput wheelEvent(eventIntervalTime, eventTimeStamp, modifiers,
                                scrollMode,
                                ScrollWheelInput::SCROLLDELTA_LINE,
                                position,
                                lineOrPageDelta.x,
                                lineOrPageDelta.y,
                                false);
    wheelEvent.mLineOrPageDeltaX = lineOrPageDelta.x;
    wheelEvent.mLineOrPageDeltaY = lineOrPageDelta.y;
    geckoChildDeathGrip->DispatchAPZWheelInputEvent(wheelEvent, false);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)handleAsyncScrollEvent:(CGEventRef)cgEvent ofType:(CGEventType)type
{
  IAPZCTreeManager* apzctm = [self apzctm];
  if (!apzctm) {
    return;
  }

  CGPoint loc = CGEventGetLocation(cgEvent);
  loc.y = nsCocoaUtils::FlippedScreenY(loc.y);
  NSPoint locationInWindow =
    nsCocoaUtils::ConvertPointFromScreen([self window], NSPointFromCGPoint(loc));
  ScreenIntPoint location = ViewAs<ScreenPixel>(
    [self convertWindowCoordinates:locationInWindow],
    PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);

  static NSTimeInterval sStartTime = [NSDate timeIntervalSinceReferenceDate];
  static TimeStamp sStartTimeStamp = TimeStamp::Now();

  if (type == kCGEventScrollWheel) {
    NSEvent* event = [NSEvent eventWithCGEvent:cgEvent];
    NSEventPhase phase = nsCocoaUtils::EventPhase(event);
    NSEventPhase momentumPhase = nsCocoaUtils::EventMomentumPhase(event);
    CGFloat pixelDeltaX = 0, pixelDeltaY = 0;
    nsCocoaUtils::GetScrollingDeltas(event, &pixelDeltaX, &pixelDeltaY);
    uint32_t eventTime = ([event timestamp] - sStartTime) * 1000;
    TimeStamp eventTimeStamp = sStartTimeStamp +
      TimeDuration::FromSeconds([event timestamp] - sStartTime);
    NSPoint locationInWindowMoved = NSMakePoint(
      locationInWindow.x + pixelDeltaX,
      locationInWindow.y - pixelDeltaY);
    ScreenIntPoint locationMoved = ViewAs<ScreenPixel>(
      [self convertWindowCoordinates:locationInWindowMoved],
      PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);
    ScreenPoint delta = ScreenPoint(locationMoved - location);
    ScrollableLayerGuid guid;

    // MayBegin and Cancelled are dispatched when the fingers start or stop
    // touching the touchpad before any scrolling has occurred. These events
    // can be used to control scrollbar visibility or interrupt scroll
    // animations. They are only dispatched on 10.8 or later, and only by
    // relatively modern devices.
    if (phase == NSEventPhaseMayBegin) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_MAYSTART, eventTime,
                               eventTimeStamp, location, ScreenPoint(0, 0), 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
      return;
    }
    if (phase == NSEventPhaseCancelled) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_CANCELLED, eventTime,
                               eventTimeStamp, location, ScreenPoint(0, 0), 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
      return;
    }

    // Legacy scroll events are dispatched by devices that do not have a
    // concept of a scroll gesture, for example by USB mice with
    // traditional mouse wheels.
    // For these kinds of scrolls, we want to surround every single scroll
    // event with a PANGESTURE_START and a PANGESTURE_END event. The APZC
    // needs to know that the real scroll gesture can end abruptly after any
    // one of these events.
    bool isLegacyScroll = (phase == NSEventPhaseNone &&
      momentumPhase == NSEventPhaseNone && delta != ScreenPoint(0, 0));

    if (phase == NSEventPhaseBegan || isLegacyScroll) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_START, eventTime,
                               eventTimeStamp, location, ScreenPoint(0, 0), 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
    }
    if (momentumPhase == NSEventPhaseNone && delta != ScreenPoint(0, 0)) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_PAN, eventTime,
                               eventTimeStamp, location, delta, 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
    }
    if (phase == NSEventPhaseEnded || isLegacyScroll) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_END, eventTime,
                               eventTimeStamp, location, ScreenPoint(0, 0), 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
    }

    // Any device that can dispatch momentum events supports all three momentum phases.
    if (momentumPhase == NSEventPhaseBegan) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_MOMENTUMSTART, eventTime,
                               eventTimeStamp, location, ScreenPoint(0, 0), 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
    }
    if (momentumPhase == NSEventPhaseChanged && delta != ScreenPoint(0, 0)) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_MOMENTUMPAN, eventTime,
                               eventTimeStamp, location, delta, 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
    }
    if (momentumPhase == NSEventPhaseEnded) {
      PanGestureInput panInput(PanGestureInput::PANGESTURE_MOMENTUMEND, eventTime,
                               eventTimeStamp, location, ScreenPoint(0, 0), 0);
      apzctm->ReceiveInputEvent(panInput, &guid, nullptr);
    }
  }
}

-(NSMenu*)menuForEvent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mGeckoChild)
    return nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild)
    return nil;

  // Cocoa doesn't always dispatch a mouseDown: for a control-click event,
  // depends on what we return from menuForEvent:. Gecko always expects one
  // and expects the mouse down event before the context menu event, so
  // get that event sent first if this is a left mouse click.
  if ([theEvent type] == NSLeftMouseDown) {
    [self mouseDown:theEvent];
    if (!mGeckoChild)
      return nil;
  }

  WidgetMouseEvent geckoEvent(true, eContextMenu, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = WidgetMouseEvent::eRightButton;
  mGeckoChild->DispatchInputEvent(&geckoEvent);
  if (!mGeckoChild)
    return nil;

  [self maybeInitContextMenuTracking];

  // Go up our view chain to fetch the correct menu to return.
  return [self contextMenu];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSMenu*)contextMenu
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSView* superView = [self superview];
  if ([superView respondsToSelector:@selector(contextMenu)])
    return [(NSView<mozView>*)superView contextMenu];

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) convertCocoaMouseWheelEvent:(NSEvent*)aMouseEvent
                        toGeckoEvent:(WidgetWheelEvent*)outWheelEvent
{
  [self convertCocoaMouseEvent:aMouseEvent toGeckoEvent:outWheelEvent];

  bool usePreciseDeltas = nsCocoaUtils::HasPreciseScrollingDeltas(aMouseEvent) &&
    Preferences::GetBool("mousewheel.enable_pixel_scrolling", true);

  outWheelEvent->mDeltaMode =
    usePreciseDeltas ? nsIDOMWheelEvent::DOM_DELTA_PIXEL
                     : nsIDOMWheelEvent::DOM_DELTA_LINE;
  outWheelEvent->mIsMomentum = nsCocoaUtils::IsMomentumScrollEvent(aMouseEvent);
}

- (void) convertCocoaMouseEvent:(NSEvent*)aMouseEvent
                   toGeckoEvent:(WidgetInputEvent*)outGeckoEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(outGeckoEvent, "convertCocoaMouseEvent:toGeckoEvent: requires non-null aoutGeckoEvent");
  if (!outGeckoEvent)
    return;

  nsCocoaUtils::InitInputEvent(*outGeckoEvent, aMouseEvent);

  // convert point to view coordinate system
  NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(aMouseEvent, [self window]);

  outGeckoEvent->mRefPoint = [self convertWindowCoordinates:locationInWindow];

  WidgetMouseEventBase* mouseEvent = outGeckoEvent->AsMouseEventBase();
  mouseEvent->buttons = 0;
  NSUInteger mouseButtons = [NSEvent pressedMouseButtons];

  if (mouseButtons & 0x01) {
    mouseEvent->buttons |= WidgetMouseEvent::eLeftButtonFlag;
  }
  if (mouseButtons & 0x02) {
    mouseEvent->buttons |= WidgetMouseEvent::eRightButtonFlag;
  }
  if (mouseButtons & 0x04) {
    mouseEvent->buttons |= WidgetMouseEvent::eMiddleButtonFlag;
  }
  if (mouseButtons & 0x08) {
    mouseEvent->buttons |= WidgetMouseEvent::e4thButtonFlag;
  }
  if (mouseButtons & 0x10) {
    mouseEvent->buttons |= WidgetMouseEvent::e5thButtonFlag;
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
        [self convertCocoaTabletPointerEvent:aMouseEvent
                                toGeckoEvent:mouseEvent->AsMouseEvent()];
      }
      break;

    default:
      // Don't check other NSEvents for pressure.
      break;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) convertCocoaTabletPointerEvent:(NSEvent*)aPointerEvent
                           toGeckoEvent:(WidgetMouseEvent*)aOutGeckoEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN
  if (!aOutGeckoEvent || !sIsTabletPointerActivated) {
    return;
  }
  if ([aPointerEvent type] != NSMouseMoved) {
    aOutGeckoEvent->pressure = [aPointerEvent pressure];
    MOZ_ASSERT(aOutGeckoEvent->pressure >= 0.0 &&
               aOutGeckoEvent->pressure <= 1.0);
  }
  aOutGeckoEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_PEN;
  aOutGeckoEvent->tiltX = lround([aPointerEvent tilt].x * 90);
  aOutGeckoEvent->tiltY = lround([aPointerEvent tilt].y * 90);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)tabletProximity:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN
  sIsTabletPointerActivated = [theEvent isEnteringProximity];
  NS_OBJC_END_TRY_ABORT_BLOCK
}

- (BOOL)shouldZoomOnDoubleClick
{
  if ([NSWindow respondsToSelector:@selector(_shouldZoomOnDoubleClick)]) {
    return [NSWindow _shouldZoomOnDoubleClick];
  }
  return nsCocoaFeatures::OnYosemiteOrLater();
}

- (BOOL)shouldMinimizeOnTitlebarDoubleClick
{
  NSString *MDAppleMiniaturizeOnDoubleClickKey =
                                      @"AppleMiniaturizeOnDoubleClick";
  NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
  bool shouldMinimize = [[userDefaults
          objectForKey:MDAppleMiniaturizeOnDoubleClickKey] boolValue];

  return shouldMinimize;
}

#pragma mark -
// NSTextInputClient implementation

- (NSRange)markedRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->MarkedRange();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRange)selectedRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->SelectedRange();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex
{
  NS_ENSURE_TRUE(mTextInputHandler, NO);
  if (charIndex == NSNotFound) {
    return NO;
  }
  return mTextInputHandler->DrawsVerticallyForCharacterAtIndex(charIndex);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
  NS_ENSURE_TRUE(mTextInputHandler, 0);
  return mTextInputHandler->CharacterIndexForPoint(thePoint);
}

- (NSArray*)validAttributesForMarkedText
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NS_ENSURE_TRUE(mTextInputHandler, [NSArray array]);
  return mTextInputHandler->GetValidAttributesForMarkedText();

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange
{
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

- (void)doCommandBySelector:(SEL)aSelector
{
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

- (void)unmarkText
{
  NS_ENSURE_TRUE_VOID(mTextInputHandler);
  mTextInputHandler->CommitIMEComposition();
}

- (BOOL) hasMarkedText
{
  NS_ENSURE_TRUE(mTextInputHandler, NO);
  return mTextInputHandler->HasMarkedText();
}

- (void)setMarkedText:(id)aString selectedRange:(NSRange)selectedRange
                               replacementRange:(NSRange)replacementRange
{
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
                                        actualRange:(NSRangePointer)actualRange
{
  NS_ENSURE_TRUE(mTextInputHandler, nil);
  return mTextInputHandler->GetAttributedSubstringFromRange(aRange,
                                                            actualRange);
}

- (NSRect)firstRectForCharacterRange:(NSRange)aRange
                         actualRange:(NSRangePointer)actualRange
{
  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRect(0.0, 0.0, 0.0, 0.0));
  return mTextInputHandler->FirstRectForCharacterRange(aRange, actualRange);
}

- (void)quickLookWithEvent:(NSEvent*)event
{
  // Show dictionary by current point
  WidgetContentCommandEvent
    contentCommandEvent(true, eContentCommandLookUpDictionary, mGeckoChild);
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  contentCommandEvent.mRefPoint = mGeckoChild->CocoaPointsToDevPixels(point);
  mGeckoChild->DispatchWindowEvent(contentCommandEvent);
  // The widget might have been destroyed.
}

- (NSInteger)windowLevel
{
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
- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event
{
  return YES;
}

- (NSEvent*)lastKeyDownEvent
{
  return mLastKeyDownEvent;
}

- (void)keyDown:(NSEvent*)theEvent
{
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
  NSWindow *viewWindow = [self window];
  if (viewWindow && [viewWindow isKeyWindow]) {
    [viewWindow orderWindow:NSWindowAbove relativeTo:0];
  }

#if !defined(RELEASE_OR_BETA) || defined(DEBUG)
  if (!Preferences::GetBool("intl.allow-insecure-text-input", false) &&
      mGeckoChild && mTextInputHandler && mTextInputHandler->IsFocused()) {
#ifdef MOZ_CRASHREPORTER
    NSWindow* window = [self window];
    NSString* info = [NSString stringWithFormat:@"\nview [%@], window [%@], window is key %i, is fullscreen %i, app is active %i",
                      self, window, [window isKeyWindow], ([window styleMask] & (1 << 14)) != 0,
                      [NSApp isActive]];
    nsAutoCString additionalInfo([info UTF8String]);
#endif
    if (mGeckoChild->GetInputContext().IsPasswordEditor() &&
               !TextInputHandler::IsSecureEventInputEnabled()) {
      #define CRASH_MESSAGE "A password editor has focus, but not in secure input mode"
#ifdef MOZ_CRASHREPORTER
      CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("\nBug 893973: ") +
                                                 NS_LITERAL_CSTRING(CRASH_MESSAGE));
      CrashReporter::AppendAppNotesToCrashReport(additionalInfo);
#endif
      MOZ_CRASH(CRASH_MESSAGE);
      #undef CRASH_MESSAGE
    } else if (!mGeckoChild->GetInputContext().IsPasswordEditor() &&
               TextInputHandler::IsSecureEventInputEnabled()) {
      #define CRASH_MESSAGE "A non-password editor has focus, but in secure input mode"
#ifdef MOZ_CRASHREPORTER
      CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("\nBug 893973: ") +
                                                 NS_LITERAL_CSTRING(CRASH_MESSAGE));
      CrashReporter::AppendAppNotesToCrashReport(additionalInfo);
#endif
      MOZ_CRASH(CRASH_MESSAGE);
      #undef CRASH_MESSAGE
    }
  }
#endif // #if !defined(RELEASE_OR_BETA) || defined(DEBUG)

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

- (void)keyUp:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  mTextInputHandler->HandleKeyUpEvent(theEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)insertNewline:(id)sender
{
  if (mTextInputHandler) {
    mTextInputHandler->InsertNewline();
  }
}

- (void)flagsChanged:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mTextInputHandler->HandleFlagsChanged(theEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL) isFirstResponder
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSResponder* resp = [[self window] firstResponder];
  return (resp == (NSResponder*)self);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (BOOL)isDragInProgress
{
  if (!mDragService)
    return NO;

  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  return dragSession != nullptr;
}

- (BOOL)inactiveWindowAcceptsMouseEvent:(NSEvent*)aEvent
{
  // If we're being destroyed assume the default -- return YES.
  if (!mGeckoChild)
    return YES;

  WidgetMouseEvent geckoEvent(true, eMouseActivate, mGeckoChild,
                              WidgetMouseEvent::eReal);
  [self convertCocoaMouseEvent:aEvent toGeckoEvent:&geckoEvent];
  return (mGeckoChild->DispatchInputEvent(&geckoEvent) != nsEventStatus_eConsumeNoDefault);
}

// We must always call through to our superclass, even when mGeckoChild is
// nil -- otherwise the keyboard focus can end up in the wrong NSView.
- (BOOL)becomeFirstResponder
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [super becomeFirstResponder];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(YES);
}

- (void)viewsWindowDidBecomeKey
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // check to see if the window implements the mozWindow protocol. This
  // allows embedders to avoid re-entrant calls to -makeKeyAndOrderFront,
  // which can happen because these activate calls propagate out
  // to the embedder via nsIEmbeddingSiteWindow::SetFocus().
  BOOL isMozWindow = [[self window] respondsToSelector:@selector(setSuppressMakeKeyFront:)];
  if (isMozWindow)
    [[self window] setSuppressMakeKeyFront:YES];

  nsIWidgetListener* listener = mGeckoChild->GetWidgetListener();
  if (listener)
    listener->WindowActivated();

  if (isMozWindow)
    [[self window] setSuppressMakeKeyFront:NO];

  if (mGeckoChild->GetInputContext().IsPasswordEditor()) {
    TextInputHandler::EnableSecureEventInput();
  } else {
    TextInputHandler::EnsureSecureEventInputDisabled();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)viewsWindowDidResignKey
{
  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  nsIWidgetListener* listener = mGeckoChild->GetWidgetListener();
  if (listener)
    listener->WindowDeactivated();

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
- (void)delayedTearDown
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self removeFromSuperview];
  [self release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -

// drag'n'drop stuff
#define kDragServiceContractID "@mozilla.org/widget/dragservice;1"

- (NSDragOperation)dragOperationFromDragAction:(int32_t)aDragAction
{
  if (nsIDragService::DRAGDROP_ACTION_LINK & aDragAction)
    return NSDragOperationLink;
  if (nsIDragService::DRAGDROP_ACTION_COPY & aDragAction)
    return NSDragOperationCopy;
  if (nsIDragService::DRAGDROP_ACTION_MOVE & aDragAction)
    return NSDragOperationGeneric;
  return NSDragOperationNone;
}

- (LayoutDeviceIntPoint)convertWindowCoordinates:(NSPoint)aPoint
{
  if (!mGeckoChild) {
    return LayoutDeviceIntPoint(0, 0);
  }

  NSPoint localPoint = [self convertPoint:aPoint fromView:nil];
  return mGeckoChild->CocoaPointsToDevPixels(localPoint);
}

- (LayoutDeviceIntPoint)convertWindowCoordinatesRoundDown:(NSPoint)aPoint
{
  if (!mGeckoChild) {
    return LayoutDeviceIntPoint(0, 0);
  }

  NSPoint localPoint = [self convertPoint:aPoint fromView:nil];
  return mGeckoChild->CocoaPointsToDevPixelsRoundDown(localPoint);
}

- (IAPZCTreeManager*)apzctm
{
  return mGeckoChild ? mGeckoChild->APZCTM() : nullptr;
}

// This is a utility function used by NSView drag event methods
// to send events. It contains all of the logic needed for Gecko
// dragging to work. Returns the appropriate cocoa drag operation code.
- (NSDragOperation)doDragAction:(EventMessage)aMessage sender:(id)aSender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mGeckoChild)
    return NSDragOperationNone;

  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView doDragAction: entered\n"));

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
    if (!mDragService)
      return NSDragOperationNone;
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
      mDragService->FireDragEventAtSource(
        eDrag, nsCocoaUtils::ModifiersForEvent([NSApp currentEvent]));
      dragSession->SetCanDrop(false);
    } else if (aMessage == eDrop) {
      // We make the assumption that the dragOver handlers have correctly set
      // the |canDrop| property of the Drag Session.
      bool canDrop = false;
      if (!NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)) || !canDrop) {
        [self doDragAction:eDragExit sender:aSender];

        nsCOMPtr<nsIDOMNode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          mDragService->EndDragSession(
            false, nsCocoaUtils::ModifiersForEvent([NSApp currentEvent]));
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
  if (!mGeckoChild)
    return NSDragOperationNone;

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
        nsDragService* dragService = static_cast<nsDragService *>(mDragService);
        int32_t childDragAction = dragService->TakeChildProcessDragAction();
        if (childDragAction != nsIDragService::DRAGDROP_ACTION_UNINITIALIZED) {
          dragAction = childDragAction;
        }

        return [self dragOperationFromDragAction:dragAction];
      }
      case eDragExit:
      case eDrop: {
        nsCOMPtr<nsIDOMNode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          // We're leaving a window while doing a drag that was
          // initiated in a different app. End the drag session,
          // since we're done with it for now (until the user
          // drags back into mozilla).
          mDragService->EndDragSession(
            false, nsCocoaUtils::ModifiersForEvent([NSApp currentEvent]));
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
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
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
- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView draggingUpdated: entered\n"));
  return [self doDragAction:eDragOver sender:sender];
}

// NSDraggingDestination
- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  MOZ_LOG(sCocoaLog, LogLevel::Info, ("ChildView draggingExited: entered\n"));

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  [self doDragAction:eDragExit sender:sender];
  NS_IF_RELEASE(mDragService);
}

// NSDraggingDestination
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  BOOL handled = [self doDragAction:eDrop sender:sender] != NSDragOperationNone;
  NS_IF_RELEASE(mDragService);
  return handled;
}

// NSDraggingSource
// This is just implemented so we comply with the NSDraggingSource protocol.
- (NSDragOperation)draggingSession:(NSDraggingSession*)session
  sourceOperationMaskForDraggingContext:(NSDraggingContext)context
{
  return UINT_MAX;
}

// NSDraggingSource
- (BOOL)ignoreModifierKeysForDraggingSession:(NSDraggingSession*)session
{
  return YES;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession
           endedAtPoint:(NSPoint)aPoint
              operation:(NSDragOperation)aOperation
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  gDraggedTransferables = nullptr;

  NSEvent* currentEvent = [NSApp currentEvent];
  gUserCancelledDrag = ([currentEvent type] == NSKeyDown &&
                        [currentEvent keyCode] == kVK_Escape);

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
  }

  if (mDragService) {
    // set the dragend point from the current mouse location
    nsDragService* dragService = static_cast<nsDragService *>(mDragService);
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
      nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
      dragService->GetDataTransfer(getter_AddRefs(dataTransfer));
      if (dataTransfer)
        dataTransfer->SetDropEffectInt(nsIDragService::DRAGDROP_ACTION_NONE);
    }

    mDragService->EndDragSession(
      true, nsCocoaUtils::ModifiersForEvent(currentEvent));
    NS_RELEASE(mDragService);
  }

  [globalDragPboard release];
  globalDragPboard = nil;
  [gLastDragMouseDownEvent release];
  gLastDragMouseDownEvent = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession
           movedToPoint:(NSPoint)aPoint
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Get the drag service if it isn't already cached. The drag service
  // isn't cached when dragging over a different application.
  nsCOMPtr<nsIDragService> dragService = mDragService;
  if (!dragService) {
    dragService = do_GetService(kDragServiceContractID);
  }

  if (dragService) {
    nsDragService* ds = static_cast<nsDragService *>(dragService.get());
    ds->DragMovedWithView(aSession, aPoint);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSDraggingSource
- (void)draggingSession:(NSDraggingSession*)aSession
       willBeginAtPoint:(NSPoint)aPoint
{
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
provideDataForType:(NSString*)aType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!gDraggedTransferables) {
    return;
  }

  uint32_t count = 0;
  gDraggedTransferables->GetLength(&count);

  for (uint32_t j = 0; j < count; j++) {
    nsCOMPtr<nsITransferable> currentTransferable =
      do_QueryElementAt(gDraggedTransferables, j);
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
      if ([curType isEqualToString:
            [UTIHelper stringFromPboardType:NSPasteboardTypeString]] ||
          [curType isEqualToString:
            [UTIHelper stringFromPboardType:kPublicUrlPboardType]] ||
          [curType isEqualToString:
            [UTIHelper stringFromPboardType:kPublicUrlNamePboardType]] ||
          [curType isEqualToString:
            [UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL]]) {
        [aPasteboard setString:[pasteboardOutputDict valueForKey:curType]
                       forType:curType];
      } else if ([curType isEqualToString:
                   [UTIHelper stringFromPboardType:
                     kUrlsWithTitlesPboardType]]) {
        [aPasteboard setPropertyList:[pasteboardOutputDict valueForKey:curType]
                       forType:curType];
      } else if ([curType isEqualToString:
                   [UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
        [aPasteboard setString:
          (nsClipboard::WrapHtmlForSystemPasteboard(
            [pasteboardOutputDict valueForKey:curType]))
                      forType:curType];
      } else if ([curType isEqualToString:
                   [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF]] ||
                 [curType isEqualToString:
                   [UTIHelper stringFromPboardType:kMozCustomTypesPboardType]]) {
        [aPasteboard setData:[pasteboardOutputDict valueForKey:curType]
                     forType:curType];
      } else if ([curType isEqualToString:
                   [UTIHelper stringFromPboardType:kMozFileUrlsPboardType]]) {
        [aPasteboard writeObjects:[pasteboardOutputDict valueForKey:curType]];
      } else if ([curType isEqualToString:
                   [UTIHelper stringFromPboardType:
                     (NSString*)kPasteboardTypeFileURLPromise]]) {


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
          nsCOMPtr<nsITransferable> item =
            do_QueryElementAt(gDraggedTransferables, i);
          if (!item) {
            NS_ERROR("no transferable");
            continue;
          }

          item->SetTransferData(kFilePromiseDirectoryMime, macLocalFile,
                                sizeof(nsIFile*));

          // Now request the kFilePromiseMime data, which will invoke the data
          // provider. If successful, the file will have been created.
          nsCOMPtr<nsISupports> fileDataPrimitive;
          uint32_t dataSize = 0;
          item->GetTransferData(kFilePromiseMime,
                                getter_AddRefs(fileDataPrimitive), &dataSize);
        }
        CFRelease(urlRef);
        CFRelease(pboardRef);

        [aPasteboard setPropertyList:[pasteboardOutputDict valueForKey:curType]
                             forType:curType];
      }
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -

// Support for the "Services" menu. We currently only support sending strings
// and HTML to system services.

- (id)validRequestorForSendType:(NSString *)sendType
                     returnType:(NSString *)returnType
{
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

  NSString* stringType =
    [UTIHelper stringFromPboardType:NSPasteboardTypeString];
  NSString* htmlType = [UTIHelper stringFromPboardType:NSPasteboardTypeHTML];
  if ((!sendType || [sendType isEqualToString:stringType] ||
        [sendType isEqualToString:htmlType]) &&
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
        WidgetContentCommandEvent command(true,
                                          eContentCommandPasteTransferable,
                                          mGeckoChild, true);
        // This might possibly destroy our widget (and null out mGeckoChild).
        mGeckoChild->DispatchWindowEvent(command);
        if (!mGeckoChild || !command.mSucceeded || !command.mIsEnabled)
          result = nil;
      }
    }
  }

  // Give the superclass a chance if this object will not handle this request.
  if (!result)
    result = [super validRequestorForSendType:sendType returnType:returnType];

  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pboard
                             types:(NSArray *)types
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // Make sure that the service will accept strings or HTML.
  if (![types containsObject:
         [UTIHelper stringFromPboardType:NSStringPboardType]] &&
      ![types containsObject:
         [UTIHelper stringFromPboardType:NSPasteboardTypeString]] &&
      ![types containsObject:
         [UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
    return NO;
  }

  // Bail out if there is no Gecko object.
  if (!mGeckoChild)
    return NO;

  // Transform the transferable to an NSDictionary.
  NSDictionary* pasteboardOutputDict = nullptr;

  pasteboardOutputDict = nsClipboard::
      PasteboardDictFromTransferable(nsClipboard::sSelectionCache);

  if (!pasteboardOutputDict)
    return NO;

  // Declare the pasteboard types.
  unsigned int typeCount = [pasteboardOutputDict count];
  NSMutableArray* declaredTypes = [NSMutableArray arrayWithCapacity:typeCount];
  [declaredTypes addObjectsFromArray:[pasteboardOutputDict allKeys]];
  [pboard declareTypes:declaredTypes owner:nil];

  // Write the data to the pasteboard.
  for (unsigned int i = 0; i < typeCount; i++) {
    NSString* currentKey = [declaredTypes objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];

    if ([currentKey isEqualToString:
          [UTIHelper stringFromPboardType:NSPasteboardTypeString]] ||
        [currentKey isEqualToString:
          [UTIHelper stringFromPboardType:kPublicUrlPboardType]] ||
        [currentKey isEqualToString:
          [UTIHelper stringFromPboardType:kPublicUrlNamePboardType]]) {
      [pboard setString:currentValue forType:currentKey];
    } else if ([currentKey isEqualToString:
                 [UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
      [pboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue))
                forType:currentKey];
    } else if ([currentKey isEqualToString:
                 [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF]]) {
      [pboard setData:currentValue forType:currentKey];
    } else if ([currentKey isEqualToString:
                 [UTIHelper stringFromPboardType:
                   (NSString*)kPasteboardTypeFileURLPromise]] ||
               [currentKey isEqualToString:
                 [UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType]]) {
      [pboard setPropertyList:currentValue forType:currentKey];
    }
  }
  return YES;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

// Called if the service wants us to replace the current selection.
- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pboard
{
  nsresult rv;
  nsCOMPtr<nsITransferable> trans = do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv))
    return NO;
  trans->Init(nullptr);

  trans->AddDataFlavor(kUnicodeMime);
  trans->AddDataFlavor(kHTMLMime);

  rv = nsClipboard::TransferableFromPasteboard(trans, pboard);
  if (NS_FAILED(rv))
    return NO;

  NS_ENSURE_TRUE(mGeckoChild, false);

  WidgetContentCommandEvent command(true,
                                    eContentCommandPasteTransferable,
                                    mGeckoChild);
  command.mTransferable = trans;
  mGeckoChild->DispatchWindowEvent(command);

  return command.mSucceeded && command.mIsEnabled;
}

nsresult
nsChildView::GetSelectionAsPlaintext(nsAString& aResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!nsClipboard::sSelectionCache) {
    MOZ_ASSERT(aResult.IsEmpty());
    return NS_OK;
  }

  // Get the current chrome or content selection.
  NSDictionary* pasteboardOutputDict = nullptr;
  pasteboardOutputDict = nsClipboard::
    PasteboardDictFromTransferable(nsClipboard::sSelectionCache);

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
- (id<mozAccessible>)accessible
{
  if (!mGeckoChild)
    return nil;

  id<mozAccessible> nativeAccessible = nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  RefPtr<nsChildView> geckoChild(mGeckoChild);
  RefPtr<a11y::Accessible> accessible = geckoChild->GetDocumentAccessible();
  if (!accessible)
    return nil;

  accessible->GetNativeInterface((void**)&nativeAccessible);

#ifdef DEBUG_hakan
  NSAssert(![nativeAccessible isExpired], @"native acc is expired!!!");
#endif

  return nativeAccessible;
}

/* Implementation of formal mozAccessible formal protocol (enabling mozViews
   to talk to mozAccessible objects in the accessibility module). */

- (BOOL)hasRepresentedView
{
  return YES;
}

- (id)representedView
{
  return self;
}

- (BOOL)isRoot
{
  return [[self accessible] isRoot];
}

#ifdef DEBUG
- (void)printHierarchy
{
  [[self accessible] printHierarchy];
}
#endif

#pragma mark -

// general

- (BOOL)accessibilityIsIgnored
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityIsIgnored];

  return [[self accessible] accessibilityIsIgnored];
}

- (id)accessibilityHitTest:(NSPoint)point
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityHitTest:point];

  return [[self accessible] accessibilityHitTest:point];
}

- (id)accessibilityFocusedUIElement
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityFocusedUIElement];

  return [[self accessible] accessibilityFocusedUIElement];
}

// actions

- (NSArray*)accessibilityActionNames
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityActionNames];

  return [[self accessible] accessibilityActionNames];
}

- (NSString*)accessibilityActionDescription:(NSString*)action
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityActionDescription:action];

  return [[self accessible] accessibilityActionDescription:action];
}

- (void)accessibilityPerformAction:(NSString*)action
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityPerformAction:action];

  return [[self accessible] accessibilityPerformAction:action];
}

// attributes

- (NSArray*)accessibilityAttributeNames
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityAttributeNames];

  return [[self accessible] accessibilityAttributeNames];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityIsAttributeSettable:attribute];

  return [[self accessible] accessibilityIsAttributeSettable:attribute];
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return [super accessibilityAttributeValue:attribute];

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

+ (uint32_t)sUniqueKeyEventId { return sUniqueKeyEventId; }

+ (NSMutableDictionary*)sNativeKeyEventsMap { return sNativeKeyEventsMap; }

@end

#pragma mark -

void
ChildViewMouseTracker::OnDestroyView(ChildView* aView)
{
  if (sLastMouseEventView == aView) {
    sLastMouseEventView = nil;
    [sLastMouseMoveEvent release];
    sLastMouseMoveEvent = nil;
  }
}

void
ChildViewMouseTracker::OnDestroyWindow(NSWindow* aWindow)
{
  if (sWindowUnderMouse == aWindow) {
    sWindowUnderMouse = nil;
  }
}

void
ChildViewMouseTracker::MouseEnteredWindow(NSEvent* aEvent)
{
  sWindowUnderMouse = [aEvent window];
  ReEvaluateMouseEnterState(aEvent);
}

void
ChildViewMouseTracker::MouseExitedWindow(NSEvent* aEvent)
{
  if (sWindowUnderMouse == [aEvent window]) {
    sWindowUnderMouse = nil;
    ReEvaluateMouseEnterState(aEvent);
  }
}

void
ChildViewMouseTracker::ReEvaluateMouseEnterState(NSEvent* aEvent, ChildView* aOldView)
{
  ChildView* oldView = aOldView ? aOldView : sLastMouseEventView;
  sLastMouseEventView = ViewForEvent(aEvent);
  if (sLastMouseEventView != oldView) {
    // Send enter and / or exit events.
    WidgetMouseEvent::ExitFrom exitFrom =
      [sLastMouseEventView window] == [oldView window] ?
        WidgetMouseEvent::eChild : WidgetMouseEvent::eTopLevel;
    [oldView sendMouseEnterOrExitEvent:aEvent
                                 enter:NO
                              exitFrom:exitFrom];
    // After the cursor exits the window set it to a visible regular arrow cursor.
    if (exitFrom == WidgetMouseEvent::eTopLevel) {
      [[nsCursorManager sharedInstance] setCursor:eCursor_standard];
    }
    [sLastMouseEventView sendMouseEnterOrExitEvent:aEvent
                                             enter:YES
                                          exitFrom:exitFrom];
  }
}

void
ChildViewMouseTracker::ResendLastMouseMoveEvent()
{
  if (sLastMouseMoveEvent) {
    MouseMoved(sLastMouseMoveEvent);
  }
}

void
ChildViewMouseTracker::MouseMoved(NSEvent* aEvent)
{
  MouseEnteredWindow(aEvent);
  [sLastMouseEventView handleMouseMoved:aEvent];
  if (sLastMouseMoveEvent != aEvent) {
    [sLastMouseMoveEvent release];
    sLastMouseMoveEvent = [aEvent retain];
  }
}

void
ChildViewMouseTracker::MouseScrolled(NSEvent* aEvent)
{
  if (!nsCocoaUtils::IsMomentumScrollEvent(aEvent)) {
    // Store the position so we can pin future momentum scroll events.
    sLastScrollEventScreenLocation = nsCocoaUtils::ScreenLocationForEvent(aEvent);
  }
}

ChildView*
ChildViewMouseTracker::ViewForEvent(NSEvent* aEvent)
{
  NSWindow* window = sWindowUnderMouse;
  if (!window)
    return nil;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, window);
  NSView* view = [[[window contentView] superview] hitTest:windowEventLocation];

  if (![view isKindOfClass:[ChildView class]])
    return nil;

  ChildView* childView = (ChildView*)view;
  // If childView is being destroyed return nil.
  if (![childView widget])
    return nil;
  return WindowAcceptsEvent(window, aEvent, childView) ? childView : nil;
}

BOOL
ChildViewMouseTracker::WindowAcceptsEvent(NSWindow* aWindow, NSEvent* aEvent,
                                          ChildView* aView, BOOL aIsClickThrough)
{
  // Right mouse down events may get through to all windows, even to a top level
  // window with an open sheet.
  if (!aWindow || [aEvent type] == NSRightMouseDown)
    return YES;

  id delegate = [aWindow delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]])
    return YES;

  nsIWidget *windowWidget = [(WindowDelegate *)delegate geckoWidget];
  if (!windowWidget)
    return YES;

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
      if ([aWindow attachedSheet])
        return NO;

      topLevelWindow = aWindow;
      break;
    case eWindowType_sheet: {
      nsIWidget* parentWidget = windowWidget->GetSheetWindowParent();
      if (!parentWidget)
        return YES;

      topLevelWindow = (NSWindow*)parentWidget->GetNativeData(NS_NATIVE_WINDOW);
      break;
    }

    default:
      return YES;
  }

  if (!topLevelWindow ||
      ([topLevelWindow isMainWindow] && !aIsClickThrough) ||
      [aEvent type] == NSOtherMouseDown ||
      (([aEvent modifierFlags] & NSCommandKeyMask) != 0 &&
       [aEvent type] != NSMouseMoved))
    return YES;

  // If we're here then we're dealing with a left click or mouse move on an
  // inactive window or something similar. Ask Gecko what to do.
  return [aView inactiveWindowAcceptsMouseEvent:aEvent];
}

#pragma mark -

@interface EventThreadRunner(Private)
- (void)runEventThread;
- (void)shutdownAndReleaseCalledOnEventThread;
- (void)shutdownAndReleaseCalledOnAnyThread;
- (void)handleEvent:(CGEventRef)cgEvent type:(CGEventType)type;
@end

static EventThreadRunner* sEventThreadRunner = nil;

@implementation EventThreadRunner

+ (void)start
{
  sEventThreadRunner = [[EventThreadRunner alloc] init];
}

+ (void)stop
{
  if (sEventThreadRunner) {
    [sEventThreadRunner shutdownAndReleaseCalledOnAnyThread];
    sEventThreadRunner = nil;
  }
}

- (id)init
{
  if ((self = [super init])) {
    mThread = nil;
    [NSThread detachNewThreadSelector:@selector(runEventThread)
                             toTarget:self
                           withObject:nil];
  }
  return self;
}

static CGEventRef
HandleEvent(CGEventTapProxy aProxy, CGEventType aType,
            CGEventRef aEvent, void* aClosure)
{
  [(EventThreadRunner*)aClosure handleEvent:aEvent type:aType];
  return aEvent;
}

- (void)runEventThread
{
  char aLocal;
  profiler_register_thread("APZC Event Thread", &aLocal);
  NS_SetCurrentThreadName("APZC Event Thread");

  mThread = [NSThread currentThread];
  ProcessSerialNumber currentProcess;
  GetCurrentProcess(&currentProcess);
  CFMachPortRef eventPort =
    CGEventTapCreateForPSN(&currentProcess,
                           kCGHeadInsertEventTap,
                           kCGEventTapOptionListenOnly,
                           CGEventMaskBit(kCGEventScrollWheel),
                           HandleEvent,
                           self);
  CFRunLoopSourceRef eventPortSource =
    CFMachPortCreateRunLoopSource(kCFAllocatorSystemDefault, eventPort, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), eventPortSource, kCFRunLoopCommonModes);
  CFRunLoopRun();
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), eventPortSource, kCFRunLoopCommonModes);
  CFRelease(eventPortSource);
  CFRelease(eventPort);
  [self release];
}

- (void)shutdownAndReleaseCalledOnEventThread
{
  CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void)shutdownAndReleaseCalledOnAnyThread
{
  [self performSelector:@selector(shutdownAndReleaseCalledOnEventThread) onThread:mThread withObject:nil waitUntilDone:NO];
}

static const CGEventField kCGWindowNumberField = (const CGEventField) 51;

// Called on scroll thread
- (void)handleEvent:(CGEventRef)cgEvent type:(CGEventType)type
{
  if (type != kCGEventScrollWheel) {
    return;
  }

  int windowNumber = CGEventGetIntegerValueField(cgEvent, kCGWindowNumberField);
  NSWindow* window = [NSApp windowWithWindowNumber:windowNumber];
  if (!window || ![window isKindOfClass:[BaseWindow class]]) {
    return;
  }

  ChildView* childView = [(BaseWindow*)window mainChildView];
  [childView handleAsyncScrollEvent:cgEvent ofType:type];
}

@end

@interface NSView (MethodSwizzling)
- (BOOL)nsChildView_NSView_mouseDownCanMoveWindow;
@end

@implementation NSView (MethodSwizzling)

// All top-level browser windows belong to the ToolbarWindow class and have
// NSTexturedBackgroundWindowMask turned on in their "style" (see particularly
// [ToolbarWindow initWithContentRect:...] in nsCocoaWindow.mm).  This style
// normally means the window "may be moved by clicking and dragging anywhere
// in the window background", but we've suppressed this by giving the
// ChildView class a mouseDownCanMoveWindow method that always returns NO.
// Normally a ToolbarWindow's contentView (not a ChildView) returns YES when
// NSTexturedBackgroundWindowMask is turned on.  But normally this makes no
// difference.  However, under some (probably very unusual) circumstances
// (and only on Leopard) it *does* make a difference -- for example it
// triggers bmo bugs 431902 and 476393.  So here we make sure that a
// ToolbarWindow's contentView always returns NO from the
// mouseDownCanMoveWindow method.
- (BOOL)nsChildView_NSView_mouseDownCanMoveWindow
{
  NSWindow *ourWindow = [self window];
  NSView *contentView = [ourWindow contentView];
  if ([ourWindow isKindOfClass:[ToolbarWindow class]] && (self == contentView))
    return [ourWindow isMovableByWindowBackground];
  return [self nsChildView_NSView_mouseDownCanMoveWindow];
}

@end

#ifdef __LP64__
// When using blocks, at least on OS X 10.7, the OS sometimes calls
// +[NSEvent removeMonitor:] more than once on a single event monitor, which
// causes crashes.  See bug 678607.  We hook these methods to work around
// the problem.
@interface NSEvent (MethodSwizzling)
+ (id)nsChildView_NSEvent_addLocalMonitorForEventsMatchingMask:(unsigned long long)mask handler:(id)block;
+ (void)nsChildView_NSEvent_removeMonitor:(id)eventMonitor;
@end

// This is a local copy of the AppKit frameworks sEventObservers hashtable.
// It only stores "local monitors".  We use it to ensure that +[NSEvent
// removeMonitor:] is never called more than once on the same local monitor.
static NSHashTable *sLocalEventObservers = nil;

@implementation NSEvent (MethodSwizzling)

+ (id)nsChildView_NSEvent_addLocalMonitorForEventsMatchingMask:(unsigned long long)mask handler:(id)block
{
  if (!sLocalEventObservers) {
    sLocalEventObservers = [[NSHashTable hashTableWithOptions:
      NSHashTableStrongMemory | NSHashTableObjectPointerPersonality] retain];
  }
  id retval =
    [self nsChildView_NSEvent_addLocalMonitorForEventsMatchingMask:mask handler:block];
  if (sLocalEventObservers && retval && ![sLocalEventObservers containsObject:retval]) {
    [sLocalEventObservers addObject:retval];
  }
  return retval;
}

+ (void)nsChildView_NSEvent_removeMonitor:(id)eventMonitor
{
  if (sLocalEventObservers && [eventMonitor isKindOfClass: ::NSClassFromString(@"_NSLocalEventObserver")]) {
    if (![sLocalEventObservers containsObject:eventMonitor]) {
      return;
    }
    [sLocalEventObservers removeObject:eventMonitor];
  }
  [self nsChildView_NSEvent_removeMonitor:eventMonitor];
}

@end
#endif // #ifdef __LP64__
