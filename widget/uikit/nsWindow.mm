/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIEvent.h>
#import <UIKit/UIGraphics.h>
#import <UIKit/UIInterface.h>
#import <UIKit/UIScreen.h>
#import <UIKit/UITapGestureRecognizer.h>
#import <UIKit/UITouch.h>
#import <UIKit/UIView.h>
#import <UIKit/UIViewController.h>
#import <UIKit/UIWindow.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>

#include "nsWindow.h"
#include "nsScreenManager.h"
#include "nsAppShell.h"

#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

#include "gfxQuartzSurface.h"
#include "gfxUtils.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "nsRegion.h"
#include "Layers.h"
#include "nsTArray.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/unused.h"

#include "GeckoProfiler.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

#define ALOG(args...) fprintf(stderr, args); fprintf(stderr, "\n")

static LayoutDeviceIntPoint
UIKitPointsToDevPixels(CGPoint aPoint, CGFloat aBackingScale)
{
    return LayoutDeviceIntPoint(NSToIntRound(aPoint.x * aBackingScale),
                                NSToIntRound(aPoint.y * aBackingScale));
}

static CGRect
DevPixelsToUIKitPoints(const LayoutDeviceIntRect& aRect, CGFloat aBackingScale)
{
    return CGRectMake((CGFloat)aRect.x / aBackingScale,
                      (CGFloat)aRect.y / aBackingScale,
                      (CGFloat)aRect.width / aBackingScale,
                      (CGFloat)aRect.height / aBackingScale);
}

// Used to retain a Cocoa object for the remainder of a method's execution.
class nsAutoRetainUIKitObject {
public:
nsAutoRetainUIKitObject(id anObject)
{
  mObject = [anObject retain];
}
~nsAutoRetainUIKitObject()
{
  [mObject release];
}
private:
  id mObject;  // [STRONG]
};

@interface ChildView : UIView
{
@public
    nsWindow* mGeckoChild; // weak ref
    BOOL mWaitingForPaint;
    CFMutableDictionaryRef mTouches;
    int mNextTouchID;
}
// sets up our view, attaching it to its owning gecko view
- (id)initWithFrame:(CGRect)inFrame geckoChild:(nsWindow*)inChild;
// Our Gecko child was Destroy()ed
- (void)widgetDestroyed;
// Tear down this ChildView
- (void)delayedTearDown;
- (void)sendMouseEvent:(EventMessage) aType point:(LayoutDeviceIntPoint)aPoint widget:(nsWindow*)aWindow;
- (void)handleTap:(UITapGestureRecognizer *)sender;
- (BOOL)isUsingMainThreadOpenGL;
- (void)drawUsingOpenGL;
- (void)drawUsingOpenGLCallback;
- (void)sendTouchEvent:(EventMessage) aType touches:(NSSet*)aTouches widget:(nsWindow*)aWindow;
// Event handling (UIResponder)
- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event;
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event;
@end

@implementation ChildView
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (id)initWithFrame:(CGRect)inFrame geckoChild:(nsWindow*)inChild
{
    self.multipleTouchEnabled = YES;
    if ((self = [super initWithFrame:inFrame])) {
      mGeckoChild = inChild;
    }
    ALOG("[ChildView[%p] initWithFrame:] (mGeckoChild = %p)", (void*)self, (void*)mGeckoChild);
    self.opaque = YES;
    self.alpha = 1.0;

    UITapGestureRecognizer *tapRecognizer = [[UITapGestureRecognizer alloc]
          initWithTarget:self action:@selector(handleTap:)];
    tapRecognizer.numberOfTapsRequired = 1;
    [self addGestureRecognizer:tapRecognizer];

    mTouches = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr);
    mNextTouchID = 0;
    return self;
}

- (void)widgetDestroyed
{
  mGeckoChild = nullptr;
  CFRelease(mTouches);
}

- (void)delayedTearDown
{
  [self removeFromSuperview];
  [self release];
}

