/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef Window_h__
#define Window_h__

#include "nsBaseWidget.h"
#include "nsdefs.h"
#include "nsObject.h"
#include "nsSwitchToUIThread.h"
#include "nsToolkit.h"

#include "nsIWidget.h"

#include "nsIMenuBar.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsString.h"

#include "nsVoidArray.h"

#include <Window.h>
#include <View.h>
#include <Region.h>

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

//#define DRAG_DROP

// forward declaration
class nsViewBeOS;
class nsIRollupListener;

/**
 * Native BeOS window wrapper. 
 */

class nsWindow : public nsObject,
                 public nsSwitchToUIThread,
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
	                               const nsRect &aRect,
	                               EVENT_CALLBACK aHandleEventFunction,
	                               nsIDeviceContext *aContext,
	                               nsIAppShell *aAppShell = nsnull,
	                               nsIToolkit *aToolkit = nsnull,
	                               nsWidgetInitData *aInitData = nsnull);
	NS_IMETHOD              Create(nsNativeWidget aParent,
	                               const nsRect &aRect,
	                               EVENT_CALLBACK aHandleEventFunction,
	                               nsIDeviceContext *aContext,
	                               nsIAppShell *aAppShell = nsnull,
	                               nsIToolkit *aToolkit = nsnull,
	                               nsWidgetInitData *aInitData = nsnull);

	// Utility method for implementing both Create(nsIWidget ...) and
	// Create(nsNativeWidget...)

	virtual nsresult        StandardWindowCreate(nsIWidget *aParent,
	                                             const nsRect &aRect,
	                                             EVENT_CALLBACK aHandleEventFunction,
	                                             nsIDeviceContext *aContext,
	                                             nsIAppShell *aAppShell,
	                                             nsIToolkit *aToolkit,
	                                             nsWidgetInitData *aInitData,
	                                             nsNativeWidget aNativeParent = nsnull);

	NS_IMETHOD              Destroy();
	virtual nsIWidget*        GetParent(void);
	NS_IMETHOD              Show(PRBool bState);
 	NS_IMETHOD              CaptureMouse(PRBool aCapture);
	NS_IMETHOD              CaptureRollupEvents(nsIRollupListener *aListener,
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
	NS_IMETHOD              Enable(PRBool aState);
	NS_IMETHOD              IsEnabled(PRBool *aState);
	NS_IMETHOD              SetFocus(PRBool aRaise);
	NS_IMETHOD              GetBounds(nsRect &aRect);
	NS_IMETHOD              GetScreenBounds(nsRect &aRect);
	NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
	NS_IMETHOD              SetForegroundColor(const nscolor &aColor);
	virtual nsIFontMetrics* GetFont(void);
	NS_IMETHOD              SetFont(const nsFont &aFont);
	NS_IMETHOD              SetCursor(nsCursor aCursor);
	NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
	NS_IMETHOD              Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
	NS_IMETHOD              InvalidateRegion(const nsIRegion *aRegion,
	                                         PRBool aIsSynchronous);
	NS_IMETHOD              Update();
	virtual void*           GetNativeData(PRUint32 aDataType);
	NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
	NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
	NS_IMETHOD              SetTitle(const nsString& aTitle);
	NS_IMETHOD              SetMenuBar(nsIMenuBar * aMenuBar) { return NS_ERROR_FAILURE; }
	NS_IMETHOD              ShowMenuBar(PRBool aShow) { return NS_ERROR_FAILURE; }
	NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
	NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
	NS_IMETHOD              BeginResizingChildren(void);
	NS_IMETHOD              EndResizingChildren(void);
	NS_IMETHOD              GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
	NS_IMETHOD              SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
	NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
	NS_IMETHOD              EnableFileDrop(PRBool aEnable);

	virtual void             ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY) {}


	// nsSwitchToUIThread interface
	virtual bool             CallMethod(MethodInfo *info);
	virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, 
	                                           nsPoint aPoint, 
	                                           PRUint32 clicks, 
	                                           PRUint32 mod);


	virtual PRBool          AutoErase();
	void                   InitEvent(nsGUIEvent& event, nsPoint* aPoint = nsnull);
	
