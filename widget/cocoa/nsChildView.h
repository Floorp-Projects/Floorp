/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChildView_h_
#define nsChildView_h_

// formal protocols
#include "mozView.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#include "mozAccessibleProtocol.h"
#endif

#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsBaseWidget.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIPluginWidget.h"
#include "nsWeakPtr.h"
#include "TextInputHandler.h"
#include "nsCocoaUtils.h"
#include "gfxQuartzSurface.h"
#include "GLContextTypes.h"
#include "mozilla/Mutex.h"
#include "nsRegion.h"
#include "mozilla/MouseEvents.h"

#include "nsString.h"
#include "nsIDragService.h"

#include "npapi.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/NSOpenGL.h>

// The header files QuickdrawAPI.h and QDOffscreen.h are missing on OS X 10.7
// and up (though the QuickDraw APIs defined in them are still present) -- so
// we need to supply the relevant parts of their contents here.  It's likely
// that Apple will eventually remove the APIs themselves (probably in OS X
// 10.8), so we need to make them weak imports, and test for their presence
// before using them.
#ifdef __cplusplus
extern "C" {
#endif
  #if !defined(__QUICKDRAWAPI__)

  extern void SetPort(GrafPtr port)
    __attribute__((weak_import));
  extern void SetOrigin(short h, short v)
    __attribute__((weak_import));
  extern RgnHandle NewRgn(void)
    __attribute__((weak_import));
  extern void DisposeRgn(RgnHandle rgn)
    __attribute__((weak_import));
  extern void RectRgn(RgnHandle rgn, const Rect * r)
    __attribute__((weak_import));
  extern GDHandle GetMainDevice(void)
    __attribute__((weak_import));
  extern Boolean IsPortOffscreen(CGrafPtr port)
    __attribute__((weak_import));
  extern void SetPortVisibleRegion(CGrafPtr port, RgnHandle visRgn)
    __attribute__((weak_import));
  extern void SetPortClipRegion(CGrafPtr port, RgnHandle clipRgn)
    __attribute__((weak_import));
  extern CGrafPtr GetQDGlobalsThePort(void)
    __attribute__((weak_import));

  #endif /* __QUICKDRAWAPI__ */

  #if !defined(__QDOFFSCREEN__)

  extern void GetGWorld(CGrafPtr *  port, GDHandle *  gdh)
    __attribute__((weak_import));
  extern void SetGWorld(CGrafPtr port, GDHandle gdh)
    __attribute__((weak_import));

  #endif /* __QDOFFSCREEN__ */
#ifdef __cplusplus
}
#endif

class gfxASurface;
class nsChildView;
class nsCocoaWindow;
union nsPluginPort;

namespace {
class GLPresenter;
class RectTextureImage;
}

namespace mozilla {
namespace layers {
class GLManager;
}
}

@interface NSEvent (Undocumented)

// Return Cocoa event's corresponding Carbon event.  Not initialized (on
// synthetic events) until the OS actually "sends" the event.  This method
// has been present in the same form since at least OS X 10.2.8.
- (EventRef)_eventRef;

@end

@interface NSView (Undocumented)

// Draws the title string of a window.
// Present on NSThemeFrame since at least 10.6.
// _drawTitleBar is somewhat complex, and has changed over the years
// since OS X 10.6.  But in that time it's never done anything that
// would break when called outside of -[NSView drawRect:] (which we
// sometimes do), or whose output can't be redirected to a
// CGContextRef object (which we also sometimes do).  This is likely
// to remain true for the indefinite future.  However we should
// check _drawTitleBar in each new major version of OS X.  For more
// information see bug 877767.
- (void)_drawTitleBar:(NSRect)aRect;

// Returns an NSRect that is the bounding box for all an NSView's dirty
// rectangles (ones that need to be redrawn).  The full list of dirty
// rectangles can be obtained by calling -[NSView _dirtyRegion] and then
// calling -[NSRegion getRects:count:] on what it returns.  Both these
// methods have been present in the same form since at least OS X 10.5.
// Unlike -[NSView getRectsBeingDrawn:count:], these methods can be called
// outside a call to -[NSView drawRect:].
- (NSRect)_dirtyRect;

@end

// Support for pixel scroll deltas, not part of NSEvent.h
// See http://lists.apple.com/archives/cocoa-dev/2007/Feb/msg00050.html
@interface NSEvent (DeviceDelta)
// Leopard and SnowLeopard
- (CGFloat)deviceDeltaX;
- (CGFloat)deviceDeltaY;
// Lion and above
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (BOOL)hasPreciseScrollingDeltas;
@end

