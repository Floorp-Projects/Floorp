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

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsString.h"

#include "Xm/Xm.h"

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))


/**
 * Native Motif window wrapper. 
 */

class nsWindow : public nsIWidget
{

public:
                            nsWindow(nsISupports *aOuter);
    virtual                 ~nsWindow();

    NS_DECL_ISUPPORTS


    NS_IMETHOD QueryObject(const nsIID& aIID, void** aInstancePtr);
    nsrefcnt AddRefObject(void);
    nsrefcnt ReleaseObject(void);

    // nsIWidget interface
    virtual void            Create(nsIWidget *aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
    virtual void            Create(nsNativeWindow aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
    virtual void            Destroy();
    virtual nsIWidget*      GetParent(void);
    virtual nsIEnumerator*  GetChildren();
    virtual void            AddChild(nsIWidget* aChild);
    virtual void            RemoveChild(nsIWidget* aChild);
    virtual void            Show(PRBool bState);
    virtual void            Move(PRUint32 aX, PRUint32 aY);
    virtual void            Resize(PRUint32 aWidth,
                                   PRUint32 aHeight, PRBool aRepaint);
    virtual void            Resize(PRUint32 aX,
                                   PRUint32 aY,
                                   PRUint32 aWidth,
                                   PRUint32 aHeight, PRBool aRepaint);
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
    virtual PRBool          DispatchMouseEvent(nsMouseEvent aEvent);

    virtual void            OnDestroy();
    virtual PRBool          OnPaint(nsPaintEvent &event);
    virtual PRBool          OnResize(nsSizeEvent &aEvent);
    virtual PRBool          OnKey(PRUint32 aEventType, PRUint32 aKeyCode);

    virtual PRBool          DispatchFocus(PRUint32 aEventType);
    virtual PRBool          OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos);

    virtual void            SetIgnoreResize(PRBool aIgnore);
    virtual PRBool          IgnoreResize();

    char gInstanceClassName[256];
protected:
  void InitCallbacks(char * aName = nsnull);
  void CreateWindow(nsNativeWindow aNativeParent, nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData);

  Widget mWidget;
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
  nsRect      mBounds;

  PRBool      mIgnoreResize;

  nsISupports* mOuter;

  class InnerSupport : public nsISupports {
  public:
      InnerSupport() {}

#define INNER_OUTER \
            ((nsWindow*)((char*)this - offsetof(nsWindow, mInner)))

      NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr)
                                { return INNER_OUTER->QueryObject(aIID, aInstancePtr); }
      NS_IMETHOD_(nsrefcnt) AddRef(void)
                                { return INNER_OUTER->AddRefObject(); }
      NS_IMETHOD_(nsrefcnt) Release(void)
                              { return INNER_OUTER->ReleaseObject(); }
  } mInner;
  friend InnerSupport;

};

//
// A child window is a window with different style
//
class ChildWindow : public nsWindow {

public:
                            ChildWindow(nsISupports *aOuter) : nsWindow(aOuter) {}

};

#define AGGRRGATE_METHOD_DEF \
public:                                                                     \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                  \
                            void** aInstancePtr);                           \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       \
  NS_IMETHOD_(nsrefcnt) Release(void);                                      \
protected:                                                                  \
  nsrefcnt mRefCnt;                                                         \
