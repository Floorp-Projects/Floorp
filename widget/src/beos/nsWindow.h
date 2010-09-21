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
 *   Paul Ashford <arougthopher@lizardland.net>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
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
#ifndef Window_h__
#define Window_h__
#define BeIME

#include "nsBaseWidget.h"
#include "nsdefs.h"
#include "nsSwitchToUIThread.h"
#include "nsToolkit.h"

#include "nsIWidget.h"

#include "nsString.h"
#include "nsRegion.h"

#include <Window.h>
#include <View.h>
#include <Region.h>

#if defined(BeIME)
#include <Messenger.h>
#endif

#include <gfxBeOSSurface.h>

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

// forward declaration
class nsViewBeOS;
class nsIRollupListener;
#if defined(BeIME)
class nsIMEBeOS;
#endif

/**
 * Native BeOS window wrapper. 
 */

class nsWindow : public nsSwitchToUIThread,
                 public nsBaseWidget
{

public:
	nsWindow();
	virtual ~nsWindow();

	// In BeOS, each window runs in its own thread.  Because of this,
	// we have a proxy layer between the mozilla UI thread, and calls made
	// within the window's thread via CallMethod().  However, since the windows
	// are still running in their own thread, and reference counting takes place within
	// that thread, we need to reference and de-reference outselves atomically.
	// See BugZilla Bug# 92793
	NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
	NS_IMETHOD_(nsrefcnt) Release(void);          

	// nsIWidget interface
	NS_IMETHOD              Create(nsIWidget *aParent,
	                               nsNativeWidget aNativeParent,
	                               const nsRect &aRect,
	                               EVENT_CALLBACK aHandleEventFunction,
	                               nsIDeviceContext *aContext,
	                               nsIAppShell *aAppShell = nsnull,
	                               nsIToolkit *aToolkit = nsnull,
	                               nsWidgetInitData *aInitData = nsnull);

	gfxASurface*            GetThebesSurface();

	NS_IMETHOD              Destroy();
	virtual nsIWidget*        GetParent(void);
	NS_IMETHOD              Show(PRBool bState);
 	NS_IMETHOD              CaptureMouse(PRBool aCapture);
	NS_IMETHOD              CaptureRollupEvents(nsIRollupListener *aListener,
	                                            nsIMenuRollup *aMenuRollup,
	                                            PRBool aDoCapture,
	                                            PRBool aConsumeRollupEvent);
	NS_IMETHOD              IsVisible(PRBool & aState);

	NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop,
	                                          PRInt32 *aX, PRInt32 *aY);
	NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
	NS_IMETHOD              Resize(PRInt32 aWidth,
	                               PRInt32 aHeight,
	                               PRBool   aRepaint);
	NS_IMETHOD              Resize(PRInt32 aX,
	                               PRInt32 aY,
	                               PRInt32 aWidth,
	                               PRInt32 aHeight,
	                               PRBool   aRepaint);
	NS_IMETHOD              SetModal(PRBool aModal);
	NS_IMETHOD              Enable(PRBool aState);
	NS_IMETHOD              IsEnabled(PRBool *aState);
	NS_IMETHOD              SetFocus(PRBool aRaise);
	NS_IMETHOD              GetScreenBounds(nsRect &aRect);
	NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
	NS_IMETHOD              SetCursor(nsCursor aCursor);
	NS_IMETHOD              Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
	NS_IMETHOD              InvalidateRegion(const nsIRegion *aRegion,
	                                         PRBool aIsSynchronous);
	NS_IMETHOD              Update();
	virtual void*           GetNativeData(PRUint32 aDataType);
	NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
	NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
	NS_IMETHOD              SetTitle(const nsAString& aTitle);
	NS_IMETHOD              SetMenuBar(void * aMenuBar) { return NS_ERROR_FAILURE; }
	NS_IMETHOD              ShowMenuBar(PRBool aShow) { return NS_ERROR_FAILURE; }
	NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
	NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
	NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
	NS_IMETHOD              HideWindowChrome(PRBool aShouldHide);

	virtual void            ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY) {}


	// nsSwitchToUIThread interface
	virtual bool            CallMethod(MethodInfo *info);
	virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, 
	                                           nsPoint aPoint, 
	                                           PRUint32 clicks, 
	                                           PRUint32 mod,
	                                           PRUint16 aButton = nsMouseEvent::eLeftButton);


	void                    InitEvent(nsGUIEvent& event, nsPoint* aPoint = nsnull);
	NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent);
