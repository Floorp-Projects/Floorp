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

class nsWindow : public nsIWidget
{

public:
                            nsWindow();
    virtual                 ~nsWindow();

    NS_DECL_ISUPPORTS

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

    NS_IMETHOD              GetClientData(void*& aClientData);
    NS_IMETHOD              SetClientData(void* aClientData);
    virtual void            Destroy();
    virtual nsIWidget*      GetParent(void);
    virtual nsIEnumerator*  GetChildren();
    virtual void            AddChild(nsIWidget* aChild);
    virtual void            RemoveChild(nsIWidget* aChild);
    virtual void            Show(PRBool bState);
    virtual void            Move(PRUint32 aX, PRUint32 aY);
    virtual void            Resize(PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
    virtual void            Resize(PRUint32 aX, PRUint32 aY,PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
    virtual void            Enable(PRBool bState);
    virtual void            SetFocus(void);
    virtual void            GetBounds(nsRect &aRect);
    virtual void            SetBounds(const nsRect &aRect);
    virtual nscolor         GetForegroundColor(void);
    virtual void            SetForegroundColor(const nscolor &aColor);
    virtual nscolor         GetBackgroundColor(void);
    virtual void            SetBackgroundColor(const nscolor &aColor);
    virtual nsIFontMetrics* GetFont(void);
    virtual void            SetFont(const nsFont &aFont);
    virtual nsCursor        GetCursor();
    virtual void            SetCursor(nsCursor aCursor);
    virtual void            Invalidate(PRBool aIsSynchronous);
    virtual void*           GetNativeData(PRUint32 aDataType);
    virtual nsIRenderingContext* GetRenderingContext();
    virtual void            SetColorMap(nsColorMap *aColorMap);
    virtual nsIDeviceContext* GetDeviceContext();
    virtual nsIAppShell*    GetAppShell();
    virtual void            Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    virtual nsIToolkit*     GetToolkit();  
    virtual void            SetBorderStyle(nsBorderStyle aBorderStyle); 
    virtual void            SetTitle(const nsString& aTitle); 
    virtual void            SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);

    virtual void            RemoveTooltips();
    virtual void            UpdateTooltips(nsRect* aNewTips[]);
    virtual void            WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void            ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void            AddMouseListener(nsIMouseListener * aListener);
    virtual void            AddEventListener(nsIEventListener * aListener);
    virtual void            BeginResizingChildren(void);
    virtual void            EndResizingChildren(void);

    static  PRBool          ConvertStatus(nsEventStatus aStatus);
    virtual PRBool          DispatchEvent(nsGUIEvent* event);
    virtual PRBool          DispatchMouseEvent(nsMouseEvent &aEvent);

    virtual void            OnDestroy();
    virtual PRBool          OnPaint(nsPaintEvent &event);
    virtual PRBool          OnResize(nsSizeEvent &aEvent);
    virtual PRBool          OnKey(PRUint32 aEventType, PRUint32 aKeyCode, nsKeyEvent* aEvent);

    virtual PRBool          DispatchFocus(nsGUIEvent &aEvent);
    virtual PRBool          OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);

    virtual void            SetIgnoreResize(PRBool aIgnore);
    virtual PRBool          IgnoreResize();

    virtual PRUint32        GetYCoord(PRUint32 aNewY);
    
    virtual void  ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY);
    

     // Resize event management
    void SetResizeRect(nsRect& aRect);
    void SetResized(PRBool aResized);
    void GetResizeRect(nsRect* aRect);
    PRBool GetResized();    
    nsWindow* FindWidgetHit(Point aThePoint);
    virtual PtInWindow(PRInt32 aX,PRInt32 aY);
  
    // Mac specific methods
	  virtual void SetBounds(const Rect& aMacRect);
    void MacRectToNSRect(const Rect& aMacRect, nsRect& aRect) const;
    void nsRectToMacRect(const nsRect& aRect, Rect& aMacRect) const;
    void DoPaintWidgets(RgnHandle	aTheRegion);
    void DoResizeWidgets(nsSizeEvent &aEvent);
    PRBool RgnIntersects(RgnHandle aTheRegion,RgnHandle aIntersectRgn);

  	void LocalToWindowCoordinate(nsPoint& aPoint);
  	void LocalToWindowCoordinate(nsRect& aRect);
		void StringToStr255(const nsString& aText, Str255& aStr255);
		void Str255ToString(const Str255& aStr255, nsString& aText);

    char gInstanceClassName[256];
protected:

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


  virtual void      UpdateVisibilityFlag();
  virtual void      UpdateDisplay();
  virtual void			CalcOffset(PRInt32 &aX,PRInt32 &aY);


protected:
  EVENT_CALLBACK 		mEventCallback;
  nsIDeviceContext 	*mContext;
  nsIFontMetrics 		*mFontMetrics;
  nsToolkit					*mToolkit;
  nsIAppShell 			*mAppShell;
  nsIMouseListener 	*mMouseListener;
  nsIEventListener 	*mEventListener;

  nscolor     			mBackground;
  nscolor     			mForeground;
  nsCursor    			mCursor;
  nsBorderStyle 		mBorderStyle;
  nsRect      			mBounds;

  PRBool      			mIgnoreResize;
  PRBool      			mShown;
  PRBool     	 			mVisible;
  PRBool      			mDisplayed;

  void*       			mClientData;
  nsIWidget*				mParent;

  // Resize event management
  nsRect 						mResizeRect;
  PRUint32 					mResized;
  PRBool 						mLowerLeft;


	// keep the list of children
	class Enumerator
	{
	    nsIWidget  	**mChildrens;
	    PRInt32     mCurrentPosition;
	    PRInt32     mArraySize;
	    PRInt32			mNumChildren;

	public:
	    Enumerator();
	    ~Enumerator();

			nsIWidget* Previous();
	    nsIWidget* Next();
	    void Reset();
	    void ResetToLast();

	    void Append(nsIWidget* aWidget);
	    void Remove(nsIWidget* aWidget);

	private:
	    void GrowArray();

	} *mChildren;

// MAC SPECIFIC MEMBERS
protected:
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





#endif // Window_h__
