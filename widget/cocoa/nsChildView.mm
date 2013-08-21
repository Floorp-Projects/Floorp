/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

#include <unistd.h>
#include <math.h>
 
#include "nsChildView.h"
#include "nsCocoaWindow.h"

#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
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
#include "nsNPAPIPluginInstance.h"
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
#ifdef __LP64__
#include "ComplexTextInputPanel.h"
#endif

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "gfxUtils.h"
#include "nsRegion.h"
#include "Layers.h"
#include "LayerManagerOGL.h"
#include "ClientLayerManager.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "GLTextureImage.h"
#include "GLContextProvider.h"
#include "mozilla/layers/GLManager.h"
#include "mozilla/layers/CompositorCocoaWidgetHelper.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/BasicCompositor.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
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

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;
using namespace mozilla::widget;

#undef DEBUG_UPDATE
#undef INVALIDATE_DEBUGGING  // flash areas as they are invalidated

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION 100

#ifdef PR_LOGGING
PRLogModuleInfo* sCocoaLog = nullptr;
#endif

extern "C" {
  CG_EXTERN void CGContextResetCTM(CGContextRef);
  CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
  CG_EXTERN void CGContextResetClip(CGContextRef);

  typedef CFTypeRef CGSRegionObj;
  CGError CGSNewRegionWithRect(const CGRect *rect, CGSRegionObj *outRegion);
}

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu; // Application menu shared by all menubars

bool gChildViewMethodsSwizzled = false;

extern nsISupportsArray *gDraggedTransferables;

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

@interface ChildView(Private)

// sets up our view, attaching it to its owning gecko view
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild;
- (void)forceRefreshOpenGL;

// set up a gecko mouse event based on a cocoa mouse event
- (void) convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(nsInputEvent*)outGeckoEvent;

- (NSMenu*)contextMenu;

- (void)setIsPluginView:(BOOL)aIsPlugin;
- (void)setPluginEventModel:(NPEventModel)eventModel;
- (void)setPluginDrawingModel:(NPDrawingModel)drawingModel;
- (NPDrawingModel)pluginDrawingModel;

- (BOOL)isRectObscuredBySubview:(NSRect)inRect;

- (void)processPendingRedraws;

- (void)drawRect:(NSRect)aRect inContext:(CGContextRef)aContext;
- (nsIntRegion)nativeDirtyRegionWithBoundingRect:(NSRect)aRect;
- (BOOL)isUsingMainThreadOpenGL;
- (BOOL)isUsingOpenGL;
- (void)drawUsingOpenGL;
- (void)drawUsingOpenGLCallback;

- (BOOL)hasRoundedBottomCorners;
- (CGFloat)cornerRadius;
- (void)clearCorners;

// Overlay drawing functions for traditional CGContext drawing
- (void)drawTitleString;
- (void)drawTitlebarHighlight;
- (void)maskTopCornersInContext:(CGContextRef)aContext;

// Called using performSelector:withObject:afterDelay:0 to release
// aWidgetArray (and its contents) the next time through the run loop.
- (void)releaseWidgets:(NSArray*)aWidgetArray;

#if USE_CLICK_HOLD_CONTEXTMENU
 // called on a timer two seconds after a mouse down to see if we should display
 // a context menu (click-hold)
- (void)clickHoldCallback:(id)inEvent;
#endif

#ifdef ACCESSIBILITY
- (id<mozAccessible>)accessible;
#endif

- (BOOL)inactiveWindowAcceptsMouseEvent:(NSEvent*)aEvent;

@end

@interface NSView(NSThemeFrameCornerRadius)
- (float)roundedCornerRadius;
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

/* Convenience routines to go from a gecko rect to cocoa NSRects and back
 *
 * Gecko rects (nsRect) contain an origin (x,y) in a coordinate
 * system with (0,0) in the top-left of the screen. Cocoa rects
 * (NSRect) contain an origin (x,y) in a coordinate system with
 * (0,0) in the bottom-left of the screen. Both nsRect and NSRect
 * contain width/height info, with no difference in their use.
 * If a Cocoa rect is from a flipped view, there is no need to
 * convert coordinate systems.
 */

static inline void
NSRectToGeckoRect(const NSRect & inCocoaRect, nsIntRect & outGeckoRect)
{
  outGeckoRect.x = NSToIntRound(inCocoaRect.origin.x);
  outGeckoRect.y = NSToIntRound(inCocoaRect.origin.y);
  outGeckoRect.width = NSToIntRound(inCocoaRect.origin.x + inCocoaRect.size.width) - outGeckoRect.x;
  outGeckoRect.height = NSToIntRound(inCocoaRect.origin.y + inCocoaRect.size.height) - outGeckoRect.y;
}

static inline void 
ConvertGeckoRectToMacRect(const nsIntRect& aRect, Rect& outMacRect)
{
  outMacRect.left = aRect.x;
  outMacRect.top = aRect.y;
  outMacRect.right = aRect.x + aRect.width;
  outMacRect.bottom = aRect.y + aRect.height;
}

// Flips a screen coordinate from a point in the cocoa coordinate system (bottom-left rect) to a point
// that is a "flipped" cocoa coordinate system (starts in the top-left).
static inline void
FlipCocoaScreenCoordinate(NSPoint &inPoint)
{
  inPoint.y = nsCocoaUtils::FlippedScreenY(inPoint.y);
}

void EnsureLogInitialized()
{
#ifdef PR_LOGGING
  if (!sCocoaLog) {
    sCocoaLog = PR_NewLogModule("nsCocoaWidgets");
  }
#endif // PR_LOGGING
}

namespace {

// Manages a texture which can resize dynamically, binds to the
// LOCAL_GL_TEXTURE_RECTANGLE_ARB texture target and is automatically backed
// by a power-of-two size GL texture. The latter two features are used for
// compatibility with older Mac hardware which we block GL layers on.
// RectTextureImages are used both for accelerated GL layers drawing and for
// OMTC BasicLayers drawing.
class RectTextureImage {
public:
  RectTextureImage(GLContext* aGLContext)
   : mGLContext(aGLContext)
   , mTexture(0)
   , mInUpdate(false)
  {}

  virtual ~RectTextureImage();

  TemporaryRef<gfx::DrawTarget>
    BeginUpdate(const nsIntSize& aNewSize,
                const nsIntRegion& aDirtyRegion = nsIntRegion());
  void EndUpdate(bool aKeepSurface = false);

  void UpdateIfNeeded(const nsIntSize& aNewSize,
                      const nsIntRegion& aDirtyRegion,
                      void (^aCallback)(gfx::DrawTarget*, const nsIntRegion&))
  {
    RefPtr<gfx::DrawTarget> drawTarget = BeginUpdate(aNewSize, aDirtyRegion);
    if (drawTarget) {
      aCallback(drawTarget, GetUpdateRegion());
      EndUpdate();
    }
  }

  void UpdateFromDrawTarget(const nsIntSize& aNewSize,
                            const nsIntRegion& aDirtyRegion,
                            gfx::DrawTarget* aFromDrawTarget);

  nsIntRegion GetUpdateRegion() {
    MOZ_ASSERT(mInUpdate, "update region only valid during update");
    return mUpdateRegion;
  }

  void Draw(mozilla::layers::GLManager* aManager,
            const nsIntPoint& aLocation,
            const gfx3DMatrix& aTransform = gfx3DMatrix());

  static nsIntSize TextureSizeForSize(const nsIntSize& aSize);

protected:

  RefPtr<gfx::DrawTarget> mUpdateDrawTarget;
  GLContext* mGLContext;
  nsIntRegion mUpdateRegion;
  nsIntSize mUsedSize;
  nsIntSize mBufferSize;
  nsIntSize mTextureSize;
  GLuint mTexture;
  bool mInUpdate;
};

// Used for OpenGL drawing from the compositor thread for OMTC BasicLayers.
// We need to use OpenGL for this because there seems to be no other robust
// way of drawing from a secondary thread without locking, which would cause
// deadlocks in our setup. See bug 882523.
class GLPresenter : public GLManager
{
public:
  static GLPresenter* CreateForWindow(nsIWidget* aWindow)
  {
    nsRefPtr<GLContext> context = gl::GLContextProvider::CreateForWindow(aWindow);
    return context ? new GLPresenter(context) : nullptr;
  }

  GLPresenter(GLContext* aContext);
  virtual ~GLPresenter();

  virtual GLContext* gl() const MOZ_OVERRIDE { return mGLContext; }
  virtual ShaderProgramOGL* GetProgram(ShaderProgramType aType) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aType == RGBARectLayerProgramType, "unexpected program type");
    return mRGBARectProgram;
  }
  virtual void BindAndDrawQuad(ShaderProgramOGL *aProg) MOZ_OVERRIDE;

  void BeginFrame(nsIntSize aRenderSize);
  void EndFrame();

  NSOpenGLContext* GetNSOpenGLContext()
  {
    return static_cast<NSOpenGLContext*>(
      mGLContext->GetNativeData(GLContext::NativeGLContext));
  }

protected:
  nsRefPtr<mozilla::gl::GLContext> mGLContext;
  nsAutoPtr<mozilla::layers::ShaderProgramOGL> mRGBARectProgram;
  GLuint mQuadVBO;
};

} // unnamed namespace

#pragma mark -

nsChildView::nsChildView() : nsBaseWidget()
, mView(nullptr)
, mParentView(nullptr)
, mParentWidget(nullptr)
, mEffectsLock("WidgetEffects")
, mShowsResizeIndicator(false)
, mHasRoundedBottomCorners(false)
, mIsCoveringTitlebar(false)
, mBackingScaleFactor(0.0)
, mVisible(false)
, mDrawing(false)
, mPluginDrawing(false)
, mIsDispatchPaint(false)
, mPluginInstanceOwner(nullptr)
{
  EnsureLogInitialized();

  memset(&mPluginCGContext, 0, sizeof(mPluginCGContext));

  SetBackgroundColor(NS_RGB(255, 255, 255));
  SetForegroundColor(NS_RGB(0, 0, 0));
}

nsChildView::~nsChildView()
{
  // Notify the children that we're gone.  childView->ResetParent() can change
  // our list of children while it's being iterated, so the way we iterate the
  // list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    nsChildView* childView = static_cast<nsChildView*>(kid);
    kid = kid->GetPrevSibling();
    childView->ResetParent();
  }
  
  NS_WARN_IF_FALSE(mOnDestroyCalled, "nsChildView object destroyed without calling Destroy()");

  DestroyCompositor();

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

NS_IMPL_ISUPPORTS_INHERITED1(nsChildView, nsBaseWidget, nsIPluginWidget)

nsresult nsChildView::Create(nsIWidget *aParent,
                             nsNativeWidget aNativeParent,
                             const nsIntRect &aRect,
                             nsDeviceContext *aContext,
                             nsWidgetInitData *aInitData)
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
    if (nsCocoaFeatures::OnLionOrLater()) {
      nsToolkit::SwizzleMethods([NSEvent class], @selector(addLocalMonitorForEventsMatchingMask:handler:),
                                @selector(nsChildView_NSEvent_addLocalMonitorForEventsMatchingMask:handler:),
                                true);
      nsToolkit::SwizzleMethods([NSEvent class], @selector(removeMonitor:),
                                @selector(nsChildView_NSEvent_removeMonitor:), true);
    }
#else
    TextInputHandler::SwizzleMethods();
#endif
    gChildViewMethodsSwizzled = true;
  }

  mBounds = aRect;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  BaseCreate(aParent, aRect, aContext, aInitData);

  // inherit things from the parent view and create our parallel 
  // NSView in the Cocoa display system
  mParentView = nil;
  if (aParent) {
    // This is the case when we're the popup content view of a popup window.
    SetBackgroundColor(aParent->GetBackgroundColor());
    SetForegroundColor(aParent->GetForegroundColor());

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

  [(ChildView*)mView setIsPluginView:(mWindowType == eWindowType_plugin)];

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

NS_IMETHODIMP nsChildView::Destroy()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mOnDestroyCalled)
    return NS_OK;
  mOnDestroyCalled = true;

  [mView widgetDestroyed];

  nsBaseWidget::Destroy();

  NotifyWindowDestroyed();
  mParentWidget = nil;

  TearDownView();

  nsBaseWidget::OnDestroy();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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

    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_PLUGIN_PORT_CG:
    {
      // The NP_CGContext pointer should always be NULL in the Cocoa event model.
      if ([(ChildView*)mView pluginEventModel] == NPEventModelCocoa)
        return nullptr;

      UpdatePluginPort();
      retVal = (void*)&mPluginCGContext;
      break;
    }
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

void nsChildView::HidePlugin()
{
  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "HidePlugin called on non-plugin view");
}

void nsChildView::UpdatePluginPort()
{
  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "UpdatePluginPort called on non-plugin view");

  // [NSGraphicsContext currentContext] is supposed to "return the
  // current graphics context of the current thread."  But sometimes
  // (when called while mView isn't focused for drawing) it returns a
  // graphics context for the wrong window.  [window graphicsContext]
  // (which "provides the graphics context associated with the window
  // for the current thread") seems always to return the "right"
  // graphics context.  See bug 500130.
  mPluginCGContext.context = NULL;
  mPluginCGContext.window = NULL;
}

static void HideChildPluginViews(NSView* aView)
{
  NSArray* subviews = [aView subviews];

  for (unsigned int i = 0; i < [subviews count]; ++i) {
    NSView* view = [subviews objectAtIndex: i];

    if (![view isKindOfClass:[ChildView class]])
      continue;

    ChildView* childview = static_cast<ChildView*>(view);
    if ([childview isPluginView]) {
      nsChildView* widget = static_cast<nsChildView*>([childview widget]);
      if (widget) {
        widget->HidePlugin();
      }
    } else {
      HideChildPluginViews(view);
    }
  }
}

