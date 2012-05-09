/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Thomas K. Dyas <tdyas@zecador.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsChildView_h_
#define nsChildView_h_

// formal protocols
#include "mozView.h"
#ifdef ACCESSIBILITY
#include "nsAccessible.h"
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

namespace mozilla {
namespace gl {
class TextureImage;
}

namespace layers {
class LayerManagerOGL;
}
}

#ifndef NP_NO_CARBON
enum {
  // Currently focused ChildView (while this TSM document is active).
  // Transient (only set while TSMProcessRawKeyEvent() is processing a key
  // event), and the ChildView will be retained and released around the call
  // to TSMProcessRawKeyEvent() -- so it can be weak.
  kFocusedChildViewTSMDocPropertyTag  = 'GKFV', // type ChildView* [WEAK]
};

// Undocumented HIToolbox function used by WebKit to allow Carbon-based IME
// to work in a Cocoa-based browser (like Safari or Cocoa-widgets Firefox).
// (Recent WebKit versions actually use a thin wrapper around this function
// called WKSendKeyEventToTSM().)
//
// Calling TSMProcessRawKeyEvent() from ChildView's keyDown: and keyUp:
// methods (when the ChildView is a plugin view) bypasses Cocoa's IME
// infrastructure and (instead) causes Carbon TSM events to be sent on each
// NSKeyDown event.  We install a Carbon event handler
// (PluginKeyEventsHandler()) to catch these events and pass them to Gecko
// (which in turn passes them to the plugin).
extern "C" long TSMProcessRawKeyEvent(EventRef carbonEvent);
#endif // NP_NO_CARBON

@interface NSEvent (Undocumented)

// Return Cocoa event's corresponding Carbon event.  Not initialized (on
// synthetic events) until the OS actually "sends" the event.  This method
// has been present in the same form since at least OS X 10.2.8.
- (EventRef)_eventRef;

@end

// Support for pixel scroll deltas, not part of NSEvent.h
// See http://lists.apple.com/archives/cocoa-dev/2007/Feb/msg00050.html
@interface NSEvent (DeviceDelta)
  - (CGFloat)deviceDeltaX;
  - (CGFloat)deviceDeltaY;
@end

// Undocumented scrollPhase flag that lets us discern between real scrolls and
// automatically firing momentum scroll events.
@interface NSEvent (ScrollPhase)
- (long long)_scrollPhase;
@end

#if !defined(MAC_OS_X_VERSION_10_6) || \
MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
@interface NSEvent (SnowLeopardEventFeatures)
+ (NSUInteger)pressedMouseButtons;
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
#ifdef __LP64__
enum {
  NSEventPhaseNone        = 0,
  NSEventPhaseBegan       = 0x1 << 0,
  NSEventPhaseStationary  = 0x1 << 1,
  NSEventPhaseChanged     = 0x1 << 2,
  NSEventPhaseEnded       = 0x1 << 3,
  NSEventPhaseCancelled   = 0x1 << 4,
};
typedef NSUInteger NSEventPhase;

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

@interface ChildView : NSView<
#ifdef ACCESSIBILITY
                              mozAccessible,
#endif
                              mozView, NSTextInput>
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

  // Support for fluid swipe tracking.
#ifdef __LP64__
  BOOL *mSwipeAnimationCancelled;
#endif

  // Whether this uses off-main-thread compositing.
  BOOL mUsingOMTCompositor;
}

// class initialization
+ (void)initialize;

// these are sent to the first responder when the window key status changes
- (void)viewsWindowDidBecomeKey;
- (void)viewsWindowDidResignKey;

// Stop NSView hierarchy being changed during [ChildView drawRect:]
- (void)delayedTearDown;

- (void)sendFocusEvent:(PRUint32)eventType;

- (void)handleMouseMoved:(NSEvent*)aEvent;

- (void)drawRect:(NSRect)aRect inTitlebarContext:(CGContextRef)aContext;

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                             type:(nsMouseEvent::exitType)aType;

- (void)update;
- (void)lockFocus;
- (void) _surfaceNeedsUpdate:(NSNotification*)notification;

- (BOOL)isPluginView;

// Are we processing an NSLeftMouseDown event that will fail to click through?
// If so, we shouldn't focus or unfocus a plugin.
- (BOOL)isInFailingLeftClickThrough;

- (void)setGLContext:(NSOpenGLContext *)aGLContext;

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
- (void)rotateWithEvent:(NSEvent *)anEvent;
- (void)endGestureWithEvent:(NSEvent *)anEvent;

