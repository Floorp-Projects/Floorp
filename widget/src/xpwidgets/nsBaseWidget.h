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
#ifndef nsBaseWidget_h__
#define nsBaseWidget_h__

#include "nsRect.h"
#include "nsIWidget.h"
#include "nsIEnumerator.h"
#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsIMenuListener.h"
#include "nsIToolkit.h"
#include "nsStringUtil.h"
#include "nsString.h"
#include "nsVoidArray.h"

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

/**
 * Common widget implementation used as base class for native
 * or crossplatform implementations of Widgets. 
 * All cross-platform behavior that all widgets need to implement 
 * should be placed in this class. 
 * (Note: widget implementations are not required to use this
 * class, but it gives them a head start.)
 */

class nsBaseWidget : public nsIWidget
{

public:
    nsBaseWidget();
    virtual ~nsBaseWidget();

    NS_DECL_ISUPPORTS

    NS_IMETHOD              PreCreateWidget(nsWidgetInitData *aWidgetInitData) { return NS_OK;}

      // nsIWidget interface
    NS_IMETHOD              GetClientData(void*& aClientData);
    NS_IMETHOD              SetClientData(void* aClientData);
    NS_IMETHOD              Destroy();
    virtual nsIWidget*      GetParent(void);
    virtual nsIEnumerator*  GetChildren();
    virtual void            AddChild(nsIWidget* aChild);
    virtual void            RemoveChild(nsIWidget* aChild);


    virtual nscolor         GetForegroundColor(void);
    NS_IMETHOD              SetForegroundColor(const nscolor &aColor);
    virtual nscolor         GetBackgroundColor(void);
    NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
    virtual nsCursor        GetCursor();
    NS_IMETHOD              SetCursor(nsCursor aCursor);
    virtual nsIRenderingContext* GetRenderingContext();
    virtual nsIDeviceContext* GetDeviceContext();
    virtual nsIAppShell *   GetAppShell();
    virtual nsIToolkit*     GetToolkit();  
    NS_IMETHOD              SetModal(void); 
    NS_IMETHOD              SetBorderStyle(nsBorderStyle aBorderStyle); 
    NS_IMETHOD              SetTitle(const nsString& aTitle); 
    NS_IMETHOD              SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);   
    NS_IMETHOD              RemoveTooltips();
    NS_IMETHOD              UpdateTooltips(nsRect* aNewTips[]);
    NS_IMETHOD              AddMouseListener(nsIMouseListener * aListener);
    NS_IMETHOD              AddEventListener(nsIEventListener * aListener);
    NS_IMETHOD 				AddMenuListener(nsIMenuListener * aListener);
    NS_IMETHOD              SetBounds(const nsRect &aRect);
    NS_IMETHOD              GetBounds(nsRect &aRect);
    NS_IMETHOD              GetBoundsAppUnits(nsRect &aRect, float aAppUnits);
    NS_IMETHOD              GetClientBounds(nsRect &aRect);
    NS_IMETHOD              GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight);
    NS_IMETHOD              Paint(nsIRenderingContext& aRenderingContext, const nsRect& aDirtyRect);
    NS_IMETHOD              SetVerticalScrollbar(nsIWidget * aScrollbar);
    NS_IMETHOD              EnableFileDrop(PRBool aEnable);
    virtual void            ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY) {}
protected:

    virtual void            DrawScaledRect(nsIRenderingContext& aRenderingContext, const nsRect & aRect, float aScale, float aAppUnits);
    virtual void            DrawScaledLine(nsIRenderingContext& aRenderingContext, 
                                           nscoord aSX, nscoord aSY, nscoord aEX, nscoord aEY, 
                                           float   aScale, float aAppUnits, PRBool aIsHorz);
    virtual void            OnDestroy();
    virtual void            BaseCreate(nsIWidget *aParent,
                            const nsRect &aRect,
                            EVENT_CALLBACK aHandleEventFunction,
                            nsIDeviceContext *aContext,
                            nsIAppShell *aAppShell,
                            nsIToolkit *aToolkit,
                            nsWidgetInitData *aInitData);

protected: 
    void*             mClientData;
    EVENT_CALLBACK    mEventCallback;
    nsIDeviceContext  *mContext;
    nsIAppShell       *mAppShell;
    nsIToolkit        *mToolkit;
    nsIMouseListener  *mMouseListener;
    nsIEventListener  *mEventListener;
    nsIMenuListener   *mMenuListener;
    nscolor           mBackground;
    nscolor           mForeground;
    nsCursor          mCursor;
    nsBorderStyle     mBorderStyle;
    PRBool            mIsShiftDown;
    PRBool            mIsControlDown;
    PRBool            mIsAltDown;
    PRBool            mIsDestroying;
    PRBool            mOnDestroyCalled;
    //PRInt32           mWidth;
    //PRInt32           mHeight;
    nsRect            mBounds;
    nsIWidget        *mVScrollbar;

    // keep the list of children
    class Enumerator : public nsIBidirectionalEnumerator {
    public:
      NS_DECL_ISUPPORTS

      Enumerator();
      virtual ~Enumerator();

      NS_IMETHOD First();
      NS_IMETHOD Last();
      NS_IMETHOD Next();
      NS_IMETHOD Prev();
      NS_IMETHOD CurrentItem(nsISupports **aItem);
      NS_IMETHOD IsDone();

      void Append(nsIWidget* aWidget);
      void Remove(nsIWidget* aWidget);

    private:
      nsVoidArray   mChildren;
      PRInt32       mCurrentPosition;
    } *mChildren;

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
    
};

#endif // nsBaseWidget_h__
