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

#include "nsToolkit.h"

#include "nsIWidget.h"
#include "nsIEnumerator.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsString.h"

#include "Xm/Xm.h"

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))


/**
 * Native WIN32 window wrapper. 
 */

class nsWindow : public nsIWidget
{

public:
                            nsWindow(nsISupports *aOuter);
    virtual                 ~nsWindow();

    NS_DECL_ISUPPORTS

    // nsIWidget interface
    virtual void            Create(nsIWidget *aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIToolkit *aToolkit = nsnull,
                                     void *aInitData = nsnull);
    virtual void            Create(nsNativeWindow aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIToolkit *aToolkit = nsnull,
                                     void *aInitData = nsnull);
    virtual void            Destroy();
    virtual nsIWidget*      GetParent(void);
    virtual nsIEnumerator*  GetChildren();
    virtual void            AddChild(nsIWidget* aChild);
    virtual void            RemoveChild(nsIWidget* aChild);
    virtual void            Show(PRBool bState);
    virtual void            Move(PRUint32 aX, PRUint32 aY);
    virtual void            Resize(PRUint32 aWidth,
                                   PRUint32 aHeight);
    virtual void            Resize(PRUint32 aX,
                                   PRUint32 aY,
                                   PRUint32 aWidth,
                                   PRUint32 aHeight);
    virtual void            Enable(PRBool bState);
    virtual void            SetFocus(void);
    virtual void            GetBounds(nsRect &aRect);
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
    virtual void            Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    virtual nsIToolkit*     GetToolkit();  
    virtual void            SetBorderStyle(nsBorderStyle aBorderStyle); 
    virtual void            SetTitle(const nsString& aTitle); 
    virtual void            SetTooltips(PRUint32 aNumberOfTips,const nsRect* aTooltipAreas);
    virtual void            RemoveTooltips();
    virtual void            UpdateTooltips(const nsRect* aNewTips);
    virtual void            WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void            ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void            AddMouseListener(nsIMouseListener * aListener);
    virtual void            AddEventListener(nsIEventListener * aListener);
    virtual void            OnPaint(nsPaintEvent &event);
    PRBool DispatchEvent(nsGUIEvent* event);
    static PRBool ConvertStatus(nsEventStatus aStatus);

private:
  Widget mWidget;
  GC mGC ;
  EVENT_CALLBACK mEventCallback;
  nsIDeviceContext *mContext;
  nsIFontMetrics *mFontMetrics;
  nsToolkit   *mToolkit;

  nsIMouseListener * mMouseListener;
  nsIEventListener * mEventListener;

  nscolor     mBackground;
  nscolor     mForeground;
  nsCursor    mCursor;
  nsBorderStyle mBorderStyle;

};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
                            ChildWindow(nsISupports *aOuter) : nsWindow(aOuter) {}

};

#define BASE_IWIDGET_IMPL  BASE_INITIALIZE_IMPL BASE_WINDOWS_METHODS


#define BASE_INITIALIZE_IMPL \
    void  Create(nsIWidget *aParent, \
                    const nsRect &aRect, \
                    EVENT_CALLBACK aHandleEventFunction, \
                    nsIDeviceContext *aContext, \
                    nsIToolkit *aToolkit = nsnull, \
                    void *aInitData = nsnull) \
    { \
        nsWindow::Create(aParent, aRect, aHandleEventFunction, aContext, aToolkit, aInitData); \
    } \