#if !defined(MAC_OS_X_VERSION_10_6) || \
MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
@interface NSEvent (SnowLeopardEventFeatures)
+ (NSUInteger)pressedMouseButtons;
+ (NSUInteger)modifierFlags;
@end
#endif

// The following section, required to support fluid swipe tracking on OS X 10.7
// and up, contains defines/declarations that are only available on 10.7 and up.
// [NSEvent trackSwipeEventWithOptions:...] also requires that the compiler
// support "blocks"
// (http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/Blocks/Articles/00_Introduction.html)
// -- which it does on 10.6 and up (using the 10.6 SDK or higher).
//
// MAC_OS_X_VERSION_MAX_ALLOWED "controls which OS functionality, if used,
// will result in a compiler error because that functionality is not
// available" (quoting from AvailabilityMacros.h).  The compiler initializes
// it to the version of the SDK being used.  Its value does *not* prevent the
// binary from running on higher OS versions.  MAC_OS_X_VERSION_10_7 and
// friends are defined (in AvailabilityMacros.h) as decimal numbers (not
// hexadecimal numbers).
#if !defined(MAC_OS_X_VERSION_10_7) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
enum {
  NSEventPhaseNone        = 0,
  NSEventPhaseBegan       = 0x1 << 0,
  NSEventPhaseStationary  = 0x1 << 1,
  NSEventPhaseChanged     = 0x1 << 2,
  NSEventPhaseEnded       = 0x1 << 3,
  NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

#ifdef __LP64__
enum {
  NSEventSwipeTrackingLockDirection = 0x1 << 0,
  NSEventSwipeTrackingClampGestureAmount = 0x1 << 1
};
typedef NSUInteger NSEventSwipeTrackingOptions;

enum {
  NSEventGestureAxisNone = 0,
  NSEventGestureAxisHorizontal,
  NSEventGestureAxisVertical
};
typedef NSInteger NSEventGestureAxis;

@interface NSEvent (FluidSwipeTracking)
+ (BOOL)isSwipeTrackingFromScrollEventsEnabled;
- (BOOL)hasPreciseScrollingDeltas;
- (CGFloat)scrollingDeltaX;
- (CGFloat)scrollingDeltaY;
- (NSEventPhase)phase;
- (void)trackSwipeEventWithOptions:(NSEventSwipeTrackingOptions)options
          dampenAmountThresholdMin:(CGFloat)minDampenThreshold
                               max:(CGFloat)maxDampenThreshold
                      usingHandler:(void (^)(CGFloat gestureAmount, NSEventPhase phase, BOOL isComplete, BOOL *stop))trackingHandler;
@end
#endif // #ifdef __LP64__
#endif // #if !defined(MAC_OS_X_VERSION_10_7) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

// Undocumented scrollPhase flag that lets us discern between real scrolls and
// automatically firing momentum scroll events.
@interface NSEvent (ScrollPhase)
// Leopard and SnowLeopard
- (long long)_scrollPhase;
// Lion and above
- (NSEventPhase)momentumPhase;
@end

@interface ChildView : NSView<
#ifdef ACCESSIBILITY
                              mozAccessible,
#endif
                              mozView, NSTextInput, NSTextInputClient>
{
@private
  // the nsChildView that created the view. It retains this NSView, so
  // the link back to it must be weak.
  nsChildView* mGeckoChild;

  // Text input handler for mGeckoChild and us.  Note that this is a weak
  // reference.  Ideally, this should be a strong reference but a ChildView
  // object can live longer than the mGeckoChild that owns it.  And if
  // mTextInputHandler were a strong reference, this would make it difficult
  // for Gecko's leak detector to detect leaked TextInputHandler objects.
  // This is initialized by [mozView installTextInputHandler:aHandler] and
  // cleared by [mozView uninstallTextInputHandler].
  mozilla::widget::TextInputHandler* mTextInputHandler;  // [WEAK]

  BOOL mIsPluginView;
  NPEventModel mPluginEventModel;
  NPDrawingModel mPluginDrawingModel;

  // when mouseDown: is called, we store its event here (strong)
  NSEvent* mLastMouseDownEvent;

  // Whether the last mouse down event was blocked from Gecko.
  BOOL mBlockedLastMouseDown;

  // when acceptsFirstMouse: is called, we store the event here (strong)
  NSEvent* mClickThroughMouseDownEvent;

  // rects that were invalidated during a draw, so have pending drawing
  NSMutableArray* mPendingDirtyRects;
  BOOL mPendingFullDisplay;
  BOOL mPendingDisplay;

  // Holds our drag service across multiple drag calls. The reference to the
  // service is obtained when the mouse enters the view and is released when
  // the mouse exits or there is a drop. This prevents us from having to
  // re-establish the connection to the service manager many times per second
  // when handling |draggingUpdated:| messages.
  nsIDragService* mDragService;

  NSOpenGLContext *mGLContext;

  // Simple gestures support
  //
  // mGestureState is used to detect when Cocoa has called both
  // magnifyWithEvent and rotateWithEvent within the same
  // beginGestureWithEvent and endGestureWithEvent sequence. We
  // discard the spurious gesture event so as not to confuse Gecko.
  //
  // mCumulativeMagnification keeps track of the total amount of
  // magnification peformed during a magnify gesture so that we can
  // send that value with the final MozMagnifyGesture event.
  //
  // mCumulativeRotation keeps track of the total amount of rotation
  // performed during a rotate gesture so we can send that value with
  // the final MozRotateGesture event.
  enum {
    eGestureState_None,
    eGestureState_StartGesture,
    eGestureState_MagnifyGesture,
    eGestureState_RotateGesture
  } mGestureState;
  float mCumulativeMagnification;
  float mCumulativeRotation;

  BOOL mDidForceRefreshOpenGL;
  BOOL mWaitingForPaint;

#ifdef __LP64__
  // Support for fluid swipe tracking.
  BOOL* mCancelSwipeAnimation;
#endif

  // Whether this uses off-main-thread compositing.
  BOOL mUsingOMTCompositor;

  // The mask image that's used when painting into the titlebar using basic
  // CGContext painting (i.e. non-accelerated).
  CGImageRef mTopLeftCornerMask;
}

// class initialization
+ (void)initialize;

+ (void)registerViewForDraggedTypes:(NSView*)aView;

// these are sent to the first responder when the window key status changes
- (void)viewsWindowDidBecomeKey;
- (void)viewsWindowDidResignKey;

// Stop NSView hierarchy being changed during [ChildView drawRect:]
- (void)delayedTearDown;

- (void)sendFocusEvent:(uint32_t)eventType;

- (void)handleMouseMoved:(NSEvent*)aEvent;

- (void)updateWindowDraggableStateOnMouseMove:(NSEvent*)theEvent;

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                             type:(mozilla::WidgetMouseEvent::exitType)aType;

- (void)updateGLContext;
- (void)_surfaceNeedsUpdate:(NSNotification*)notification;

- (BOOL)isPluginView;

// Are we processing an NSLeftMouseDown event that will fail to click through?
// If so, we shouldn't focus or unfocus a plugin.
- (BOOL)isInFailingLeftClickThrough;

- (void)setGLContext:(NSOpenGLContext *)aGLContext;
- (bool)preRender:(NSOpenGLContext *)aGLContext;
- (void)postRender:(NSOpenGLContext *)aGLContext;

- (BOOL)isCoveringTitlebar;

// Simple gestures support
//
// XXX - The swipeWithEvent, beginGestureWithEvent, magnifyWithEvent,
// rotateWithEvent, and endGestureWithEvent methods are part of a
// PRIVATE interface exported by nsResponder and reverse-engineering
// was necessary to obtain the methods' prototypes. Thus, Apple may
// change the interface in the future without notice.
//
// The prototypes were obtained from the following link:
// http://cocoadex.com/2008/02/nsevent-modifications-swipe-ro.html
- (void)swipeWithEvent:(NSEvent *)anEvent;
- (void)beginGestureWithEvent:(NSEvent *)anEvent;
- (void)magnifyWithEvent:(NSEvent *)anEvent;
- (void)smartMagnifyWithEvent:(NSEvent *)anEvent;
- (void)rotateWithEvent:(NSEvent *)anEvent;
- (void)endGestureWithEvent:(NSEvent *)anEvent;

// Helper function for Lion smart magnify events
+ (BOOL)isLionSmartMagnifyEvent:(NSEvent*)anEvent;

// Support for fluid swipe tracking.
#ifdef __LP64__
- (void)maybeTrackScrollEventAsSwipe:(NSEvent *)anEvent
                      scrollOverflow:(double)overflow;
#endif

- (void)setUsingOMTCompositor:(BOOL)aUseOMTC;
@end

class ChildViewMouseTracker {

public:

