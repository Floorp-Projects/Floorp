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


//-------------------------------------------------------------------------
//
// nsWindow
//
//-------------------------------------------------------------------------
//	Generic object

class nsWindow : public nsBaseWidget
{

public:
                            nsWindow();
    virtual                 ~nsWindow();

    // nsIWidget interface
    NS_IMETHOD              Create(nsIWidget *aParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD              Create(nsNativeWidget aNativeParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);

     // Utility method for implementing both Create(nsIWidget ...) and
     // Create(nsNativeWidget...)

    virtual nsresult        StandardCreate(nsIWidget *aParent,
		                            const nsRect &aRect,
		                            EVENT_CALLBACK aHandleEventFunction,
		                            nsIDeviceContext *aContext,
		                            nsIAppShell *aAppShell,
		                            nsIToolkit *aToolkit,
		                            nsWidgetInitData *aInitData,
		                            nsNativeWidget aNativeParent = nsnull);

    NS_IMETHOD            	Destroy();
    virtual nsIWidget*    	GetParent(void);

    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD 				IsVisible(PRBool & aState);

    NS_IMETHOD            	Move(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD            	Resize(PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD            	Resize(PRUint32 aX, PRUint32 aY,PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);

    NS_IMETHOD            	Enable(PRBool bState);
    NS_IMETHOD            	SetFocus(void);
    NS_IMETHOD            	GetBounds(nsRect &aRect);

    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD            	SetFont(const nsFont &aFont);
    NS_IMETHOD            	Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD				Invalidate(const nsRect &aRect,PRBool aIsSynchronous);

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

    virtual void         	StartDraw(nsIRenderingContext* aRenderingContext = nsnull);
    virtual void         	EndDraw();
    virtual PRBool          OnPaint(nsPaintEvent &event);
		NS_IMETHOD				Update();
    
    virtual void  ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);
	virtual void  LocalToWindowCoordinate(nsPoint& aPoint);
	virtual void  LocalToWindowCoordinate(nscoord& aX, nscoord& aY);
    virtual void  LocalToWindowCoordinate(nsRect& aRect);
    
    virtual void  ConvertToLocalCoordinates(nscoord &aX, nscoord &aY);
	virtual void  WindowToLocalCoordinate(nsPoint& aPoint);
	virtual void  WindowToLocalCoordinate(nscoord& aX, nscoord& aY);
    virtual void  WindowToLocalCoordinate(nsRect& aRect);

    NS_IMETHOD SetMenuBar(nsIMenuBar * aMenuBar);
    virtual nsIMenuBar* GetMenuBar();
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
    
    NS_IMETHOD       SetCursor(nsCursor aCursor);
  
    // Mac specific methods
    void nsRectToMacRect(const nsRect& aRect, Rect& aMacRect) const;
    void DoPaintWidgets(RgnHandle	aTheRegion,nsIRenderingContext	*aRC);
    PRBool RgnIntersects(RgnHandle aTheRegion,RgnHandle aIntersectRgn);

		void StringToStr255(const nsString& aText, Str255& aStr255);
		void Str255ToString(const Str255& aStr255, nsString& aText);

		virtual PRBool PointInWidget(Point aThePoint);
		virtual nsWindow* FindWidgetHit(Point aThePoint);

    char gInstanceClassName[256];

  virtual PRBool DispatchWindowEvent(nsGUIEvent& event);
protected:

	PRBool				ReportDestroyEvent();
	PRBool				ReportMoveEvent();
	PRBool				ReportSizeEvent();

  NS_IMETHOD			CalcOffset(PRInt32 &aX,PRInt32 &aY);


protected:
  nsIWidget*			mParent;
  
  PRBool     	 		mVisible;
  PRBool     	 		mEnabled;

  PRInt32				mPreferredWidth;
  PRInt32				mPreferredHeight;

  nsIFontMetrics*		mFontMetrics;
  nsIMenuBar* 			mMenuBar;
  nsIRenderingContext*  mTempRenderingContext;

// MAC SPECIFIC MEMBERS
protected:
	RgnHandle			mWindowRegion;				// the region defining this window
	WindowPtr			mWindowPtr;
	PRBool				mDrawing;
	PRBool				mDestroyCalled;
};

// =============================================================================
class ChildWindow : public nsWindow {

public:
	NS_IMETHOD_(nsrefcnt) AddRef();
	NS_IMETHOD_(nsrefcnt) Release();

	ChildWindow();

};


#endif // Window_h__
