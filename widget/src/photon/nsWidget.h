/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWidget_h__
#define nsWidget_h__

#include "nsBaseWidget.h"
#include "nsIKBStateControl.h"
#include "nsIRegion.h"

class nsILookAndFeel;
class nsIAppShell;
class nsIToolkit;

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

class nsWidget : public nsBaseWidget, nsIKBStateControl
{
public:
  nsWidget();
  virtual ~nsWidget();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIWidget

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
  virtual void OnDestroy();

  NS_IMETHOD SetModal(PRBool aModal);
  NS_IMETHOD Show(PRBool state);
  NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD IsVisible(PRBool &aState);

  NS_IMETHOD ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  NS_IMETHOD Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                    PRInt32 aHeight, PRBool aRepaint);

  NS_IMETHOD Enable(PRBool aState);
  NS_IMETHOD SetFocus(PRBool aRaise);

  virtual void LoseFocus(void);

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

  // Use this to set the name of a widget for normal widgets.. not the same as the nsWindow version
  NS_IMETHOD SetTitle(const nsString& aTitle);


  virtual void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);

  // the following are nsWindow specific, and just stubbed here
  NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
 
  NS_IMETHOD SetMenuBar(nsIMenuBar *aMenuBar);
  NS_IMETHOD ShowMenuBar(PRBool aShow);
  // *could* be done on a widget, but that would be silly wouldn't it?
  NS_IMETHOD CaptureMouse(PRBool aCapture) { return NS_ERROR_FAILURE; }


  NS_IMETHOD Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
  NS_IMETHOD Update(void);
  NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);


  // nsIKBStateControl
  NS_IMETHOD ResetInputState();
  NS_IMETHOD PasswordFieldInit();

  void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull);

  // Utility functions

  PRBool     ConvertStatus(nsEventStatus aStatus);
  PRBool     DispatchMouseEvent(nsMouseEvent& aEvent);
  PRBool     DispatchStandardEvent(PRUint32 aMsg);
  PRBool     DispatchFocus(nsGUIEvent &aEvent);

  // are we a "top level" widget?
  PRBool     mIsToplevel;

  // Set/Get the Pt_ARG_USER_DATA associated with each PtWidget_t *
  // this is always set to the "this" that owns the PtWidget_t
  static PRBool     SetInstance( PtWidget_t *pWidget, nsWidget * inst );
  static nsWidget*  GetInstance( PtWidget_t *pWidget );

protected:
  virtual void InitCallbacks(char * aName = nsnull);

  NS_IMETHOD CreateNative(PtWidget_t *parentWindow) { return NS_OK; }

  nsresult CreateWidget(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeParent = nsnull);

//#if defined(DEBUG)
  static nsAutoString GuiEventToString(nsGUIEvent * aGuiEvent);
  static nsAutoString PhotonEventToString(PhEvent_t * aPhEvent);
//#endif

  PRBool DispatchWindowEvent(nsGUIEvent* event);

  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void DestroyNative(void);

  // this is set when a given widget has the focus.
  PRBool       mHasFocus;	
  // 
  PRBool mIsDragDest;

  //////////////////////////////////////////////////////////////////
  //
  // Photon event support methods
  //
  //////////////////////////////////////////////////////////////////
  static int       RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  virtual PRBool   HandleEvent( PtCallbackInfo_t* aCbInfo );
  PRBool           DispatchMouseEvent(PhPoint_t &aPos, PRUint32 aEvent);
  PRBool           DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent);
  virtual void     ScreenToWidget( PhPoint_t &pt );

  void             InitKeyEvent(PhKeyEvent_t *aPhKeyEvent, nsWidget *aWidget,
                                nsKeyEvent &aKeyEvent, PRUint32 aEventType);
  void             InitMouseEvent(PhPointerEvent_t * aPhButtonEvent,
                                  nsWidget         * aWidget,
                                  nsMouseEvent     & anEvent,
                                  PRUint32           aEventType);


  /* Convert Photon key codes to Mozilla key codes */
  PRUint32   nsConvertKey(unsigned long keysym, PRBool *aIsChar);

  //Enable/Disable Photon Damage		  
  void       EnableDamage( PtWidget_t *widget, PRBool enable );

  //Get the Parent's area minus the children's area for clipping reasons
  PRBool     GetParentClippedArea( nsRect &rect );

  //////////////////////////////////////////////////////////////////
  //
  // Damage Queue Methods and Members
  //
  //////////////////////////////////////////////////////////////////
  void  InitDamageQueue();
  void  RemoveDamagedWidget(PtWidget_t *aWidget);
  void  QueueWidgetDamage();
  void  UpdateWidgetDamage();

  // The Damage Queue Work Proc.
  static int        WorkProc( void *data );

  static DamageQueueEntry     *mDmgQueue;
  static PRBool                mDmgQueueInited;
  static PtWorkProcId_t       *mWorkProcID;

  //////////////////////////////////////////////////////////////////
  //
  // Photon widget callbacks
  //
  //////////////////////////////////////////////////////////////////
  static int  GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int  LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int  DestroyedCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );

  PtWidget_t          *mWidget;
  nsIWidget						*mParent;
#if 0
  // This is the composite update area (union of all the calls to Invalidate)
  nsIRegion *mUpdateArea;
#endif
  PRBool mShown;

  PRUint32 mPreferredWidth, mPreferredHeight;
  PRBool       mListenForResizes;
   
  // Focus used global variable
  static nsWidget* sFocusWidget; //Current Focus Widget
  static PRBool    sJustGotDeactivated;
  static PRBool    sJustGotActivated; //For getting rid of the ASSERT ERROR due to reducing suppressing of focus.
  
  /* this is the rollup listener variables */
  /* These variables help us close the Menu when the clicks outside the application */
  static nsIRollupListener *gRollupListener;
  static nsIWidget         *gRollupWidget;

  static nsILookAndFeel *sLookAndFeel;
  static PRUint32 sWidgetCount;
};

#endif /* nsWidget_h__ */