  static void MouseMoved(NSEvent* aEvent);
  static void MouseScrolled(NSEvent* aEvent);
  static void OnDestroyView(ChildView* aView);
  static void OnDestroyWindow(NSWindow* aWindow);
  static BOOL WindowAcceptsEvent(NSWindow* aWindow, NSEvent* aEvent,
                                 ChildView* aView, BOOL isClickThrough = NO);
  static void MouseExitedWindow(NSEvent* aEvent);
  static void MouseEnteredWindow(NSEvent* aEvent);
  static void ReEvaluateMouseEnterState(NSEvent* aEvent = nil, ChildView* aOldView = nil);
  static void ResendLastMouseMoveEvent();
  static ChildView* ViewForEvent(NSEvent* aEvent);
  static void AttachPluginEvent(mozilla::WidgetMouseEventBase& aMouseEvent,
                                ChildView* aView,
                                NSEvent* aNativeMouseEvent,
                                int aPluginEventType,
                                void* aPluginEventHolder);

  static ChildView* sLastMouseEventView;
  static NSEvent* sLastMouseMoveEvent;
  static NSWindow* sWindowUnderMouse;
  static NSPoint sLastScrollEventScreenLocation;
};

//-------------------------------------------------------------------------
//
// nsChildView
//
//-------------------------------------------------------------------------

class nsChildView : public nsBaseWidget,
                    public nsIPluginWidget
{
private:
  typedef nsBaseWidget Inherited;

public:
                          nsChildView();
  virtual                 ~nsChildView();
  
  NS_DECL_ISUPPORTS_INHERITED

  // nsIWidget interface
  NS_IMETHOD              Create(nsIWidget *aParent,
                                 nsNativeWidget aNativeParent,
                                 const nsIntRect &aRect,
                                 nsDeviceContext *aContext,
                                 nsWidgetInitData *aInitData = nullptr);

  NS_IMETHOD              Destroy();

  NS_IMETHOD              Show(bool aState);
  virtual bool            IsVisible() const;

  NS_IMETHOD              SetParent(nsIWidget* aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual float           GetDPI();

  NS_IMETHOD              ConstrainPosition(bool aAllowSlop,
                                            int32_t *aX, int32_t *aY);
  NS_IMETHOD              Move(double aX, double aY);
  NS_IMETHOD              Resize(double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD              Resize(double aX, double aY,
                                 double aWidth, double aHeight, bool aRepaint);

  NS_IMETHOD              Enable(bool aState);
  virtual bool            IsEnabled() const;
  NS_IMETHOD              SetFocus(bool aRaise);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);

  // Returns the "backing scale factor" of the view's window, which is the
  // ratio of pixels in the window's backing store to Cocoa points. Prior to
  // HiDPI support in OS X 10.7, this was always 1.0, but in HiDPI mode it
  // will be 2.0 (and might potentially other values as screen resolutions
  // evolve). This gives the relationship between what Gecko calls "device
  // pixels" and the Cocoa "points" coordinate system.
  CGFloat                 BackingScaleFactor();

  // Call if the window's backing scale factor changes - i.e., it is moved
  // between HiDPI and non-HiDPI screens
  void                    BackingScaleFactorChanged();

  virtual double          GetDefaultScaleInternal();

  virtual int32_t         RoundsWidgetCoordinatesTo() MOZ_OVERRIDE;

  NS_IMETHOD              Invalidate(const nsIntRect &aRect);

  virtual void*           GetNativeData(uint32_t aDataType);
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  virtual nsIntPoint      WidgetToScreenOffset();
  virtual bool            ShowsResizeIndicator(nsIntRect* aResizerRect);

  static  bool            ConvertStatus(nsEventStatus aStatus)
                          { return aStatus == nsEventStatus_eConsumeNoDefault; }
  NS_IMETHOD              DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                        nsEventStatus& aStatus);

  virtual bool            ComputeShouldAccelerate(bool aDefault);
  virtual bool            ShouldUseOffMainThreadCompositing() MOZ_OVERRIDE;

  NS_IMETHOD        SetCursor(nsCursor aCursor);
  NS_IMETHOD        SetCursor(imgIContainer* aCursor, uint32_t aHotspotX, uint32_t aHotspotY);

  NS_IMETHOD        CaptureRollupEvents(nsIRollupListener * aListener, bool aDoCapture);
  NS_IMETHOD        SetTitle(const nsAString& title);

  NS_IMETHOD        GetAttention(int32_t aCycleCount);

  virtual bool HasPendingInputEvent();

  NS_IMETHOD        ActivateNativeMenuItemAt(const nsAString& indexString);
  NS_IMETHOD        ForceUpdateNativeMenuAt(const nsAString& indexString);

  NS_IMETHOD        NotifyIME(NotificationToIME aNotification) MOZ_OVERRIDE;
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction);
  NS_IMETHOD_(InputContext) GetInputContext();
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() MOZ_OVERRIDE;
  NS_IMETHOD        GetToggledKeyState(uint32_t aKeyCode,
                                       bool* aLEDState);