protected:

	static PRBool           EventIsInsideWindow(nsWindow* aWindow, nsPoint pos) ;
	static PRBool           DealWithPopups(uint32 methodID, nsPoint pos);
    // Following methods need to be virtual if we will subclassing
	void                    OnDestroy();
	void                    OnWheel(PRInt32 aDirection, uint32 aButtons, BPoint aPoint, nscoord aDelta);
	PRBool                  OnMove(PRInt32 aX, PRInt32 aY);
	nsresult                OnPaint(BRegion *breg);
	PRBool                  OnResize(nsRect &aWindowRect);
	PRBool                  OnKeyDown(PRUint32 aEventType, 
	                                  const char *bytes, 
	                                  int32 numBytes, 
	                                  PRUint32 mod, 
	                                  PRUint32 bekeycode, 
	                                  int32 rawcode);
	PRBool                  OnKeyUp(PRUint32 aEventType, 
	                                const char *bytes, 
	                                int32 numBytes, 
	                                PRUint32 mod, 
	                                PRUint32 bekeycode, 
	                                int32 rawcode);
	PRBool                  DispatchKeyEvent(PRUint32 aEventType, PRUint32 aCharCode,
                                           PRUint32 aKeyCode, PRUint32 aFlags = 0);
	PRBool                  DispatchFocus(PRUint32 aEventType);
	static PRBool           ConvertStatus(nsEventStatus aStatus)
	                        { return aStatus == nsEventStatus_eConsumeNoDefault; }
	PRBool                  DispatchStandardEvent(PRUint32 aMsg);

	PRBool                  DispatchWindowEvent(nsGUIEvent* event);
	void                    HideKids(PRBool state);

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_POPUP_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

	nsCOMPtr<nsIWidget> mParent;
	nsWindow*        mWindowParent;
	nsCOMPtr<nsIRegion> mUpdateArea;
	nsIFontMetrics*  mFontMetrics;

	nsViewBeOS*      mView;
	window_feel      mBWindowFeel;
	window_look      mBWindowLook;

	nsRefPtr<gfxBeOSSurface> mThebesSurface;

	//Just for saving space we use packed bools.
	PRPackedBool           mIsTopWidgetWindow;
	PRPackedBool           mIsMetaDown;
	PRPackedBool           mIsShiftDown;
	PRPackedBool           mIsControlDown;
	PRPackedBool           mIsAltDown;
	PRPackedBool           mIsDestroying;
	PRPackedBool           mIsVisible;
	PRPackedBool           mEnabled;
	PRPackedBool           mIsScrolling;
	PRPackedBool           mListenForResizes;
	
public:	// public on BeOS to allow BViews to access it

	nsToolkit *GetToolkit() { return (nsToolkit *)nsBaseWidget::GetToolkit(); }
};

//
// Each class need to subclass this as part of the subclass
//
class nsIWidgetStore
{
public:
	                        nsIWidgetStore(nsIWidget *aWindow);
	virtual                ~nsIWidgetStore();

	virtual nsIWidget      *GetMozillaWidget(void);

private:
	nsIWidget       *mWidget;
};

//
// A BWindow subclass
//
class nsWindowBeOS : public BWindow, public nsIWidgetStore
{
public:
	                        nsWindowBeOS(nsIWidget *aWidgetWindow,  
	                                     BRect aFrame, 
	                                     const char *aName, 
	                                     window_look aLook,
	                                     window_feel aFeel, 
	                                     int32 aFlags, 
	                                     int32 aWorkspace = B_CURRENT_WORKSPACE);
	virtual                ~nsWindowBeOS();

	virtual bool            QuitRequested( void );
	virtual void            MessageReceived(BMessage *msg);
	virtual void            DispatchMessage(BMessage *msg, BHandler *handler);
	virtual void            WindowActivated(bool active);
	virtual void            FrameMoved(BPoint origin);
	virtual void            WorkspacesChanged(uint32 oldworkspace, uint32 newworkspace);
	virtual void            FrameResized(float width, float height);
	bool                    fJustGotBounds;	
private:
	BPoint          lastWindowPoint;
};

//
// A BView subclass
//
class nsViewBeOS : public BView, public nsIWidgetStore
{
public:
	                        nsViewBeOS(nsIWidget *aWidgetWindow, 
	                                   BRect aFrame, 
	                                   const char *aName,
	                                   uint32 aResizingMode, 
	                                   uint32 aFlags);

	virtual void            Draw(BRect updateRect);
	virtual void            MouseDown(BPoint point);
	virtual void            MouseMoved(BPoint point, 
	                                   uint32 transit, 
	                                   const BMessage *message);
	virtual void            MouseUp(BPoint point);
	bool                    GetPaintRegion(BRegion *breg);
	void                    Validate(BRegion *reg);
	BPoint                  GetWheel();
	void                    KeyDown(const char *bytes, int32 numBytes);
	void                    KeyUp(const char *bytes, int32 numBytes);
	virtual void            MakeFocus(bool focused);
	virtual void            MessageReceived(BMessage *msg);
	void                    SetVisible(bool visible);
	bool                    Visible();
	BRegion                 paintregion;
	uint32                  buttons;

private:
#if defined(BeIME)
 	void                 DoIME(BMessage *msg);
#endif
	BPoint               mousePos;
	uint32               mouseMask;
	// actually it is delta, not point, using it as convenient x,y storage
	BPoint               wheel;
	bool                 fRestoreMouseMask;	
	bool                 fJustValidated;
	bool                 fWheelDispatched;
	bool                 fVisible;
};

#if defined(BeIME)
class nsIMEBeOS 
{
public:
	nsIMEBeOS();
//	virtual ~nsIMEBeOS();
	void	RunIME(uint32 *args, nsWindow *owner, BView* view);
	void	DispatchText(nsString &text, PRUint32 txtCount, nsTextRange* txtRuns);
	void	DispatchIME(PRUint32 what);
	void	DispatchCancelIME();
	PRBool	DispatchWindowEvent(nsGUIEvent* event);
	
	static  nsIMEBeOS *GetIME();

private:
	nsWindow*	imeTarget;
	BMessenger	imeMessenger;
	nsString	imeText;
	BPoint		imeCaret;
	PRUint32	imeState, imeWidth, imeHeight;
	static	    nsIMEBeOS *beosIME;
};
#endif
#endif // Window_h__