public: \
    virtual void            Create(nsIWidget *aParent, \
                                     const nsRect &aRect, \
                                     EVENT_CALLBACK aHandleEventFunction, \
                                     nsIDeviceContext *aContext, \
                                     nsIToolkit *aToolkit = nsnull, \
                                     nsWidgetInitData *aInitData = nsnull); \
    virtual void            Create(nsNativeWindow aParent, \
                                     const nsRect &aRect, \
                                     EVENT_CALLBACK aHandleEventFunction, \
                                     nsIDeviceContext *aContext, \
                                     nsIToolkit *aToolkit = nsnull, \
                                     nsWidgetInitData *aInitData = nsnull); \
    virtual void            Destroy(); \
    virtual nsIWidget*      GetParent(void); \
    virtual nsIEnumerator*  GetChildren(); \
    virtual void            AddChild(nsIWidget* aChild); \
    virtual void            RemoveChild(nsIWidget* aChild); \
    virtual void            Show(PRBool bState); \
    virtual void            Move(PRUint32 aX, PRUint32 aY); \
    virtual void            Resize(PRUint32 aWidth, \
                                   PRUint32 aHeight, PRBool aRepaint); \
    virtual void            Resize(PRUint32 aX, \
                                   PRUint32 aY, \
                                   PRUint32 aWidth, \
                                   PRUint32 aHeight, PRBool aRepaint); \
    virtual void            Enable(PRBool bState); \
    virtual void            SetFocus(void); \
    virtual void            GetBounds(nsRect &aRect); \
    virtual void            SetBounds(const nsRect &aRect); \
    virtual nscolor         GetForegroundColor(void); \
    virtual void            SetForegroundColor(const nscolor &aColor); \
    virtual nscolor         GetBackgroundColor(void); \
    virtual void            SetBackgroundColor(const nscolor &aColor); \
    virtual nsIFontMetrics* GetFont(void); \
    virtual void            SetFont(const nsFont &aFont); \
    virtual nsCursor        GetCursor(); \
    virtual void            SetCursor(nsCursor aCursor); \
    virtual void            Invalidate(PRBool aIsSynchronous); \
    virtual void*           GetNativeData(PRUint32 aDataType); \
    virtual nsIRenderingContext* GetRenderingContext(); \
    virtual void            SetColorMap(nsColorMap *aColorMap); \
    virtual nsIDeviceContext* GetDeviceContext(); \
    virtual void            Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect); \
    virtual nsIToolkit*     GetToolkit(); \
    virtual void            SetBorderStyle(nsBorderStyle aBorderStyle); \
    virtual void            SetTitle(const nsString& aTitle); \
    virtual void            SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]); \
    virtual void            RemoveTooltips(); \
    virtual void            UpdateTooltips(nsRect* aNewTips[]); \
    virtual void            WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect); \
    virtual void            ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect); \
    virtual void            AddMouseListener(nsIMouseListener * aListener); \
    virtual void            AddEventListener(nsIEventListener * aListener); \
    virtual void            BeginResizingChildren(void); \
    virtual void            EndResizingChildren(void); \
    virtual PRBool          DispatchEvent(nsGUIEvent* event); \
    virtual PRBool          DispatchMouseEvent(nsMouseEvent aEvent); \
    virtual void            OnDestroy(); \
    virtual PRBool          OnPaint(nsPaintEvent & event); \
    virtual PRBool          OnResize(nsSizeEvent &aEvent); \
    virtual PRBool          OnKey(PRUint32 aEventType, PRUint32 aKeyCode); \
    virtual PRBool          DispatchFocus(PRUint32 aEventType); \
    virtual PRBool          OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos); 