  // nsIPluginWidget
  // outClipRect and outOrigin are in display pixels (not device pixels)
  NS_IMETHOD        GetPluginClipRect(nsIntRect& outClipRect, nsIntPoint& outOrigin, bool& outWidgetVisible);
  NS_IMETHOD        StartDrawPlugin();
  NS_IMETHOD        EndDrawPlugin();
  NS_IMETHOD        SetPluginInstanceOwner(nsIPluginInstanceOwner* aInstanceOwner);

  NS_IMETHOD        SetPluginEventModel(int inEventModel);
  NS_IMETHOD        GetPluginEventModel(int* outEventModel);
  NS_IMETHOD        SetPluginDrawingModel(int inDrawingModel);

  NS_IMETHOD        StartComplexTextInputForCurrentEvent();

  virtual nsTransparencyMode GetTransparencyMode();
  virtual void                SetTransparencyMode(nsTransparencyMode aMode);

  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                            int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters);

  virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags);

  virtual nsresult SynthesizeNativeMouseMove(nsIntPoint aPoint)
  { return SynthesizeNativeMouseEvent(aPoint, NSMouseMoved, 0); }

  // Mac specific methods
  
  virtual bool      DispatchWindowEvent(mozilla::WidgetGUIEvent& event);

  void WillPaintWindow();
  bool PaintWindow(nsIntRegion aRegion);