#define BASE_WINDOWS_METHODS \
    void Create(nsNativeWindow aParent, \
                 const nsRect &aRect, \
                 EVENT_CALLBACK aHandleEventFunction, \
                 nsIDeviceContext *aContext, \
                 nsIToolkit *aToolkit = nsnull, \
                 void *aInitData = nsnull) \
    { \
        nsWindow::Create(aParent, aRect, aHandleEventFunction, aContext, aToolkit, aInitData); \
    } \
    void Destroy(void) \
    { \
        nsWindow::Destroy(); \
    } \
    nsIWidget* GetParent(void) \
    { \
        return nsWindow::GetParent(); \
    } \
    nsIEnumerator* GetChildren(void) \
    { \
        return nsWindow::GetChildren(); \
    } \
    void AddChild(nsIWidget* aChild) \
    { \
        nsWindow::AddChild(aChild); \
    } \
    void RemoveChild(nsIWidget* aChild) \
    { \
        nsWindow::RemoveChild(aChild); \
    } \
    void Show(PRBool bState) \
    { \
        nsWindow::Show(bState); \
    } \
    void Move(PRUint32 aX, PRUint32 aY) \
    { \
        nsWindow::Move(aX, aY); \
    } \
    void Resize(PRUint32 aWidth, \
                PRUint32 aHeight) \
    { \
        nsWindow::Resize(aWidth, aHeight); \
    } \
    void Resize(PRUint32 aX, \
                PRUint32 aY, \
                PRUint32 aWidth, \
                PRUint32 aHeight) \
    { \
        nsWindow::Resize(aX, aY, aWidth, aHeight); \
    } \
    void Enable(PRBool bState) \
    { \
        nsWindow::Enable(bState); \
    } \
    void SetFocus(void) \
    { \
        nsWindow::SetFocus(); \
    } \
    void GetBounds(nsRect &aRect) \
    { \
        nsWindow::GetBounds(aRect); \
    } \
    nscolor GetForegroundColor(void) \
    { \
        return nsWindow::GetForegroundColor(); \
    } \
    void SetForegroundColor(const nscolor &aColor) \
    { \
        nsWindow::SetForegroundColor(aColor); \
    } \
    nscolor GetBackgroundColor(void) \
    { \
        return nsWindow::GetBackgroundColor(); \
    } \
    void SetBackgroundColor(const nscolor &aColor) \
    { \
        nsWindow::SetBackgroundColor(aColor); \
    } \
    nsIFontMetrics* GetFont(void) \
    { \
        return nsWindow::GetFont(); \
    } \
    void SetFont(const nsFont &aFont) \
    { \
        nsWindow::SetFont(aFont); \
    } \
    nsCursor GetCursor() \
    { \
        return nsWindow::GetCursor(); \
    } \
    void SetCursor(nsCursor aCursor) \
    { \
        nsWindow::SetCursor(aCursor); \
    } \
    void Invalidate(PRBool aIsSynchronous) \
    { \
        nsWindow::Invalidate(aIsSynchronous); \
    } \
    void* GetNativeData(PRUint32 aDataType) \
    { \
        return nsWindow::GetNativeData(aDataType); \
    } \
    nsIRenderingContext* GetRenderingContext() \
    { \
        return nsWindow::GetRenderingContext(); \
    } \
    nsIDeviceContext* GetDeviceContext() \
    { \
        return nsWindow::GetDeviceContext(); \
    } \
    void Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) \
    { \
        nsWindow::Scroll(aDx, aDy, aClipRect); \
    } \
    nsIToolkit* GetToolkit() \
    { \
        return nsWindow::GetToolkit(); \
    } \
    void SetColorMap(nsColorMap *aColorMap) \
    { \
        nsWindow::SetColorMap(aColorMap); \
    } \
    void AddMouseListener(nsIMouseListener * aListener) \
    { \
       nsWindow::AddMouseListener(aListener); \
    } \
    void AddEventListener(nsIEventListener * aListener) \
    { \
       nsWindow::AddEventListener(aListener); \
    } \
    PRBool OnKey(PRUint32 aEventType, PRUint32 aKeyCode) \
    { \
      return nsWindow::OnKey(aEventType, aKeyCode); \
    } \
    void SetBorderStyle(nsBorderStyle aBorderStyle) \
    { \
      nsWindow::SetBorderStyle(aBorderStyle); \
    } \
    void SetTitle(const nsString& aTitle) \
    { \
      nsWindow::SetTitle(aTitle); \
    } \
    void SetTooltips(PRUint32 aNumberOfTips,const nsRect* aTooltipAreas) \
    { \
      nsWindow::SetTooltips(aNumberOfTips, aTooltipAreas); \
    } \
    void UpdateTooltips(const nsRect* aNewTips) \
    { \
      nsWindow::UpdateTooltips(aNewTips); \
    } \
    void RemoveTooltips() \
    { \
      nsWindow::RemoveTooltips(); \
    } \
    void WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect) \
    { \
      nsWindow::WidgetToScreen(aOldRect, aNewRect); \
    } \
    void ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect) \
    { \
      nsWindow::ScreenToWidget(aOldRect, aNewRect); \
    } 



#endif // Window_h__