#define BASE_IWIDGET_IMPL_NO_SHOW(_classname, _aggname) \
    _classname::_aggname::_aggname() \
    { \
    } \
    _classname::_aggname::~_aggname() \
    { \
    } \
    nsrefcnt _classname::_aggname::AddRef() \
    { \
      return GET_OUTER()->AddRef(); \
    } \
    nsrefcnt _classname::_aggname::Release() \
    { \
      return GET_OUTER()->Release(); \
    } \
    nsresult _classname::_aggname::QueryInterface(REFNSIID aIID, void** aInstancePtr) \
    { \
      return GET_OUTER()->QueryInterface(aIID, aInstancePtr); \
    } \
    void  _classname::_aggname::Create(nsIWidget *aParent, \
                    const nsRect &aRect, \
                    EVENT_CALLBACK aHandleEventFunction, \
                    nsIDeviceContext *aContext, \
                    nsIToolkit *aToolkit, \
                    nsWidgetInitData *aInitData) \
    { \
        GET_OUTER()->Create(aParent, aRect, aHandleEventFunction, aContext, aToolkit, aInitData); \
    } \
    void _classname::_aggname::Create(nsNativeWindow aParent, \
                 const nsRect &aRect, \
                 EVENT_CALLBACK aHandleEventFunction, \
                 nsIDeviceContext *aContext, \
                 nsIToolkit *aToolkit, \
                 nsWidgetInitData *aInitData) \
    { \
        GET_OUTER()->Create(aParent, aRect, aHandleEventFunction, aContext, aToolkit, aInitData); \
    } \
    void _classname::_aggname::Destroy() \
    { \
        GET_OUTER()->Destroy(); \
    } \
    nsIWidget* _classname::_aggname::GetParent(void) \
    { \
        return GET_OUTER()->GetParent(); \
    } \
    nsIEnumerator* _classname::_aggname::GetChildren() \
    { \
        return GET_OUTER()->GetChildren(); \
    } \
    void _classname::_aggname::AddChild(nsIWidget* aChild) \
    { \
        GET_OUTER()->AddChild(aChild); \
    } \
    void _classname::_aggname::RemoveChild(nsIWidget* aChild) \
    { \
        GET_OUTER()->RemoveChild(aChild); \
    } \
    void _classname::_aggname::Move(PRUint32 aX, PRUint32 aY) \
    { \
        GET_OUTER()->Move(aX, aY); \
    } \
    void _classname::_aggname::Resize(PRUint32 aWidth, \
                PRUint32 aHeight, PRBool aRepaint) \
    { \
        GET_OUTER()->Resize(aWidth, aHeight, aRepaint); \
    } \
    void _classname::_aggname::Resize(PRUint32 aX, \
                PRUint32 aY, \
                PRUint32 aWidth, \
                PRUint32 aHeight, PRBool aRepaint) \
    { \
        GET_OUTER()->Resize(aX, aY, aWidth, aHeight, aRepaint); \
    } \
    void _classname::_aggname::Enable(PRBool bState) \
    { \
        GET_OUTER()->Enable(bState); \
    } \
    void _classname::_aggname::SetFocus(void) \
    { \
        GET_OUTER()->SetFocus(); \
    } \
    void _classname::_aggname::GetBounds(nsRect &aRect) \
    { \
        GET_OUTER()->GetBounds(aRect); \
    } \
    void _classname::_aggname::SetBounds(const nsRect &aRect) \
    { \
        GET_OUTER()->SetBounds(aRect); \
    } \
    nscolor _classname::_aggname::GetForegroundColor(void) \
    { \
        return GET_OUTER()->GetForegroundColor(); \
    } \
    void _classname::_aggname::SetForegroundColor(const nscolor &aColor) \
    { \
        GET_OUTER()->SetForegroundColor(aColor); \
    } \
    nscolor _classname::_aggname::GetBackgroundColor(void) \
    { \
        return GET_OUTER()->GetBackgroundColor(); \
    } \
    void _classname::_aggname::SetBackgroundColor(const nscolor &aColor) \
    { \
        GET_OUTER()->SetBackgroundColor(aColor); \
    } \
    nsIFontMetrics* _classname::_aggname::GetFont(void) \
    { \
        return GET_OUTER()->GetFont(); \
    } \
    void _classname::_aggname::SetFont(const nsFont &aFont) \
    { \
        GET_OUTER()->SetFont(aFont); \
    } \
    nsCursor _classname::_aggname::GetCursor() \
    { \
        return GET_OUTER()->GetCursor(); \
    } \
    void _classname::_aggname::SetCursor(nsCursor aCursor) \
    { \
        GET_OUTER()->SetCursor(aCursor); \
    } \
    void _classname::_aggname::Invalidate(PRBool aIsSynchronous) \
    { \
        GET_OUTER()->Invalidate(aIsSynchronous); \
    } \
    void* _classname::_aggname::GetNativeData(PRUint32 aDataType) \
    { \
        return GET_OUTER()->GetNativeData(aDataType); \
    } \
    nsIRenderingContext* _classname::_aggname::GetRenderingContext() \
    { \
        return GET_OUTER()->GetRenderingContext(); \
    } \
    nsIDeviceContext* _classname::_aggname::GetDeviceContext() \
    { \
        return GET_OUTER()->GetDeviceContext(); \
    } \
    void _classname::_aggname::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) \
    { \
        GET_OUTER()->Scroll(aDx, aDy, aClipRect); \
    } \
    nsIToolkit* _classname::_aggname::GetToolkit() \
    { \
        return GET_OUTER()->GetToolkit(); \
    } \
    void _classname::_aggname::SetColorMap(nsColorMap *aColorMap) \
    { \
        GET_OUTER()->SetColorMap(aColorMap); \
    } \
    void _classname::_aggname::AddMouseListener(nsIMouseListener * aListener) \
    { \
       GET_OUTER()->AddMouseListener(aListener); \
    } \
    void _classname::_aggname::AddEventListener(nsIEventListener * aListener) \
    { \
       GET_OUTER()->AddEventListener(aListener); \
    } \
    void _classname::_aggname::SetBorderStyle(nsBorderStyle aBorderStyle) \
    { \
      GET_OUTER()->SetBorderStyle(aBorderStyle); \
    } \
    void _classname::_aggname::SetTitle(const nsString& aTitle) \
    { \
      GET_OUTER()->SetTitle(aTitle); \
    } \
    void _classname::_aggname::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]) \
    { \
      GET_OUTER()->SetTooltips(aNumberOfTips, aTooltipAreas); \
    } \
    void _classname::_aggname::UpdateTooltips(nsRect* aNewTips[]) \
    { \
      GET_OUTER()->UpdateTooltips(aNewTips); \
    } \
    void _classname::_aggname::RemoveTooltips() \
    { \
      GET_OUTER()->RemoveTooltips(); \
    } \
    void _classname::_aggname::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect) \
    { \
      GET_OUTER()->WidgetToScreen(aOldRect, aNewRect); \
    } \
    void _classname::_aggname::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect) \
    { \
      GET_OUTER()->ScreenToWidget(aOldRect, aNewRect); \
    } \
    PRBool _classname::_aggname::DispatchEvent(nsGUIEvent* event) \
    { \
      return GET_OUTER()->DispatchEvent(event); \
    } \
    PRBool _classname::_aggname::DispatchMouseEvent(nsMouseEvent event) \
    { \
      return GET_OUTER()->DispatchMouseEvent(event); \
    } \
    PRBool _classname::_aggname::OnPaint(nsPaintEvent &event) \
    { \
      return GET_OUTER()->OnPaint(event); \
    } \
    void _classname::_aggname::BeginResizingChildren() \
    { \
      GET_OUTER()->BeginResizingChildren(); \
    } \
    void _classname::_aggname::EndResizingChildren() \
    { \
      GET_OUTER()->EndResizingChildren(); \
    } \
    void _classname::_aggname::OnDestroy() \
    { \
      GET_OUTER()->OnDestroy(); \
    } \
    PRBool _classname::_aggname::OnResize(nsSizeEvent &aEvent) \
    { \
      GET_OUTER()->OnResize(aEvent); \
    } \
    PRBool _classname::_aggname::OnKey(PRUint32 aEventType, PRUint32 aKeyCode) \
    { \
      return GET_OUTER()->OnKey(aEventType, aKeyCode); \
    } \
    PRBool _classname::_aggname::DispatchFocus(PRUint32 aEventType) \
    { \
      return GET_OUTER()->DispatchFocus(aEventType); \
    } \
    PRBool _classname::_aggname::OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos) \
    { \
      return GET_OUTER()->OnScroll(aEvent, cPos); \
    }

#define BASE_IWIDGET_IMPL_SHOW(_classname, _aggname) \
    void _classname::_aggname::Show(PRBool bState) \
    { \
        GET_OUTER()->Show(bState); \
    }

#define BASE_IWIDGET_IMPL(_classname, _aggname) \
  BASE_IWIDGET_IMPL_NO_SHOW(_classname, _aggname)  \
  BASE_IWIDGET_IMPL_SHOW(_classname, _aggname)  



#endif // Window_h__