protected:

	static PRBool           EventIsInsideWindow(nsWindow* aWindow, nsPoint pos) ;
	static PRBool           DealWithPopups(uint32 methodID, nsPoint pos);

	// Allow Derived classes to modify the height that is passed
	// when the window is created or resized.
	virtual PRInt32         GetHeight(PRInt32 aProposedHeight);
	virtual void             OnDestroy();
	virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
	virtual PRBool          OnPaint(nsRect &r);
	virtual PRBool          OnResize(nsRect &aWindowRect);
	virtual PRBool          OnKeyDown(PRUint32 aEventType, 
	                                  const char *bytes, 
	                                  int32 numBytes, 
	                                  PRUint32 mod, 
	                                  PRUint32 bekeycode, 
	                                  int32 rawcode);
	virtual PRBool          OnKeyUp(PRUint32 aEventType, 
	                                const char *bytes, 
	                                int32 numBytes, 
	                                PRUint32 mod, 
	                                PRUint32 bekeycode, 
	                                int32 rawcode);
	virtual PRBool          DispatchKeyEvent(PRUint32 aEventType, PRUint32 aCharCode, PRUint32 aKeyCode);
	virtual PRBool          DispatchFocus(PRUint32 aEventType);
	virtual PRBool          OnScroll();
	static PRBool           ConvertStatus(nsEventStatus aStatus);
	PRBool                DispatchStandardEvent(PRUint32 aMsg);

	virtual PRBool          DispatchWindowEvent(nsGUIEvent* event);
	virtual BView          *CreateBeOSView();


	BView           *mView;
	nsIWidget       *mParent;
	PRBool           mIsTopWidgetWindow;
	BView           *mBorderlessParent;

	// I would imagine this would be in nsBaseWidget, but alas, it is not
	PRBool           mIsMetaDown;

	PRBool           mIsDestroying;
	PRBool           mOnDestroyCalled;
	PRBool           mIsVisible;
	// XXX Temporary, should not be caching the font
	nsFont          *mFont;

	PRInt32          mPreferredWidth;
	PRInt32          mPreferredHeight;

	PRBool           mEnabled;
	PRBool           mJustGotActivate;
	PRBool           mJustGotDeactivate;	

public:	// public on BeOS to allow BViews to access it
	// Enumeration of the methods which are accessable on the "main GUI thread"
	// via the CallMethod(...) mechanism...
	// see nsSwitchToUIThread
	enum {
	    CREATE       = 0x0101,
	    CREATE_NATIVE,
	    DESTROY,
	    SET_FOCUS,
	    GOT_FOCUS,
	    KILL_FOCUS,
	    SET_CURSOR,
	    CREATE_HACK,
	    ONMOUSE,
	    ONWHEEL,
	    ONPAINT,
	    ONSCROLL,
	    ONRESIZE,
	    CLOSEWINDOW,
	    MENU,
	    ONKEY,
	    BTNCLICK,
	    ONACTIVATE,
	    ONMOVE,
	    ONWORKSPACE,
	};
	nsToolkit *GetToolkit() { return (nsToolkit *)nsBaseWidget::GetToolkit(); }
};

//
// Each class need to subclass this as part of the subclass
//
class nsIWidgetStore {
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
class nsWindowBeOS : public BWindow, public nsIWidgetStore {
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

private:
	BPoint          lastWindowPoint;
};

//
// A BView subclass
//
class nsViewBeOS : public BView, public nsIWidgetStore {
	BRegion          paintregion;
	uint32           buttons;

public:
	                        nsViewBeOS(nsIWidget *aWidgetWindow, 
	                                   BRect aFrame, 
	                                   const char *aName,
	                                   uint32 aResizingMode, 
	                                   uint32 aFlags);

	virtual void            AttachedToWindow();
	virtual void            Draw(BRect updateRect);
	virtual void            DrawAfterChildren(BRect updateRect);
	virtual void            MouseDown(BPoint point);
	virtual void            MouseMoved(BPoint point, 
	                                   uint32 transit, 
	                                   const BMessage *message);
	virtual void            MouseUp(BPoint point);
	bool                  GetPaintRegion(BRegion *breg);
	void                  KeyDown(const char *bytes, int32 numBytes);
	void                  KeyUp(const char *bytes, int32 numBytes);
	virtual void            MakeFocus(bool focused);
	virtual void            MessageReceived(BMessage *msg);
	virtual void            FrameResized(float width, float height);
	virtual void            FrameMoved(BPoint origin);

private:
	void                  DoDraw(BRect updateRect);
	float                 lastViewWidth;
	float                 lastViewHeight;
	BPoint               lastViewPoint;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
public:
	                        ChildWindow() {};
	virtual PRBool          IsChild() { return(PR_TRUE); };
};

#endif // Window_h__
