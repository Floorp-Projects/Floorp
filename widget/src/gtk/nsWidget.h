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

#ifndef nsWidget_h__
#define nsWidget_h__

#include "nsIWidget.h"
#include "nsToolkit.h"
#include "nsIAppShell.h"
#include <gtk/gtk.h>

/**
 * Base of all GTK native widgets.
 */

class nsWidget : public nsIWidget
{
 public:
    nsWidget();
    virtual ~nsWidget();

    NS_DECL_ISUPPORTS

    nsIWidget* GetParent(void);
    nsIEnumerator* GetChildren(void);

    NS_IMETHOD Show(PRBool state);
    NS_IMETHOD IsVisible(PRBool &aState);

    NS_IMETHOD Move(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth,
		      PRUint32 aHeight, PRBool aRepaint);

    NS_IMETHOD Enable(PRBool aState);
    NS_IMETHOD SetFocus(void);

    NS_IMETHOD GetBounds(nsRect &aRect);
    NS_IMETHOD GetClientBounds(nsRect &aRect);
    NS_IMETHOD GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight);

    nscolor GetForegroundColor(void);
    NS_IMETHOD SetForegroundColor(const nscolor &aColor);
    nscolor GetBackgroundColor(void);
    NS_IMETHOD SetBackgroundColor(const nscolor &aColor);

    nsIFontMetrics *GetFont(void);
    NS_IMETHOD SetFont(const nsFont &aFont);

    nsCursor GetCursor(void);
    NS_IMETHOD SetCursor(nsCursor aCursor);

    NS_IMETHOD Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
    NS_IMETHOD Update(void);

    NS_IMETHOD AddMouseListener(nsIMouseListener *aListener);
    nsIToolkit *GetToolkit(void);
    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);

    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);

    void AddChild(nsIWidget *aChild);
    void RemoveChild(nsIWidget *aChild);
    void* GetNativeData(PRUint32 aDataType);
    nsIRenderingContext *GetRenderingContext(void);
    nsIDeviceContext *GetDeviceContext(void);
    nsIAppShell *GetAppShell(void);

    NS_IMETHOD SetBorderStyle(nsBorderStyle aBorderStyle);
    NS_IMETHOD SetTitle(const nsString & aTitle);
    NS_IMETHOD SetMenuBar(nsIMenuBar *aMenuBar);

    NS_IMETHOD SetTooltips(PRUint32 aNumberOfTips, nsRect* aTooltipAreas[]);
    NS_IMETHOD UpdateTooltips(nsRect *aNewTips[]);
    NS_IMETHOD RemoveTooltips(void);

    NS_IMETHOD WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);
    NS_IMETHOD ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect);

    NS_IMETHOD BeginResizingChildren(void);
    NS_IMETHOD EndResizingChildren(void);

    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

    NS_IMETHOD GetClientData(void*& aClientData);
    NS_IMETHOD SetClientData(void* aClientData);

    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus &aStatus);
    NS_IMETHOD DispatchMouseEvent(nsMouseEvent& aEvent);
    virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

 protected:
    void InitToolkit(nsIToolkit *aToolKit, nsIWidget *aParent);
    void InitDeviceContext(nsIDeviceContext *aContext,
			   GtkWidget *aParentWidget);
    void InitCallbacks(char * aName = nsnull);

    GtkWidget *mWidget;
    nsWidget *mParent;
    nsToolkit *mToolkit;
    nsIDeviceContext *mDeviceContext;
    nsIRenderingContext *mRenderingContext;
    nsIAppShell *mAppShell;
    PRBool mShown;
    nscolor mForeground, mBackground;
    nsIMouseListener *mListener;
    PRUint32 mPreferredWidth, mPreferredHeight;
    nsRect mBounds;
    void *mClientData;
    nsCursor mCursor;
    GdkGC *mGC;
    EVENT_CALLBACK mEventCallback;
};

#endif /* nsWidget_h__ */
