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

#ifndef nsIWidget_h__
#define nsIWidget_h__

#include "nsISupports.h"
#include "nsColor.h"
#include "nsIMouseListener.h"
#include "nsIImage.h"

#include "prthread.h"
#include "nsGUIEvent.h"

// forward declarations
class   nsIToolkit;
class   nsIFontMetrics;
class   nsIToolkit;
class   nsIRenderingContext;
class   nsIEnumerator;
class   nsIDeviceContext;
struct  nsRect;
struct  nsFont;

/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsWidget : public nsISupports {

  public:

    /**
     * Create and initialize a widget. 
     *
     * The widget represents a window that can be drawn into. It also is the 
     * base class for user-interface widgets such as buttons and text boxes.
     *
     * All the arguments can be NULL in which case a top level window
     * with size 0 is created. The event callback function has to be
     * provided only if the caller wants to deal with the events this
     * widget receives.  The event callback is basically a preprocess
     * hook called synchronously. The return value determines whether
     * the event goes to the default window procedure or it is hidden
     * to the os. The assumption is that if the event handler returns
     * false the widget does not see the event. The widget should not 
     * automatically clear the window to the background color. The 
     * calling code must handle paint messages and clear the background 
     * itself. 
     *
     * @param     parent or null if it's a top level window
     * @param     aRect     the widget dimension
     * @param     aHandleEventFunction the event handler callback function
     * @param     aInitData data that is used for widget initialization
     *
     */
    virtual void Create(nsIWidget        *aParent,
                        const nsRect     &aRect,
                        EVENT_CALLBACK   aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIToolkit       *aToolkit = nsnull,
                        void             *aInitData = nsnull);

    /**
     * Create and initialize a widget with a native window parent
     *
     * The widget represents a window that can be drawn into. It also is the 
     * base class for user-interface widgets such as buttons and text boxes.
     *
     * All the arguments can be NULL in which case a top level window
     * with size 0 is created. The event callback function has to be
     * provided only if the caller wants to deal with the events this
     * widget receives.  The event callback is basically a preprocess
     * hook called synchronously. The return value determines whether
     * the event goes to the default window procedure or it is hidden
     * to the os. The assumption is that if the event handler returns
     * false the widget does not see the event.
     *
     * @param     aParent   native window.
     * @param     aRect     the widget dimension
     * @param     aHandleEventFunction the event handler callback function
     */
    virtual void Create(nsNativeWindow aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIToolkit *aToolkit = nsnull,
                        void *aInitData = nsnull);

    virtual void Destroy(void);
    virtual nsIWidget* GetParent(void);
    virtual nsIEnumerator*  GetChildren(void);
    virtual void Show(PRBool aState);
    virtual void Move(PRUint32 aX, PRUint32 aY);
    virtual void Resize(PRUint32 aWidth,
                        PRUint32 aHeight);
    virtual void Resize(PRUint32 aX,
                        PRUint32 aY,
                        PRUint32 aWidth,
                        PRUint32 aHeight);

    virtual void     Enable(PRBool aState);
    virtual void     SetFocus(void);
    virtual void     GetBounds(nsRect &aRect);
    virtual nscolor  GetForegroundColor(void);
    virtual void     SetForegroundColor(const nscolor &aColor);
    virtual nscolor  GetBackgroundColor(void);
    virtual void     SetBackgroundColor(const nscolor &aColor);
    virtual nsIFontMetrics* GetFont(void);
    virtual void     SetFont(const nsFont &aFont);
    virtual nsCursor GetCursor(void);
    virtual void SetCursor(nsCursor aCursor);
    virtual void Invalidate(PRBool aIsSynchronous);
    virtual void AddMouseListener(nsIMouseListener * aListener);
    virtual nsIToolkit* GetToolkit();    
    virtual void SetColorMap(nsColorMap *aColorMap);
    virtual void Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);

    /** 
     * Internal methods
     */

    //@{
    virtual void AddChild(nsIWidget* aChild);
    virtual void RemoveChild(nsIWidget* aChild);
    virtual void* GetNativeData(PRUint32 aDataType);
    virtual nsIRenderingContext* GetRenderingContext();
    virtual nsIDeviceContext* GetDeviceContext();
    //@}

    virtual void SetBorderStyle(nsBorderStyle aBorderStyle);
    virtual void SetTitle(const nsString& aTitle);
    virtual void SetTooltips(PRUint32 aNumberOfTips,const nsRect* aTooltipAreas);
    virtual void UpdateTooltips(const nsRect* aNewTips);
    virtual void RemoveTooltips();
    virtual void WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    virtual void BeginResizingChildren(void);
    virtual void EndResizingChildren(void);

  protected:
    EVENT_CALLBACK mEventCallback;
    nsIDeviceContext *mContext;
    nsIFontMetrics   *mFontMetrics;
    nsIToolkit       *mToolkit;

    //nsIMouseListener * mMouseListener;
    //nsIEventListener * mEventListener;

    nscolor     mBackground;
    //HBRUSH      mBrush;
    nscolor     mForeground;
    nsCursor    mCursor;
    nsBorderStyle mBorderStyle;

    PRBool      mIsShiftDown;
    PRBool      mIsControlDown;
    PRBool      mIsAltDown;
    PRBool      mIsDestroying;

    // keep the list of children
    class Enumerator : public nsIEnumerator {
        nsIWidget   **mChildrens;
        int         mCurrentPosition;
        int         mArraySize;

    public:
        NS_DECL_ISUPPORTS

        Enumerator();
        ~Enumerator();

        NS_IMETHOD_(nsISupports*) Next();
        NS_IMETHOD_(void) Reset();

        void Append(nsIWidget* aWidget);
        void Remove(nsIWidget* aWidget);

    private:
        void GrowArray();

    } *mChildren;

};

#endif // nsIWidget_h__
