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

#include "nsISupports.h"
#include "nsBaseWidget.h"

#include "nsToolkit.h"

#include "nsIWidget.h"
#include "nsIEnumerator.h"
#include "nsIAppShell.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsString.h"


#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))


// =============================================================================

/**
 * Native Macintosh window wrapper. 
 */

class nsWindow : public nsBaseWidget
{

public:
                            nsWindow();
    virtual                 ~nsWindow();

    NS_DECL_ISUPPORTS

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

    NS_IMETHOD            	Destroy();
    virtual nsIWidget*    	GetParent(void);
    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD 							IsVisible(PRBool & aState);
    NS_IMETHOD            	Move(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD            	Resize(PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD            	Resize(PRUint32 aX, PRUint32 aY,PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD            	Enable(PRBool bState);
    NS_IMETHOD            	SetFocus(void);
    NS_IMETHOD            	GetBounds(nsRect &aRect);
    NS_IMETHOD            	SetBounds(const nsRect &aRect);
    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD            	SetFont(const nsFont &aFont);
    NS_IMETHOD            	Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD							Invalidate(const nsRect &aRect,PRBool aIsSynchronous);
    virtual void*           GetNativeData(PRUint32 aDataType);
    NS_IMETHOD            	SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD            	Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    NS_IMETHOD            	WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD            	ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD            	BeginResizingChildren(void);
    NS_IMETHOD            	EndResizingChildren(void);

    static  PRBool          ConvertStatus(nsEventStatus aStatus);
    NS_IMETHOD          	DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
    virtual PRBool          DispatchMouseEvent(nsMouseEvent &aEvent);

    virtual PRBool          OnPaint(nsPaintEvent &event);
    virtual PRBool          OnResize(nsSizeEvent &aEvent);
    virtual PRBool          OnKey(PRUint32 aEventType, PRUint32 aKeyCode, nsKeyEvent* aEvent);

    virtual PRBool          DispatchFocus(nsGUIEvent &aEvent);
    virtual PRBool          OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);

    virtual PRUint32        GetYCoord(PRUint32 aNewY);
    
    virtual void  ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY);
    
    NS_IMETHOD SetMenuBar(nsIMenuBar * aMenuBar);
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

     // Resize event management
    void SetResizeRect(nsRect& aRect);
    void SetResized(PRBool aResized);
    void GetResizeRect(nsRect* aRect);
    PRBool GetResized();    
    nsWindow* FindWidgetHit(Point aThePoint);
    virtual PtInWindow(PRInt32 aX,PRInt32 aY);
  
    // Mac specific methods
	  NS_IMETHOD SetBounds(const Rect& aMacRect);
    void MacRectToNSRect(const Rect& aMacRect, nsRect& aRect) const;
    void nsRectToMacRect(const nsRect& aRect, Rect& aMacRect) const;
    void DoPaintWidgets(RgnHandle	aTheRegion,nsIRenderingContext	*aRC);
    void DoResizeWidgets(nsSizeEvent &aEvent);
    PRBool RgnIntersects(RgnHandle aTheRegion,RgnHandle aIntersectRgn);

  	void LocalToWindowCoordinate(nsPoint& aPoint);
  	void LocalToWindowCoordinate(nsRect& aRect);
		void StringToStr255(const nsString& aText, Str255& aStr255);
		void Str255ToString(const Str255& aStr255, nsString& aText);

    char gInstanceClassName[256];
protected:
  virtual PRBool DispatchWindowEvent(nsGUIEvent* event);

  void CreateWindow(nsNativeWidget aNativeParent, nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData);

  void CreateMainWindow(nsNativeWidget aNativeParent, nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData);

  void CreateChildWindow(nsNativeWidget aNativeParent, nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData);


  void InitToolkit(nsIToolkit *aToolkit, nsIWidget * aWidgetParent);


  NS_IMETHOD      UpdateVisibilityFlag();
  NS_IMETHOD      UpdateDisplay();
  NS_IMETHOD			CalcOffset(PRInt32 &aX,PRInt32 &aY);
  NS_IMETHOD			CalcTotalOffset(PRInt32 &aX,PRInt32 &aY);


protected:
  nsRect      			mBounds;

  PRBool      			mShown;
  PRBool     	 			mVisible;
  PRBool      			mDisplayed;

  nsIWidget*				mParent;

  // Resize event management
  nsRect 						mResizeRect;
  PRUint32 					mResized;
  PRBool 						mLowerLeft;
  PRInt32						mPreferredWidth;
  PRInt32						mPreferredHeight;



// MAC SPECIFIC MEMBERS
public:
	RgnHandle			mWindowRegion;				// the region defining this window
	// parent window -- this is only used for the main window widget
	WindowRecord	*mWindowRecord;
	WindowPtr			mWindowPtr;
	PRBool				mWindowMadeHere;			// if main window and we created, true
	PRBool				mIsMainWindow;				// top level Mac window
	
	public:
		WindowPtr GetWindowPtr() {return mWindowPtr;}
protected:
	void InitDeviceContext(nsIDeviceContext *aContext,nsNativeWidget aParentWidget);
	
};

// ============================================================================

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
	ChildWindow() : nsWindow() {}

};

// =============================================================================

// =============================================================================

// Class to pass data around in the mac windows refcon

class nsRefData
{

protected:
	void*		mParentWidget;						// top level widget, parent widget, etc.  nsWindow with no parent
	void*		mCurrentWidget;						// store the current widget, for get native data
	void*		mGeneralPointer;					// Place to store embedding applications data, guarenteed not to be used by widget code

public:
	void*		GetTopWidget() {return(mParentWidget);}
	void*		GetCurWidget() {return(mCurrentWidget);}
	void*		GetGenPointer() {return(mGeneralPointer);}
	void		SetTopWidget(void* aParentWidget) {mParentWidget=aParentWidget;}
	void		SetCurWidget(void*  aCurrentWidget) {mCurrentWidget=aCurrentWidget;}
	void		SetGenPointer(void*  aGeneralPointer) {mGeneralPointer=aGeneralPointer;}

};


#endif // Window_h__
