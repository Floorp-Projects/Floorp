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

#include <gtk/gtk.h>


/**
 * Base of all GTK+ native widgets.
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

    NS_IMETHOD SetModal(void);
    NS_IMETHOD Show(PRBool state);
    NS_IMETHOD IsVisible(PRBool &aState);

    NS_IMETHOD Move(PRUint32 aX, PRUint32 aY);
    NS_IMETHOD Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint);
    NS_IMETHOD Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth,
		      PRUint32 aHeight, PRBool aRepaint);

    NS_IMETHOD Enable(PRBool aState);
    NS_IMETHOD SetFocus(void);

    NS_IMETHOD GetBounds(nsRect &aRect);

    virtual PRBool OnResize(nsRect &aRect);
    virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);

    nsIFontMetrics *GetFont(void);
    NS_IMETHOD SetFont(const nsFont &aFont);

    NS_IMETHOD SetCursor(nsCursor aCursor);

    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);

    void* GetNativeData(PRUint32 aDataType);
#ifdef NS_GTK_REF
    void ReleaseNativeData(PRUint32 aDataType);
#endif

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

    NS_IMETHOD Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
    NS_IMETHOD Update(void);
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);

    void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull);
    
  // Utility functions

    PRBool     ConvertStatus(nsEventStatus aStatus);
    PRBool     DispatchMouseEvent(nsMouseEvent& aEvent);
    PRBool     DispatchStandardEvent(PRUint32 aMsg);
  // are we a "top level" widget?
    PRBool     mIsToplevel;

 protected:

    virtual void InitCallbacks(char * aName = nsnull);
    virtual void OnDestroy();

    NS_IMETHOD CreateNative(GtkWidget *parentWindow) { return NS_OK; }

    nsresult CreateWidget(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent = nsnull);


    PRBool DispatchWindowEvent(nsGUIEvent* event);

  //////////////////////////////////////////////////////////////////
  //
  // GTK signal installers
  //
  //////////////////////////////////////////////////////////////////
  void InstallMotionNotifySignal(GtkWidget * aWidget);

  void InstallEnterNotifySignal(GtkWidget * aWidget);

  void InstallLeaveNotifySignal(GtkWidget * aWidget);

  void InstallButtonPressSignal(GtkWidget * aWidget);

  void InstallButtonReleaseSignal(GtkWidget * aWidget);

  void AddToEventMask(GtkWidget * aWidget,
                      gint        aEventMask);

  //////////////////////////////////////////////////////////////////
  //
  // OnSomething handlers
  //
  //////////////////////////////////////////////////////////////////
  virtual void OnMotionNotifySignal(GdkEventMotion * aGdkMotionEvent);
  virtual void OnEnterNotifySignal(GdkEventCrossing * aGdkCrossingEvent);
  virtual void OnLeaveNotifySignal(GdkEventCrossing * aGdkCrossingEvent);
  virtual void OnButtonPressSignal(GdkEventButton * aGdkButtonEvent);
  virtual void OnButtonReleaseSignal(GdkEventButton * aGdkButtonEvent);

private:

  //////////////////////////////////////////////////////////////////
  //
  // GTK widget signals
  //
  //////////////////////////////////////////////////////////////////

  static gint MotionNotifySignal(GtkWidget *       aWidget,
                                 GdkEventMotion *  aGdkMotionEvent,
                                 gpointer          aData);

  static gint EnterNotifySignal(GtkWidget *        aWidget, 
                                GdkEventCrossing * aGdkCrossingEvent, 
                                gpointer           aData);
  
  static gint LeaveNotifySignal(GtkWidget *        aWidget, 
                                GdkEventCrossing * aGdkCrossingEvent, 
                                gpointer           aData);

  static gint ButtonPressSignal(GtkWidget *      aWidget, 
                                GdkEventButton * aGdkButtonEvent, 
                                gpointer         aData);

  static gint ButtonReleaseSignal(GtkWidget *      aWidget, 
                                  GdkEventButton * aGdkButtonEvent, 
                                  gpointer         aData);

  //////////////////////////////////////////////////////////////////
  //
  // GTK event support methods
  //
  //////////////////////////////////////////////////////////////////
  void InstallSignal(GtkWidget *   aWidget,
                     gchar *       aSignalName,
                     GtkSignalFunc aSignalFunction);
  
  PRBool DropEvent(GtkWidget * aWidget, 
                   GdkWindow * aEventWindow);
  
  void InitMouseEvent(GdkEventButton * aGdkButtonEvent,
                      nsMouseEvent &   anEvent,
                      PRUint32         aEventType);
protected:
    GtkWidget *mWidget;
    nsIWidget *mParent;

    // This is the composite update area (union of all the calls to
    // Invalidate)
    nsRect mUpdateArea;

    PRBool mShown;

    PRUint32 mPreferredWidth, mPreferredHeight;
private:
    static nsILookAndFeel *sLookAndFeel;
    static PRUint32 sWidgetCount;
};

#endif /* nsWidget_h__ */
