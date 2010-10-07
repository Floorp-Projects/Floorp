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
#include "nsCocoaTextInputHandler.h"
#include "nsCocoaUtils.h"

#include "nsIAppShell.h"

#include "nsString.h"
#include "nsIDragService.h"

#include "npapi.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/NSOpenGL.h>

class gfxASurface;
class nsChildView;
class nsCocoaWindow;
union nsPluginPort;

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

  BOOL mIsPluginView;
  NPEventModel mPluginEventModel;
  NPDrawingModel mPluginDrawingModel;

  // The following variables are only valid during key down event processing.
  // Their current usage needs to be fixed to avoid problems with nested event
  // loops that can confuse them. Once a variable is set during key down event
  // processing, if an event spawns a nested event loop the previously set value
  // will be wiped out.
  NSEvent* mCurKeyEvent;
  PRBool mKeyDownHandled;
  // While we process key down events we need to keep track of whether or not
  // we sent a key press event. This helps us make sure we do send one
  // eventually.
  BOOL mKeyPressSent;
  // Valid when mKeyPressSent is true.
  PRBool mKeyPressHandled;

  // needed for NSTextInput implementation
  NSRange mMarkedRange;
  
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

#ifndef NP_NO_CARBON
  // For use with plugins, so that we can support IME in them.  We can't use
  // Cocoa TSM documents (those created and managed by the NSTSMInputContext
  // class) -- for some reason TSMProcessRawKeyEvent() doesn't work with them.
  TSMDocumentID mPluginTSMDoc;
#endif
  BOOL mPluginComplexTextInputRequested;

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

- (void)drawRect:(NSRect)aRect inContext:(CGContextRef)aContext;

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                             type:(nsMouseEvent::exitType)aType;

#ifndef NP_NO_CARBON
- (void) processPluginKeyEvent:(EventRef)aKeyEvent;
#endif
- (void)pluginRequestsComplexTextInputForCurrentEvent;

- (void)update;
- (void)lockFocus;
- (void) _surfaceNeedsUpdate:(NSNotification*)notification;

- (BOOL)isPluginView;

- (BOOL)isUsingOpenGL;

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
@end

class ChildViewMouseTracker {

public:

  static void MouseMoved(NSEvent* aEvent);
  static void OnDestroyView(ChildView* aView);
  static BOOL WindowAcceptsEvent(NSWindow* aWindow, NSEvent* aEvent,
                                 ChildView* aView, BOOL isClickThrough = NO);
  static void ReEvaluateMouseEnterState(NSEvent* aEvent = nil);
  static ChildView* ViewForEvent(NSEvent* aEvent);

  static ChildView* sLastMouseEventView;

private:

  static NSWindow* WindowForEvent(NSEvent* aEvent);
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
                                 nsIDeviceContext *aContext,
                                 nsIAppShell *aAppShell = nsnull,
                                 nsIToolkit *aToolkit = nsnull,
                                 nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD              Destroy();

  NS_IMETHOD              Show(PRBool aState);
  NS_IMETHOD              IsVisible(PRBool& outState);

  NS_IMETHOD              SetParent(nsIWidget* aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual float           GetDPI();

  NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop,
                                            PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD              Enable(PRBool aState);
  NS_IMETHOD              IsEnabled(PRBool *aState);
  NS_IMETHOD              SetFocus(PRBool aRaise);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);

  NS_IMETHOD              Invalidate(const nsIntRect &aRect, PRBool aIsSynchronous);

  virtual void*           GetNativeData(PRUint32 aDataType);
  virtual nsresult        ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  virtual nsIntPoint      WidgetToScreenOffset();
  virtual PRBool          ShowsResizeIndicator(nsIntRect* aResizerRect);

