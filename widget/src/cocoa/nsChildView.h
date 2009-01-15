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
#include "nsIAccessible.h"
#include "mozAccessibleProtocol.h"
#endif

#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsBaseWidget.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIPluginWidget.h"
#include "nsIScrollableView.h"
#include "nsWeakPtr.h"

#include "nsIWidget.h"
#include "nsIAppShell.h"

#include "nsIEventListener.h"
#include "nsString.h"
#include "nsIDragService.h"

#include "nsplugindefs.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

class gfxASurface;
class nsChildView;
union nsPluginPort;

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

@interface NSEvent (Undocumented)

// Return Cocoa event's corresponding Carbon event.  Not initialized (on
// synthetic events) until the OS actually "sends" the event.  This method
// has been present in the same form since at least OS X 10.2.8.
- (EventRef)_eventRef;

@end

// Needed to support pixel scrolling.
// See http://developer.apple.com/qa/qa2005/qa1453.html.
// kEventMouseScroll is only defined on 10.5+. Using the moz prefix avoids
// potential symbol conflicts.
// This should be changed when 10.4 support is dropped.
enum {
  mozkEventMouseScroll             = 11
};

// Support for pixel scroll deltas, not part of NSEvent.h
// See http://lists.apple.com/archives/cocoa-dev/2007/Feb/msg00050.html
@interface NSEvent (DeviceDelta)
  - (float)deviceDeltaX;
  - (float)deviceDeltaY;
@end

@interface ChildView : NSView<
#ifdef ACCESSIBILITY
                              mozAccessible,
#endif
                              mozView, NSTextInput>
{
@private
  NSWindow* mWindow; // shortcut to the top window, [WEAK]
  
  // the nsChildView that created the view. It retains this NSView, so
  // the link back to it must be weak.
  nsChildView* mGeckoChild;
    
  // Whether we're a plugin view.
  BOOL mIsPluginView;

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

  BOOL mInHandScroll; // true for as long as we are hand scrolling
  // hand scroll locations
  NSPoint mHandScrollStartMouseLoc;
  nscoord mHandScrollStartScrollX, mHandScrollStartScrollY;
  
  // when mouseDown: is called, we store its event here (strong)
  NSEvent* mLastMouseDownEvent;
  
  // rects that were invalidated during a draw, so have pending drawing
  NSMutableArray* mPendingDirtyRects;
  BOOL mPendingFullDisplay;

  // All views are always opaque (non-transparent). The only exception is when we're
  // the content view in a transparent XUL window.
  BOOL mIsTransparent;
  PRIntervalTime mLastShadowInvalidation;
  BOOL mNeedsShadowInvalidation;

  // Holds our drag service across multiple drag calls. The reference to the
  // service is obtained when the mouse enters the view and is released when
  // the mouse exits or there is a drop. This prevents us from having to
  // re-establish the connection to the service manager many times per second
  // when handling |draggingUpdated:| messages.
  nsIDragService* mDragService;
  
  // For use with plugins, so that we can support IME in them.  We can't use
  // Cocoa TSM documents (those created and managed by the NSTSMInputContext
  // class) -- for some reason TSMProcessRawKeyEvent() doesn't work with them.
  TSMDocumentID mPluginTSMDoc;

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

- (void)setTransparent:(BOOL)transparent;

- (void)sendFocusEvent:(PRUint32)eventType;

- (void) processPluginKeyEvent:(EventRef)aKeyEvent;

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



//-------------------------------------------------------------------------
//
// nsTSMManager
//
//-------------------------------------------------------------------------

class nsTSMManager {
public:
  static PRBool IsComposing() { return sComposingView ? PR_TRUE : PR_FALSE; }
  static PRBool IsIMEEnabled() { return sIsIMEEnabled; }
  static PRBool IgnoreCommit() { return sIgnoreCommit; }

  static void OnDestroyView(NSView<mozView>* aDestroyingView);

  // Note that we cannot get the actual state in TSM. But we can trust this
  // value. Because nsIMEStateManager reset this at every focus changing.
  static PRBool IsRomanKeyboardsOnly() { return sIsRomanKeyboardsOnly; }

  static PRBool GetIMEOpenState();

  static void InitTSMDocument(NSView<mozView>* aViewForCaret);
  static void StartComposing(NSView<mozView>* aComposingView);
  static void UpdateComposing(NSString* aComposingString);
  static void EndComposing();
  static void EnableIME(PRBool aEnable);
  static void SetIMEOpenState(PRBool aOpen);
  static void SetRomanKeyboardsOnly(PRBool aRomanOnly);

  static void CommitIME();
  static void CancelIME();
private:
  static PRBool sIsIMEEnabled;
  static PRBool sIsRomanKeyboardsOnly;
  static PRBool sIgnoreCommit;
  static NSView<mozView>* sComposingView;
  static TSMDocumentID sDocumentID;
  static NSString* sComposingString;

  static void KillComposing();
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
                                 const nsIntRect &aRect,
                                 EVENT_CALLBACK aHandleEventFunction,
                                 nsIDeviceContext *aContext,
                                 nsIAppShell *aAppShell = nsnull,
                                 nsIToolkit *aToolkit = nsnull,
                                 nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD              Create(nsNativeWidget aNativeParent,
                                 const nsIntRect &aRect,
                                 EVENT_CALLBACK aHandleEventFunction,
                                 nsIDeviceContext *aContext,
                                 nsIAppShell *aAppShell = nsnull,
                                 nsIToolkit *aToolkit = nsnull,
                                 nsWidgetInitData *aInitData = nsnull);

   // Utility method for implementing both Create(nsIWidget ...) and
   // Create(nsNativeWidget...)

  virtual nsresult        StandardCreate(nsIWidget *aParent,
                              const nsIntRect &aRect,
                              EVENT_CALLBACK aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell *aAppShell,
                              nsIToolkit *aToolkit,
                              nsWidgetInitData *aInitData,
                              nsNativeWidget aNativeParent = nsnull);

  NS_IMETHOD              Destroy();

  NS_IMETHOD              Show(PRBool aState);
  NS_IMETHOD              IsVisible(PRBool& outState);

  NS_IMETHOD              SetParent(nsIWidget* aNewParent);
  virtual nsIWidget*      GetParent(void);

  NS_IMETHOD              ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                           PRBool *aForWindow);

  NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop,
                                            PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD              Enable(PRBool aState);
  NS_IMETHOD              IsEnabled(PRBool *aState);
  NS_IMETHOD              SetFocus(PRBool aRaise);
  NS_IMETHOD              SetBounds(const nsIntRect &aRect);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);

  NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD              Invalidate(const nsIntRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD              InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
  NS_IMETHOD              Validate();

  virtual void*           GetNativeData(PRUint32 aDataType);
  NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
  NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsIntRect *aClipRect);
  NS_IMETHOD              WidgetToScreen(const nsIntRect& aOldRect, nsIntRect& aNewRect);
  NS_IMETHOD              ScreenToWidget(const nsIntRect& aOldRect, nsIntRect& aNewRect);
  NS_IMETHOD              BeginResizingChildren(void);
  NS_IMETHOD              EndResizingChildren(void);
  virtual PRBool          ShowsResizeIndicator(nsIntRect* aResizerRect);

  static  PRBool          ConvertStatus(nsEventStatus aStatus)
                          { return aStatus == nsEventStatus_eConsumeNoDefault; }
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);

  NS_IMETHOD              Update();

  virtual void      ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);
  void              LocalToWindowCoordinate(nsIntPoint& aPoint)         { ConvertToDeviceCoordinates(aPoint.x, aPoint.y); }
  void              LocalToWindowCoordinate(nscoord& aX, nscoord& aY)   { ConvertToDeviceCoordinates(aX, aY); }
  void              LocalToWindowCoordinate(nsIntRect& aRect)           { ConvertToDeviceCoordinates(aRect.x, aRect.y); }

  NS_IMETHOD        SetMenuBar(void* aMenuBar);
  NS_IMETHOD        ShowMenuBar(PRBool aShow);

  NS_IMETHOD        GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
  NS_IMETHOD        SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
  
  NS_IMETHOD        SetCursor(nsCursor aCursor);
  NS_IMETHOD        SetCursor(imgIContainer* aCursor, PRUint32 aHotspotX, PRUint32 aHotspotY);
  
  NS_IMETHOD        CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD        SetTitle(const nsAString& title);

  NS_IMETHOD        GetAttention(PRInt32 aCycleCount);

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

  // nsIPluginWidget
  NS_IMETHOD        GetPluginClipRect(nsIntRect& outClipRect, nsIntPoint& outOrigin, PRBool& outWidgetVisible);
  NS_IMETHOD        StartDrawPlugin();
  NS_IMETHOD        EndDrawPlugin();
  NS_IMETHOD        SetPluginInstanceOwner(nsIPluginInstanceOwner* aInstanceOwner);
  
  virtual nsTransparencyMode GetTransparencyMode();
  virtual void                SetTransparencyMode(nsTransparencyMode aMode);
  NS_IMETHOD        SetWindowShadowStyle(PRInt32 aStyle);
  
  // Mac specific methods
  virtual PRBool    PointInWidget(Point aThePoint);
  
  virtual PRBool    DispatchWindowEvent(nsGUIEvent& event);
  
  void              LiveResizeStarted();
  void              LiveResizeEnded();
  
