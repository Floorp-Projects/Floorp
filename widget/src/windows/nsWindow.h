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
#include "nsIEnumerator.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsStringUtil.h"
#include "nsString.h"

#include "nsVoidArray.h"


#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

/**
 * Native WIN32 window wrapper. 
 */

class nsWindow : public nsObject,
                 public nsSwitchToUIThread,
                 public nsBaseWidget
{

public:
    nsWindow();
    virtual ~nsWindow();

    virtual void            PreCreateWidget(nsWidgetInitData *aWidgetInitData) {}
    // nsIWidget interface
    virtual void            Create(nsIWidget *aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
    virtual void            Create(nsNativeWidget aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);

    virtual void            Destroy();
    virtual nsIWidget*      GetParent(void);
    virtual void            Show(PRBool bState);
    virtual void            Move(PRUint32 aX, PRUint32 aY);
    virtual void            Resize(PRUint32 aWidth,
                                   PRUint32 aHeight,
                                   PRBool   aRepaint);
    virtual void            Resize(PRUint32 aX,
                                   PRUint32 aY,
                                   PRUint32 aWidth,
                                   PRUint32 aHeight,
                                   PRBool   aRepaint);
    virtual void            Enable(PRBool bState);
    virtual void            SetFocus(void);
    virtual void            GetBounds(nsRect &aRect);
    virtual void            SetBackgroundColor(const nscolor &aColor);
    virtual nsIFontMetrics* GetFont(void);
    virtual void            SetFont(const nsFont &aFont);
    virtual void            SetCursor(nsCursor aCursor);
    virtual void            Invalidate(PRBool aIsSynchronous);
    virtual void*           GetNativeData(PRUint32 aDataType);
    virtual void            SetColorMap(nsColorMap *aColorMap);
    virtual void            Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    virtual void            SetTitle(const nsString& aTitle); 
    virtual void            SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);   
    virtual void            RemoveTooltips();
    virtual void            UpdateTooltips(nsRect* aNewTips[]);
    virtual void            WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void            ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void            BeginResizingChildren(void);
    virtual void            EndResizingChildren(void);

    virtual void            SetUpForPaint(HDC aHDC);
   	virtual void            ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY) {}


    // nsSwitchToUIThread interface
    virtual BOOL            CallMethod(MethodInfo *info);

            HWND            GetWindowHandle() { return mWnd; }
            WNDPROC         GetPrevWindowProc() { return mPrevWndProc; }

    virtual PRBool          DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint = nsnull);
    virtual PRBool          AutoErase();
    nsPoint*                GetLastPoint() { return &mLastPoint; }

protected:

  virtual PRBool          ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue);

     // Allow Derived classes to modify the height that is passed
     // when the window is created or resized.
    virtual PRInt32         GetHeight(PRInt32 aProposedHeight);
    virtual LPCTSTR         WindowClass();
    virtual DWORD           WindowStyle();
    virtual DWORD           WindowExStyle();

    virtual void            SubclassWindow(BOOL bState);

    virtual void            OnDestroy();
    virtual PRBool          OnMove(PRInt32 aX, PRInt32 aY);
    virtual PRBool          OnPaint();
    virtual PRBool          OnResize(nsRect &aWindowRect);
    virtual PRBool          OnKey(PRUint32 aEventType, PRUint32 aKeyCode);

    virtual PRBool          DispatchFocus(PRUint32 aEventType);
    virtual PRBool          OnScroll(UINT scrollCode, int cPos);
    virtual HBRUSH          OnControlColor();

    static LRESULT CALLBACK WindowProc(HWND hWnd,
                                        UINT msg,
                                        WPARAM wParam,
                                        LPARAM lParam);
    
    static PRBool ConvertStatus(nsEventStatus aStatus);
    DWORD  GetBorderStyle(nsBorderStyle aBorderStyle);

    void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull);
    PRBool DispatchEvent(nsGUIEvent* event);
    PRBool DispatchStandardEvent(PRUint32 aMsg);
    void AddTooltip(HWND hwndOwner, nsRect* aRect, int aId);
    void RelayMouseEvent(UINT aMsg, WPARAM wParam, LPARAM lParam);

    void GetNonClientBounds(nsRect &aRect);

protected:
    static      nsWindow* gCurrentWindow;
    nsPoint     mLastPoint;
    HWND        mWnd;
    HWND        mTooltip;
    HPALETTE    mPalette;
    WNDPROC     mPrevWndProc;
  
    HBRUSH      mBrush;
    PRBool      mIsShiftDown;
    PRBool      mIsControlDown;
    PRBool      mIsAltDown;
    PRBool      mIsDestroying;
    PRBool      mOnDestroyCalled;

    // Enumeration of the methods which are accessable on the "main GUI thread"
    // via the CallMethod(...) mechanism...
    // see nsSwitchToUIThread
    enum {
        CREATE       = 0x0101,
        CREATE_NATIVE,
        DESTROY, 
        SET_FOCUS,
        SET_CURSOR,
        CREATE_HACK
    };

    static BOOL sIsRegistered;

    HDWP mDeferredPositioner;
};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
                            ChildWindow(){}
    PRBool          DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint = nsnull);

protected:
    virtual DWORD           WindowStyle();
    virtual DWORD           WindowExStyle() { return 0; }
};


#endif // Window_h__