#ifdef ACCESSIBILITY
  already_AddRefed<mozilla::a11y::Accessible> GetDocumentAccessible();
#endif

  virtual void CreateCompositor();
  virtual gfxASurface* GetThebesSurface();
  virtual void PrepareWindowEffects() MOZ_OVERRIDE;
  virtual void CleanupWindowEffects() MOZ_OVERRIDE;
  virtual bool PreRender(LayerManager* aManager) MOZ_OVERRIDE;
  virtual void PostRender(LayerManager* aManager) MOZ_OVERRIDE;
  virtual void DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect) MOZ_OVERRIDE;

  virtual void UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries);

  void              HidePlugin();
  void              UpdatePluginPort();

  void              ResetParent();

  static bool DoHasPendingInputEvent();
  static uint32_t GetCurrentInputEventCount();
  static void UpdateCurrentInputEventCount();

  NSView<mozView>* GetEditorView();

  bool IsPluginView() { return (mWindowType == eWindowType_plugin); }

  nsCocoaWindow*    GetXULWindowWidget();

  NS_IMETHOD        ReparentNativeWidget(nsIWidget* aNewParent);

  mozilla::widget::TextInputHandler* GetTextInputHandler()
  {
    return mTextInputHandler;
  }

  void              NotifyDirtyRegion(const nsIntRegion& aDirtyRegion);

  // unit conversion convenience functions
  int32_t           CocoaPointsToDevPixels(CGFloat aPts) {
    return nsCocoaUtils::CocoaPointsToDevPixels(aPts, BackingScaleFactor());
  }
  nsIntPoint        CocoaPointsToDevPixels(const NSPoint& aPt) {
    return nsCocoaUtils::CocoaPointsToDevPixels(aPt, BackingScaleFactor());
  }
  nsIntRect         CocoaPointsToDevPixels(const NSRect& aRect) {
    return nsCocoaUtils::CocoaPointsToDevPixels(aRect, BackingScaleFactor());
  }
  CGFloat           DevPixelsToCocoaPoints(int32_t aPixels) {
    return nsCocoaUtils::DevPixelsToCocoaPoints(aPixels, BackingScaleFactor());
  }
  NSRect            DevPixelsToCocoaPoints(const nsIntRect& aRect) {
    return nsCocoaUtils::DevPixelsToCocoaPoints(aRect, BackingScaleFactor());
  }

  mozilla::TemporaryRef<mozilla::gfx::DrawTarget> StartRemoteDrawing() MOZ_OVERRIDE;
  void EndRemoteDrawing() MOZ_OVERRIDE;
  void CleanupRemoteDrawing() MOZ_OVERRIDE;

