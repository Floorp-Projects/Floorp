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
#include "nsToolkit.h"
#include "nsIAppShell.h"

#include <Xm/Xm.h>

class nsFont;

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))


/**
 * Native Motif window wrapper. 
 */

class nsWindow : public nsBaseWidget
{

public:
      // nsIWidget interface

    nsWindow();
    virtual ~nsWindow();

    // nsISupports
    NS_DECL_ISUPPORTS

    virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

    NS_IMETHOD            Create(nsIWidget *aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD            Create(nsNativeWidget aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD              GetClientData(void*& aClientData);
    NS_IMETHOD              SetClientData(void* aClientData);
    NS_IMETHOD              Destroy();
    virtual nsIWidget*      GetParent(void);
    virtual nsIEnumerator*  GetChildren(void);
    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD              SetModal(void);
    NS_IMETHOD              IsVisible(PRBool & aState);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    virtual void            AddChild(nsIWidget* aChild);
    virtual void            RemoveChild(nsIWidget* aChild);

    NS_IMETHOD            Resize(PRInt32 aWidth,
                                   PRInt32 aHeight,
                                   PRBool   aRepaint);
    NS_IMETHOD            Resize(PRInt32 aX,
                                   PRInt32 aY,
                                   PRInt32 aWidth,
                                   PRInt32 aHeight,
                                   PRBool   aRepaint);
    NS_IMETHOD            Enable(PRBool bState);
    NS_IMETHOD            SetFocus(void);
    NS_IMETHOD            GetBounds(nsRect &aRect);
    virtual nscolor         GetForegroundColor(void);
    NS_IMETHOD            SetForegroundColor(const nscolor &aColor);
    virtual nscolor         GetBackgroundColor(void);
    NS_IMETHOD            SetBackgroundColor(const nscolor &aColor);
    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD            SetFont(const nsFont &aFont);
    virtual nsCursor        GetCursor();
    NS_IMETHOD            SetCursor(nsCursor aCursor);
    NS_IMETHOD            Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD            Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
    NS_IMETHOD            Update();
    virtual void*           GetNativeData(PRUint32 aDataType);
    virtual void FreeNativeData(void * data, PRUint32 aDataType);//~~~
    virtual nsIRenderingContext* GetRenderingContext();
    NS_IMETHOD            SetColorMap(nsColorMap *aColorMap);
    virtual nsIDeviceContext* GetDeviceContext();
    virtual nsIAppShell *   GetAppShell();
    NS_IMETHOD            Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    virtual nsIToolkit*     GetToolkit();  
    NS_IMETHOD            SetBorderStyle(nsBorderStyle aBorderStyle); 
    NS_IMETHOD            SetTitle(const nsString& aTitle); 
    NS_IMETHOD            SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);   
    NS_IMETHOD            RemoveTooltips();
    NS_IMETHOD            UpdateTooltips(nsRect* aNewTips[]);
    NS_IMETHOD            WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD            ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD            AddMouseListener(nsIMouseListener * aListener);
    NS_IMETHOD            AddEventListener(nsIEventListener * aListener);
    NS_IMETHOD            AddMenuListener(nsIMenuListener * aListener);
    NS_IMETHOD            BeginResizingChildren(void);
    NS_IMETHOD            EndResizingChildren(void);
    NS_IMETHOD            SetMenuBar(nsIMenuBar * aMenuBar);
    NS_IMETHOD            ShowMenuBar(PRBool aShow);
    NS_IMETHOD            GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD            SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
    NS_IMETHOD            DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
    NS_IMETHOD            EnableFileDrop(PRBool aEnable);
    NS_IMETHOD            CaptureMouse(PRBool aCapture);

#ifdef DEBUG
  void                    DebugPrintEvent(nsGUIEvent & aEvent,Widget aWidget);
#endif

    virtual PRBool IsChild() { return(PR_FALSE); };

     // Utility methods
    NS_IMETHOD              SetBounds(const nsRect &aRect);
    PRBool   ConvertStatus(nsEventStatus aStatus);
    virtual  PRBool OnPaint(nsPaintEvent &event);
    virtual  void   OnDestroy();
    PRBool   OnKey(PRUint32 aEventType, PRUint32 aKeyCode, nsKeyEvent* aEvent);
    PRBool   DispatchFocus(nsGUIEvent &aEvent);
    virtual  PRBool OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);
    PRUint32 GetYCoord(PRUint32 aNewY);
    PRBool   DispatchMouseEvent(nsMouseEvent& aEvent);
    virtual  PRBool OnResize(nsSizeEvent &aEvent);
   
     // Resize event management
    void   SetResizeRect(nsRect& aRect);
    void   SetResized(PRBool aResized);
    void   GetResizeRect(nsRect* aRect);
    PRBool GetResized();

    char gInstanceClassName[256];
protected:
  void   InitCallbacks(char * aName = nsnull);
  PRBool DispatchWindowEvent(nsGUIEvent* event);


  void CreateGC();
  void CreateWindow(nsNativeWidget aNativeParent, nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData);

  void InitToolkit(nsIToolkit *aToolkit, nsIWidget * aWidgetParent);
  void InitDeviceContext(nsIDeviceContext *aContext, Widget aWidgetParent);

  virtual void            UpdateDisplay();

public:
  Widget mWidget;
  XtAppContext mAppContext;

protected:
  EVENT_CALLBACK mEventCallback;
  nsIDeviceContext *mContext;
  nsIFontMetrics *mFontMetrics;
  nsToolkit   *mToolkit;
  nsIAppShell *mAppShell;
  nsIMouseListener * mMouseListener;
  nsIEventListener * mEventListener;
  nscolor     mBackground;
  nscolor     mForeground;
  unsigned long  mBackgroundPixel;
  unsigned long  mForegroundPixel;
  nsCursor    mCursor;
  nsBorderStyle mBorderStyle;
  nsRect      mBounds;
  PRBool      mIgnoreResize;
  PRBool      mShown;
  PRBool      mVisible;
  PRBool      mDisplayed;
  void*       mClientData;

  // XXX Temporary, should not be caching the font
  nsFont *    mFont;
  PRInt32     mPreferredWidth;
  PRInt32     mPreferredHeight;

  // Resize event management
  nsRect mResizeRect;
  int mResized;
  PRBool mLowerLeft;

private:
  GC mGC;
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