// Support for fluid swipe tracking.
#ifdef __LP64__
- (void)maybeTrackScrollEventAsSwipe:(NSEvent *)anEvent
                      scrollOverflow:(PRInt32)overflow;
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
  static void ReEvaluateMouseEnterState(NSEvent* aEvent = nil);
  static void ResendLastMouseMoveEvent();
  static ChildView* ViewForEvent(NSEvent* aEvent);

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
                                 EVENT_CALLBACK aHandleEventFunction,
                                 nsDeviceContext *aContext,
                                 nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD              Destroy();

  NS_IMETHOD              Show(bool aState);
  NS_IMETHOD              IsVisible(bool& outState);

  NS_IMETHOD              SetParent(nsIWidget* aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual float           GetDPI();

  NS_IMETHOD              ConstrainPosition(bool aAllowSlop,
                                            PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, bool aRepaint);
  NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight, bool aRepaint);

  NS_IMETHOD              Enable(bool aState);
  NS_IMETHOD              IsEnabled(bool *aState);
  NS_IMETHOD              SetFocus(bool aRaise);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);

  NS_IMETHOD              Invalidate(const nsIntRect &aRect);

  virtual void*           GetNativeData(PRUint32 aDataType);
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  virtual nsIntPoint      WidgetToScreenOffset();
  virtual bool            ShowsResizeIndicator(nsIntRect* aResizerRect);

  static  bool            ConvertStatus(nsEventStatus aStatus)
                          { return aStatus == nsEventStatus_eConsumeNoDefault; }
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);

  virtual bool            GetShouldAccelerate();

  NS_IMETHOD        SetCursor(nsCursor aCursor);
  NS_IMETHOD        SetCursor(imgIContainer* aCursor, PRUint32 aHotspotX, PRUint32 aHotspotY);

  NS_IMETHOD        CaptureRollupEvents(nsIRollupListener * aListener, bool aDoCapture, bool aConsumeRollupEvent);
  NS_IMETHOD        SetTitle(const nsAString& title);

  NS_IMETHOD        GetAttention(PRInt32 aCycleCount);

  virtual bool HasPendingInputEvent();

  NS_IMETHOD        ActivateNativeMenuItemAt(const nsAString& indexString);
  NS_IMETHOD        ForceUpdateNativeMenuAt(const nsAString& indexString);

  NS_IMETHOD        ResetInputState();
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction);
  NS_IMETHOD_(InputContext) GetInputContext();
  NS_IMETHOD        CancelIMEComposition();
  NS_IMETHOD        GetToggledKeyState(PRUint32 aKeyCode,
                                       bool* aLEDState);
  NS_IMETHOD        OnIMEFocusChange(bool aFocus);

  // nsIPluginWidget
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

  virtual nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                            PRInt32 aNativeKeyCode,
                                            PRUint32 aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters);

  virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                              PRUint32 aNativeMessage,
                                              PRUint32 aModifierFlags);

  virtual nsresult SynthesizeNativeMouseMove(nsIntPoint aPoint)
  { return SynthesizeNativeMouseEvent(aPoint, NSMouseMoved, 0); }

  // Mac specific methods
  
  virtual bool      DispatchWindowEvent(nsGUIEvent& event);
  
#ifdef ACCESSIBILITY
  already_AddRefed<nsAccessible> GetDocumentAccessible();
#endif

  virtual void CreateCompositor();
  virtual gfxASurface* GetThebesSurface();
  virtual void DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect);

  virtual void UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries);

  NS_IMETHOD BeginSecureKeyboardInput();
  NS_IMETHOD EndSecureKeyboardInput();

  void              HidePlugin();
  void              UpdatePluginPort();

  void              ResetParent();

  static bool DoHasPendingInputEvent();
  static PRUint32 GetCurrentInputEventCount();
  static void UpdateCurrentInputEventCount();

  NSView<mozView>* GetEditorView();

  bool IsPluginView() { return (mWindowType == eWindowType_plugin); }

  void PaintQD();

  nsCocoaWindow*    GetXULWindowWidget();

  NS_IMETHOD        ReparentNativeWidget(nsIWidget* aNewParent);

  mozilla::widget::TextInputHandler* GetTextInputHandler()
  {
    return mTextInputHandler;
  }

protected:

  bool              ReportDestroyEvent();
  bool              ReportMoveEvent();
  bool              ReportSizeEvent();

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
  nsRefPtr<mozilla::gl::TextureImage> mResizerImage;

  bool                  mVisible;
  bool                  mDrawing;
  bool                  mPluginDrawing;
  bool                  mIsDispatchPaint; // Is a paint event being dispatched

  NP_CGContext          mPluginCGContext;
#ifndef NP_NO_QUICKDRAW
  NP_Port               mPluginQDPort;
#endif
  nsIPluginInstanceOwner* mPluginInstanceOwner; // [WEAK]

  static PRUint32 sLastInputEventCount;
};

void NS_InstallPluginKeyEventsHandler();
void NS_RemovePluginKeyEventsHandler();

#endif // nsChildView_h_