protected:

  void              ReportMoveEvent();
  void              ReportSizeEvent();

  // override to create different kinds of child views. Autoreleases, so
  // caller must retain.
  virtual NSView*   CreateCocoaView(NSRect inFrame);
  void              TearDownView();

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_POPUP_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

  void DoRemoteComposition(const nsIntRect& aRenderRect);

  // Overlay drawing functions for OpenGL drawing
  void DrawWindowOverlay(mozilla::layers::GLManager* aManager, nsIntRect aRect);
  void MaybeDrawResizeIndicator(mozilla::layers::GLManager* aManager, const nsIntRect& aRect);
  void MaybeDrawRoundedCorners(mozilla::layers::GLManager* aManager, const nsIntRect& aRect);
  void MaybeDrawTitlebar(mozilla::layers::GLManager* aManager, const nsIntRect& aRect);

  // Redraw the contents of mTitlebarImageBuffer on the main thread, as
  // determined by mDirtyTitlebarRegion.
  void UpdateTitlebarImageBuffer();

  nsIntRect RectContainingTitlebarControls();

  nsIWidget* GetWidgetForListenerEvents();

protected:

  NSView<mozView>*      mView;      // my parallel cocoa view (ChildView or NativeScrollbarView), [STRONG]
  nsRefPtr<mozilla::widget::TextInputHandler> mTextInputHandler;
  InputContext          mInputContext;

  NSView<mozView>*      mParentView;
  nsIWidget*            mParentWidget;

#ifdef ACCESSIBILITY
  // weak ref to this childview's associated mozAccessible for speed reasons 
  // (we get queried for it *a lot* but don't want to own it)
  nsWeakPtr             mAccessible;
#endif


  nsRefPtr<gfxASurface> mTempThebesSurface;

  // Protects the view from being teared down while a composition is in
  // progress on the compositor thread.
  mozilla::Mutex mViewTearDownLock;

  mozilla::Mutex mEffectsLock;

  // May be accessed from any thread, protected
  // by mEffectsLock.
  bool mShowsResizeIndicator;
  nsIntRect mResizeIndicatorRect;
  bool mHasRoundedBottomCorners;
  int mDevPixelCornerRadius;
  bool mIsCoveringTitlebar;
  nsIntRect mTitlebarRect;

  // The area of mTitlebarImageBuffer that needs to be redrawn during the next
  // transaction. Accessed from any thread, protected by mEffectsLock.
  nsIntRegion mUpdatedTitlebarRegion;

  mozilla::RefPtr<mozilla::gfx::DrawTarget> mTitlebarImageBuffer;

  // Compositor thread only
  nsAutoPtr<RectTextureImage> mResizerImage;
  nsAutoPtr<RectTextureImage> mCornerMaskImage;
  nsAutoPtr<RectTextureImage> mTitlebarImage;
  nsAutoPtr<RectTextureImage> mBasicCompositorImage;

  // The area of mTitlebarImageBuffer that has changed and needs to be
  // uploaded to to mTitlebarImage. Main thread only.
  nsIntRegion           mDirtyTitlebarRegion;

  // Cached value of [mView backingScaleFactor], to avoid sending two obj-c
  // messages (respondsToSelector, backingScaleFactor) every time we need to
  // use it.
  // ** We'll need to reinitialize this if the backing resolution changes. **
  CGFloat               mBackingScaleFactor;

  bool                  mVisible;
  bool                  mDrawing;
  bool                  mPluginDrawing;
  bool                  mIsDispatchPaint; // Is a paint event being dispatched

  NP_CGContext          mPluginCGContext;
  nsIPluginInstanceOwner* mPluginInstanceOwner; // [WEAK]

  // Used in OMTC BasicLayers mode. Presents the BasicCompositor result
  // surface to the screen using an OpenGL context.
  nsAutoPtr<GLPresenter> mGLPresenter;

  static uint32_t sLastInputEventCount;
};

void NS_InstallPluginKeyEventsHandler();
void NS_RemovePluginKeyEventsHandler();

#endif // nsChildView_h_