// Hide or show this component
NS_IMETHODIMP nsChildView::Show(bool aState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (aState != mVisible) {
    // Provide an autorelease pool because this gets called during startup
    // on the "hidden window", resulting in cocoa object leakage if there's
    // no pool in place.
    nsAutoreleasePool localPool;

    [mView setHidden:!aState];
    mVisible = aState;
    if (!mVisible && IsPluginView())
      HidePlugin();
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Change the parent of this widget
NS_IMETHODIMP
nsChildView::SetParent(nsIWidget* aNewParent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mOnDestroyCalled)
    return NS_OK;

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

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsChildView::ReparentNativeWidget(nsIWidget* aNewParent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_PRECONDITION(aNewParent, "");

  if (mOnDestroyCalled)
    return NS_OK;

  NSView<mozView>* newParentView =
   (NSView<mozView>*)aNewParent->GetNativeData(NS_NATIVE_WIDGET); 
  NS_ENSURE_TRUE(newParentView, NS_ERROR_FAILURE);

  // we hold a ref to mView, so this is safe
  [mView removeFromSuperview];
  mParentView = newParentView;
  [mParentView addSubview:mView];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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

NS_IMETHODIMP nsChildView::Enable(bool aState)
{
  return NS_OK;
}

bool nsChildView::IsEnabled() const
{
  return true;
}

NS_IMETHODIMP nsChildView::SetFocus(bool aRaise)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSWindow* window = [mView window];
  if (window)
    [window makeFirstResponder:mView];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Override to set the cursor on the mac
NS_IMETHODIMP nsChildView::SetCursor(nsCursor aCursor)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if ([mView isDragInProgress])
    return NS_OK; // Don't change the cursor during dragging.

  nsBaseWidget::SetCursor(aCursor);
  return [[nsCursorManager sharedInstance] setCursor:aCursor];

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// implement to fix "hidden virtual function" warning
NS_IMETHODIMP nsChildView::SetCursor(imgIContainer* aCursor,
                                      uint32_t aHotspotX, uint32_t aHotspotY)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsBaseWidget::SetCursor(aCursor, aHotspotX, aHotspotY);
  return [[nsCursorManager sharedInstance] setCursorWithImage:aCursor hotSpotX:aHotspotX hotSpotY:aHotspotY];

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#pragma mark -

// Get this component dimension
NS_IMETHODIMP nsChildView::GetBounds(nsIntRect &aRect)
{
  if (!mView) {
    aRect = mBounds;
  } else {
    aRect = CocoaPointsToDevPixels([mView frame]);
  }
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetClientBounds(nsIntRect &aRect)
{
  GetBounds(aRect);
  if (!mParentWidget) {
    // For top level widgets we want the position on screen, not the position
    // of this view inside the window.
    MOZ_ASSERT(mWindowType != eWindowType_plugin, "plugin widgets should have parents");
    aRect.MoveTo(WidgetToScreenOffset());
  }
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetScreenBounds(nsIntRect &aRect)
{
  GetBounds(aRect);
  aRect.MoveTo(WidgetToScreenOffset());
  return NS_OK;
}

double
nsChildView::GetDefaultScaleInternal()
{
  return BackingScaleFactor();
}

CGFloat
nsChildView::BackingScaleFactor()
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

  if (mWidgetListener && !mWidgetListener->GetXULWindow()) {
    nsIPresShell* presShell = mWidgetListener->GetPresShell();
    if (presShell) {
      presShell->BackingScaleFactorChanged();
    }
  }

  if (IsPluginView()) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsGUIEvent guiEvent(true, NS_PLUGIN_RESOLUTION_CHANGED, this);
    guiEvent.time = PR_IntervalNow();
    DispatchEvent(&guiEvent, status);
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

NS_IMETHODIMP nsChildView::ConstrainPosition(bool aAllowSlop,
                                             int32_t *aX, int32_t *aY)
{
  return NS_OK;
}

// Move this component, aX and aY are in the parent widget coordinate system
NS_IMETHODIMP nsChildView::Move(double aX, double aY)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  int32_t x = NSToIntRound(aX);
  int32_t y = NSToIntRound(aY);

  if (!mView || (mBounds.x == x && mBounds.y == y))
    return NS_OK;

  mBounds.x = x;
  mBounds.y = y;

  [mView setFrame:DevPixelsToCocoaPoints(mBounds)];

  if (mVisible)
    [mView setNeedsDisplay:YES];

  NotifyRollupGeometryChange();
  ReportMoveEvent();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::Resize(double aWidth, double aHeight, bool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);

  if (!mView || (mBounds.width == width && mBounds.height == height))
    return NS_OK;

  mBounds.width  = width;
  mBounds.height = height;

  [mView setFrame:DevPixelsToCocoaPoints(mBounds)];

  if (mVisible && aRepaint)
    [mView setNeedsDisplay:YES];

  NotifyRollupGeometryChange();
  ReportSizeEvent();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::Resize(double aX, double aY,
                                  double aWidth, double aHeight, bool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  int32_t x = NSToIntRound(aX);
  int32_t y = NSToIntRound(aY);
  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);

  BOOL isMoving = (mBounds.x != x || mBounds.y != y);
  BOOL isResizing = (mBounds.width != width || mBounds.height != height);
  if (!mView || (!isMoving && !isResizing))
    return NS_OK;

  if (isMoving) {
    mBounds.x = x;
    mBounds.y = y;
  }
  if (isResizing) {
    mBounds.width  = width;
    mBounds.height = height;
  }

  [mView setFrame:DevPixelsToCocoaPoints(mBounds)];

  if (mVisible && aRepaint)
    [mView setNeedsDisplay:YES];

  NotifyRollupGeometryChange();
  if (isMoving) {
    ReportMoveEvent();
    if (mOnDestroyCalled)
      return NS_OK;
  }
  if (isResizing)
    ReportSizeEvent();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

static const int32_t resizeIndicatorWidth = 15;
static const int32_t resizeIndicatorHeight = 15;
bool nsChildView::ShowsResizeIndicator(nsIntRect* aResizerRect)
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

// In QuickDraw mode the coordinate system used here should be that of the
// browser window's content region (defined as everything but the 22-pixel
// high titlebar).  But in CoreGraphics mode the coordinate system should be
// that of the browser window as a whole (including its titlebar).  Both
// coordinate systems have a top-left origin.  See bmo bug 474491.
//
// There's a bug in this method's code -- it currently uses the QuickDraw
// coordinate system for both the QuickDraw and CoreGraphics drawing modes.
// This bug is fixed by the patch for bug 474491.  But the Flash plugin (both
// version 10.0.12.36 from Adobe and version 9.0 r151 from Apple) has Mozilla-
// specific code to work around this bug, which breaks when we fix it (see bmo
// bug 477077).  So we'll need to coordinate releasing a fix for this bug with
// Adobe and other major plugin vendors that support the CoreGraphics mode.
//
// outClipRect and outOrigin are in display pixels, not device pixels.
NS_IMETHODIMP nsChildView::GetPluginClipRect(nsIntRect& outClipRect, nsIntPoint& outOrigin, bool& outWidgetVisible)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "GetPluginClipRect must only be called on a plugin widget");
  if (mWindowType != eWindowType_plugin) return NS_ERROR_FAILURE;
  
  NSWindow* window = [mView window];
  if (!window) return NS_ERROR_FAILURE;
  
  NSPoint viewOrigin = [mView convertPoint:NSZeroPoint toView:nil];
  NSRect frame = [[window contentView] frame];
  viewOrigin.y = frame.size.height - viewOrigin.y;
  
  // set up the clipping region for plugins.
  NSRect visibleBounds = [mView visibleRect];
  NSPoint clipOrigin   = [mView convertPoint:visibleBounds.origin toView:nil];
  
  // Convert from cocoa to QuickDraw coordinates
  clipOrigin.y = frame.size.height - clipOrigin.y;
  
  outClipRect.x = NSToIntRound(clipOrigin.x);
  outClipRect.y = NSToIntRound(clipOrigin.y);

  // need to convert view's origin to window coordinates.
  // then, encode as "SetOrigin" ready values.
  outOrigin.x = -NSToIntRound(viewOrigin.x);
  outOrigin.y = -NSToIntRound(viewOrigin.y);

  if (IsVisible() && [mView window] != nil) {
    outClipRect.width  = NSToIntRound(visibleBounds.origin.x + visibleBounds.size.width) - NSToIntRound(visibleBounds.origin.x);
    outClipRect.height = NSToIntRound(visibleBounds.origin.y + visibleBounds.size.height) - NSToIntRound(visibleBounds.origin.y);

    if (mClipRects) {
      nsIntRect clipBounds;
      for (uint32_t i = 0; i < mClipRectCount; ++i) {
        NSRect cocoaPoints = DevPixelsToCocoaPoints(mClipRects[i]);
        clipBounds.UnionRect(clipBounds, nsIntRect(NSToIntRound(cocoaPoints.origin.x),
                                                   NSToIntRound(cocoaPoints.origin.y),
                                                   NSToIntRound(cocoaPoints.size.width),
                                                   NSToIntRound(cocoaPoints.size.height)));
      }
      outClipRect.IntersectRect(outClipRect, clipBounds - outOrigin);
    }

    // XXXroc should this be !outClipRect.IsEmpty()?
    outWidgetVisible = true;
  }
  else {
    outClipRect.width = 0;
    outClipRect.height = 0;
    outWidgetVisible = false;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::StartDrawPlugin()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "StartDrawPlugin must only be called on a plugin widget");
  if (mWindowType != eWindowType_plugin) return NS_ERROR_FAILURE;

  // This code is necessary for CoreGraphics in 32-bit builds.
  // See comments below about why. In 64-bit CoreGraphics mode we will not keep
  // this region up to date, plugins should not depend on it.
#ifndef __LP64__
  NSWindow* window = [mView window];
  if (!window)
    return NS_ERROR_FAILURE;

  // In QuickDraw drawing mode we used to prevent reentrant handling of any
  // plugin event. But in CoreGraphics drawing mode only do this if the current
  // plugin event isn't an update/paint event.  This allows popupcontextmenu()
  // to work properly from a plugin that supports the Cocoa event model,
  // without regressing bug 409615.  See bug 435041.  (StartDrawPlugin() and
  // EndDrawPlugin() wrap every call to nsIPluginInstance::HandleEvent() --
  // not just calls that "draw" or paint.)
  if (mIsDispatchPaint && mPluginDrawing) {
    return NS_ERROR_FAILURE;
  }

  // It appears that the WindowRef from which we get the plugin port undergoes the
  // traditional BeginUpdate/EndUpdate cycle, which, if you recall, sets the visible
  // region to the intersection of the visible region and the update region. Since
  // we don't know here if we're being drawn inside a BeginUpdate/EndUpdate pair
  // (which seem to occur in [NSWindow display]), and we don't want to have the burden
  // of correctly doing Carbon invalidates of the plugin rect, we manually set the
  // visible region to be the entire port every time. It is necessary to set up our
  // window's port even for CoreGraphics plugins, because they may still use Carbon
  // internally (see bug #420527 for details).
  //
  // Don't use this code if any of the QuickDraw APIs it currently requires are
  // missing (as they probably will be on OS X 10.8 and up).
  if (::NewRgn && ::GetQDGlobalsThePort && ::GetGWorld && ::SetGWorld &&
      ::IsPortOffscreen && ::GetMainDevice && ::SetOrigin && ::RectRgn &&
      ::SetPortVisibleRegion && ::SetPortClipRegion && ::DisposeRgn) {
    RgnHandle pluginRegion = ::NewRgn();
    if (pluginRegion) {
      CGrafPtr port = ::GetWindowPort(WindowRef([window windowRef]));
      bool portChanged = (port != CGrafPtr(::GetQDGlobalsThePort()));
      CGrafPtr oldPort;
      GDHandle oldDevice;

      if (portChanged) {
        ::GetGWorld(&oldPort, &oldDevice);
        ::SetGWorld(port, ::IsPortOffscreen(port) ? nullptr : ::GetMainDevice());
      }

      ::SetOrigin(0, 0);

      nsIntRect clipRect; // this is in native window coordinates
      nsIntPoint origin;
      bool visible;
      GetPluginClipRect(clipRect, origin, visible);

      // XXX if we're not visible, set an empty clip region?
      Rect pluginRect;
      ConvertGeckoRectToMacRect(clipRect, pluginRect);

      ::RectRgn(pluginRegion, &pluginRect);
      ::SetPortVisibleRegion(port, pluginRegion);
      ::SetPortClipRegion(port, pluginRegion);

      // now set up the origin for the plugin
      ::SetOrigin(origin.x, origin.y);

      ::DisposeRgn(pluginRegion);

      if (portChanged) {
        ::SetGWorld(oldPort, oldDevice);
      }
    }
  }
#endif

  mPluginDrawing = true;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::EndDrawPlugin()
{
  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "EndDrawPlugin must only be called on a plugin widget");
  if (mWindowType != eWindowType_plugin) return NS_ERROR_FAILURE;

  mPluginDrawing = false;
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetPluginInstanceOwner(nsIPluginInstanceOwner* aInstanceOwner)
{
  mPluginInstanceOwner = aInstanceOwner;

  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetPluginEventModel(int inEventModel)
{
  [(ChildView*)mView setPluginEventModel:(NPEventModel)inEventModel];
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetPluginEventModel(int* outEventModel)
{
  *outEventModel = [(ChildView*)mView pluginEventModel];
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetPluginDrawingModel(int inDrawingModel)
{
  [(ChildView*)mView setPluginDrawingModel:(NPDrawingModel)inDrawingModel];
  return NS_OK;
}

NS_IMETHODIMP nsChildView::StartComplexTextInputForCurrentEvent()
{
  return mTextInputHandler->StartComplexTextInputForCurrentEvent();
}

nsresult nsChildView::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                               int32_t aNativeKeyCode,
                                               uint32_t aModifierFlags,
                                               const nsAString& aCharacters,
                                               const nsAString& aUnmodifiedCharacters)
{
  return mTextInputHandler->SynthesizeNativeKeyEvent(aNativeKeyboardLayout,
                                                     aNativeKeyCode,
                                                     aModifierFlags,
                                                     aCharacters,
                                                     aUnmodifiedCharacters);
}

nsresult nsChildView::SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                 uint32_t aNativeMessage,
                                                 uint32_t aModifierFlags)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSPoint pt =
    nsCocoaUtils::DevPixelsToCocoaPoints(aPoint, BackingScaleFactor());

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(NSPointToCGPoint(pt));
  CGAssociateMouseAndMouseCursorPosition(true);

  // aPoint is given with the origin on the top left, but convertScreenToBase
  // expects a point in a coordinate system that has its origin on the bottom left.
  NSPoint screenPoint = NSMakePoint(pt.x, nsCocoaUtils::FlippedScreenY(pt.y));
  NSPoint windowPoint = [[mView window] convertScreenToBase:screenPoint];

  NSEvent* event = [NSEvent mouseEventWithType:aNativeMessage
                                      location:windowPoint
                                 modifierFlags:aModifierFlags
                                     timestamp:[NSDate timeIntervalSinceReferenceDate]
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

// Used for testing native menu system structure and event handling.
NS_IMETHODIMP nsChildView::ActivateNativeMenuItemAt(const nsAString& indexString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString* locationString = [NSString stringWithCharacters:indexString.BeginReading() length:indexString.Length()];
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
NS_IMETHODIMP nsChildView::ForceUpdateNativeMenuAt(const nsAString& indexString)
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
NS_IMETHODIMP nsChildView::Invalidate(const nsIntRect &aRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mView || !mVisible)
    return NS_OK;

  NS_ASSERTION(GetLayerManager()->GetBackendType() != LAYERS_CLIENT,
               "Shouldn't need to invalidate with accelerated OMTC layers!");

  if ([NSView focusView]) {
    // if a view is focussed (i.e. being drawn), then postpone the invalidate so that we
    // don't lose it.
    [mView setNeedsPendingDisplayInRect:DevPixelsToCocoaPoints(aRect)];
  }
  else {
    [mView setNeedsDisplayInRect:DevPixelsToCocoaPoints(aRect)];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

bool
nsChildView::ComputeShouldAccelerate(bool aDefault)
{
  // Don't use OpenGL for transparent windows or for popup windows.
  if (!mView || ![[mView window] isOpaque] ||
      [[mView window] isKindOfClass:[PopupWindow class]])
    return false;

  return nsBaseWidget::ComputeShouldAccelerate(aDefault);
}

bool
nsChildView::ShouldUseOffMainThreadCompositing()
{
  // When acceleration is off, default to false, but allow force-enabling
  // using the layers.offmainthreadcomposition.prefer-basic pref.
  if (!ComputeShouldAccelerate(mUseLayersAcceleration) &&
      !Preferences::GetBool("layers.offmainthreadcomposition.prefer-basic", false)) {
    return false;
  }

  // Don't use OMTC (which requires OpenGL) for transparent windows or for
  // popup windows.
  if (!mView || ![[mView window] isOpaque] ||
      [[mView window] isKindOfClass:[PopupWindow class]])
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
  for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& config = aConfigurations[i];
    nsChildView* child = static_cast<nsChildView*>(config.mChild);
#ifdef DEBUG
    nsWindowType kidType;
    child->GetWindowType(kidType);
#endif
    NS_ASSERTION(kidType == eWindowType_plugin,
                 "Configured widget is not a plugin type");
    NS_ASSERTION(child->GetParent() == this,
                 "Configured widget is not a child of the right widget");

    // nsIWidget::Show() doesn't get called on plugin widgets unless we call
    // it from here.  See bug 592563.
    child->Show(!config.mClipRegion.IsEmpty());

    child->Resize(
        config.mBounds.x, config.mBounds.y,
        config.mBounds.width, config.mBounds.height,
        false);

    // Store the clip region here in case GetPluginClipRect needs it.
    child->StoreWindowClipRegion(config.mClipRegion);
  }
  return NS_OK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
NS_IMETHODIMP nsChildView::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, event->widget, event, nsAutoCString("something"), 0);
#endif

  NS_ASSERTION(!(mTextInputHandler && mTextInputHandler->IsIMEComposing() &&
                 NS_IS_KEY_EVENT(event)),
    "Any key events should not be fired during IME composing");

  if (event->mFlags.mIsSynthesizedForTests && NS_IS_KEY_EVENT(event)) {
    nsKeyEvent* keyEvent = reinterpret_cast<nsKeyEvent*>(event);
    nsresult rv = mTextInputHandler->AttachNativeKeyEvent(*keyEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aStatus = nsEventStatus_eIgnore;

  nsIWidgetListener* listener = mWidgetListener;

  // If the listener is NULL, check if the parent is a popup. If it is, then
  // this child is the popup content view attached to a popup. Get the
  // listener from the parent popup instead.
  nsCOMPtr<nsIWidget> kungFuDeathGrip = do_QueryInterface(mParentWidget ? mParentWidget : this);
  if (!listener && mParentWidget) {
    nsWindowType type;
    mParentWidget->GetWindowType(type);
    if (type == eWindowType_popup) {
      // Check just in case event->widget isn't this widget
      if (event->widget)
        listener = event->widget->GetWidgetListener();
      if (!listener) {
        event->widget = mParentWidget;
        listener = mParentWidget->GetWidgetListener();
      }
    }
  }

  if (listener)
    aStatus = listener->HandleEvent(event, mUseAttachedEvents);

  return NS_OK;
}

bool nsChildView::DispatchWindowEvent(nsGUIEvent &event)
{
  nsEventStatus status;
  DispatchEvent(&event, status);
  return ConvertStatus(status);
}

nsIWidget*
nsChildView::GetWidgetForListenerEvents()
{
  // If there is no listener, use the parent popup's listener if that exists.
  if (!mWidgetListener && mParentWidget) {
    nsWindowType type;
    mParentWidget->GetWindowType(type);
    if (type == eWindowType_popup) {
      return mParentWidget;
    }
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

bool nsChildView::PaintWindow(nsIntRegion aRegion)
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

#pragma mark -

void nsChildView::ReportMoveEvent()
{
  if (mWidgetListener)
    mWidgetListener->WindowMoved(this, mBounds.x, mBounds.y);
}

void nsChildView::ReportSizeEvent()
{
  if (mWidgetListener)
    mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
}

#pragma mark -

//    Return the offset between this child view and the screen.
//    @return       -- widget origin in device-pixel coords
nsIntPoint nsChildView::WidgetToScreenOffset()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSPoint origin = NSMakePoint(0, 0);

  // 1. First translate view origin point into window coords.
  // The returned point is in bottom-left coordinates.
  origin = [mView convertPoint:origin toView:nil];

  // 2. We turn the window-coord rect's origin into screen (still bottom-left) coords.
  origin = [[mView window] convertBaseToScreen:origin];

  // 3. Since we're dealing in bottom-left coords, we need to make it top-left coords
  //    before we pass it back to Gecko.
  FlipCocoaScreenCoordinate(origin);

  // convert to device pixels
  return CocoaPointsToDevPixels(origin);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsIntPoint(0,0));
}

NS_IMETHODIMP nsChildView::CaptureRollupEvents(nsIRollupListener * aListener,
                                               bool aDoCapture)
{
  // this never gets called, only top-level windows can be rollup widgets
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetTitle(const nsAString& title)
{
  // child views don't have titles
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetAttention(int32_t aCycleCount)
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

NS_IMETHODIMP
nsChildView::NotifyIME(NotificationToIME aNotification)
{
  switch (aNotification) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
      mTextInputHandler->CommitIMEComposition();
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
      mTextInputHandler->CancelIMEComposition();
      return NS_OK;
    case NOTIFY_IME_OF_FOCUS:
      if (mInputContext.IsPasswordEditor()) {
        TextInputHandler::EnableSecureEventInput();
      }

      NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
      mTextInputHandler->OnFocusChangeInGecko(true);
      return NS_OK;
    case NOTIFY_IME_OF_BLUR:
      // When we're going to be deactive, we must disable the secure event input
      // mode, see the Carbon Event Manager Reference.
      TextInputHandler::EnsureSecureEventInputDisabled();

      NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
      mTextInputHandler->OnFocusChangeInGecko(false);
      return NS_OK;
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      NS_ENSURE_TRUE(mTextInputHandler, NS_ERROR_NOT_AVAILABLE);
      mTextInputHandler->OnSelectionChange();
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(void)
nsChildView::SetInputContext(const InputContext& aContext,
                             const InputContextAction& aAction)
{
  // XXX Ideally, we should check if this instance has focus or not.
  //     However, this is called only when this widget has focus, so,
  //     it's not problem at least for now.
  if (aContext.IsPasswordEditor()) {
    TextInputHandler::EnableSecureEventInput();
  } else {
    TextInputHandler::EnsureSecureEventInputDisabled();
  }

  NS_ENSURE_TRUE_VOID(mTextInputHandler);
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

NS_IMETHODIMP_(InputContext)
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
    default:
      mInputContext.mIMEState.mOpen = IMEState::CLOSED;
      break;
  }
  mInputContext.mNativeIMEContext = [mView inputContext];
  // If input context isn't available on this widget, we should set |this|
  // instead of nullptr since nullptr means that the platform has only one
  // context per process.
  if (!mInputContext.mNativeIMEContext) {
    mInputContext.mNativeIMEContext = this;
  }
  return mInputContext;
}

nsIMEUpdatePreference
nsChildView::GetIMEUpdatePreference()
{
  return nsIMEUpdatePreference(nsIMEUpdatePreference::NOTIFY_SELECTION_CHANGE,
                               false);
}

NS_IMETHODIMP nsChildView::GetToggledKeyState(uint32_t aKeyCode,
                                              bool* aLEDState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(aLEDState);
  uint32_t key;
  switch (aKeyCode) {
    case NS_VK_CAPS_LOCK:
      key = alphaLock;
      break;
    case NS_VK_NUM_LOCK:
      key = kEventKeyModifierNumLockMask;
      break;
    // Mac doesn't support SCROLL_LOCK state.
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  uint32_t modifierFlags = ::GetCurrentKeyModifiers();
  *aLEDState = (modifierFlags & key) != 0;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NSView<mozView>* nsChildView::GetEditorView()
{
  NSView<mozView>* editorView = mView;
  // We need to get editor's view. E.g., when the focus is in the bookmark
  // dialog, the view is <panel> element of the dialog.  At this time, the key
  // events are processed the parent window's view that has native focus.
  nsQueryContentEvent textContent(true, NS_QUERY_TEXT_CONTENT, this);
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
  if (mCompositorChild) {
    LayerManagerComposite *manager =
      compositor::GetLayerManager(mCompositorParent);
    Compositor *compositor = manager->GetCompositor();

    ClientLayerManager *clientManager = static_cast<ClientLayerManager*>(GetLayerManager());
    if (clientManager->GetCompositorBackendType() == LAYERS_OPENGL) {
      CompositorOGL *compositorOGL = static_cast<CompositorOGL*>(compositor);

      NSOpenGLContext *glContext = (NSOpenGLContext *)compositorOGL->gl()->GetNativeData(GLContext::NativeGLContext);

      [(ChildView *)mView setGLContext:glContext];
    }
    [(ChildView *)mView setUsingOMTCompositor:true];
  }
}

gfxASurface*
nsChildView::GetThebesSurface()
{
  if (!mTempThebesSurface) {
    mTempThebesSurface = new gfxQuartzSurface(gfxSize(1, 1), gfxASurface::ImageFormatARGB32);
  }

  return mTempThebesSurface;
}

void
nsChildView::NotifyDirtyRegion(const nsIntRegion& aDirtyRegion)
{
  if ([(ChildView*)mView isCoveringTitlebar]) {
    // We store the dirty region so that we know what to repaint in the titlebar.
    mDirtyTitlebarRegion.Or(mDirtyTitlebarRegion, aDirtyRegion);
    mDirtyTitlebarRegion.And(mDirtyTitlebarRegion, RectContainingTitlebarControls());
  }
}

static const CGFloat kTitlebarHighlightHeight = 6;

nsIntRect
nsChildView::RectContainingTitlebarControls()
{
  // Start with a 6px high strip at the top of the window for the highlight line.
  NSRect rect = NSMakeRect(0, 0, [mView bounds].size.width,
                           kTitlebarHighlightHeight);

  // Add the rects of the titlebar controls.
  for (id view in [(BaseWindow*)[mView window] titlebarControls]) {
    rect = NSUnionRect(rect, [mView convertRect:[view bounds] fromView:view]);
  }
  return CocoaPointsToDevPixels(rect);
}

void
nsChildView::PrepareWindowEffects()
{
  MutexAutoLock lock(mEffectsLock);
  mShowsResizeIndicator = ShowsResizeIndicator(&mResizeIndicatorRect);
  mHasRoundedBottomCorners = [(ChildView*)mView hasRoundedBottomCorners];
  CGFloat cornerRadius = [(ChildView*)mView cornerRadius];
  mDevPixelCornerRadius = cornerRadius * BackingScaleFactor();
  mIsCoveringTitlebar = [(ChildView*)mView isCoveringTitlebar];
  if (mIsCoveringTitlebar) {
    mTitlebarRect = RectContainingTitlebarControls();
    UpdateTitlebarImageBuffer();
  }
}

void
nsChildView::CleanupWindowEffects()
{
  mResizerImage = nullptr;
  mCornerMaskImage = nullptr;
  mTitlebarImage = nullptr;
}

void
nsChildView::PreRender(LayerManager* aManager)
{
  nsAutoPtr<GLManager> manager(GLManager::CreateGLManager(aManager));
  if (!manager) {
    return;
  }
  NSOpenGLContext *glContext = (NSOpenGLContext *)manager->gl()->GetNativeData(GLContext::NativeGLContext);
  [(ChildView*)mView preRender:glContext];
}

void
nsChildView::DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect)
{
  nsAutoPtr<GLManager> manager(GLManager::CreateGLManager(aManager));
  if (manager) {
    DrawWindowOverlay(manager, aRect);
  }
}

void
nsChildView::DrawWindowOverlay(GLManager* aManager, nsIntRect aRect)
{
  MaybeDrawTitlebar(aManager, aRect);
  MaybeDrawResizeIndicator(aManager, aRect);
  MaybeDrawRoundedCorners(aManager, aRect);
}

static void
ClearRegion(gfx::DrawTarget *aDT, nsIntRegion aRegion)
{
  gfxUtils::ClipToRegion(aDT, aRegion);
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
nsChildView::MaybeDrawResizeIndicator(GLManager* aManager, const nsIntRect& aRect)
{
  MutexAutoLock lock(mEffectsLock);
  if (!mShowsResizeIndicator) {
    return;
  }

  if (!mResizerImage) {
    mResizerImage = new RectTextureImage(aManager->gl());
  }

  nsIntSize size = mResizeIndicatorRect.Size();
  mResizerImage->UpdateIfNeeded(size, nsIntRegion(), ^(gfx::DrawTarget* drawTarget, const nsIntRegion& updateRegion) {
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
  NSRect pathRect = NSMakeRect(0, 0, aWindowSize.width, kTitlebarHighlightHeight + 2);
  [path appendBezierPathWithRect:pathRect];
  pathRect = NSInsetRect(pathRect, aDevicePixelWidth, aDevicePixelWidth);
  CGFloat innerRadius = aRadius - aDevicePixelWidth;
  [path appendBezierPathWithRoundedRect:pathRect xRadius:innerRadius yRadius:innerRadius];
  [path addClip];

  // Now we fill the path with a subtle highlight gradient.
  NSColor* topColor = [NSColor colorWithDeviceWhite:1.0 alpha:0.4];
  NSColor* bottomColor = [NSColor colorWithDeviceWhite:1.0 alpha:0.0];
  NSGradient* gradient = [[NSGradient alloc] initWithStartingColor:topColor endingColor:bottomColor];
  [gradient drawInRect:NSMakeRect(0, 0, aWindowSize.width, kTitlebarHighlightHeight) angle:90];
  [gradient release];

  [NSGraphicsContext restoreGraphicsState];
}

// When this method is entered, mEffectsLock is already being held.
void
nsChildView::UpdateTitlebarImageBuffer()
{
  nsIntRegion dirtyTitlebarRegion = mDirtyTitlebarRegion;
  mDirtyTitlebarRegion.SetEmpty();

  nsIntSize texSize = RectTextureImage::TextureSizeForSize(mTitlebarRect.Size());
  gfx::IntSize titlebarBufferSize(texSize.width, texSize.height);
  if (!mTitlebarImageBuffer ||
      mTitlebarImageBuffer->GetSize() != titlebarBufferSize) {
    dirtyTitlebarRegion = mTitlebarRect;

    mTitlebarImageBuffer =
      gfx::Factory::CreateDrawTarget(gfx::BACKEND_COREGRAPHICS,
                                     titlebarBufferSize,
                                     gfx::FORMAT_B8G8R8A8);
  }

  if (dirtyTitlebarRegion.IsEmpty())
    return;

  ClearRegion(mTitlebarImageBuffer, dirtyTitlebarRegion);

  gfx::BorrowedCGContext borrow(mTitlebarImageBuffer);
  CGContextRef ctx = borrow.cg;

  double scale = BackingScaleFactor();
  CGContextScaleCTM(ctx, scale, scale);
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
  if ([frameView respondsToSelector:@selector(_drawTitleBar:)]) {
    [frameView _drawTitleBar:[frameView bounds]];
  }

  // Draw the titlebar controls into the titlebar image.
  for (id view in [window titlebarControls]) {
    NSRect viewFrame = [view frame];
    nsIntRect viewRect = CocoaPointsToDevPixels([mView convertRect:viewFrame fromView:frameView]);
    nsIntRegion intersection;
    intersection.And(dirtyTitlebarRegion, viewRect);
    if (intersection.IsEmpty()) {
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

    NSRect intersectRect = DevPixelsToCocoaPoints(intersection.GetBounds());
    [cell drawWithFrame:[view convertRect:intersectRect fromView:mView] inView:button];

    [NSGraphicsContext setCurrentContext:context];
    CGContextRestoreGState(ctx);
  }

  CGContextRestoreGState(ctx);

  DrawTitlebarHighlight([frameView bounds].size, [(ChildView*)mView cornerRadius],
                        DevPixelsToCocoaPoints(1));

  [NSGraphicsContext setCurrentContext:oldContext];
  borrow.Finish();

  mUpdatedTitlebarRegion.Or(mUpdatedTitlebarRegion, dirtyTitlebarRegion);
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
nsChildView::MaybeDrawTitlebar(GLManager* aManager, const nsIntRect& aRect)
{
  MutexAutoLock lock(mEffectsLock);
  if (!mIsCoveringTitlebar) {
    return;
  }

  nsIntRegion updatedTitlebarRegion;
  updatedTitlebarRegion.And(mUpdatedTitlebarRegion, mTitlebarRect);
  mUpdatedTitlebarRegion.SetEmpty();

  if (!mTitlebarImage) {
    mTitlebarImage = new RectTextureImage(aManager->gl());
  }

  mTitlebarImage->UpdateFromDrawTarget(mTitlebarRect.Size(),
                                       updatedTitlebarRegion,
                                       mTitlebarImageBuffer);

  mTitlebarImage->Draw(aManager, mTitlebarRect.TopLeft());
}

static void
DrawTopLeftCornerMask(CGContextRef aCtx, int aRadius)
{
  CGContextSetRGBFillColor(aCtx, 1.0, 1.0, 1.0, 1.0);
  CGContextFillEllipseInRect(aCtx, CGRectMake(0, 0, aRadius * 2, aRadius * 2));
}

void
nsChildView::MaybeDrawRoundedCorners(GLManager* aManager, const nsIntRect& aRect)
{
  MutexAutoLock lock(mEffectsLock);

  if (!mCornerMaskImage) {
    mCornerMaskImage = new RectTextureImage(aManager->gl());
  }

  nsIntSize size(mDevPixelCornerRadius, mDevPixelCornerRadius);
  mCornerMaskImage->UpdateIfNeeded(size, nsIntRegion(), ^(gfx::DrawTarget* drawTarget, const nsIntRegion& updateRegion) {
    ClearRegion(drawTarget, updateRegion);
    gfx::BorrowedCGContext borrow(drawTarget);
    DrawTopLeftCornerMask(borrow.cg, mDevPixelCornerRadius);
    borrow.Finish();
  });

  // Use operator destination in: multiply all 4 channels with source alpha.
  aManager->gl()->fBlendFuncSeparate(LOCAL_GL_ZERO, LOCAL_GL_SRC_ALPHA,
                                     LOCAL_GL_ZERO, LOCAL_GL_SRC_ALPHA);

  gfx3DMatrix flipX = gfx3DMatrix::ScalingMatrix(-1, 1, 1);
  gfx3DMatrix flipY = gfx3DMatrix::ScalingMatrix(1, -1, 1);

  if (mIsCoveringTitlebar) {
    // Mask the top corners.
    mCornerMaskImage->Draw(aManager, aRect.TopLeft());
    mCornerMaskImage->Draw(aManager, aRect.TopRight(), flipX);
  }

  if (mHasRoundedBottomCorners) {
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
    if ((g.mWidgetType == NS_THEME_WINDOW_TITLEBAR) &&
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
    if ((g.mWidgetType == NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR ||
         g.mWidgetType == NS_THEME_TOOLBAR) &&
        g.mRect.X() <= 0 &&
        g.mRect.XMost() >= aWindowWidth &&
        g.mRect.Y() <= aTitlebarBottom) {
      unifiedToolbarBottom = std::max(unifiedToolbarBottom, g.mRect.YMost());
    }
  }
  return unifiedToolbarBottom;
}

void
nsChildView::UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries)
{
  if (![mView window] || ![[mView window] isKindOfClass:[ToolbarWindow class]])
    return;

  int32_t windowWidth = mBounds.width;
  int32_t titlebarBottom = FindTitlebarBottom(aThemeGeometries, windowWidth);
  int32_t unifiedToolbarBottom =
    FindUnifiedToolbarBottom(aThemeGeometries, windowWidth, titlebarBottom);

  ToolbarWindow* win = (ToolbarWindow*)[mView window];
  bool drawsContentsIntoWindowFrame = [win drawsContentsIntoWindowFrame];
  int32_t titlebarHeight = CocoaPointsToDevPixels([win titlebarHeight]);
  int32_t contentOffset = drawsContentsIntoWindowFrame ? titlebarHeight : 0;
  int32_t devUnifiedHeight = titlebarHeight + unifiedToolbarBottom - contentOffset;
  [win setUnifiedToolbarHeight:DevPixelsToCocoaPoints(devUnifiedHeight)];
}

TemporaryRef<gfx::DrawTarget>
nsChildView::StartRemoteDrawing()
{
  if (!mGLPresenter) {
    mGLPresenter = GLPresenter::CreateForWindow(this);

    if (!mGLPresenter) {
      return nullptr;
    }
  }

  nsIntRegion dirtyRegion = mBounds;
  nsIntSize renderSize = mBounds.Size();

  if (!mBasicCompositorImage) {
    mBasicCompositorImage = new RectTextureImage(mGLPresenter->gl());
  }

  RefPtr<gfx::DrawTarget> drawTarget =
    mBasicCompositorImage->BeginUpdate(renderSize, dirtyRegion);

  if (!drawTarget) {
    // Composite unchanged textures.
    DoRemoteComposition(mBounds);
    return nullptr;
  }

  return drawTarget;
}

void
nsChildView::EndRemoteDrawing()
{
  mBasicCompositorImage->EndUpdate(true);
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

void
nsChildView::DoRemoteComposition(const nsIntRect& aRenderRect)
{
  [(ChildView*)mView preRender:mGLPresenter->GetNSOpenGLContext()];
  mGLPresenter->BeginFrame(aRenderRect.Size());

  // Draw the result from the basic compositor.
  mBasicCompositorImage->Draw(mGLPresenter, nsIntPoint(0, 0));

  // DrawWindowOverlay doesn't do anything for non-GL, so it didn't paint
  // anything during the basic compositor transaction. Draw the overlay now.
  DrawWindowOverlay(mGLPresenter, aRenderRect);

  mGLPresenter->EndFrame();
}

#ifdef ACCESSIBILITY
already_AddRefed<a11y::Accessible>
nsChildView::GetDocumentAccessible()
{
  if (!mozilla::a11y::ShouldA11yBeEnabled())
    return nullptr;

  if (mAccessible) {
    nsRefPtr<a11y::Accessible> ret;
    CallQueryReferent(mAccessible.get(),
                      static_cast<a11y::Accessible**>(getter_AddRefs(ret)));
    return ret.forget();
  }

  // need to fetch the accessible anew, because it has gone away.
  // cache the accessible in our weak ptr
  nsRefPtr<a11y::Accessible> acc = GetRootAccessible();
  mAccessible = do_GetWeakReference(static_cast<nsIAccessible *>(acc.get()));

  return acc.forget();
}
#endif

// RectTextureImage implementation

RectTextureImage::~RectTextureImage()
{
  if (mTexture) {
    mGLContext->MakeCurrent();
    mGLContext->fDeleteTextures(1, &mTexture);
    mTexture = 0;
  }
}

nsIntSize
RectTextureImage::TextureSizeForSize(const nsIntSize& aSize)
{
  return nsIntSize(gfx::NextPowerOfTwo(aSize.width),
                   gfx::NextPowerOfTwo(aSize.height));
}

TemporaryRef<gfx::DrawTarget>
RectTextureImage::BeginUpdate(const nsIntSize& aNewSize,
                              const nsIntRegion& aDirtyRegion)
{
  MOZ_ASSERT(!mInUpdate, "Beginning update during update!");
  mUpdateRegion = aDirtyRegion;
  if (aNewSize != mUsedSize) {
    mUsedSize = aNewSize;
    mUpdateRegion = nsIntRect(nsIntPoint(0, 0), aNewSize);
  }

  if (mUpdateRegion.IsEmpty()) {
    return nullptr;
  }

  nsIntSize neededBufferSize = TextureSizeForSize(mUsedSize);
  if (!mUpdateDrawTarget || mBufferSize != neededBufferSize) {
    gfx::IntSize size(neededBufferSize.width, neededBufferSize.height);
    mUpdateDrawTarget =
      gfx::Factory::CreateDrawTarget(gfx::BACKEND_COREGRAPHICS, size,
                                     gfx::FORMAT_B8G8R8A8);
    mBufferSize = neededBufferSize;
  }

  mInUpdate = true;

  RefPtr<gfx::DrawTarget> drawTarget = mUpdateDrawTarget;
  return drawTarget;
}

#define NSFoundationVersionWithProperStrideSupportForSubtextureUpload NSFoundationVersionNumber10_6_3

static bool
CanUploadSubtextures()
{
  return NSFoundationVersionNumber >= NSFoundationVersionWithProperStrideSupportForSubtextureUpload;
}

void
RectTextureImage::EndUpdate(bool aKeepSurface)
{
  MOZ_ASSERT(mInUpdate, "Ending update while not in update");

  bool overwriteTexture = false;
  nsIntRegion updateRegion = mUpdateRegion;
  if (!mTexture || (mTextureSize != mBufferSize)) {
    overwriteTexture = true;
    mTextureSize = mBufferSize;
  }

  if (overwriteTexture || !CanUploadSubtextures()) {
    updateRegion = nsIntRect(nsIntPoint(0, 0), mTextureSize);
  }

  RefPtr<gfx::SourceSurface> snapshot = mUpdateDrawTarget->Snapshot();
  RefPtr<gfx::DataSourceSurface> dataSnapshot = snapshot->GetDataSurface();

  mGLContext->UploadSurfaceToTexture(dataSnapshot,
                                     updateRegion,
                                     mTexture,
                                     overwriteTexture,
                                     updateRegion.GetBounds().TopLeft(),
                                     false,
                                     LOCAL_GL_TEXTURE0,
                                     LOCAL_GL_TEXTURE_RECTANGLE_ARB);

  if (!aKeepSurface) {
    mUpdateDrawTarget = nullptr;
  }

  mInUpdate = false;
}

void
RectTextureImage::UpdateFromDrawTarget(const nsIntSize& aNewSize,
                                       const nsIntRegion& aDirtyRegion,
                                       gfx::DrawTarget* aFromDrawTarget)
{
  mUpdateDrawTarget = aFromDrawTarget;
  mBufferSize.SizeTo(aFromDrawTarget->GetSize().width, aFromDrawTarget->GetSize().height);
  RefPtr<gfx::DrawTarget> drawTarget = BeginUpdate(aNewSize, aDirtyRegion);
  if (drawTarget) {
    if (drawTarget != aFromDrawTarget) {
      RefPtr<gfx::SourceSurface> source = aFromDrawTarget->Snapshot();
      gfx::Rect rect(0, 0, aFromDrawTarget->GetSize().width, aFromDrawTarget->GetSize().height);
      gfxUtils::ClipToRegion(drawTarget, GetUpdateRegion());
      drawTarget->DrawSurface(source, rect, rect,
                              gfx::DrawSurfaceOptions(),
                              gfx::DrawOptions(1.0, gfx::OP_SOURCE));
      drawTarget->PopClip();
    }
    EndUpdate();
  }
  mUpdateDrawTarget = nullptr;
}

void
RectTextureImage::Draw(GLManager* aManager,
                       const nsIntPoint& aLocation,
                       const gfx3DMatrix& aTransform)
{
  ShaderProgramOGL* program = aManager->GetProgram(RGBARectLayerProgramType);

  aManager->gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, mTexture);

  program->Activate();
  program->SetLayerQuadRect(nsIntRect(nsIntPoint(0, 0), mUsedSize));
  program->SetLayerTransform(aTransform * gfx3DMatrix::Translation(aLocation.x, aLocation.y, 0));
  program->SetTextureTransform(gfx3DMatrix());
  program->SetLayerOpacity(1.0);
  program->SetRenderOffset(nsIntPoint(0, 0));
  program->SetTexCoordMultiplier(mUsedSize.width, mUsedSize.height);
  program->SetTextureUnit(0);

  aManager->BindAndDrawQuad(program);

  aManager->gl()->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, 0);
}

// GLPresenter implementation

GLPresenter::GLPresenter(GLContext* aContext)
 : mGLContext(aContext)
{
  mGLContext->SetFlipped(true);
  mGLContext->MakeCurrent();
  mRGBARectProgram = new ShaderProgramOGL(mGLContext,
    ProgramProfileOGL::GetProfileFor(RGBARectLayerProgramType, MaskNone));

  // Create mQuadVBO.
  mGLContext->fGenBuffers(1, &mQuadVBO);
  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);

  GLfloat vertices[] = {
    /* First quad vertices */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    /* Then quad texcoords */
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
  };
  mGLContext->fBufferData(LOCAL_GL_ARRAY_BUFFER, sizeof(vertices), vertices, LOCAL_GL_STATIC_DRAW);
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
GLPresenter::BindAndDrawQuad(ShaderProgramOGL* aProgram)
{
  mGLContext->MakeCurrent();

  GLuint vertAttribIndex = aProgram->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLuint texCoordAttribIndex = aProgram->AttribLocation(ShaderProgramOGL::TexCoordAttrib);

  mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mQuadVBO);
  mGLContext->fVertexAttribPointer(vertAttribIndex, 2,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   (GLvoid*)0);
  mGLContext->fEnableVertexAttribArray(vertAttribIndex);
  mGLContext->fVertexAttribPointer(texCoordAttribIndex, 2,
                                   LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0,
                                   (GLvoid*) (sizeof(float)*4*2));
  mGLContext->fEnableVertexAttribArray(texCoordAttribIndex);
  mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
  mGLContext->fDisableVertexAttribArray(vertAttribIndex);
  mGLContext->fDisableVertexAttribArray(texCoordAttribIndex);
}

void
GLPresenter::BeginFrame(nsIntSize aRenderSize)
{
  mGLContext->MakeCurrent();

  mGLContext->fViewport(0, 0, aRenderSize.width, aRenderSize.height);

  // Matrix to transform (0, 0, width, height) to viewport space (-1.0, 1.0,
  // 2, 2) and flip the contents.
  gfxMatrix viewMatrix;
  viewMatrix.Translate(-gfxPoint(1.0, -1.0));
  viewMatrix.Scale(2.0f / float(aRenderSize.width), 2.0f / float(aRenderSize.height));
  viewMatrix.Scale(1.0f, -1.0f);

  gfx3DMatrix matrix3d = gfx3DMatrix::From2D(viewMatrix);
  matrix3d._33 = 0.0f;

  mRGBARectProgram->CheckAndSetProjectionMatrix(matrix3d);

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

    NSArray *sendTypes = [[NSArray alloc] initWithObjects:NSStringPboardType,NSHTMLPboardType,nil];
    NSArray *returnTypes = [[NSArray alloc] initWithObjects:NSStringPboardType,NSHTMLPboardType,nil];
    
    [NSApp registerServicesMenuSendTypes:sendTypes returnTypes:returnTypes];

    [sendTypes release];
    [returnTypes release];

    initialized = YES;
  }
}

+ (void)registerViewForDraggedTypes:(NSView*)aView
{
  [aView registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
                                                           NSStringPboardType,
                                                           NSHTMLPboardType,
                                                           NSURLPboardType,
                                                           NSFilesPromisePboardType,
                                                           kWildcardPboardType,
                                                           kCorePboardType_url,
                                                           kCorePboardType_urld,
                                                           kCorePboardType_urln,
                                                           nil]];
}

// initWithFrame:geckoChild:
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super initWithFrame:inFrame])) {
    mGeckoChild = inChild;
    mIsPluginView = NO;
#ifndef NP_NO_CARBON
    // We don't support the Carbon event model but it's still the default
    // model for i386 per NPAPI.
    mPluginEventModel = NPEventModelCarbon;
#else
    mPluginEventModel = NPEventModelCocoa;
#endif
#ifndef NP_NO_QUICKDRAW
    // We don't support the Quickdraw drawing model any more but it's still
    // the default model for i386 per NPAPI.
    mPluginDrawingModel = NPDrawingModelQuickDraw;
#else
    mPluginDrawingModel = NPDrawingModelCoreGraphics;
#endif
    mPendingDisplay = NO;
    mBlockedLastMouseDown = NO;

    mLastMouseDownEvent = nil;
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

    [self setFocusRingType:NSFocusRingTypeNone];

#ifdef __LP64__
    mCancelSwipeAnimation = nil;
    mCurrentSwipeDir = 0;
#endif

    mTopLeftCornerMask = NULL;
  }

  // register for things we'll take from other applications
  [ChildView registerViewForDraggedTypes:self];

  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(windowBecameMain:)
                                               name:NSWindowDidBecomeMainNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(windowResignedMain:)
                                               name:NSWindowDidResignMainNotification
                                             object:nil];
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
  [mGLContext setView:self];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setGLContext:(NSOpenGLContext *)aGLContext
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mGLContext = aGLContext;
  [mGLContext retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

-(void)preRender:(NSOpenGLContext *)aGLContext
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGLContext) {
    [self setGLContext:aGLContext];
  }

  [aGLContext setView:self];
  [aGLContext update];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mGLContext release];
  [mPendingDirtyRects release];
  [mLastMouseDownEvent release];
  [mClickThroughMouseDownEvent release];
  CGImageRelease(mTopLeftCornerMask);
  ChildViewMouseTracker::OnDestroyView(self);

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)updatePluginTopLevelWindowStatus:(BOOL)hasMain
{
  if (!mGeckoChild)
    return;

  nsPluginEvent pluginEvent(true, NS_PLUGIN_FOCUS_EVENT, mGeckoChild);
  NPCocoaEvent cocoaEvent;
  nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
  cocoaEvent.type = NPCocoaEventWindowFocusChanged;
  cocoaEvent.data.focus.hasFocus = hasMain;
  nsCocoaUtils::InitPluginEvent(pluginEvent, cocoaEvent);
  mGeckoChild->DispatchWindowEvent(pluginEvent);
}

- (void)windowBecameMain:(NSNotification*)inNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mIsPluginView && mPluginEventModel == NPEventModelCocoa) {
    if ((NSWindow*)[inNotification object] == [self window]) {
      [self updatePluginTopLevelWindowStatus:YES];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowResignedMain:(NSNotification*)inNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mIsPluginView && mPluginEventModel == NPEventModelCocoa) {
    if ((NSWindow*)[inNotification object] == [self window]) {
      [self updatePluginTopLevelWindowStatus:NO];
    }
  }

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
      listener->GetPresShell()->ReconstructFrames();
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
  return [[self window] isOpaque] && !mIsPluginView;
}

-(void)setIsPluginView:(BOOL)aIsPlugin
{
  mIsPluginView = aIsPlugin;
}

-(BOOL)isPluginView
{
  return mIsPluginView;
}

// Are we processing an NSLeftMouseDown event that will fail to click through?
// If so, we shouldn't focus or unfocus a plugin.
- (BOOL)isInFailingLeftClickThrough
{
  if (!mGeckoChild)
    return NO;

  if (!mClickThroughMouseDownEvent ||
      [mClickThroughMouseDownEvent type] != NSLeftMouseDown)
    return NO;

  BOOL retval =
    !ChildViewMouseTracker::WindowAcceptsEvent([self window],
                                               mClickThroughMouseDownEvent,
                                               self, true);

  // If we return YES here, this will result in us not being focused,
  // which will stop us receiving mClickThroughMouseDownEvent in
  // [ChildView mouseDown:].  So we need to release and null-out
  // mClickThroughMouseDownEvent here.
  if (retval) {
    [mClickThroughMouseDownEvent release];
    mClickThroughMouseDownEvent = nil;
  }

  return retval;
}

- (void)setPluginEventModel:(NPEventModel)eventModel
{
  mPluginEventModel = eventModel;
}

- (void)setPluginDrawingModel:(NPDrawingModel)drawingModel
{
  mPluginDrawingModel = drawingModel;
}

- (NPEventModel)pluginEventModel
{
  return mPluginEventModel;
}

- (NPDrawingModel)pluginDrawingModel
{
  return mPluginDrawingModel;
}

- (void)sendFocusEvent:(uint32_t)eventType
{
  if (!mGeckoChild)
    return;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent focusGuiEvent(true, eventType, mGeckoChild);
  focusGuiEvent.time = PR_IntervalNow();
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

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!newWindow)
    HideChildPluginViews(self);

  [super viewWillMoveToWindow:newWindow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)viewDidMoveToWindow
{
  if (mPluginEventModel == NPEventModelCocoa &&
      [self window] && [self isPluginView] && mGeckoChild) {
    mGeckoChild->UpdatePluginPort();
  }

  [super viewDidMoveToWindow];
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
  return [[self window] isMovableByWindowBackground];
}

- (void)lockFocus
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [super lockFocus];

  if (mGLContext) {
    if ([mGLContext view] != self) {
      [mGLContext setView:self];
    }

    [mGLContext makeCurrentContext];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

-(void)update
{
  if (mGLContext) {
    [mGLContext update];
  }
}

- (void) _surfaceNeedsUpdate:(NSNotification*)notification
{
   [self update];
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

- (nsIntRegion)nativeDirtyRegionWithBoundingRect:(NSRect)aRect
{
  nsIntRect boundingRect = mGeckoChild->CocoaPointsToDevPixels(aRect);
  const NSRect *rects;
  NSInteger count;
  [self getRectsBeingDrawn:&rects count:&count];

  if (count > MAX_RECTS_IN_REGION) {
    return boundingRect;
  }

  nsIntRegion region;
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

  // If we're a transparent window and our contents have changed, we need
  // to make sure the shadow is updated to the new contents.
  if ([[self window] isKindOfClass:[BaseWindow class]]) {
    [(BaseWindow*)[self window] deferredInvalidateShadow];
  }
}

- (void)drawRect:(NSRect)aRect inContext:(CGContextRef)aContext
{
  if (!mGeckoChild || !mGeckoChild->IsVisible())
    return;

  // Don't ever draw plugin views explicitly; they'll be drawn as part of their parent widget.
  if (mIsPluginView)
    return;

#ifdef DEBUG_UPDATE
  nsIntRect geckoBounds;
  mGeckoChild->GetBounds(geckoBounds);

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
    // corners - the rest of the window is covered by opaque content in our
    // OpenGL surface.
    // So we need to clear the pixel buffer contents in the corners.
    [self clearCorners];

    // Do GL composition and return.
    [self drawUsingOpenGL];
    return;
  }

  PROFILER_LABEL("widget", "ChildView::drawRect");

  // The CGContext that drawRect supplies us with comes with a transform that
  // scales one user space unit to one Cocoa point, which can consist of
  // multiple dev pixels. But Gecko expects its supplied context to be scaled
  // to device pixels, so we need to reverse the scaling.
  double scale = mGeckoChild->BackingScaleFactor();
  CGContextSaveGState(aContext);
  CGContextScaleCTM(aContext, 1.0 / scale, 1.0 / scale);

  NSSize viewSize = [self bounds].size;
  nsIntSize backingSize(viewSize.width * scale, viewSize.height * scale);

  CGContextSaveGState(aContext);

  nsIntRegion region = [self nativeDirtyRegionWithBoundingRect:aRect];

  // Create Cairo objects.
  nsRefPtr<gfxQuartzSurface> targetSurface =
    new gfxQuartzSurface(aContext, backingSize);
  targetSurface->SetAllowUseAsSource(false);

  nsRefPtr<gfxContext> targetContext = new gfxContext(targetSurface);

  // Set up the clip region.
  nsIntRegionRectIterator iter(region);
  targetContext->NewPath();
  for (;;) {
    const nsIntRect* r = iter.Next();
    if (!r)
      break;
    targetContext->Rectangle(gfxRect(r->x, r->y, r->width, r->height));
  }
  targetContext->Clip();

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  bool painted = false;
  if (mGeckoChild->GetLayerManager()->GetBackendType() == LAYERS_BASIC) {
    nsBaseWidget::AutoLayerManagerSetup
      setupLayerManager(mGeckoChild, targetContext, BUFFER_NONE);
    painted = mGeckoChild->PaintWindow(region);
  } else if (mGeckoChild->GetLayerManager()->GetBackendType() == LAYERS_CLIENT) {
    // We only need this so that we actually get DidPaintWindow fired
    painted = mGeckoChild->PaintWindow(region);
  }

  targetContext = nullptr;
  targetSurface = nullptr;

  CGContextRestoreGState(aContext);

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

  return mGeckoChild->GetLayerManager(nullptr)->GetBackendType() == mozilla::layers::LAYERS_OPENGL;
}

- (BOOL)isUsingOpenGL
{
  if (!mGeckoChild || ![self window])
    return NO;

  return mGLContext || [self isUsingMainThreadOpenGL];
}

- (void)drawUsingOpenGL
{
  PROFILER_LABEL("widget", "ChildView::drawUsingOpenGL");

  if (![self isUsingOpenGL] || !mGeckoChild->IsVisible())
    return;

  mWaitingForPaint = NO;

  nsIntRect geckoBounds;
  mGeckoChild->GetBounds(geckoBounds);
  nsIntRegion region(geckoBounds);

  if ([self isUsingMainThreadOpenGL]) {
    LayerManagerOGL *manager = static_cast<LayerManagerOGL*>(mGeckoChild->GetLayerManager(nullptr));
    manager->SetClippingRegion(region);
    NSOpenGLContext *glContext = (NSOpenGLContext *)manager->GetNSOpenGLContext();

    if (!mGLContext) {
      [self setGLContext:glContext];
    }

    [glContext setView:self];
    [glContext update];
  }

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
  NSView* frameView = [[[self window] contentView] superview];
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

- (void)releaseWidgets:(NSArray*)aWidgetArray
{
  if (!aWidgetArray) {
    return;
  }
  NSInteger count = [aWidgetArray count];
  for (NSInteger i = 0; i < count; ++i) {
    NSNumber* pointer = (NSNumber*) [aWidgetArray objectAtIndex:i];
    nsIWidget* widget = (nsIWidget*) [pointer unsignedIntegerValue];
    NS_RELEASE(widget);
  }
}

- (void)viewWillDraw
{
  if (mGeckoChild) {
    // The OS normally *will* draw our NSWindow, no matter what we do here.
    // But Gecko can delete our parent widget(s) (along with mGeckoChild)
    // while processing a paint request, which closes our NSWindow and
    // makes the OS throw an NSInternalInconsistencyException assertion when
    // it tries to draw it.  Sometimes the OS also aborts the browser process.
    // So we need to retain our parent(s) here and not release it/them until
    // the next time through the main thread's run loop.  When we do this we
    // also need to retain and release mGeckoChild, which holds a strong
    // reference to us (otherwise we might have been deleted by the time
    // releaseWidgets: is called on us).  See bug 550392.
    nsIWidget* parent = mGeckoChild->GetParent();
    if (parent) {
      NSMutableArray* widgetArray = [NSMutableArray arrayWithCapacity:3];
      while (parent) {
        NS_ADDREF(parent);
        [widgetArray addObject:[NSNumber numberWithUnsignedInteger:(NSUInteger)parent]];
        parent = parent->GetParent();
      }
      NS_ADDREF(mGeckoChild);
      [widgetArray addObject:[NSNumber numberWithUnsignedInteger:(NSUInteger)mGeckoChild]];
      [self performSelector:@selector(releaseWidgets:)
                 withObject:widgetArray
                 afterDelay:0];
    }

    if ([self isUsingOpenGL]) {
      // When our view covers the titlebar, we need to repaint the titlebar
      // texture buffer when, for example, the window buttons are hovered.
      // So we notify our nsChildView about any areas needing repainting.
      mGeckoChild->NotifyDirtyRegion([self nativeDirtyRegionWithBoundingRect:[self bounds]]);

      if (mGeckoChild->GetLayerManager()->GetBackendType() == LAYERS_CLIENT) {
        ClientLayerManager *manager = static_cast<ClientLayerManager*>(mGeckoChild->GetLayerManager());
        manager->WindowOverlayChanged();
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
      nsAutoTArray<nsIWidget*, 5> widgetChain;
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
        consumeEvent = (BOOL)rollupListener->Rollup(popupsToRollup, nullptr);
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

  if (!anEvent || !mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float deltaX = [anEvent deltaX];  // left=1.0, right=-1.0
  float deltaY = [anEvent deltaY];  // up=1.0, down=-1.0

  // Setup the "swipe" event.
  nsSimpleGestureEvent geckoEvent(true, NS_SIMPLE_GESTURE_SWIPE, mGeckoChild, 0, 0.0);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

  // Record the left/right direction.
  if (deltaX > 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
  else if (deltaX < 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_RIGHT;

  // Record the up/down direction.
  if (deltaY > 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_UP;
  else if (deltaY < 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_DOWN;

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)beginGestureWithEvent:(NSEvent *)anEvent
{
  NS_ASSERTION(mGestureState == eGestureState_None, "mGestureState should be eGestureState_None");

  if (!anEvent)
    return;

  mGestureState = eGestureState_StartGesture;
  mCumulativeMagnification = 0;
  mCumulativeRotation = 0.0;
}

- (void)magnifyWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild)
    return;

  /*
   * In OS X 10.7.* (Lion), smart zoom events come through magnifyWithEvent,
   * instead of smartMagnifyWithEvent. See bug 863841.
   */
  if ([ChildView isLionSmartMagnifyEvent: anEvent]) {
    [self smartMagnifyWithEvent: anEvent];
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float deltaZ = [anEvent deltaZ];

  uint32_t msg;
  switch (mGestureState) {
  case eGestureState_StartGesture:
    msg = NS_SIMPLE_GESTURE_MAGNIFY_START;
    mGestureState = eGestureState_MagnifyGesture;
    break;

  case eGestureState_MagnifyGesture:
    msg = NS_SIMPLE_GESTURE_MAGNIFY_UPDATE;
    break;

  case eGestureState_None:
  case eGestureState_RotateGesture:
  default:
    return;
  }

  // Setup the event.
  nsSimpleGestureEvent geckoEvent(true, msg, mGeckoChild, 0, deltaZ);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Keep track of the cumulative magnification for the final "magnify" event.
  mCumulativeMagnification += deltaZ;
  
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)smartMagnifyWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild) {
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // Setup the "double tap" event.
  nsSimpleGestureEvent geckoEvent(true, NS_SIMPLE_GESTURE_TAP,
                                  mGeckoChild, 0, 0.0);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
  geckoEvent.clickCount = 1;

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Clear the gesture state
  mGestureState = eGestureState_None;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rotateWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float rotation = [anEvent rotation];

  uint32_t msg;
  switch (mGestureState) {
  case eGestureState_StartGesture:
    msg = NS_SIMPLE_GESTURE_ROTATE_START;
    mGestureState = eGestureState_RotateGesture;
    break;

  case eGestureState_RotateGesture:
    msg = NS_SIMPLE_GESTURE_ROTATE_UPDATE;
    break;

  case eGestureState_None:
  case eGestureState_MagnifyGesture:
  default:
    return;
  }

  // Setup the event.
  nsSimpleGestureEvent geckoEvent(true, msg, mGeckoChild, 0, 0.0);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
  geckoEvent.delta = -rotation;
  if (rotation > 0.0) {
    geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
  } else {
    geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
  }

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Keep track of the cumulative rotation for the final "rotate" event.
  mCumulativeRotation += rotation;

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
      nsSimpleGestureEvent geckoEvent(true, NS_SIMPLE_GESTURE_MAGNIFY,
                                      mGeckoChild, 0, mCumulativeMagnification);
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
    break;

  case eGestureState_RotateGesture:
    {
      // Setup the "rotate" event.
      nsSimpleGestureEvent geckoEvent(true, NS_SIMPLE_GESTURE_ROTATE, mGeckoChild, 0, 0.0);
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
      geckoEvent.delta = -mCumulativeRotation;
      if (mCumulativeRotation > 0.0) {
        geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
      } else {
        geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
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

+ (BOOL)isLionSmartMagnifyEvent:(NSEvent*)anEvent
{
  /*
   * On Lion, smart zoom events have type NSEventTypeGesture, subtype 0x16,
   * whereas pinch zoom events have type NSEventTypeMagnify. So, use that to
   * discriminate between the two. Smart zoom gestures do not call
   * beginGestureWithEvent or endGestureWithEvent, so mGestureState is not
   * changed. Documentation couldn't be found for the meaning of the subtype
   * 0x16, but it will probably never change. See bug 863841.
   */
  return nsCocoaFeatures::OnLionOrLater() &&
         !nsCocoaFeatures::OnMountainLionOrLater() &&
         [anEvent type] == NSEventTypeGesture &&
         [anEvent subtype] == 0x16;
}

#ifdef __LP64__
- (bool)sendSwipeEvent:(NSEvent*)aEvent
                withKind:(uint32_t)aMsg
       allowedDirections:(uint32_t*)aAllowedDirections
               direction:(uint32_t)aDirection
                   delta:(double)aDelta
{
  if (!mGeckoChild)
    return false;

  nsSimpleGestureEvent geckoEvent(true, aMsg, mGeckoChild, aDirection, aDelta);
  geckoEvent.allowedDirections = *aAllowedDirections;
  [self convertCocoaMouseEvent:aEvent toGeckoEvent:&geckoEvent];
  bool eventCancelled = mGeckoChild->DispatchWindowEvent(geckoEvent);
  *aAllowedDirections = geckoEvent.allowedDirections;
  return eventCancelled; // event cancelled == swipe should start
}

- (void)sendSwipeEndEvent:(NSEvent *)anEvent
        allowedDirections:(uint32_t)aAllowedDirections
{
    // Tear down animation overlay by sending a swipe end event.
    uint32_t allowedDirectionsCopy = aAllowedDirections;
    [self sendSwipeEvent:anEvent
                withKind:NS_SIMPLE_GESTURE_SWIPE_END
       allowedDirections:&allowedDirectionsCopy
               direction:0
                   delta:0.0];
}

// Support fluid swipe tracking on OS X 10.7 and higher. We must be careful
// to only invoke this support on a two-finger gesture that really
// is a swipe (and not a scroll) -- in other words, the app is responsible
// for deciding which is which. But once the decision is made, the OS tracks
// the swipe until it has finished, and decides whether or not it succeeded.
// A horizontal swipe has the same functionality as the Back and Forward
// buttons.
// This method is partly based on Apple sample code available at
// developer.apple.com/library/mac/#releasenotes/Cocoa/AppKitOlderNotes.html
// (under Fluid Swipe Tracking API).
- (void)maybeTrackScrollEventAsSwipe:(NSEvent *)anEvent
                     scrollOverflowX:(double)overflowX
                     scrollOverflowY:(double)overflowY
{
  if (!nsCocoaFeatures::OnLionOrLater()) {
    return;
  }

  // This method checks whether the AppleEnableSwipeNavigateWithScrolls global
  // preference is set.  If it isn't, fluid swipe tracking is disabled, and a
  // horizontal two-finger gesture is always a scroll (even in Safari).  This
  // preference can't (currently) be set from the Preferences UI -- only using
  // 'defaults write'.
  if (![NSEvent isSwipeTrackingFromScrollEventsEnabled]) {
    return;
  }

  // Verify that this is a scroll wheel event with proper phase to be tracked
  // by the OS.
  if ([anEvent type] != NSScrollWheel || [anEvent phase] == NSEventPhaseNone) {
    return;
  }

  // Only initiate tracking if the user has tried to scroll past the edge of
  // the current page (as indicated by 'overflowX' or 'overflowY' being
  // non-zero).  Gecko only sets nsMouseScrollEvent.scrollOverflow when it's
  // processing NS_MOUSE_PIXEL_SCROLL events (not NS_MOUSE_SCROLL events).
  if (overflowX == 0.0 && overflowY == 0.0) {
    return;
  }

  CGFloat deltaX, deltaY;
  if ([anEvent hasPreciseScrollingDeltas]) {
    deltaX = [anEvent scrollingDeltaX];
    deltaY = [anEvent scrollingDeltaY];
  }
  else {
    return;
  }

  uint32_t vDirs = (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_DOWN |
                   (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_UP;
  uint32_t direction = 0;
  // Only initiate horizontal tracking for events whose horizontal element is
  // at least eight times larger than its vertical element. This minimizes
  // performance problems with vertical scrolls (by minimizing the possibility
  // that they'll be misinterpreted as horizontal swipes), while still
  // tolerating a small vertical element to a true horizontal swipe.  The number
  // '8' was arrived at by trial and error.
  if (overflowX != 0.0 && deltaX != 0.0 &&
      fabsf(deltaX) > fabsf(deltaY) * 8) {
    // Only initiate horizontal tracking for gestures that have just begun --
    // otherwise a scroll to one side of the page can have a swipe tacked on
    // to it.
    if ([anEvent phase] != NSEventPhaseBegan)
      return;

    if (deltaX < 0.0)
      direction = (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_RIGHT;
    else
      direction = (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
  }
  // Only initiate vertical tracking for events whose vertical element is
  // at least two times larger than its horizontal element. This minimizes
  // performance problems. The number '2' was arrived at by trial and error.
  else if (overflowY != 0.0 && deltaY != 0.0 &&
           fabsf(deltaY) > fabsf(deltaX) * 2) {
    if (deltaY < 0.0)
      direction = (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_DOWN;
    else
      direction = (uint32_t)nsIDOMSimpleGestureEvent::DIRECTION_UP;

    if ((mCurrentSwipeDir & vDirs) && (mCurrentSwipeDir != direction)) {
      // If a swipe is currently being tracked kill it -- it's been interrupted
      // by another gesture event.
      if (mCancelSwipeAnimation && *mCancelSwipeAnimation == NO) {
        *mCancelSwipeAnimation = YES;
        mCancelSwipeAnimation = nil;
        [self sendSwipeEndEvent:anEvent allowedDirections:0];
      }
      return;
    }
  }
  else {
    return;
  }

  // Track the direction we're going in.
  mCurrentSwipeDir = direction;

  // If a swipe is currently being tracked kill it -- it's been interrupted
  // by another gesture event.
  if (mCancelSwipeAnimation && *mCancelSwipeAnimation == NO) {
    *mCancelSwipeAnimation = YES;
    mCancelSwipeAnimation = nil;
  }

  uint32_t allowedDirections = 0;
  // We're ready to start the animation. Tell Gecko about it, and at the same
  // time ask it if it really wants to start an animation for this event.
  // This event also reports back the directions that we can swipe in.
  bool shouldStartSwipe = [self sendSwipeEvent:anEvent
                                      withKind:NS_SIMPLE_GESTURE_SWIPE_START
                             allowedDirections:&allowedDirections
                                     direction:direction
                                         delta:0.0];

  if (!shouldStartSwipe) {
    return;
  }

  CGFloat min = 0.0;
  CGFloat max = 0.0;
  if (!(direction & vDirs)) {
    min = (allowedDirections & nsIDOMSimpleGestureEvent::DIRECTION_RIGHT) ?
          -1.0 : 0.0;
    max = (allowedDirections & nsIDOMSimpleGestureEvent::DIRECTION_LEFT) ?
          1.0 : 0.0;
  }

  __block BOOL animationCanceled = NO;
  // At this point, anEvent is the first scroll wheel event in a two-finger
  // horizontal gesture that we've decided to treat as a swipe.  When we call
  // [NSEvent trackSwipeEventWithOptions:...], the OS interprets all
  // subsequent scroll wheel events that are part of this gesture as a swipe,
  // and stops sending them to us.  The OS calls the trackingHandler "block"
  // multiple times, asynchronously (sometimes after [NSEvent
  // maybeTrackScrollEventAsSwipe:...] has returned).  The OS determines when
  // the gesture has finished, and whether or not it was "successful" -- this
  // information is passed to trackingHandler.  We must be careful to only
  // call [NSEvent maybeTrackScrollEventAsSwipe:...] on a "real" swipe --
  // otherwise two-finger scrolling performance will suffer significantly.
  // Note that we use anEvent inside the block. This extends the lifetime of
  // the anEvent object because it's retained by the block, see bug 682445.
  // The block will release it when the block goes away at the end of the
  // animation, or when the animation is canceled.
  [anEvent trackSwipeEventWithOptions:NSEventSwipeTrackingLockDirection |
                                      NSEventSwipeTrackingClampGestureAmount
             dampenAmountThresholdMin:min
                                  max:max
                         usingHandler:^(CGFloat gestureAmount,
                                        NSEventPhase phase,
                                        BOOL isComplete,
                                        BOOL *stop) {
    uint32_t allowedDirectionsCopy = allowedDirections;
    // Since this tracking handler can be called asynchronously, mGeckoChild
    // might have become NULL here (our child widget might have been
    // destroyed).
    // Checking for gestureAmount == 0.0 also works around bug 770626, which
    // happens when DispatchWindowEvent() triggers a modal dialog, which spins
    // the event loop and confuses the OS. This results in several re-entrant
    // calls to this handler.
    if (animationCanceled || !mGeckoChild || gestureAmount == 0.0) {
      *stop = YES;
      animationCanceled = YES;
      if (gestureAmount == 0.0 ||
          ((direction & vDirs) && (direction != mCurrentSwipeDir))) {
        if (mCancelSwipeAnimation)
          *mCancelSwipeAnimation = YES;
        mCancelSwipeAnimation = nil;
        [self sendSwipeEndEvent:anEvent
              allowedDirections:allowedDirectionsCopy];
      }
      mCurrentSwipeDir = 0;
      return;
    }

    // Update animation overlay to match gestureAmount.
    [self sendSwipeEvent:anEvent
                withKind:NS_SIMPLE_GESTURE_SWIPE_UPDATE
       allowedDirections:&allowedDirectionsCopy
               direction:0.0
                   delta:gestureAmount];

    if (phase == NSEventPhaseEnded) {
      // The result of the swipe is now known, so the main event can be sent.
      // The animation might continue even after this event was sent, so
      // don't tear down the animation overlay yet.

      uint32_t directionCopy = direction;

      // gestureAmount is documented to be '-1', '0' or '1' when isComplete
      // is TRUE, but the docs don't say anything about its value at other
      // times.  However, tests show that, when phase == NSEventPhaseEnded,
      // gestureAmount is negative when it will be '-1' at isComplete, and
      // positive when it will be '1'.  And phase is never equal to
      // NSEventPhaseEnded when gestureAmount will be '0' at isComplete.
      [self sendSwipeEvent:anEvent
                  withKind:NS_SIMPLE_GESTURE_SWIPE
         allowedDirections:&allowedDirectionsCopy
                 direction:directionCopy
                     delta:0.0];
    }

    if (isComplete) {
      [self sendSwipeEndEvent:anEvent allowedDirections:allowedDirectionsCopy];
      mCurrentSwipeDir = 0;
      mCancelSwipeAnimation = nil;
    }
  }];

  mCancelSwipeAnimation = &animationCanceled;
}
#endif // #ifdef __LP64__

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

  NSUInteger modifierFlags = [theEvent modifierFlags];

  nsMouseEvent geckoEvent(true, NS_MOUSE_BUTTON_DOWN, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  NSInteger clickCount = [theEvent clickCount];
  if (mBlockedLastMouseDown && clickCount > 1) {
    // Don't send a double click if the first click of the double click was
    // blocked.
    clickCount--;
  }
  geckoEvent.clickCount = clickCount;

  if (modifierFlags & NSControlKeyMask)
    geckoEvent.button = nsMouseEvent::eRightButton;
  else
    geckoEvent.button = nsMouseEvent::eLeftButton;

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseDown;
    cocoaEvent.data.mouse.modifierFlags = modifierFlags;
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = clickCount;
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);
  mBlockedLastMouseDown = NO;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseUp:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild || mBlockedLastMouseDown)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  NPCocoaEvent cocoaEvent;

  nsMouseEvent geckoEvent(true, NS_MOUSE_BUTTON_UP, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  if ([theEvent modifierFlags] & NSControlKeyMask)
    geckoEvent.button = nsMouseEvent::eRightButton;
  else
    geckoEvent.button = nsMouseEvent::eLeftButton;

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
  if (mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
      NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
      cocoaEvent.type = NPCocoaEventMouseUp;
      cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = point.x;
      cocoaEvent.data.mouse.pluginY = point.y;
      cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
      cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
      cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
      geckoEvent.pluginEvent = &cocoaEvent;
    }
  }

  // This might destroy our widget (and null out mGeckoChild).
  bool defaultPrevented = mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Check to see if we are double-clicking in the titlebar.
  CGFloat locationInTitlebar = [[self window] frame].size.height - [theEvent locationInWindow].y;
  if (!defaultPrevented && [theEvent clickCount] == 2 &&
      [[self window] isMovableByWindowBackground] &&
      [self shouldMinimizeOnTitlebarDoubleClick] &&
      [[self window] isKindOfClass:[ToolbarWindow class]] &&
      (locationInTitlebar < [(ToolbarWindow*)[self window] titlebarHeight] ||
       locationInTitlebar < [(ToolbarWindow*)[self window] unifiedToolbarHeight])) {

    NSButton *minimizeButton = [[self window] standardWindowButton:NSWindowMiniaturizeButton];
    [minimizeButton performClick:self];
  }

  // If our mouse-up event's location is over some other object (as might
  // happen if it came at the end of a dragging operation), also send our
  // Gecko frame a mouse-exit event.
  if (mGeckoChild && mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      if (ChildViewMouseTracker::ViewForEvent(theEvent) != self) {
        nsMouseEvent geckoExitEvent(true, NS_MOUSE_EXIT, mGeckoChild, nsMouseEvent::eReal);
        [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoExitEvent];

        NPCocoaEvent cocoaEvent;
        nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
        NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
        cocoaEvent.type = NPCocoaEventMouseExited;
        cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
        cocoaEvent.data.mouse.pluginX = point.x;
        cocoaEvent.data.mouse.pluginY = point.y;
        cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
        cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
        cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
        cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
        geckoExitEvent.pluginEvent = &cocoaEvent;

        mGeckoChild->DispatchWindowEvent(geckoExitEvent);
      }
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                             type:(nsMouseEvent::exitType)aType
{
  if (!mGeckoChild)
    return;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, [self window]);
  NSPoint localEventLocation = [self convertPoint:windowEventLocation fromView:nil];

  uint32_t msg = aEnter ? NS_MOUSE_ENTER : NS_MOUSE_EXIT;
  nsMouseEvent event(true, msg, mGeckoChild, nsMouseEvent::eReal);
  event.refPoint = LayoutDeviceIntPoint::FromUntyped(
    mGeckoChild->CocoaPointsToDevPixels(localEventLocation));

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
  NPCocoaEvent cocoaEvent;
  if (mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
      cocoaEvent.type = ((msg == NS_MOUSE_ENTER) ? NPCocoaEventMouseEntered : NPCocoaEventMouseExited);
      cocoaEvent.data.mouse.modifierFlags = [aEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = 5;
      cocoaEvent.data.mouse.pluginY = 5;
      cocoaEvent.data.mouse.buttonNumber = [aEvent buttonNumber];
      cocoaEvent.data.mouse.deltaX = [aEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [aEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [aEvent deltaZ];
      event.pluginEvent = &cocoaEvent;
    }
  }

  event.exit = aType;

  nsEventStatus status; // ignored
  mGeckoChild->DispatchEvent(&event, status);
}

- (void)updateWindowDraggableStateOnMouseMove:(NSEvent*)theEvent
{
  if (!theEvent || !mGeckoChild) {
    return;
  }

  nsCocoaWindow* windowWidget = mGeckoChild->GetXULWindowWidget();
  if (!windowWidget) {
    return;
  }

  // We assume later on that sending a hit test event won't cause widget destruction.
  nsMouseEvent hitTestEvent(true, NS_MOUSE_MOZHITTEST, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&hitTestEvent];
  bool result = mGeckoChild->DispatchWindowEvent(hitTestEvent);

  [windowWidget->GetCocoaWindow() setMovableByWindowBackground:result];
}

- (void)handleMouseMoved:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(true, NS_MOUSE_MOVE, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
  NPCocoaEvent cocoaEvent;
  if (mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
      NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
      cocoaEvent.type = NPCocoaEventMouseMoved;
      cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = point.x;
      cocoaEvent.data.mouse.pluginY = point.y;
      cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
      cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
      cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
      geckoEvent.pluginEvent = &cocoaEvent;
    }
  }
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  gLastDragView = self;

  NPCocoaEvent cocoaEvent;

  nsMouseEvent geckoEvent(true, NS_MOUSE_MOVE, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  // create event for use by plugins
  if (mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
      NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
      cocoaEvent.type = NPCocoaEventMouseDragged;
      cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = point.x;
      cocoaEvent.data.mouse.pluginY = point.y;
      cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
      cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
      cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
      geckoEvent.pluginEvent = &cocoaEvent;
    }
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);

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

  // The right mouse went down, fire off a right mouse down event to gecko
  nsMouseEvent geckoEvent(true, NS_MOUSE_BUTTON_DOWN, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;
  geckoEvent.clickCount = [theEvent clickCount];

  // create event for use by plugins
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseDown;
    cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);
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

  NPCocoaEvent cocoaEvent;

  nsMouseEvent geckoEvent(true, NS_MOUSE_BUTTON_UP, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;
  geckoEvent.clickCount = [theEvent clickCount];

  // create event for use by plugins
  if (mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
      NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
      cocoaEvent.type = NPCocoaEventMouseUp;
      cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = point.x;
      cocoaEvent.data.mouse.pluginY = point.y;
      cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
      cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
      cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
      geckoEvent.pluginEvent = &cocoaEvent;
    }
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDragged:(NSEvent*)theEvent
{
  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(true, NS_MOUSE_MOVE, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchWindowEvent(geckoEvent);
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

  nsMouseEvent geckoEvent(true, NS_MOUSE_BUTTON_DOWN, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eMiddleButton;
  geckoEvent.clickCount = [theEvent clickCount];

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(true, NS_MOUSE_BUTTON_UP, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eMiddleButton;

  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

- (void)otherMouseDragged:(NSEvent*)theEvent
{
  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(true, NS_MOUSE_MOVE, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eMiddleButton;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

static int32_t RoundUp(double aDouble)
{
  return aDouble < 0 ? static_cast<int32_t>(floor(aDouble)) :
                       static_cast<int32_t>(ceil(aDouble));
}

- (void)scrollWheel:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  ChildViewMouseTracker::MouseScrolled(theEvent);

  if ([self maybeRollup:theEvent]) {
    return;
  }

  if (!mGeckoChild) {
    return;
  }

  WheelEvent wheelEvent(true, NS_WHEEL_WHEEL, mGeckoChild);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&wheelEvent];
  wheelEvent.deltaMode =
    Preferences::GetBool("mousewheel.enable_pixel_scrolling", true) ?
      nsIDOMWheelEvent::DOM_DELTA_PIXEL : nsIDOMWheelEvent::DOM_DELTA_LINE;

  // Calling deviceDeltaX or deviceDeltaY on theEvent will trigger a Cocoa
  // assertion and an Objective-C NSInternalInconsistencyException if the
  // underlying "Carbon" event doesn't contain pixel scrolling information.
  // For these events, carbonEventKind is kEventMouseWheelMoved instead of
  // kEventMouseScroll.
  if (wheelEvent.deltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL) {
    EventRef theCarbonEvent = [theEvent _eventRef];
    UInt32 carbonEventKind = theCarbonEvent ? ::GetEventKind(theCarbonEvent) : 0;
    if (carbonEventKind != kEventMouseScroll) {
      wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;
    }
  }

  wheelEvent.lineOrPageDeltaX = RoundUp(-[theEvent deltaX]);
  wheelEvent.lineOrPageDeltaY = RoundUp(-[theEvent deltaY]);

  if (wheelEvent.deltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL) {
    // Some scrolling devices supports pixel scrolling, e.g. a Macbook
    // touchpad or a Mighty Mouse. On those devices, [theEvent deviceDeltaX/Y]
    // contains the amount of pixels to scroll. Since Lion this has changed 
    // to [theEvent scrollingDeltaX/Y].
    double scale = mGeckoChild->BackingScaleFactor();
    if ([theEvent respondsToSelector:@selector(scrollingDeltaX)]) {
      wheelEvent.deltaX = -[theEvent scrollingDeltaX] * scale;
      wheelEvent.deltaY = -[theEvent scrollingDeltaY] * scale;
    } else {
      wheelEvent.deltaX = -[theEvent deviceDeltaX] * scale;
      wheelEvent.deltaY = -[theEvent deviceDeltaY] * scale;
    }
  } else {
    wheelEvent.deltaX = -[theEvent deltaX];
    wheelEvent.deltaY = -[theEvent deltaY];
  }

  // TODO: We should not set deltaZ for now because we're not sure if we should
  //       revert the sign.
  // wheelEvent.deltaZ = [theEvent deltaZ];

  if (!wheelEvent.deltaX && !wheelEvent.deltaY && !wheelEvent.deltaZ) {
    // No sense in firing off a Gecko event.
    return;
  }

  wheelEvent.isMomentum = nsCocoaUtils::IsMomentumScrollEvent(theEvent);

  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventScrollWheel;
    cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = 0;
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    wheelEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(wheelEvent);
  if (!mGeckoChild) {
    return;
  }

#ifdef __LP64__
  // overflowDeltaX and overflowDeltaY tell us when the user has tried to
  // scroll past the edge of a page (in those cases it's non-zero).
  if ((wheelEvent.deltaMode == nsIDOMWheelEvent::DOM_DELTA_PIXEL) &&
      (wheelEvent.viewPortIsScrollTargetParent) &&
      (wheelEvent.deltaX != 0.0 || wheelEvent.deltaY != 0.0)) {
    [self maybeTrackScrollEventAsSwipe:theEvent
                       scrollOverflowX:wheelEvent.overflowDeltaX
                       scrollOverflowY:wheelEvent.overflowDeltaY];
  }
#endif // #ifdef __LP64__

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

-(NSMenu*)menuForEvent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mGeckoChild || [self isPluginView])
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

  nsMouseEvent geckoEvent(true, NS_CONTEXTMENU, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;
  mGeckoChild->DispatchWindowEvent(geckoEvent);
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

- (void) convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(nsInputEvent*)outGeckoEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(outGeckoEvent, "convertCocoaMouseEvent:toGeckoEvent: requires non-null aoutGeckoEvent");
  if (!outGeckoEvent)
    return;

  nsCocoaUtils::InitInputEvent(*outGeckoEvent, aMouseEvent);

  // convert point to view coordinate system
  NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(aMouseEvent, [self window]);
  NSPoint localPoint = [self convertPoint:locationInWindow fromView:nil];

  outGeckoEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(
    mGeckoChild->CocoaPointsToDevPixels(localPoint));

  nsMouseEvent_base* mouseEvent =
    static_cast<nsMouseEvent_base*>(outGeckoEvent);
  mouseEvent->buttons = 0;
  NSUInteger mouseButtons = [NSEvent pressedMouseButtons];

  if (mouseButtons & 0x01) {
    mouseEvent->buttons |= nsMouseEvent::eLeftButtonFlag;
  }
  if (mouseButtons & 0x02) {
    mouseEvent->buttons |= nsMouseEvent::eRightButtonFlag;
  }
  if (mouseButtons & 0x04) {
    mouseEvent->buttons |= nsMouseEvent::eMiddleButtonFlag;
  }
  if (mouseButtons & 0x08) {
    mouseEvent->buttons |= nsMouseEvent::e4thButtonFlag;
  }
  if (mouseButtons & 0x10) {
    mouseEvent->buttons |= nsMouseEvent::e5thButtonFlag;
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
      if ([aMouseEvent subtype] == NSTabletPointEventSubtype) {
        mouseEvent->pressure = [aMouseEvent pressure];
      }
      break;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


#pragma mark -
// NSTextInput implementation

- (void)insertText:(id)insertString
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE_VOID(mGeckoChild);

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  NSAttributedString* attrStr;
  if ([insertString isKindOfClass:[NSAttributedString class]]) {
    attrStr = static_cast<NSAttributedString*>(insertString);
  } else {
    attrStr =
      [[[NSAttributedString alloc] initWithString:insertString] autorelease];
  }

  mTextInputHandler->InsertText(attrStr);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)insertNewline:(id)sender
{
  [self insertText:@"\n"];
}

- (void) doCommandBySelector:(SEL)aSelector
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

- (void) setMarkedText:(id)aString selectedRange:(NSRange)selRange
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

  mTextInputHandler->SetMarkedText(attrStr, selRange);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) unmarkText
{
  NS_ENSURE_TRUE(mTextInputHandler, );
  mTextInputHandler->CommitIMEComposition();
}

- (BOOL) hasMarkedText
{
  NS_ENSURE_TRUE(mTextInputHandler, NO);
  return mTextInputHandler->HasMarkedText();
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

- (NSInteger) conversationIdentifier
{
  NS_ENSURE_TRUE(mTextInputHandler, reinterpret_cast<NSInteger>(self));
  return mTextInputHandler->ConversationIdentifier();
}

- (NSAttributedString *) attributedSubstringFromRange:(NSRange)theRange
{
  NS_ENSURE_TRUE(mTextInputHandler, nil);
  return mTextInputHandler->GetAttributedSubstringFromRange(theRange);
}

- (NSRange) markedRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->MarkedRange();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRange) selectedRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, NSMakeRange(NSNotFound, 0));
  return mTextInputHandler->SelectedRange();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRect) firstRectForCharacterRange:(NSRange)theRange
{
  NSRect rect;
  NS_ENSURE_TRUE(mTextInputHandler, rect);
  return mTextInputHandler->FirstRectForCharacterRange(theRange);
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
  NS_ENSURE_TRUE(mTextInputHandler, 0);
  return mTextInputHandler->CharacterIndexForPoint(thePoint);
}

- (NSArray*) validAttributesForMarkedText
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NS_ENSURE_TRUE(mTextInputHandler, [NSArray array]);
  return mTextInputHandler->GetValidAttributesForMarkedText();

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#pragma mark -
// NSTextInputClient implementation

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

- (NSInteger)windowLevel
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ENSURE_TRUE(mTextInputHandler, [[self window] level]);
  return mTextInputHandler->GetWindowLevel();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSNormalWindowLevel);
}

#pragma mark -

#ifdef __LP64__
- (NSTextInputContext *)inputContext
{
  if (mIsPluginView && mPluginEventModel == NPEventModelCocoa)
    return [[ComplexTextInputPanel sharedComplexTextInputPanel] inputContext];
  else
    return [super inputContext];
}
#endif

// This is a private API that Cocoa uses.
// Cocoa will call this after the menu system returns "NO" for "performKeyEquivalent:".
// We want all they key events we can get so just return YES. In particular, this fixes
// ctrl-tab - we don't get a "keyDown:" call for that without this.
- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event
{
  return YES;
}

- (void)keyDown:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#if !defined(RELEASE_BUILD) || defined(DEBUG)
  if (mGeckoChild && mTextInputHandler && mTextInputHandler->IsFocused()) {
#ifdef MOZ_CRASHREPORTER
    NSWindow* window = [self window];
    NSString* info = [NSString stringWithFormat:@"\nview [%@], window [%@], key event [%@], window is key %i, is fullscreen %i, app is active %i",
                      self, window, theEvent, [window isKeyWindow], ([window styleMask] & (1 << 14)) != 0,
                      [NSApp isActive]];
    nsAutoCString additionalInfo([info UTF8String]);
#endif
    if (mIsPluginView) {
      if (TextInputHandler::IsSecureEventInputEnabled()) {
        #define CRASH_MESSAGE "While a plugin has focus, we must not be in secure mode"
#ifdef MOZ_CRASHREPORTER
        CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("\nBug 893973: ") +
                                                   NS_LITERAL_CSTRING(CRASH_MESSAGE));
        CrashReporter::AppendAppNotesToCrashReport(additionalInfo);
#endif
        MOZ_CRASH(CRASH_MESSAGE);
        #undef CRASH_MESSAGE
      }
    } else if (mGeckoChild->GetInputContext().IsPasswordEditor() &&
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
#endif // #if !defined(RELEASE_BUILD) || defined(DEBUG)

  if (mGeckoChild && mTextInputHandler && mIsPluginView) {
    mTextInputHandler->HandleKeyDownEventForPlugin(theEvent);
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  bool handled = false;
  if (mGeckoChild && mTextInputHandler) {
    handled = mTextInputHandler->HandleKeyDownEvent(theEvent);
  }

  // We always allow keyboard events to propagate to keyDown: but if they are not
  // handled we give special Application menu items a chance to act.
  if (!handled && sApplicationMenu) {
    [sApplicationMenu performKeyEquivalent:theEvent];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)keyUp:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ENSURE_TRUE(mGeckoChild, );

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if (mIsPluginView) {
    mTextInputHandler->HandleKeyUpEventForPlugin(theEvent);
    return;
  }

  mTextInputHandler->HandleKeyUpEvent(theEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
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

  nsMouseEvent geckoEvent(true, NS_MOUSE_ACTIVATE, mGeckoChild, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:aEvent toGeckoEvent:&geckoEvent];
  return !mGeckoChild->DispatchWindowEvent(geckoEvent);
}

// Don't focus a plugin if the user has clicked on a DOM element above it.
// In this case the user has actually clicked on the plugin's ChildView
// (underneath the non-plugin DOM element).  But we shouldn't allow the
// ChildView to be focused.  See bug 627649.
- (BOOL)currentEventShouldFocusPlugin
{
  if (!mGeckoChild)
    return NO;

  NSEvent* currentEvent = [NSApp currentEvent];
  if ([currentEvent type] != NSLeftMouseDown)
    return YES;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // hitTest needs coordinates in device pixels
  NSPoint eventLoc = nsCocoaUtils::ScreenLocationForEvent(currentEvent);
  eventLoc.y = nsCocoaUtils::FlippedScreenY(eventLoc.y);
  LayoutDeviceIntPoint widgetLoc = LayoutDeviceIntPoint::FromUntyped(
    mGeckoChild->CocoaPointsToDevPixels(eventLoc) -
    mGeckoChild->WidgetToScreenOffset());

  nsQueryContentEvent hitTest(true, NS_QUERY_DOM_WIDGET_HITTEST, mGeckoChild);
  hitTest.InitForQueryDOMWidgetHittest(widgetLoc);
  // This might destroy our widget (and null out mGeckoChild).
  mGeckoChild->DispatchWindowEvent(hitTest);
  if (!mGeckoChild)
    return NO;
  if (hitTest.mSucceeded && !hitTest.mReply.mWidgetIsHit)
    return NO;

  return YES;
}

// Don't focus a plugin if we're in a left click-through that will fail (see
// [ChildView isInFailingLeftClickThrough] above).
- (BOOL)shouldFocusPlugin:(BOOL)getFocus
{
  if (!mGeckoChild)
    return NO;

  nsCocoaWindow* windowWidget = mGeckoChild->GetXULWindowWidget();
  if (windowWidget && !windowWidget->ShouldFocusPlugin())
    return NO;

  if (getFocus && ![self currentEventShouldFocusPlugin])
    return NO;

  return YES;
}

// Returns NO if the plugin shouldn't be focused/unfocused.
- (BOOL)updatePluginFocusStatus:(BOOL)getFocus
{
  if (!mGeckoChild)
    return NO;

  if (![self shouldFocusPlugin:getFocus])
    return NO;

  if (mPluginEventModel == NPEventModelCocoa) {
    nsPluginEvent pluginEvent(true, NS_PLUGIN_FOCUS_EVENT, mGeckoChild);
    NPCocoaEvent cocoaEvent;
    nsCocoaUtils::InitNPCocoaEvent(&cocoaEvent);
    cocoaEvent.type = NPCocoaEventFocusChanged;
    cocoaEvent.data.focus.hasFocus = getFocus;
    nsCocoaUtils::InitPluginEvent(pluginEvent, cocoaEvent);
    mGeckoChild->DispatchWindowEvent(pluginEvent);

    if (getFocus)
      [self sendFocusEvent:NS_PLUGIN_FOCUS];
  }

  return YES;
}

// We must always call through to our superclass, even when mGeckoChild is
// nil -- otherwise the keyboard focus can end up in the wrong NSView.
- (BOOL)becomeFirstResponder
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (mIsPluginView) {
    if (![self updatePluginFocusStatus:YES])
      return NO;
  }

  return [super becomeFirstResponder];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(YES);
}

// We must always call through to our superclass, even when mGeckoChild is
// nil -- otherwise the keyboard focus can end up in the wrong NSView.
- (BOOL)resignFirstResponder
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (mIsPluginView) {
    if (![self updatePluginFocusStatus:NO])
      return NO;
  }

  return [super resignFirstResponder];

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

- (NSDragOperation)dragOperationForSession:(nsIDragSession*)aDragSession
{
  uint32_t dragAction;
  aDragSession->GetDragAction(&dragAction);
  if (nsIDragService::DRAGDROP_ACTION_LINK & dragAction)
    return NSDragOperationLink;
  if (nsIDragService::DRAGDROP_ACTION_COPY & dragAction)
    return NSDragOperationCopy;
  if (nsIDragService::DRAGDROP_ACTION_MOVE & dragAction)
    return NSDragOperationGeneric;
  return NSDragOperationNone;
}

// This is a utility function used by NSView drag event methods
// to send events. It contains all of the logic needed for Gecko
// dragging to work. Returns the appropriate cocoa drag operation code.
- (NSDragOperation)doDragAction:(uint32_t)aMessage sender:(id)aSender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mGeckoChild)
    return NSDragOperationNone;

  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView doDragAction: entered\n"));

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
    if (!mDragService)
      return NSDragOperationNone;
  }

  if (aMessage == NS_DRAGDROP_ENTER)
    mDragService->StartDragSession();

  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession) {
    if (aMessage == NS_DRAGDROP_OVER) {
      // fire the drag event at the source. Just ignore whether it was
      // cancelled or not as there isn't actually a means to stop the drag
      mDragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
      dragSession->SetCanDrop(false);
    }
    else if (aMessage == NS_DRAGDROP_DROP) {
      // We make the assumption that the dragOver handlers have correctly set
      // the |canDrop| property of the Drag Session.
      bool canDrop = false;
      if (!NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)) || !canDrop) {
        [self doDragAction:NS_DRAGDROP_EXIT sender:aSender];

        nsCOMPtr<nsIDOMNode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          mDragService->EndDragSession(false);
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
  nsDragEvent geckoEvent(true, aMessage, mGeckoChild);
  nsCocoaUtils::InitInputEvent(geckoEvent, [NSApp currentEvent]);

  // Use our own coordinates in the gecko event.
  // Convert event from gecko global coords to gecko view coords.
  NSPoint draggingLoc = [aSender draggingLocation];
  NSPoint localPoint = [self convertPoint:draggingLoc fromView:nil];

  geckoEvent.refPoint = LayoutDeviceIntPoint::FromUntyped(
    mGeckoChild->CocoaPointsToDevPixels(localPoint));

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchWindowEvent(geckoEvent);
  if (!mGeckoChild)
    return NSDragOperationNone;

  if (dragSession) {
    switch (aMessage) {
      case NS_DRAGDROP_ENTER:
      case NS_DRAGDROP_OVER:
        return [self dragOperationForSession:dragSession];
      case NS_DRAGDROP_EXIT:
      case NS_DRAGDROP_DROP: {
        nsCOMPtr<nsIDOMNode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          // We're leaving a window while doing a drag that was
          // initiated in a different app. End the drag session,
          // since we're done with it for now (until the user
          // drags back into mozilla).
          mDragService->EndDragSession(false);
        }
      }
    }
  }

  return NSDragOperationGeneric;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSDragOperationNone);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView draggingEntered: entered\n"));
  
  // there should never be a globalDragPboard when "draggingEntered:" is
  // called, but just in case we'll take care of it here.
  [globalDragPboard release];

  // Set the global drag pasteboard that will be used for this drag session.
  // This will be set back to nil when the drag session ends (mouse exits
  // the view or a drop happens within the view).
  globalDragPboard = [[sender draggingPasteboard] retain];

  return [self doDragAction:NS_DRAGDROP_ENTER sender:sender];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSDragOperationNone);
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView draggingUpdated: entered\n"));

  return [self doDragAction:NS_DRAGDROP_OVER sender:sender];
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView draggingExited: entered\n"));

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  [self doDragAction:NS_DRAGDROP_EXIT sender:sender];
  NS_IF_RELEASE(mDragService);
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  BOOL handled = [self doDragAction:NS_DRAGDROP_DROP sender:sender] != NSDragOperationNone;
  NS_IF_RELEASE(mDragService);
  return handled;
}

// NSDraggingSource
- (void)draggedImage:(NSImage *)anImage movedTo:(NSPoint)aPoint
{
  // Get the drag service if it isn't already cached. The drag service
  // isn't cached when dragging over a different application.
  nsCOMPtr<nsIDragService> dragService = mDragService;
  if (!dragService) {
    dragService = do_GetService(kDragServiceContractID);
  }

  if (dragService) {
    NSPoint pnt = [NSEvent mouseLocation];
    FlipCocoaScreenCoordinate(pnt);
    dragService->DragMoved(NSToIntRound(pnt.x), NSToIntRound(pnt.y));
  }
}

// NSDraggingSource
- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  gDraggedTransferables = nullptr;

  NSEvent *currentEvent = [NSApp currentEvent];
  gUserCancelledDrag = ([currentEvent type] == NSKeyDown &&
                        [currentEvent keyCode] == kVK_Escape);

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
  }

  if (mDragService) {
    // set the dragend point from the current mouse location
    nsDragService* dragService = static_cast<nsDragService *>(mDragService);
    NSPoint pnt = [NSEvent mouseLocation];
    FlipCocoaScreenCoordinate(pnt);
    dragService->SetDragEndPoint(nsIntPoint(NSToIntRound(pnt.x), NSToIntRound(pnt.y)));

    // XXX: dropEffect should be updated per |operation|. 
    // As things stand though, |operation| isn't well handled within "our"
    // events, that is, when the drop happens within the window: it is set
    // either to NSDragOperationGeneric or to NSDragOperationNone.
    // For that reason, it's not yet possible to override dropEffect per the
    // given OS value, and it's also unclear what's the correct dropEffect 
    // value for NSDragOperationGeneric that is passed by other applications.
    // All that said, NSDragOperationNone is still reliable.
    if (operation == NSDragOperationNone) {
      nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
      dragService->GetDataTransfer(getter_AddRefs(dataTransfer));
      if (dataTransfer)
        dataTransfer->SetDropEffectInt(nsIDragService::DRAGDROP_ACTION_NONE);
    }

    mDragService->EndDragSession(true);
    NS_RELEASE(mDragService);
  }

  [globalDragPboard release];
  globalDragPboard = nil;
  [gLastDragMouseDownEvent release];
  gLastDragMouseDownEvent = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSDraggingSource
// this is just implemented so we comply with the NSDraggingSource informal protocol
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
  return UINT_MAX;
}

// This method is a callback typically invoked in response to a drag ending on the desktop
// or a Findow folder window; the argument passed is a path to the drop location, to be used
// in constructing a complete pathname for the file(s) we want to create as a result of
// the drag.
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDestination
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsresult rv;

  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView namesOfPromisedFilesDroppedAtDestination: entering callback for promised files\n"));

  nsCOMPtr<nsIFile> targFile;
  NS_NewLocalFile(EmptyString(), true, getter_AddRefs(targFile));
  nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(targFile);
  if (!macLocalFile) {
    NS_ERROR("No Mac local file");
    return nil;
  }

  if (!NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)dropDestination))) {
    NS_ERROR("failed InitWithCFURL");
    return nil;
  }

  if (!gDraggedTransferables)
    return nil;

  uint32_t transferableCount;
  rv = gDraggedTransferables->Count(&transferableCount);
  if (NS_FAILED(rv))
    return nil;

  for (uint32_t i = 0; i < transferableCount; i++) {
    nsCOMPtr<nsISupports> genericItem;
    gDraggedTransferables->GetElementAt(i, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> item(do_QueryInterface(genericItem));
    if (!item) {
      NS_ERROR("no transferable");
      return nil;
    }
    item->Init(nullptr);

    item->SetTransferData(kFilePromiseDirectoryMime, macLocalFile, sizeof(nsIFile*));
    
    // now request the kFilePromiseMime data, which will invoke the data provider
    // If successful, the returned data is a reference to the resulting file.
    nsCOMPtr<nsISupports> fileDataPrimitive;
    uint32_t dataSize = 0;
    item->GetTransferData(kFilePromiseMime, getter_AddRefs(fileDataPrimitive), &dataSize);
  }
  
  NSPasteboard* generalPboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  NSData* data = [generalPboard dataForType:@"application/x-moz-file-promise-dest-filename"];
  NSString* name = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  NSArray* rslt = [NSArray arrayWithObject:name];

  [name release];

  return rslt;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
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

#define IsSupportedType(typeStr) ([typeStr isEqual:NSStringPboardType] || [typeStr isEqual:NSHTMLPboardType])

  id result = nil;

  if ((!sendType || IsSupportedType(sendType)) &&
      (!returnType || IsSupportedType(returnType))) {
    if (mGeckoChild) {
      // Assume that this object will be able to handle this request.
      result = self;

      // Keep the ChildView alive during this operation.
      nsAutoRetainCocoaObject kungFuDeathGrip(self);
      
      // Determine if there is a selection (if sending to the service).
      if (sendType) {
        nsQueryContentEvent event(true, NS_QUERY_CONTENT_STATE, mGeckoChild);
        // This might destroy our widget (and null out mGeckoChild).
        mGeckoChild->DispatchWindowEvent(event);
        if (!mGeckoChild || !event.mSucceeded || !event.mReply.mHasSelection)
          result = nil;
      }

      // Determine if we can paste (if receiving data from the service).
      if (mGeckoChild && returnType) {
        nsContentCommandEvent command(true, NS_CONTENT_COMMAND_PASTE_TRANSFERABLE, mGeckoChild, true);
        // This might possibly destroy our widget (and null out mGeckoChild).
        mGeckoChild->DispatchWindowEvent(command);
        if (!mGeckoChild || !command.mSucceeded || !command.mIsEnabled)
          result = nil;
      }
    }
  }

#undef IsSupportedType

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
  if ([types containsObject:NSStringPboardType] == NO &&
      [types containsObject:NSHTMLPboardType] == NO)
    return NO;

  // Bail out if there is no Gecko object.
  if (!mGeckoChild)
    return NO;

  // Obtain the current selection.
  nsQueryContentEvent event(true,
                            NS_QUERY_SELECTION_AS_TRANSFERABLE,
                            mGeckoChild);
  mGeckoChild->DispatchWindowEvent(event);
  if (!event.mSucceeded || !event.mReply.mTransferable)
    return NO;

  // Transform the transferable to an NSDictionary.
  NSDictionary* pasteboardOutputDict = nsClipboard::PasteboardDictFromTransferable(event.mReply.mTransferable);
  if (!pasteboardOutputDict)
    return NO;

  // Declare the pasteboard types.
  unsigned int typeCount = [pasteboardOutputDict count];
  NSMutableArray * types = [NSMutableArray arrayWithCapacity:typeCount];
  [types addObjectsFromArray:[pasteboardOutputDict allKeys]];
  [pboard declareTypes:types owner:nil];

  // Write the data to the pasteboard.
  for (unsigned int i = 0; i < typeCount; i++) {
    NSString* currentKey = [types objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];

    if (currentKey == NSStringPboardType ||
        currentKey == kCorePboardType_url ||
        currentKey == kCorePboardType_urld ||
        currentKey == kCorePboardType_urln) {
      [pboard setString:currentValue forType:currentKey];
    } else if (currentKey == NSHTMLPboardType) {
      [pboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue)) forType:currentKey];
    } else if (currentKey == NSTIFFPboardType) {
      [pboard setData:currentValue forType:currentKey];
    } else if (currentKey == NSFilesPromisePboardType) {
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

  nsContentCommandEvent command(true,
                                NS_CONTENT_COMMAND_PASTE_TRANSFERABLE,
                                mGeckoChild);
  command.mTransferable = trans;
  mGeckoChild->DispatchWindowEvent(command);
  
  return command.mSucceeded && command.mIsEnabled;
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
  nsCOMPtr<nsIWidget> kungFuDeathGrip2(mGeckoChild);
  nsRefPtr<a11y::Accessible> accessible = mGeckoChild->GetDocumentAccessible();
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
ChildViewMouseTracker::ReEvaluateMouseEnterState(NSEvent* aEvent)
{
  ChildView* oldView = sLastMouseEventView;
  sLastMouseEventView = ViewForEvent(aEvent);
  if (sLastMouseEventView != oldView) {
    // Send enter and / or exit events.
    nsMouseEvent::exitType type = [sLastMouseEventView window] == [oldView window] ?
                                    nsMouseEvent::eChild : nsMouseEvent::eTopLevel;
    [oldView sendMouseEnterOrExitEvent:aEvent enter:NO type:type];
    // After the cursor exits the window set it to a visible regular arrow cursor.
    if (type == nsMouseEvent::eTopLevel) {
      [[nsCursorManager sharedInstance] setCursor:eCursor_standard];
    }
    [sLastMouseEventView sendMouseEnterOrExitEvent:aEvent enter:YES type:type];
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

  nsWindowType windowType;
  windowWidget->GetWindowType(windowType);

  NSWindow* topLevelWindow = nil;

  switch (windowType) {
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
