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

#include "nsBaseWidget.h"
#include "nsToolkit.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"

#include "nsLookAndFeel.h"

#include <Pt.h>

class nsWidget;

typedef struct DamageQueueEntry_s
{
  PtWidget_t          *widget;
  nsWidget            *inst;
  DamageQueueEntry_s  *next;
}DamageQueueEntry;

#define NS_TO_PH_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)
#define PH_TO_NS_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)


/**
 * Base of all Photon native widgets.
 */

class nsWidget : public nsBaseWidget
{
 public:
    nsWidget();
    virtual ~nsWidget();

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

    NS_IMETHOD Destroy(void);
    nsIWidget* GetParent(void);

    NS_IMETHOD Show(PRBool state);
    NS_IMETHOD IsVisible(PRBool &aState);

    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
		              PRInt32 aHeight, PRBool aRepaint);

    NS_IMETHOD Enable(PRBool aState);
    NS_IMETHOD SetFocus(void);

//    NS_IMETHOD GetBounds(nsRect &aRect);

    virtual PRBool OnResize(nsRect &aRect);
    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);

    NS_IMETHOD SetBackgroundColor(const nscolor &aColor);

    nsIFontMetrics *GetFont(void);
    NS_IMETHOD SetFont(const nsFont &aFont);

    NS_IMETHOD SetCursor(nsCursor aCursor);

    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);

    void* GetNativeData(PRUint32 aDataType);

    NS_IMETHOD WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);
    NS_IMETHOD ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect);

    NS_IMETHOD BeginResizingChildren(void);
    NS_IMETHOD EndResizingChildren(void);

    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);

    virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

  // the following are nsWindow specific, and just stubbed here

    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    NS_IMETHOD SetMenuBar(nsIMenuBar *aMenuBar);
    NS_IMETHOD ShowMenuBar(PRBool aShow);

    NS_IMETHOD Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
    NS_IMETHOD Update(void);
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
    virtual void ScreenToWidget( PhPoint_t &pt );

    void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull);
    static int RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
    virtual PRBool HandleEvent( PtCallbackInfo_t* aCbInfo );
    
  // Utility functions

    PRBool     ConvertStatus(nsEventStatus aStatus);
	/* Convert Photon key codes to Mozilla key codes */
    PRUint32   nsConvertKey(unsigned long keysym); 
    PRBool     DispatchMouseEvent(PhPoint_t &aPos, PRUint32 aEvent);
    PRBool     DispatchStandardEvent(PRUint32 aMsg);
    PRBool     DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent);
  // are we a "top level" widget?
    PRBool     mIsToplevel;
    void       EnableDamage( PtWidget_t *widget, PRBool enable );
    PRBool     GetParentClippedArea( nsRect &rect );

 protected:
    virtual void InitCallbacks(char * aName = nsnull);
    virtual void OnDestroy();

    NS_IMETHOD CreateNative(PtWidget_t *parentWindow) { return NS_OK; }

    nsresult CreateWidget(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent = nsnull);


    PRBool            DispatchWindowEvent(nsGUIEvent* event);
    static PRBool     SetInstance( PtWidget_t *pWidget, nsWidget * inst );
    static nsWidget*  GetInstance( PtWidget_t *pWidget );
    void              RemoveDamagedWidget(PtWidget_t *aWidget);
    void              QueueWidgetDamage();
    void              UpdateWidgetDamage();
    static int        WorkProc( void *data );
    void              InitDamageQueue();
    static int        GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
    static int        LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
//    virtual void      NativePaint( PhRect_t &extent );

    PtWidget_t *mWidget;
    nsIWidget  *mParent;
    nsIMenuBar *mMenuBar;
    PtWidget_t *mClient;

    // This is the composite update area (union of all the calls to
    // Invalidate)
    nsRect mUpdateArea;

    PRBool mShown;

    PRUint32 mPreferredWidth, mPreferredHeight;
    
    static DamageQueueEntry *mDmgQueue;
    static PRBool           mDmgQueueInited;
    static PtWorkProcId_t   *mWorkProcID;
//    PRBool mCreateHold;
//    PRBool mHold;
};

#endif /* nsWidget_h__ */
