/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *		John C. Griggs <johng@corel.com>
 *
 */
#ifndef nsWidget_h__
#define nsWidget_h__

#include "nsBaseWidget.h"
#include "nsWeakReference.h"
#include "nsIKBStateControl.h"
#include "nsWidgetsCID.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsLookAndFeel.h"
#include "nsQWidget.h"

class nsIAppShell;
class nsIToolkit;

class nsWidget : public nsBaseWidget, public nsIKBStateControl, public nsSupportsWeakReference
{
public:
    nsWidget();
    virtual ~nsWidget();

    NS_DECL_ISUPPORTS_INHERITED
  
    NS_IMETHOD Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell = nsnull,
                      nsIToolkit *aToolkit = nsnull,
                      nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell = nsnull,
                      nsIToolkit *aToolkit = nsnull,
                      nsWidgetInitData *aInitData = nsnull);
  
    NS_IMETHOD Destroy(void);
    nsIWidget* GetParent(void);

    NS_IMETHOD SetModal(PRBool aModal);
    NS_IMETHOD Show(PRBool state);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener, 
                                   PRBool aDoCapture, 
                                   PRBool aConsumeRollupEvent);
    NS_IMETHOD IsVisible(PRBool &aState);

    NS_IMETHOD ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, 
                      PRBool aRepaint);

    NS_IMETHOD Enable(PRBool aState);
    NS_IMETHOD SetFocus(PRBool aRaise);

    PRBool OnResize(nsSizeEvent event);
    virtual PRBool OnResize(nsRect &aRect);
    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);

    nsIFontMetrics *GetFont(void);
    NS_IMETHOD SetFont(const nsFont &aFont);

    NS_IMETHOD SetBackgroundColor(const nscolor &aColor);

    NS_IMETHOD SetCursor(nsCursor aCursor);

    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);

    void* GetNativeData(PRUint32 aDataType);

    NS_IMETHOD WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);
    NS_IMETHOD ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect);

    NS_IMETHOD BeginResizingChildren(void);
    NS_IMETHOD EndResizingChildren(void);

    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

    NS_IMETHOD SetTitle(const nsString& aTitle);

    virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

    // the following are nsWindow specific, and just stubbed here
    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    NS_IMETHOD SetMenuBar(nsIMenuBar *aMenuBar) { return NS_ERROR_FAILURE; }
    NS_IMETHOD ShowMenuBar(PRBool aShow) { return NS_ERROR_FAILURE; }
    NS_IMETHOD CaptureMouse(PRBool aCapture) { return NS_ERROR_FAILURE; }
    virtual PRBool OnPaint(nsPaintEvent &event) { return PR_FALSE; };
    virtual PRBool OnKey(nsKeyEvent &aEvent) { return PR_FALSE; };
    virtual PRBool DispatchFocus(nsGUIEvent &aEvent) { return PR_FALSE; };

    NS_IMETHOD Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
    NS_IMETHOD InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
    NS_IMETHOD Update(void);
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);

    // nsIKBStateControl
    NS_IMETHOD ResetInputState();
    NS_IMETHOD PasswordFieldInit();

    void InitEvent(nsGUIEvent& event,PRUint32 aEventType,nsPoint* aPoint = nsnull);

    // Utility functions
    const char *GetName();
    PRBool ConvertStatus(nsEventStatus aStatus);
    PRBool DispatchMouseEvent(nsMouseEvent& aEvent);
    PRBool DispatchStandardEvent(PRUint32 aMsg);

    virtual PRBool IsPopup() const { return PR_FALSE; };
    virtual PRBool IsDialog() const { return PR_FALSE; };

    // Deal with rollup for popups
    PRBool HandlePopup(PRInt32 inMouseX,PRInt32 inMouseY);

protected:
    virtual void InitCallbacks(char * aName = nsnull);
    virtual void OnDestroy();

    NS_IMETHOD CreateNative(QWidget *parentWindow);

    nsresult CreateWidget(nsIWidget *aParent,
                          const nsRect &aRect,
                          EVENT_CALLBACK aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell *aAppShell,
                          nsIToolkit *aToolkit,
                          nsWidgetInitData *aInitData,
                          nsNativeWidget aNativeParent = nsnull);

    PRBool DispatchWindowEvent(nsGUIEvent* event);

protected:
    nsQBaseWidget       *mWidget;
    nsCOMPtr<nsIWidget> mParent;

    // composite update area - union of all Invalidate calls
    nsCOMPtr<nsIRegion> mUpdateArea;
    PRUint32            mPreferredWidth;
    PRUint32            mPreferredHeight;
    PRBool       	mListenForResizes;
    PRBool              mIsToplevel;
    PRUint32		mWidgetID;

    // this is the rollup listener variables
    static nsCOMPtr<nsIRollupListener> gRollupListener;
    static nsWeakPtr                   gRollupWidget;
    static PRBool                      gRollupConsumeRollupEvent;
};

#endif /* nsWidget_h__ */