#ifdef ACCESSIBILITY
  void              GetDocumentAccessible(nsIAccessible** aAccessible);
#endif

  virtual gfxASurface* GetThebesSurface();

  NS_IMETHOD BeginSecureKeyboardInput();
  NS_IMETHOD EndSecureKeyboardInput();

  void              HidePlugin();

protected:

  PRBool            ReportDestroyEvent();
  PRBool            ReportMoveEvent();
  PRBool            ReportSizeEvent();

  NS_IMETHOD        CalcOffset(PRInt32 &aX,PRInt32 &aY);

  virtual PRBool    OnPaint(nsPaintEvent & aEvent);

  // override to create different kinds of child views. Autoreleases, so
  // caller must retain.
  virtual NSView*   CreateCocoaView(NSRect inFrame);
  void              TearDownView();

  virtual nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                            PRInt32 aNativeKeyCode,
                                            PRUint32 aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters);

protected:

  NSView<mozView>*      mView;      // my parallel cocoa view (ChildView or NativeScrollbarView), [STRONG]

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
  PRPackedBool          mLiveResizeInProgress;
  PRPackedBool          mIsPluginView; // true if this is a plugin view
  PRPackedBool          mPluginDrawing;
  PRPackedBool          mPluginIsCG; // true if this is a CoreGraphics plugin

  PRPackedBool          mInSetFocus;

  nsPluginPort          mPluginPort;
  nsIPluginInstanceOwner* mPluginInstanceOwner; // [WEAK]
};

void NS_InstallPluginKeyEventsHandler();
void NS_RemovePluginKeyEventsHandler();

#endif // nsChildView_h_
