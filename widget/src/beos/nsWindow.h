/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
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
#include "nsStringUtil.h"
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

    // nsIWidget interface
    NS_IMETHOD            Create(nsIWidget *aParent,
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
    virtual nsIWidget*      GetParent(void);
    NS_IMETHOD              Show(PRBool bState);
    NS_IMETHOD              IsVisible(PRBool & aState);

    NS_IMETHOD              Minimize(void);
    NS_IMETHOD              Maximize(void);
    NS_IMETHOD              Restore(void);

    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              Resize(PRInt32 aWidth,
                                   PRInt32 aHeight,
                                   PRBool   aRepaint);
    NS_IMETHOD              Resize(PRInt32 aX,
                                   PRInt32 aY,
                                   PRInt32 aWidth,
                                   PRInt32 aHeight,
                                   PRBool   aRepaint);
    NS_IMETHOD              Enable(PRBool bState);
    NS_IMETHOD              SetFocus(void);
    NS_IMETHOD              GetBounds(nsRect &aRect);
    NS_IMETHOD              GetClientBounds(nsRect &aRect);
    NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD              SetFont(const nsFont &aFont);
    NS_IMETHOD              SetCursor(nsCursor aCursor);
    NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD              Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
    NS_IMETHOD              Update();
    virtual void*           GetNativeData(PRUint32 aDataType);
    NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    NS_IMETHOD              SetTitle(const nsString& aTitle); 
    NS_IMETHOD              SetMenuBar(nsIMenuBar * aMenuBar); 
    NS_IMETHOD              ShowMenuBar(PRBool aShow);
    NS_IMETHOD              SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);   
    NS_IMETHOD              RemoveTooltips();
    NS_IMETHOD              UpdateTooltips(nsRect* aNewTips[]);
    NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD              BeginResizingChildren(void);
    NS_IMETHOD              EndResizingChildren(void);
    NS_IMETHOD              GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD              SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
    NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
    NS_IMETHOD              EnableFileDrop(PRBool aEnable);

  virtual void            ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY) {}


    // nsSwitchToUIThread interface
    virtual bool            CallMethod(MethodInfo *info);
	virtual PRBool			DispatchMouseEvent(PRUint32 aEventType, nsPoint aPoint, PRUint32 clicks, PRUint32 mod);
    virtual PRBool          AutoErase();

//    PRInt32                 GetNewCmdMenuId() { mMenuCmdId++; return mMenuCmdId;}

    void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull);

protected:

     // Allow Derived classes to modify the height that is passed
     // when the window is created or resized.
    virtual PRInt32         GetHeight(PRInt32 aProposedHeight);
    virtual void            OnDestroy();
    virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool          OnPaint(nsRect &r);
    virtual PRBool          OnResize(nsRect &aWindowRect);
	virtual PRBool			OnKey(PRUint32 aEventType, const char *bytes, int32 numBytes, PRUint32 mod);

    virtual PRBool          DispatchFocus(PRUint32 aEventType);
	virtual PRBool			OnScroll();
    static PRBool ConvertStatus(nsEventStatus aStatus);
    PRBool DispatchStandardEvent(PRUint32 aMsg);
    void AddTooltip(BView *hwndOwner, nsRect* aRect, int aId);

    virtual PRBool          DispatchWindowEvent(nsGUIEvent* event);
	virtual BView			*CreateBeOSView();

#if 0
    virtual PRBool          ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue);
    nsIMenuItem *           FindMenuItem(nsIMenu * aMenu, PRUint32 aId);
    nsIMenu *               FindMenu(nsIMenu * aMenu, HMENU aNativeMenu, PRInt32 &aDepth);
    nsresult                MenuHasBeenSelected(HMENU aNativeMenu, UINT aItemNum, UINT aFlags, UINT aCommand);

    virtual LPCTSTR         WindowClass();
    virtual DWORD           WindowStyle();
    virtual DWORD           WindowExStyle();

    static LRESULT CALLBACK WindowProc(BWindow * hWnd,
                                        UINT msg,
                                        WPARAM wParam,
                                        LPARAM lParam);
    
    void RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);

    BWindow		*mTooltip;
#endif

	BView		*mView;

    PRBool      mIsDestroying;
    PRBool      mOnDestroyCalled;
    PRBool      mIsVisible;
    // XXX Temporary, should not be caching the font
    nsFont *    mFont;

    PRInt32     mPreferredWidth;
    PRInt32     mPreferredHeight;

    nsIMenuBar  * mMenuBar;
    PRInt32       mMenuCmdId;
    nsIMenu     * mHitMenu;
    nsVoidArray * mHitSubMenus;

#if 0
    // Drag & Drop

#ifdef DRAG_DROP
    //nsDropTarget * mDropTarget;
    CfDropSource * mDropSource;
    CfDropTarget * mDropTarget;
    CfDragDrop   * mDragDrop;
#endif
#endif

public:	// public on BeOS to allow BViews to access it
    // Enumeration of the methods which are accessable on the "main GUI thread"
    // via the CallMethod(...) mechanism...
    // see nsSwitchToUIThread
    enum {
        CREATE       = 0x0101,
        CREATE_NATIVE,
        DESTROY, 
        SET_FOCUS,
        SET_CURSOR,
        CREATE_HACK,
		ONMOUSE,
		ONPAINT,
		ONSCROLL,
		ONRESIZE,
		CLOSEWINDOW,
		MENU,
		ONKEY,
		BTNCLICK
    };
	nsToolkit *GetToolkit() { return (nsToolkit *)nsBaseWidget::GetToolkit(); }
};

//
// Each class need to subclass this as part of the subclass
//
class nsIWidgetStore {
  public:
    nsIWidgetStore(nsIWidget *aWindow);
    virtual ~nsIWidgetStore();
    
    virtual nsIWidget *GetMozillaWidget(void);
    
  private:
    nsIWidget *mWidget;
};

//
// A BWindow subclass
//
class nsWindowBeOS : public BWindow, public nsIWidgetStore {
  public:
    nsWindowBeOS(nsIWidget *aWidgetWindow,  BRect aFrame, const char *aName, window_look aLook,
        window_feel aFeel, int32 aFlags, int32 aWorkspace = B_CURRENT_WORKSPACE);

    virtual bool QuitRequested( void );  
	virtual void MessageReceived(BMessage *msg);
};

//
// A BView subclass
//
class nsViewBeOS : public BView, public nsIWidgetStore {
	BRegion	paintregion;
	uint32	buttons;
	nsRect	currsizerect;
	bool	currsizechanged;

public:
					nsViewBeOS( nsIWidget *aWidgetWindow, BRect aFrame, const char *aName,
						uint32 aResizingMode, uint32 aFlags );

	virtual void	AttachedToWindow();
	virtual void	Draw(BRect updateRect);
	virtual void	FrameResized(float width, float height);
	virtual void	MouseDown(BPoint point);
	virtual void	MouseMoved(BPoint point, uint32 transit, const BMessage *message);
	virtual void	MouseUp(BPoint point);
	bool			GetPaintRect(nsRect &r);
	bool			GetSizeRect(nsRect &r);
	void KeyDown(const char *bytes, int32 numBytes);
	void KeyUp(const char *bytes, int32 numBytes);
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {
  public:
    ChildWindow() {};
    virtual PRBool IsChild() { return(PR_TRUE); };
};

#endif // Window_h__
