/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWidget_h__
#define nsWidget_h__

#include "nsBaseWidget.h"
#include "nsIKBStateControl.h"
#include "nsIRegion.h"
#ifdef PHOTON_DND
#include "nsIDragService.h"
#endif

class nsILookAndFeel;
class nsIAppShell;
class nsIToolkit;

#include <Pt.h>

class nsWidget;

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

	// create with nsIWidget parent
  inline NS_IMETHOD            Create(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell = nsnull,
                               nsIToolkit *aToolkit = nsnull,
                               nsWidgetInitData *aInitData = nsnull)
		{
		return(CreateWidget(aParent, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData, nsnull));
		}

	// create with a native parent
  inline NS_IMETHOD            Create(nsNativeWidget aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell = nsnull,
                               nsIToolkit *aToolkit = nsnull,
                               nsWidgetInitData *aInitData = nsnull)
		{
		return(CreateWidget(nsnull, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData,aParent));
		}

  NS_IMETHOD Destroy(void);
  inline nsIWidget* GetParent(void)
		{
		if( mIsDestroying ) return nsnull;
		nsIWidget* result = mParent;
		if( mParent ) NS_ADDREF( result );
		return result;
		}

  virtual void OnDestroy();

  NS_IMETHOD SetModal(PRBool aModal);
  NS_IMETHOD Show(PRBool state);
  inline NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent) { return NS_OK; }

  inline NS_IMETHOD IsVisible(PRBool &aState) { aState = mShown; return NS_OK; }

  inline NS_IMETHOD ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY) { return NS_OK; }
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
  inline NS_IMETHOD Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
		{
		Move(aX,aY);
		Resize(aWidth,aHeight,aRepaint);
		return NS_OK;
		}

  inline NS_IMETHOD Enable(PRBool aState)
		{
		if( mWidget ) PtSetResource( mWidget, Pt_ARG_FLAGS, aState ? 0 : Pt_BLOCKED, Pt_BLOCKED );
		return NS_OK;
		}

  inline NS_IMETHOD IsEnabled(PRBool *aState)
		{
		if(PtWidgetFlags(mWidget) & Pt_BLOCKED) *aState = PR_FALSE;
		else *aState = PR_TRUE;
		return NS_OK;
		}

  PRBool OnResize(nsSizeEvent event);
  virtual PRBool OnResize(nsRect &aRect);
  virtual PRBool OnMove(PRInt32 aX, PRInt32 aY);

  inline nsIFontMetrics *GetFont(void) { return nsnull; }
  NS_IMETHOD SetFont(const nsFont &aFont);

  inline NS_IMETHOD SetBackgroundColor(const nscolor &aColor)
		{
		nsBaseWidget::SetBackgroundColor( aColor );
		if( mWidget ) PtSetResource( mWidget, Pt_ARG_FILL_COLOR, NS_TO_PH_RGB( aColor ), 0 );
		return NS_OK;
		}

  NS_IMETHOD SetCursor(nsCursor aCursor);

  inline NS_IMETHOD SetColorMap(nsColorMap *aColorMap) { return NS_OK; }

  inline void* GetNativeData(PRUint32 aDataType) { return (void *)mWidget; }

  NS_IMETHOD WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect);
  NS_IMETHOD ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect);

  inline NS_IMETHOD BeginResizingChildren(void)
		{
		PtHold();
		return NS_OK;
		}

  inline NS_IMETHOD EndResizingChildren(void)
		{
		PtRelease();
		return NS_OK;
		}

  inline NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
		{
		aWidth  = mPreferredWidth;
		aHeight = mPreferredHeight;
		return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
		}

  inline NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
		{
		mPreferredWidth  = aWidth;
		mPreferredHeight = aHeight;
		return NS_OK;
		}

  // Use this to set the name of a widget for normal widgets.. not the same as the nsWindow version
  inline NS_IMETHOD SetTitle(const nsAString& aTitle) { return NS_OK; }

  inline void ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY) { }

  // the following are nsWindow specific, and just stubbed here
  inline NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) { return NS_OK; }
  inline NS_IMETHOD ScrollWidgets(PRInt32 aDx, PRInt32 aDy) { return NS_OK; }
 
  inline NS_IMETHOD SetMenuBar(nsIMenuBar *aMenuBar) { return NS_ERROR_FAILURE; }
  inline NS_IMETHOD ShowMenuBar(PRBool aShow) { return NS_ERROR_FAILURE; }
  // *could* be done on a widget, but that would be silly wouldn't it?
  NS_IMETHOD CaptureMouse(PRBool aCapture) { return NS_ERROR_FAILURE; }


  NS_IMETHOD Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD Invalidate(const nsRect &aRect, PRBool aIsSynchronous);
  NS_IMETHOD InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
  inline NS_IMETHOD Update(void)
		{
		/* if the widget has been invalidated or damaged then re-draw it */
		PtFlush();
		return NS_OK;
		}

  NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);


  // nsIKBStateControl
  NS_IMETHOD ResetInputState();
  NS_IMETHOD PasswordFieldInit();

  inline void InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint = nsnull)
		{
		if( aPoint == nsnull ) {
		  event.point.x = 0;
		  event.point.y = 0;
		  }
		else {
		  event.point.x = aPoint->x;
		  event.point.y = aPoint->y;
		  }
		event.widget = this;
		event.time = PR_IntervalNow();
		event.message = aEventType;
		}

  // Utility functions

  inline PRBool     ConvertStatus(nsEventStatus aStatus)
		{
		switch(aStatus) {
		  case nsEventStatus_eIgnore:
		  case nsEventStatus_eConsumeDoDefault:
		    return(PR_FALSE);
		  case nsEventStatus_eConsumeNoDefault:
		    return(PR_TRUE);
		  }
		NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
		return PR_FALSE;
		}

  PRBool     DispatchMouseEvent(nsMouseEvent& aEvent);
  inline PRBool     DispatchStandardEvent(PRUint32 aMsg)
		{
		nsGUIEvent event;
		InitEvent(event, aMsg);
		return DispatchWindowEvent(&event);
		}

  // are we a "top level" widget?
  PRBool     mIsToplevel;

  // Set/Get the Pt_ARG_USER_DATA associated with each PtWidget_t *
  // this is always set to the "this" that owns the PtWidget_t
  static inline void SetInstance( PtWidget_t *pWidget, nsWidget * inst ) { PtSetResource( pWidget, Pt_ARG_POINTER, (void *)inst, 0 ); }
  static inline nsWidget*  GetInstance( PtWidget_t *pWidget )
		{
		nsWidget *data;
		PtGetResource( pWidget, Pt_ARG_POINTER, &data, 0 );
		return data;
		}