- (void)sendMouseEvent:(EventMessage) aType point:(LayoutDeviceIntPoint)aPoint widget:(nsWindow*)aWindow
{
    WidgetMouseEvent event(true, aType, aWindow,
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

    event.mRefPoint = aPoint;
    event.mClickCount = 1;
    event.button = WidgetMouseEvent::eLeftButton;
    event.mTime = PR_IntervalNow();
    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;

    nsEventStatus status;
    aWindow->DispatchEvent(&event, status);
}

- (void)handleTap:(UITapGestureRecognizer *)sender
{
    if (sender.state == UIGestureRecognizerStateEnded) {
        ALOG("[ChildView[%p] handleTap]", self);
        LayoutDeviceIntPoint lp = UIKitPointsToDevPixels([sender locationInView:self], [self contentScaleFactor]);
        [self sendMouseEvent:eMouseMove point:lp widget:mGeckoChild];
        [self sendMouseEvent:eMouseDown point:lp widget:mGeckoChild];
        [self sendMouseEvent:eMouseUp point:lp widget:mGeckoChild];
    }
}

- (void)sendTouchEvent:(EventMessage) aType touches:(NSSet*)aTouches widget:(nsWindow*)aWindow
{
    WidgetTouchEvent event(true, aType, aWindow);
    //XXX: I think nativeEvent.timestamp * 1000 is probably usable here but
    // I don't care that much right now.
    event.mTime = PR_IntervalNow();
    event.mTouches.SetCapacity(aTouches.count);
    for (UITouch* touch in aTouches) {
        LayoutDeviceIntPoint loc = UIKitPointsToDevPixels([touch locationInView:self], [self contentScaleFactor]);
        LayoutDeviceIntPoint radius = UIKitPointsToDevPixels([touch majorRadius], [touch majorRadius]);
        void* value;
        if (!CFDictionaryGetValueIfPresent(mTouches, touch, (const void**)&value)) {
            // This shouldn't happen.
            NS_ASSERTION(false, "Got a touch that we didn't know about");
            continue;
        }
        int id = reinterpret_cast<int>(value);
        RefPtr<Touch> t = new Touch(id, loc, radius, 0.0f, 1.0f);
        event.mRefPoint = loc;
        event.mTouches.AppendElement(t);
    }
    aWindow->DispatchInputEvent(&event);
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    ALOG("[ChildView[%p] touchesBegan", self);
    if (!mGeckoChild)
        return;

    for (UITouch* touch : touches) {
        CFDictionaryAddValue(mTouches, touch, (void*)mNextTouchID);
        mNextTouchID++;
    }
    [self sendTouchEvent:eTouchStart
                 touches:[event allTouches]
                  widget:mGeckoChild];
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    ALOG("[ChildView[%p] touchesCancelled", self);
    [self sendTouchEvent:eTouchCancel touches:touches widget:mGeckoChild];
    for (UITouch* touch : touches) {
        CFDictionaryRemoveValue(mTouches, touch);
    }
    if (CFDictionaryGetCount(mTouches) == 0) {
        mNextTouchID = 0;
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    ALOG("[ChildView[%p] touchesEnded", self);
    if (!mGeckoChild)
        return;

    [self sendTouchEvent:eTouchEnd touches:touches widget:mGeckoChild];
    for (UITouch* touch : touches) {
        CFDictionaryRemoveValue(mTouches, touch);
    }
    if (CFDictionaryGetCount(mTouches) == 0) {
        mNextTouchID = 0;
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    ALOG("[ChildView[%p] touchesMoved", self);
    if (!mGeckoChild)
        return;

    [self sendTouchEvent:eTouchMove
                 touches:[event allTouches]
                  widget:mGeckoChild];
}

- (void)setNeedsDisplayInRect:(CGRect)aRect
{
    if ([self isUsingMainThreadOpenGL]) {
    // Draw without calling drawRect. This prevent us from
    // needing to access the normal window buffer surface unnecessarily, so we
    // waste less time synchronizing the two surfaces.
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

- (BOOL)isUsingMainThreadOpenGL
{
  if (!mGeckoChild || ![self window])
    return NO;

  return mGeckoChild->GetLayerManager(nullptr)->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_OPENGL;
}

- (void)drawUsingOpenGL
{
    ALOG("drawUsingOpenGL");
  PROFILER_LABEL("ChildView", "drawUsingOpenGL",
    js::ProfileEntry::Category::GRAPHICS);

  if (!mGeckoChild->IsVisible())
    return;

  mWaitingForPaint = NO;

  LayoutDeviceIntRect geckoBounds;
  mGeckoChild->GetBounds(geckoBounds);
  LayoutDeviceIntRegion region(geckoBounds);

  mGeckoChild->PaintWindow(region);
}

// Called asynchronously after setNeedsDisplay in order to avoid entering the
// normal drawing machinery.
- (void)drawUsingOpenGLCallback
{
  if (mWaitingForPaint) {
    [self drawUsingOpenGL];
  }
}

// The display system has told us that a portion of our view is dirty. Tell
// gecko to paint it
- (void)drawRect:(CGRect)aRect
{
    CGContextRef cgContext = UIGraphicsGetCurrentContext();
    [self drawRect:aRect inContext:cgContext];
}

- (void)drawRect:(CGRect)aRect inContext:(CGContextRef)aContext
{
#ifdef DEBUG_UPDATE
  LayoutDeviceIntRect geckoBounds;
  mGeckoChild->GetBounds(geckoBounds);

  fprintf (stderr, "---- Update[%p][%p] [%f %f %f %f] cgc: %p\n  gecko bounds: [%d %d %d %d]\n",
           self, mGeckoChild,
           aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height, aContext,
           geckoBounds.x, geckoBounds.y, geckoBounds.width, geckoBounds.height);

  CGAffineTransform xform = CGContextGetCTM(aContext);
  fprintf (stderr, "  xform in: [%f %f %f %f %f %f]\n", xform.a, xform.b, xform.c, xform.d, xform.tx, xform.ty);
#endif

  if (true) {
    // For Gecko-initiated repaints in OpenGL mode, drawUsingOpenGL is
    // directly called from a delayed perform callback - without going through
    // drawRect.
    // Paints that come through here are triggered by something that Cocoa
    // controls, for example by window resizing or window focus changes.

    // Do GL composition and return.
    [self drawUsingOpenGL];
    return;
  }
  PROFILER_LABEL("ChildView", "drawRect",
    js::ProfileEntry::Category::GRAPHICS);

  // The CGContext that drawRect supplies us with comes with a transform that
  // scales one user space unit to one Cocoa point, which can consist of
  // multiple dev pixels. But Gecko expects its supplied context to be scaled
  // to device pixels, so we need to reverse the scaling.
  double scale = mGeckoChild->BackingScaleFactor();
  CGContextSaveGState(aContext);
  CGContextScaleCTM(aContext, 1.0 / scale, 1.0 / scale);

  CGSize viewSize = [self bounds].size;
  gfx::IntSize backingSize(viewSize.width * scale, viewSize.height * scale);

  CGContextSaveGState(aContext);

  LayoutDeviceIntRegion region =
    LayoutDeviceIntRect(NSToIntRound(aRect.origin.x * scale),
                        NSToIntRound(aRect.origin.y * scale),
                        NSToIntRound(aRect.size.width * scale),
                        NSToIntRound(aRect.size.height * scale));

  // Create Cairo objects.
  RefPtr<gfxQuartzSurface> targetSurface;

  RefPtr<gfxContext> targetContext;
  if (gfxPlatform::GetPlatform()->SupportsAzureContentForType(gfx::BackendType::COREGRAPHICS)) {
    RefPtr<gfx::DrawTarget> dt =
      gfx::Factory::CreateDrawTargetForCairoCGContext(aContext, backingSize);
    if (!dt || !dt->IsValid()) {
        gfxDevCrash(mozilla::gfx::LogReason::InvalidContext) << "Window context problem 1 " << backingSize;
        return;
    }
    dt->AddUserData(&gfxContext::sDontUseAsSourceKey, dt, nullptr);
    targetContext = gfxContext::CreateOrNull(dt);
  } else if (gfxPlatform::GetPlatform()->SupportsAzureContentForType(gfx::BackendType::CAIRO)) {
    // This is dead code unless you mess with prefs, but keep it around for
    // debugging.
    targetSurface = new gfxQuartzSurface(aContext, backingSize);
    targetSurface->SetAllowUseAsSource(false);
    RefPtr<gfx::DrawTarget> dt =
      gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(targetSurface,
                                                             backingSize);
    if (!dt || !dt->IsValid()) {
        gfxDevCrash(mozilla::gfx::LogReason::InvalidContext) << "Window context problem 2 " << backingSize;
        return;
    }
    dt->AddUserData(&gfxContext::sDontUseAsSourceKey, dt, nullptr);
    targetContext = gfxContext::CreateOrNull(dt);
  } else {
    MOZ_ASSERT_UNREACHABLE("COREGRAPHICS is the only supported backend");
  }
  MOZ_ASSERT(targetContext); // already checked for valid draw targets above

  // Set up the clip region.
  targetContext->NewPath();
  for (auto iter = region.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect& r = iter.Get();
    targetContext->Rectangle(gfxRect(r.x, r.y, r.width, r.height));
  }
  targetContext->Clip();

  //nsAutoRetainCocoaObject kungFuDeathGrip(self);
  bool painted = false;
  if (mGeckoChild->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    nsBaseWidget::AutoLayerManagerSetup
      setupLayerManager(mGeckoChild, targetContext, BufferMode::BUFFER_NONE);
    painted = mGeckoChild->PaintWindow(region);
  } else if (mGeckoChild->GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
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
    CGContextFillRect(aContext, aRect);
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
  CGContextStrokeRect(aContext, aRect);
#endif
}
@end

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, Inherited)

nsWindow::nsWindow()
: mNativeView(nullptr),
  mVisible(false),
  mParent(nullptr)
{
}

nsWindow::~nsWindow()
{
    [mNativeView widgetDestroyed]; // Safe if mNativeView is nil.
    TearDownView(); // Safe if called twice.
}

void nsWindow::TearDownView()
{
  if (!mNativeView)
    return;

  [mNativeView performSelectorOnMainThread:@selector(delayedTearDown) withObject:nil waitUntilDone:false];
  mNativeView = nil;
}

bool
nsWindow::IsTopLevel()
{
    return mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog ||
        mWindowType == eWindowType_invisible;
}

//
// nsIWidget
//

NS_IMETHODIMP
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
    ALOG("nsWindow[%p]::Create %p/%p [%d %d %d %d]", (void*)this, (void*)aParent, (void*)aNativeParent, aRect.x, aRect.y, aRect.width, aRect.height);
    nsWindow* parent = (nsWindow*) aParent;
    ChildView* nativeParent = (ChildView*)aNativeParent;

    if (parent == nullptr && nativeParent)
        parent = nativeParent->mGeckoChild;
    if (parent && nativeParent == nullptr)
        nativeParent = parent->mNativeView;

    // for toplevel windows, bounds are fixed to full screen size
    if (parent == nullptr) {
        if (nsAppShell::gWindow == nil) {
            mBounds = UIKitScreenManager::GetBounds();
        } else {
            CGRect cgRect = [nsAppShell::gWindow bounds];
            mBounds.x = cgRect.origin.x;
            mBounds.y = cgRect.origin.y;
            mBounds.width = cgRect.size.width;
            mBounds.height = cgRect.size.height;
        }
    } else {
        mBounds = aRect;
    }

    ALOG("nsWindow[%p]::Create bounds: %d %d %d %d", (void*)this,
         mBounds.x, mBounds.y, mBounds.width, mBounds.height);

    // Set defaults which can be overriden from aInitData in BaseCreate
    mWindowType = eWindowType_toplevel;
    mBorderStyle = eBorderStyle_default;

    Inherited::BaseCreate(aParent, aInitData);

    NS_ASSERTION(IsTopLevel() || parent, "non top level window doesn't have a parent!");

    mNativeView = [[ChildView alloc] initWithFrame:DevPixelsToUIKitPoints(mBounds, BackingScaleFactor()) geckoChild:this];
    mNativeView.hidden = YES;

    if (parent) {
        parent->mChildren.AppendElement(this);
        mParent = parent;
    }

    if (nativeParent) {
        [nativeParent addSubview:mNativeView];
    } else if (nsAppShell::gWindow) {
        [nsAppShell::gWindow.rootViewController.view addSubview:mNativeView];
    }
    else {
        [nsAppShell::gTopLevelViews addObject:mNativeView];
    }

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    for (uint32_t i = 0; i < mChildren.Length(); ++i) {
        // why do we still have children?
        mChildren[i]->SetParent(nullptr);
    }

    if (mParent)
        mParent->mChildren.RemoveElement(this);

    [mNativeView widgetDestroyed];

    nsBaseWidget::Destroy();

    //ReportDestroyEvent();

    TearDownView();

    nsBaseWidget::OnDestroy();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& config)
{
    for (uint32_t i = 0; i < config.Length(); ++i) {
        nsWindow *childWin = (nsWindow*) config[i].mChild.get();
        childWin->Resize(config[i].mBounds.x,
                         config[i].mBounds.y,
                         config[i].mBounds.width,
                         config[i].mBounds.height,
                         false);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
  if (aState != mVisible) {
      mNativeView.hidden = aState ? NO : YES;
      if (aState) {
          UIView* parentView = mParent ? mParent->mNativeView : nsAppShell::gWindow.rootViewController.view;
          [parentView bringSubviewToFront:mNativeView];
          [mNativeView setNeedsDisplay];
      }
      mVisible = aState;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(bool aModal)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop,
                            int32_t *aX,
                            int32_t *aY)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX, double aY)
{
  if (!mNativeView || (mBounds.x == aX && mBounds.y == aY))
    return NS_OK;

  //XXX: handle this
  // The point we have is in Gecko coordinates (origin top-left). Convert
  // it to Cocoa ones (origin bottom-left).
  mBounds.x = aX;
  mBounds.y = aY;

  mNativeView.frame = DevPixelsToUIKitPoints(mBounds, BackingScaleFactor());

  if (mVisible)
    [mNativeView setNeedsDisplay];

  ReportMoveEvent();
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aX, double aY,
                 double aWidth, double aHeight,
                 bool aRepaint)
{
    BOOL isMoving = (mBounds.x != aX || mBounds.y != aY);
    BOOL isResizing = (mBounds.width != aWidth || mBounds.height != aHeight);
    if (!mNativeView || (!isMoving && !isResizing))
        return NS_OK;

    if (isMoving) {
        mBounds.x = aX;
        mBounds.y = aY;
    }
    if (isResizing) {
        mBounds.width  = aWidth;
        mBounds.height  = aHeight;
    }

    [mNativeView setFrame:DevPixelsToUIKitPoints(mBounds, BackingScaleFactor())];

    if (mVisible && aRepaint)
        [mNativeView setNeedsDisplay];

    if (isMoving)
        ReportMoveEvent();

    if (isResizing)
        ReportSizeEvent();

    return NS_OK;
}

NS_IMETHODIMP nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
    if (!mNativeView || (mBounds.width == aWidth && mBounds.height == aHeight))
        return NS_OK;

    mBounds.width  = aWidth;
    mBounds.height = aHeight;

    [mNativeView setFrame:DevPixelsToUIKitPoints(mBounds, BackingScaleFactor())];

    if (mVisible && aRepaint)
        [mNativeView setNeedsDisplay];

    ReportSizeEvent();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                      nsIWidget *aWidget,
                      bool aActivate)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetSizeMode(nsSizeMode aMode)
{
    if (aMode == static_cast<int32_t>(mSizeMode)) {
        return NS_OK;
    }

    nsresult rv = NS_OK;
    mSizeMode = static_cast<nsSizeMode>(aMode);
    if (aMode == nsSizeMode_Maximized || aMode == nsSizeMode_Fullscreen) {
        // Resize to fill screen
        rv = nsBaseWidget::MakeFullScreen(true);
    }
    ReportSizeModeEvent(aMode);
    return rv;
}

NS_IMETHODIMP
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
  if (!mNativeView || !mVisible)
    return NS_OK;

  MOZ_RELEASE_ASSERT(GetLayerManager()->GetBackendType() != LayersBackend::LAYERS_CLIENT,
                     "Shouldn't need to invalidate with accelerated OMTC layers!");


  [mNativeView setNeedsLayout];
  [mNativeView setNeedsDisplayInRect:DevPixelsToUIKitPoints(mBounds, BackingScaleFactor())];

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
  [[mNativeView window] makeKeyWindow];
  [mNativeView becomeFirstResponder];
  return NS_OK;
}

void nsWindow::WillPaintWindow()
{
  if (mWidgetListener) {
    mWidgetListener->WillPaintWindow(this);
  }
}

bool nsWindow::PaintWindow(LayoutDeviceIntRegion aRegion)
{
  if (!mWidgetListener)
    return false;

  bool returnValue = false;
  returnValue = mWidgetListener->PaintWindow(this, aRegion);

  if (mWidgetListener) {
    mWidgetListener->DidPaintWindow();
  }

  return returnValue;
}

void nsWindow::ReportMoveEvent()
{
    NotifyWindowMoved(mBounds.x, mBounds.y);
}

void nsWindow::ReportSizeModeEvent(nsSizeMode aMode)
{
    if (mWidgetListener) {
        // This is terrible.
        nsSizeMode theMode;
        switch (aMode) {
        case nsSizeMode_Maximized:
            theMode = nsSizeMode_Maximized;
            break;
        case nsSizeMode_Fullscreen:
            theMode = nsSizeMode_Fullscreen;
            break;
        default:
            return;
        }
        mWidgetListener->SizeModeChanged(theMode);
    }
}

void nsWindow::ReportSizeEvent()
{
    if (mWidgetListener) {
        LayoutDeviceIntRect innerBounds;
        GetClientBounds(innerBounds);
        mWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
    }
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(LayoutDeviceIntRect& aRect)
{
    LayoutDeviceIntPoint p = WidgetToScreenOffset();

    aRect.x = p.x;
    aRect.y = p.y;
    aRect.width = mBounds.width;
    aRect.height = mBounds.height;

    return NS_OK;
}

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset()
{
    LayoutDeviceIntPoint offset(0, 0);
    if (mParent) {
        offset = mParent->WidgetToScreenOffset();
    }

    CGPoint temp = [mNativeView convertPoint:temp toView:nil];

    if (!mParent && nsAppShell::gWindow) {
        // convert to screen coords
        temp = [nsAppShell::gWindow convertPoint:temp toWindow:nil];
    }

    offset.x += temp.x;
    offset.y += temp.y;

    return offset;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;
  nsCOMPtr<nsIWidget> kungFuDeathGrip(aEvent->mWidget);

  if (mWidgetListener)
    aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    //TODO: actually show VKB
    mInputContext = aContext;
}

NS_IMETHODIMP_(mozilla::widget::InputContext)
nsWindow::GetInputContext()
{
    return mInputContext;
}

void
nsWindow::SetBackgroundColor(const nscolor &aColor)
{
    mNativeView.backgroundColor = [UIColor colorWithRed:NS_GET_R(aColor)
                                   green:NS_GET_G(aColor)
                                   blue:NS_GET_B(aColor)
                                   alpha:NS_GET_A(aColor)];
}

void* nsWindow::GetNativeData(uint32_t aDataType)
{
  void* retVal = nullptr;

  switch (aDataType)
  {
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = (void*)mNativeView;
      break;

    case NS_NATIVE_WINDOW:
      retVal = [mNativeView window];
      break;

    case NS_NATIVE_GRAPHIC:
      NS_ERROR("Requesting NS_NATIVE_GRAPHIC on a UIKit child view!");
      break;

    case NS_NATIVE_OFFSETX:
      retVal = 0;
      break;

    case NS_NATIVE_OFFSETY:
      retVal = 0;
      break;

    case NS_NATIVE_PLUGIN_PORT:
        // not implemented
        break;

    case NS_RAW_NATIVE_IME_CONTEXT:
      retVal = GetPseudoIMEContext();
      if (retVal) {
        break;
      }
      retVal = NS_ONLY_ONE_NATIVE_IME_CONTEXT;
      break;
  }

  return retVal;
}

CGFloat
nsWindow::BackingScaleFactor()
{
    if (mNativeView) {
        return [mNativeView contentScaleFactor];
    }
    return [UIScreen mainScreen].scale;
}

int32_t
nsWindow::RoundsWidgetCoordinatesTo()
{
  if (BackingScaleFactor() == 2.0) {
    return 2;
  }
  return 1;
}