  static  PRBool          ConvertStatus(nsEventStatus aStatus)
                          { return aStatus == nsEventStatus_eConsumeNoDefault; }
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);

  NS_IMETHOD              Update();
  virtual PRBool          GetShouldAccelerate();

  NS_IMETHOD        SetCursor(nsCursor aCursor);
  NS_IMETHOD        SetCursor(imgIContainer* aCursor, PRUint32 aHotspotX, PRUint32 aHotspotY);
  
  NS_IMETHOD        CaptureRollupEvents(nsIRollupListener * aListener, nsIMenuRollup * aMenuRollup, 
                                        PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD        SetTitle(const nsAString& title);

  NS_IMETHOD        GetAttention(PRInt32 aCycleCount);

  virtual PRBool HasPendingInputEvent();

  NS_IMETHOD        ActivateNativeMenuItemAt(const nsAString& indexString);
  NS_IMETHOD        ForceUpdateNativeMenuAt(const nsAString& indexString);

  NS_IMETHOD        ResetInputState();
  NS_IMETHOD        SetIMEOpenState(PRBool aState);
  NS_IMETHOD        GetIMEOpenState(PRBool* aState);
  NS_IMETHOD        SetIMEEnabled(PRUint32 aState);
  NS_IMETHOD        GetIMEEnabled(PRUint32* aState);
  NS_IMETHOD        CancelIMEComposition();
  NS_IMETHOD        GetToggledKeyState(PRUint32 aKeyCode,
                                       PRBool* aLEDState);
  NS_IMETHOD        OnIMEFocusChange(PRBool aFocus);

  // nsIPluginWidget
  NS_IMETHOD        GetPluginClipRect(nsIntRect& outClipRect, nsIntPoint& outOrigin, PRBool& outWidgetVisible);
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
  
  // Mac specific methods
  
  virtual PRBool    DispatchWindowEvent(nsGUIEvent& event);
  
#ifdef ACCESSIBILITY
  already_AddRefed<nsAccessible> GetDocumentAccessible();
#endif

  virtual gfxASurface* GetThebesSurface();

  NS_IMETHOD BeginSecureKeyboardInput();
  NS_IMETHOD EndSecureKeyboardInput();

  void              HidePlugin();
  void              UpdatePluginPort();

  void              ResetParent();

  static PRBool DoHasPendingInputEvent();
  static PRUint32 GetCurrentInputEventCount();
  static void UpdateCurrentInputEventCount();

  static void ApplyConfiguration(nsIWidget* aExpectedParent,
                                 const nsIWidget::Configuration& aConfiguration,
                                 PRBool aRepaint);

  nsCocoaTextInputHandler* TextInputHandler() { return &mTextInputHandler; }
  NSView<mozView>* GetEditorView();

  PRBool IsPluginView() { return (mWindowType == eWindowType_plugin); }

  void PaintQD();

  NS_IMETHOD        ReparentNativeWidget(nsIWidget* aNewParent);
protected:

  PRBool            ReportDestroyEvent();
  PRBool            ReportMoveEvent();
  PRBool            ReportSizeEvent();

  // override to create different kinds of child views. Autoreleases, so
  // caller must retain.
  virtual NSView*   CreateCocoaView(NSRect inFrame);
  void              TearDownView();
  nsCocoaWindow*    GetXULWindowWidget();

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_POPUP_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

protected:

  NSView<mozView>*      mView;      // my parallel cocoa view (ChildView or NativeScrollbarView), [STRONG]
  nsCocoaTextInputHandler mTextInputHandler;

  NSView<mozView>*      mParentView;
  nsIWidget*            mParentWidget;

#ifdef ACCESSIBILITY
  // weak ref to this childview's associated mozAccessible for speed reasons 
  // (we get queried for it *a lot* but don't want to own it)
  nsWeakPtr             mAccessible;
#endif

  nsRefPtr<gfxASurface> mTempThebesSurface;

  PRPackedBool          mVisible;
  PRPackedBool          mDrawing;
  PRPackedBool          mPluginDrawing;
  PRPackedBool          mIsDispatchPaint; // Is a paint event being dispatched

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