protected:
  NS_IMETHOD CreateNative(PtWidget_t *parentWindow) { return NS_OK; }

  nsresult CreateWidget(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeParent = nsnull);

  inline PRBool DispatchWindowEvent(nsGUIEvent* event)
		{
		nsEventStatus status;
		DispatchEvent(event, status);
		return ConvertStatus(status);
		}

#ifdef PHOTON_DND
	void DispatchDragDropEvent( PhEvent_t *phevent, PRUint32 aEventType, PhPoint_t *pos );
	void ProcessDrag( PhEvent_t *event, PRUint32 aEventType, PhPoint_t *pos );
#endif

  // this is the "native" destroy code that will destroy any
  // native windows / widgets for this logical widget
  virtual void DestroyNative(void);

  //////////////////////////////////////////////////////////////////
  //
  // Photon event support methods
  //
  //////////////////////////////////////////////////////////////////
  static int      RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  inline PRBool		HandleEvent( PtWidget_t *, PtCallbackInfo_t* aCbInfo );
  PRBool          DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent);

  inline void ScreenToWidgetPos( PhPoint_t &pt )
		{
		// pt is in screen coordinates - convert it to be relative to ~this~ widgets origin
		short x, y;
		PtGetAbsPosition( mWidget, &x, &y );
		pt.x -= x; pt.y -= y;
		}

	inline void InitKeyEvent(PhKeyEvent_t *aPhKeyEvent, nsKeyEvent &anEvent );
	inline void InitKeyPressEvent(PhKeyEvent_t *aPhKeyEvent, nsKeyEvent &anEvent );
  void InitMouseEvent( PhPointerEvent_t * aPhButtonEvent,
                       nsWidget         * aWidget,
                       nsMouseEvent     & anEvent,
                       PRUint32         aEventType );


  /* Convert Photon key codes to Mozilla key codes */
  PRUint32 nsConvertKey( PhKeyEvent_t *aPhKeyEvent );

#if 0
  //Enable/Disable Photon Damage		  
	inline void EnableDamage( PtWidget_t *widget, PRBool enable )
		{
		PtWidget_t *top = PtFindDisjoint( widget );
		if( top ) {
			if( PR_TRUE == enable ) PtEndFlux( top );
			else PtStartFlux( top );
			}
		}
#endif


  //////////////////////////////////////////////////////////////////
  //
  // Photon widget callbacks
  //
  //////////////////////////////////////////////////////////////////
  static int  GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int  LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
  static int  DestroyedCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
#ifdef PHOTON_DND
  static int  DndCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo );
#endif

  PtWidget_t          *mWidget;
  nsIWidget						*mParent;
  PRBool mShown;

  PRUint32 mPreferredWidth, mPreferredHeight;
  PRBool       mListenForResizes;
   
  // Focus used global variable
  static nsWidget* sFocusWidget; //Current Focus Widget
  
  static nsILookAndFeel *sLookAndFeel;
#ifdef PHOTON_DND
	static nsIDragService *sDragService;
#endif
  static PRUint32 sWidgetCount;
};

#endif /* nsWidget_h__ */
