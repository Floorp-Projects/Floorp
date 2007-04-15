/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Jerry.Kirk@Nexwarecorp.com
 *   Michael.Kedl@Nexwarecorp.com
 *   Dale.Stansberry@Nexwarecorop.com
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


#include "nsWidget.h"

#include "nsIAppShell.h"
#include "nsIComponentManager.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsToolkit.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include <Pt.h>
#include "PtRawDrawContainer.h"
#include "nsIRollupListener.h"
#include "nsIServiceManager.h"
#include "nsWindow.h"
#ifdef PHOTON_DND
#include "nsDragService.h"
#endif
#include "nsReadableUtils.h"

#include "nsClipboard.h"

#include <errno.h>
#include <photon/PtServer.h>

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

// BGR, not RGB - REVISIT
#define NSCOLOR_TO_PHCOLOR(g,n) \
  g.red=NS_GET_B(n); \
  g.green=NS_GET_G(n); \
  g.blue=NS_GET_R(n);

////////////////////////////////////////////////////////////////////
//
// Define and Initialize global variables
//
////////////////////////////////////////////////////////////////////


//
// Keep track of the last widget being "dragged"
//
nsILookAndFeel     *nsWidget::sLookAndFeel = nsnull;
#ifdef PHOTON_DND
nsIDragService     *nsWidget::sDragService = nsnull;
#endif
nsClipboard        *nsWidget::sClipboard = nsnull;
PRUint32            nsWidget::sWidgetCount = 0;
nsWidget*						nsWidget::sFocusWidget = 0;

nsWidget::nsWidget()
{
  if (!sLookAndFeel) {
    CallGetService(kLookAndFeelCID, &sLookAndFeel);
  }

  if( sLookAndFeel )
    sLookAndFeel->GetColor( nsILookAndFeel::eColor_WindowBackground, mBackground );

#ifdef PHOTON_DND
	if( !sDragService ) {
		nsresult rv;
		nsCOMPtr<nsIDragService> s;
		s = do_GetService( "@mozilla.org/widget/dragservice;1", &rv );
		sDragService = ( nsIDragService * ) s;
		if( NS_FAILED( rv ) ) sDragService = 0;
		}
#endif

	if( !sClipboard ) {
		nsresult rv;
		nsCOMPtr<nsClipboard> s;
		s = do_GetService( kCClipboardCID, &rv );
		sClipboard = ( nsClipboard * ) s;
		if( NS_FAILED( rv ) ) sClipboard = 0;
		}

  mWidget = nsnull;
  mParent = nsnull;
  mPreferredWidth  = 0;
  mPreferredHeight = 0;
  mShown = PR_FALSE;
  mBounds.x = 0;
  mBounds.y = 0;
  mBounds.width = 0;
  mBounds.height = 0;
  mIsDestroying = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mIsToplevel = PR_FALSE;
  mListenForResizes = PR_FALSE;
  sWidgetCount++;
}


nsWidget::~nsWidget( ) {

	if( sFocusWidget == this ) sFocusWidget = 0;

  // it's safe to always call Destroy() because it will only allow itself to be called once
  Destroy();

  if( !sWidgetCount-- ) {
    NS_IF_RELEASE( sLookAndFeel );
  	}
	}

//-------------------------------------------------------------------------
//
// nsISupport stuff
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(nsWidget, nsBaseWidget, nsIKBStateControl)

NS_METHOD nsWidget::WidgetToScreen( const nsRect& aOldRect, nsRect& aNewRect ) {
  if( mWidget ) {
    /* This is NOT correct */
    aNewRect.x = aOldRect.x;
    aNewRect.y = aOldRect.y;
  	}
  return NS_OK;
	}

// nsWidget::ScreenToWidget - Not Implemented
NS_METHOD nsWidget::ScreenToWidget( const nsRect& aOldRect, nsRect& aNewRect ) {
  return NS_OK;
	}

//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Destroy( void ) {

 // make sure we don't call this more than once.
  if( mIsDestroying ) return NS_OK;

  // ok, set our state
  mIsDestroying = PR_TRUE;

  // call in and clean up any of our base widget resources
  // are released
  nsBaseWidget::Destroy();

  // destroy our native windows
  DestroyNative();

  // make sure to call the OnDestroy if it hasn't been called yet
  if( mOnDestroyCalled == PR_FALSE ) OnDestroy();

  // make sure no callbacks happen
  mEventCallback = nsnull;

  return NS_OK;
	}

// this is the function that will destroy the native windows for this widget.

/* virtual */
void nsWidget::DestroyNative( void ) {
  if( mWidget ) {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
	  //EnableDamage( mWidget, PR_FALSE );
	  PtDestroyWidget( mWidget );
	  //EnableDamage( mWidget, PR_TRUE );
    mWidget = nsnull;
  	}
	}

// make sure that we clean up here

void nsWidget::OnDestroy( ) {
  mOnDestroyCalled = PR_TRUE;
  // release references to children, device context, toolkit + app shell
  nsBaseWidget::OnDestroy();
  DispatchStandardEvent(NS_DESTROY);
	}

//////////////////////////////////////////////////////////////////////
//
// nsIKBStateControl Mehthods
//
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsWidget::ResetInputState( ) {
  return NS_OK;
	}

NS_IMETHODIMP nsWidget::SetIMEOpenState(PRBool aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
	}

NS_IMETHODIMP nsWidget::GetIMEOpenState(PRBool* aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
	}

NS_IMETHODIMP nsWidget::SetIMEEnabled(PRUint32 aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
	}

NS_IMETHODIMP nsWidget::GetIMEEnabled(PRUint32* aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
	}

NS_IMETHODIMP nsWidget::CancelIMEComposition() {
  return NS_ERROR_NOT_IMPLEMENTED;
	}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Show( PRBool bState ) {

  if( !mWidget ) return NS_OK; // Will be null durring printing

  PtArg_t   arg;

  if( bState ) {

		if( mWindowType != eWindowType_child ) {

		  if (PtWidgetIsRealized(mWidget)) {
			  mShown = PR_TRUE; 
			  return NS_OK;
		  	}

		  //EnableDamage( mWidget, PR_FALSE );
		  PtRealizeWidget(mWidget);

		  if( mWidget->rid == -1 ) {
			  //EnableDamage( mWidget, PR_TRUE );
			  NS_ASSERTION(0,"nsWidget::Show mWidget's rid == -1\n");
			  mShown = PR_FALSE; 
			  return NS_ERROR_FAILURE;
		  	}

		  PtSetArg(&arg, Pt_ARG_FLAGS, 0, Pt_DELAY_REALIZE);
		  PtSetResources(mWidget, 1, &arg);
		  //EnableDamage( mWidget, PR_TRUE );
		  PtDamageWidget(mWidget);
#ifdef Ph_REGION_NOTIFY			
		  PhRegion_t region;
		  PtWidget_t *mWgt;
		  mWgt = (PtWidget_t*) GetNativeData( NS_NATIVE_WIDGET );
		  region.flags = Ph_REGION_NOTIFY | Ph_FORCE_BOUNDARY;
		  region.rid = PtWidgetRid(mWgt);
		  PhRegionChange(Ph_REGION_FLAGS, 0, &region, NULL, NULL);
#endif
		}
		else {
			PtWidgetToFront( mWidget );
			if( !mShown || !( mWidget->flags & Pt_REALIZED ) ) PtRealizeWidget( mWidget );
			}
  	}
  else {
		if( mWindowType != eWindowType_child ) {
      //EnableDamage( mWidget, PR_FALSE );
      PtUnrealizeWidget(mWidget);

      //EnableDamage( mWidget, PR_TRUE );

      PtSetArg(&arg, Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
      PtSetResources(mWidget, 1, &arg);
			}
		else {
			//EnableDamage( mWidget, PR_FALSE );
			PtWidgetToBack( mWidget );
			if( mShown ) PtUnrealizeWidget( mWidget );
			//EnableDamage( mWidget, PR_TRUE );
			}
  	}

  mShown = bState;
  return NS_OK;
	}


// nsWidget::SetModal - Not Implemented
NS_IMETHODIMP nsWidget::SetModal( PRBool aModal ) {
  return NS_ERROR_FAILURE;
	}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Move( PRInt32 aX, PRInt32 aY ) {

	if( ( mBounds.x == aX ) && ( mBounds.y == aY ) ) return NS_OK; 

	mBounds.x = aX;
	mBounds.y = aY;
	
	if( mWidget ) {
		if(( mWidget->area.pos.x != aX ) || ( mWidget->area.pos.y != aY )) {
			PhPoint_t pos = { aX, aY };
			PtSetResource( mWidget, Pt_ARG_POS, &pos, 0 );
		}
	}
  return NS_OK;
}


NS_METHOD nsWidget::Resize( PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint ) {

	if( ( mBounds.width == aWidth ) && ( mBounds.height == aHeight ) ) 
	   return NS_OK;

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if( mWidget ) {
		PhDim_t dim = { aWidth, aHeight };
		//EnableDamage( mWidget, PR_FALSE );
		PtSetResource( mWidget, Pt_ARG_DIM, &dim, 0 );
		//EnableDamage( mWidget, PR_TRUE );
		}

	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize( nsRect &aRect ) {

  PRBool result = PR_FALSE;

  // call the event callback
  if( mEventCallback ) {
		nsSizeEvent event(PR_TRUE, 0, nsnull);

	  InitEvent(event, NS_SIZE);

		nsRect *foo = new nsRect(0, 0, aRect.width, aRect.height);
		event.windowSize = foo;

		event.refPoint.x = 0;
		event.refPoint.y = 0;
		event.mWinWidth = aRect.width;
		event.mWinHeight = aRect.height;
  
		NS_ADDREF_THIS();
		result = DispatchWindowEvent(&event);
		NS_RELEASE_THIS();
		delete foo;
		}

	return result;
	}

//------
// Move
//------
PRBool nsWidget::OnMove( PRInt32 aX, PRInt32 aY ) {
  nsGUIEvent event(PR_TRUE, 0, nsnull);
  InitEvent(event, NS_MOVE);
  event.refPoint.x = aX;
  event.refPoint.y = aY;
  return DispatchWindowEvent(&event);
	}


//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFont( const nsFont &aFont ) {

  nsIFontMetrics* mFontMetrics;
  mContext->GetMetricsFor(aFont, mFontMetrics);

	if( mFontMetrics ) {
		PtArg_t arg;

		nsFontHandle aFontHandle;
		mFontMetrics->GetFontHandle(aFontHandle);
		nsString *aString;
		aString = (nsString *) aFontHandle;
		char *str = ToNewCString(*aString);

		PtSetArg( &arg, Pt_ARG_TEXT_FONT, str, 0 );
		PtSetResources( mWidget, 1, &arg );

		delete [] str;
		NS_RELEASE(mFontMetrics);
		}
	return NS_OK;
	}

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetCursor( nsCursor aCursor ) {

  // Only change cursor if it's changing
  if( aCursor != mCursor ) {

  	unsigned short curs = Ph_CURSOR_POINTER;
  	PgColor_t color = Ph_CURSOR_DEFAULT_COLOR;

	switch( aCursor ) {
		case eCursor_nw_resize:
			curs = Ph_CURSOR_DRAG_TL;
			break;
		case eCursor_se_resize:
			curs = Ph_CURSOR_DRAG_BR;
			break;
		case eCursor_ne_resize:
			curs = Ph_CURSOR_DRAG_TL;
			break;
		case eCursor_sw_resize:
			curs = Ph_CURSOR_DRAG_BL;
			break;

		case eCursor_crosshair:
			curs = Ph_CURSOR_CROSSHAIR;
			break;

		case eCursor_copy:
		case eCursor_alias:
		case eCursor_context_menu:
			// XXX: No suitable cursor, needs implementing
			break;

		case eCursor_cell:
			// XXX: No suitable cursor, needs implementing
			break;

		case eCursor_spinning:
			// XXX: No suitable cursor, needs implementing
			break;

		case eCursor_move:
		  curs = Ph_CURSOR_MOVE;
		  break;
	  
		case eCursor_help:
		  curs = Ph_CURSOR_QUESTION_POINT;
		  break;
	  
		case eCursor_grab:
		case eCursor_grabbing:
		  curs = Ph_CURSOR_FINGER;
		  break;
	  
		case eCursor_select:
		  curs = Ph_CURSOR_INSERT;
		  color = Pg_BLACK;
		  break;
	  
		case eCursor_wait:
		  curs = Ph_CURSOR_LONG_WAIT;
		  break;

		case eCursor_hyperlink:
		  curs = Ph_CURSOR_FINGER;
		  break;

		case eCursor_standard:
		  curs = Ph_CURSOR_POINTER;
		  break;

		case eCursor_n_resize:
		case eCursor_s_resize:
		  curs = Ph_CURSOR_DRAG_VERTICAL;
		  break;

		case eCursor_w_resize:
		case eCursor_e_resize:
		  curs = Ph_CURSOR_DRAG_HORIZONTAL;
		  break;

		case eCursor_zoom_in:
		case eCursor_zoom_out:
		  // XXX: No suitable cursor, needs implementing
		  break;

		case eCursor_not_allowed:
		case eCursor_no_drop:
		  curs = Ph_CURSOR_DONT;
		  break;

		case eCursor_col_resize:
		  // XXX: not 100% appropriate perhaps
		  curs = Ph_CURSOR_DRAG_HORIZONTAL;
		  break;

		case eCursor_row_resize:
		  // XXX: not 100% appropriate perhaps
		  curs = Ph_CURSOR_DRAG_VERTICAL;
		  break;

		case eCursor_vertical_text:
		  curs = Ph_CURSOR_INSERT;
		  color = Pg_BLACK;
		  break;

		case eCursor_all_scroll:
		  // XXX: No suitable cursor, needs implementing
		  break;

		case eCursor_nesw_resize:
		  curs = Ph_CURSOR_DRAG_FOREDIAG;
		  break;

		case eCursor_nwse_resize:
		  curs = Ph_CURSOR_DRAG_BACKDIAG;
		  break;

		case eCursor_ns_resize:
		  curs = Ph_CURSOR_DRAG_VERTICAL;
		  break;

		case eCursor_ew_resize:
		  curs = Ph_CURSOR_DRAG_HORIZONTAL;
		  break;

		default:
		  NS_ASSERTION(0, "Invalid cursor type");
		  break;
  		}

  	if( mWidget ) {
  	  PtArg_t args[2];

			PtSetArg( &args[0], Pt_ARG_CURSOR_TYPE, curs, 0 );
			PtSetArg( &args[1], Pt_ARG_CURSOR_COLOR, color, 0 );
			PtSetResources( mWidget, 2, args );
  		}
		mCursor = aCursor;
		}

	return NS_OK;
	}


NS_METHOD nsWidget::Invalidate( PRBool aIsSynchronous ) {

	// mWidget will be null during printing
	if( !mWidget || !PtWidgetIsRealized( mWidget ) ) return NS_OK;

	PtWidget_t *aWidget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);
	PtDamageWidget( aWidget );
	if( aIsSynchronous ) PtFlush();
	return NS_OK;
	}

NS_METHOD nsWidget::Invalidate( const nsRect & aRect, PRBool aIsSynchronous ) {

  if( !mWidget ) return NS_OK; // mWidget will be null during printing
  if( !PtWidgetIsRealized( mWidget ) ) return NS_OK;

	PhRect_t prect;
	prect.ul.x = aRect.x;
	prect.ul.y = aRect.y;
	prect.lr.x = prect.ul.x + aRect.width - 1;
	prect.lr.y = prect.ul.y + aRect.height - 1;
	if( ! ( mWidget->class_rec->flags & Pt_DISJOINT ) )
		PhTranslateRect( &prect, &mWidget->area.pos );
	PtDamageExtent( mWidget, &prect );
	if( aIsSynchronous ) PtFlush( );
	return NS_OK;
	}

NS_IMETHODIMP nsWidget::InvalidateRegion( const nsIRegion *aRegion, PRBool aIsSynchronous ) {
	PhTile_t *tiles = NULL;
	aRegion->GetNativeRegion( ( void*& ) tiles );
	if( tiles ) {
		PhTranslateTiles( tiles, &mWidget->area.pos );
		PtDamageTiles( mWidget, tiles );
		if( aIsSynchronous ) PtFlush( );
		}
  return NS_OK;
	}

nsresult nsWidget::CreateWidget(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData,
                                nsNativeWidget aNativeParent)
{

  PtWidget_t *parentWidget = nsnull;



  if( aNativeParent ) {
    parentWidget = (PtWidget_t*)aNativeParent;
    // we've got a native parent so listen for resizes
    mListenForResizes = PR_TRUE;
  	}
  else if( aParent ) {
    parentWidget = (PtWidget_t*) (aParent->GetNativeData(NS_NATIVE_WIDGET));
		mListenForResizes = aInitData ? aInitData->mListenForResizes : PR_FALSE;
  	}

	if( aInitData->mWindowType == eWindowType_child && !parentWidget ) return NS_ERROR_FAILURE;

  nsIWidget *baseParent = aInitData &&
                            (aInitData->mWindowType == eWindowType_dialog ||
                             aInitData->mWindowType == eWindowType_toplevel ||
                             aInitData->mWindowType == eWindowType_invisible) ?
                          nsnull : aParent;

  BaseCreate( baseParent, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData );

  mParent = aParent;
  mBounds = aRect;

  CreateNative (parentWidget);

	if( aRect.width > 1 && aRect.height > 1 ) Resize(aRect.width, aRect.height, PR_FALSE);

  if( mWidget ) {
    SetInstance(mWidget, this);
    PtAddCallback( mWidget, Pt_CB_GOT_FOCUS, GotFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_LOST_FOCUS, LostFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_IS_DESTROYED, DestroyedCallback, this );
#ifdef PHOTON_DND
    PtAddCallback( mWidget, Pt_CB_DND, DndCallback, this );
#endif
  	}

  DispatchStandardEvent(NS_CREATE);

  return NS_OK;
	}


//-------------------------------------------------------------------------
//
// Invokes callback and ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent( nsGUIEvent *aEvent, nsEventStatus &aStatus ) {

  NS_ADDREF(aEvent->widget);

  if( nsnull != mMenuListener ) {
    if( NS_MENU_EVENT == aEvent->eventStructType )
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *aEvent));
  	}

  aStatus = nsEventStatus_eIgnore;

///* ATENTIE */ printf( "mEventCallback call (%d %d) this=%p\n", aEvent->point.x, aEvent->point.y, this );

  if( nsnull != mEventCallback ) aStatus = (*mEventCallback)(aEvent);

  // Dispatch to event listener if event was not consumed
  if( (aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener) )
    aStatus = mEventListener->ProcessEvent(*aEvent);

   NS_IF_RELEASE(aEvent->widget);

  return NS_OK;
	}

//==============================================================
void nsWidget::InitMouseEvent(PhPointerEvent_t *aPhButtonEvent,
                              nsWidget * aWidget,
                              nsMouseEvent &anEvent,
                              PRUint32   aEventType,
                              PRInt16    aButton)
{
  anEvent.message = aEventType;
  anEvent.widget  = aWidget;

  if (aPhButtonEvent != nsnull) {
    anEvent.time =      PR_IntervalNow();
    anEvent.isShift =   ( aPhButtonEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhButtonEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhButtonEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
		anEvent.isMeta =		PR_FALSE;
    anEvent.refPoint.x =   aPhButtonEvent->pos.x; 
    anEvent.refPoint.y =   aPhButtonEvent->pos.y;
    anEvent.clickCount = aPhButtonEvent->click_count;
    anEvent.button = aButton;
  	}
	}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchMouseEvent( nsMouseEvent& aEvent ) {

  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) return result;

  // call the event callback
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&aEvent);
    return result;
  	}

  if (nsnull != mMouseListener) {

    switch (aEvent.message) {
      case NS_MOUSE_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(aEvent));
        break;

      case NS_MOUSE_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(aEvent));
        result = ConvertStatus(mMouseListener->MouseClicked(aEvent));
        break;

    	case NS_DRAGDROP_DROP:
    	  break;

			case NS_MOUSE_MOVE:
    	  	break;

    	default:
    	  break;

    	} // switch
  	}
  return result;
	}

struct nsKeyConverter {
  PRUint32       vkCode; // Platform independent key code
  unsigned long  keysym; // Photon key_sym key code
};

static struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_PAGE_UP,    Pk_Pg_Up },
  { NS_VK_PAGE_DOWN,  Pk_Pg_Down },
  { NS_VK_UP,         Pk_Up },
  { NS_VK_DOWN,       Pk_Down },
  { NS_VK_TAB,        Pk_Tab },
  { NS_VK_TAB,        Pk_KP_Tab },
  { NS_VK_HOME,       Pk_Home },
  { NS_VK_END,        Pk_End },
  { NS_VK_LEFT,       Pk_Left },
  { NS_VK_RIGHT,      Pk_Right },
  { NS_VK_DELETE,     Pk_Delete },
  { NS_VK_CANCEL,     Pk_Cancel },
  { NS_VK_BACK,       Pk_BackSpace },
  { NS_VK_CLEAR,      Pk_Clear },
  { NS_VK_RETURN,     Pk_Return },
  { NS_VK_SHIFT,      Pk_Shift_L },
  { NS_VK_SHIFT,      Pk_Shift_R },
  { NS_VK_CONTROL,    Pk_Control_L },
  { NS_VK_CONTROL,    Pk_Control_R },
  { NS_VK_ALT,        Pk_Alt_L },
  { NS_VK_ALT,        Pk_Alt_R },
  { NS_VK_INSERT,     Pk_Insert },
  { NS_VK_PAUSE,      Pk_Pause },
  { NS_VK_CAPS_LOCK,  Pk_Caps_Lock },
  { NS_VK_ESCAPE,     Pk_Escape },
  { NS_VK_PRINTSCREEN,Pk_Print },
  { NS_VK_RETURN,     Pk_KP_Enter },
  { NS_VK_INSERT,     Pk_KP_0 },
  { NS_VK_END,        Pk_KP_1 },
  { NS_VK_DOWN,       Pk_KP_2 },
  { NS_VK_PAGE_DOWN,  Pk_KP_3 },
  { NS_VK_LEFT,       Pk_KP_4 },
  { NS_VK_NUMPAD5,    Pk_KP_5 },
  { NS_VK_RIGHT,      Pk_KP_6 },
  { NS_VK_HOME,       Pk_KP_7 },
  { NS_VK_UP,         Pk_KP_8 },
  { NS_VK_PAGE_UP,    Pk_KP_9 },
  { NS_VK_COMMA,      Pk_KP_Separator }
  };


// Input keysym is in photon format; output is in NS_VK format
PRUint32 nsWidget::nsConvertKey( PhKeyEvent_t *aPhKeyEvent ) {

	unsigned long keysym, keymods;

  const int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

	keymods = aPhKeyEvent->key_mods;
	if( aPhKeyEvent->key_flags & Pk_KF_Sym_Valid )
		keysym = aPhKeyEvent->key_sym;
	else if( aPhKeyEvent->key_flags & Pk_KF_Cap_Valid )
		keysym = aPhKeyEvent->key_cap;
	else return 0;
	
  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  if (keysym >= Pk_a && keysym <= Pk_z)
     return keysym - Pk_a + NS_VK_A;

  if (keysym >= Pk_A && keysym <= Pk_Z)
     return keysym - Pk_A + NS_VK_A;

  if (keysym >= Pk_0 && keysym <= Pk_9)
     return keysym - Pk_0 + NS_VK_0;
		  
  if (keysym >= Pk_F1 && keysym <= Pk_F24) {
     return keysym - Pk_F1 + NS_VK_F1;
  	}

	if( keymods & Pk_KM_Num_Lock ) {
  	if( keysym >= Pk_KP_0 && keysym <= Pk_KP_9 )
     	return keysym - Pk_0 + NS_VK_0;
		}

  for (int i = 0; i < length; i++) {
    if( nsKeycodes[i].keysym == keysym ) {
      return (nsKeycodes[i].vkCode);
    	}
  	}

  return((int) 0);
	}

PRBool  nsWidget::DispatchKeyEvent( PhKeyEvent_t *aPhKeyEvent ) {
  NS_ASSERTION(aPhKeyEvent, "nsWidget::DispatchKeyEvent a NULL PhKeyEvent was passed in");

  if( !(aPhKeyEvent->key_flags & (Pk_KF_Cap_Valid|Pk_KF_Sym_Valid) ) ) {
		//printf("nsWidget::DispatchKeyEvent throwing away invalid key: Modifiers Valid=<%d,%d,%d> this=<%p>\n",
		//(aPhKeyEvent->key_flags & Pk_KF_Scan_Valid), (aPhKeyEvent->key_flags & Pk_KF_Sym_Valid), (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid), this );
		return PR_FALSE;
		}

  if ( PtIsFocused(mWidget) != 2) {
     //printf("nsWidget::DispatchKeyEvent Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(mWidget));
     return PR_FALSE;
  	}
  
  if ( ( aPhKeyEvent->key_cap == Pk_Shift_L )
       || ( aPhKeyEvent->key_cap == Pk_Shift_R )
       || ( aPhKeyEvent->key_cap == Pk_Control_L )
       || ( aPhKeyEvent->key_cap ==  Pk_Control_R )
       || ( aPhKeyEvent->key_cap ==  Pk_Num_Lock )
       || ( aPhKeyEvent->key_cap ==  Pk_Scroll_Lock )
     )
    return PR_TRUE;

  nsWindow *w = (nsWindow *) this;

  w->AddRef();
 
  if (aPhKeyEvent->key_flags & Pk_KF_Key_Down) {
		nsKeyEvent keyDownEvent(PR_TRUE, NS_KEY_DOWN, w);
		InitKeyEvent(aPhKeyEvent, keyDownEvent);
		PRBool noDefault = w->OnKey(keyDownEvent);

		nsKeyEvent keyPressEvent(PR_TRUE, NS_KEY_PRESS, w);
		InitKeyPressEvent(aPhKeyEvent, keyPressEvent);
		if (noDefault) {  // If prevent default set for keydown, do same for keypress
			keyPressEvent.flags = NS_EVENT_FLAG_NO_DEFAULT;
		}
		w->OnKey(keyPressEvent);
  	}
  else if (aPhKeyEvent->key_flags & Pk_KF_Key_Repeat) {
		nsKeyEvent keyPressEvent(PR_TRUE, NS_KEY_PRESS, w);
		InitKeyPressEvent(aPhKeyEvent, keyPressEvent);
		w->OnKey(keyPressEvent);
  	}
  else if (PkIsKeyDown(aPhKeyEvent->key_flags) == 0) {
		nsKeyEvent kevent(PR_TRUE, NS_KEY_UP, w);
		InitKeyEvent(aPhKeyEvent, kevent);
		w->OnKey(kevent);
  	}

  w->Release();

  return PR_TRUE;
	}

inline void nsWidget::InitKeyEvent(PhKeyEvent_t *aPhKeyEvent, nsKeyEvent &anEvent )
{
	anEvent.keyCode = 	nsConvertKey( aPhKeyEvent );
	anEvent.time = 			PR_IntervalNow();
	anEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
	anEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
	anEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
	anEvent.isMeta =    PR_FALSE;
}

/* similar to PhKeyToMb */
inline int key_sym_displayable(const PhKeyEvent_t *kevent)
{
  if(kevent->key_flags & Pk_KF_Sym_Valid) {
    unsigned long const sym = kevent->key_sym;
    if  ( sym >= 0xF000
      ? sym >= 0xF100 && ( sizeof(wchar_t) > 2 || sym < 0x10000 )
      : ( sym & ~0x9F ) != 0 // exclude 0...0x1F and 0x80...0x9F
        ) return 1;
  }
  return 0;
}

/* similar to PhKeyToMb */
inline int key_cap_displayable(const PhKeyEvent_t *kevent)
{
  if(kevent->key_flags & Pk_KF_Cap_Valid) {
    unsigned long const cap = kevent->key_cap;
    if  ( cap >= 0xF000
      ? cap >= 0xF100 && ( sizeof(wchar_t) > 2 || cap < 0x10000 )
      : ( cap & ~0x9F ) != 0 // exclude 0...0x1F and 0x80...0x9F
        ) return 1;
  }
  return 0;
}

inline void nsWidget::InitKeyPressEvent(PhKeyEvent_t *aPhKeyEvent, nsKeyEvent &anEvent )
{
	anEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
	anEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
	anEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
	anEvent.isMeta =    PR_FALSE;

	if( key_sym_displayable( aPhKeyEvent ) ) anEvent.charCode = aPhKeyEvent->key_sym;
	else {
		/* in photon Ctrl<something> or Alt<something> is not a displayable character, but
			mozilla wants the keypress event as a charCode+isControl+isAlt, instead of a keyCode */
		if( ( anEvent.isControl || anEvent.isAlt ) && key_cap_displayable( aPhKeyEvent ) )
			anEvent.charCode = aPhKeyEvent->key_cap;
		else anEvent.keyCode = nsConvertKey( aPhKeyEvent );
		}

	anEvent.time = 			PR_IntervalNow();
}

// used only once
inline PRBool nsWidget::HandleEvent( PtWidget_t *widget, PtCallbackInfo_t* aCbInfo ) {
  PRBool  result = PR_TRUE; // call the default nsWindow proc
  PhEvent_t* event = aCbInfo->event;

	if (event->processing_flags & Ph_CONSUMED) return PR_TRUE;

	switch ( event->type ) {

      case Ph_EV_PTR_MOTION_NOBUTTON:
       	{
	    	PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    	nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);

        if( ptrev ) {

					if( ptrev->flags & Ph_PTR_FLAG_Z_ONLY ) break; // sometimes z presses come out of nowhere */

///* ATENTIE */ printf( "Ph_EV_PTR_MOTION_NOBUTTON (%d %d)\n", ptrev->pos.x, ptrev->pos.y );

        	ScreenToWidgetPos( ptrev->pos );
 	      	InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
        	result = DispatchMouseEvent(theMouseEvent);
        	}
       	}
	    	break;

      case Ph_EV_BUT_PRESS:
       {

	    	PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
   		  nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);

				/* there should be no reason to do this - mozilla should figure out how to call SetFocus */
				/* this though fixes the problem with the plugins capturing the focus */
				PtWidget_t *disjoint = PtFindDisjoint( widget );
 				if( PtWidgetIsClassMember( disjoint, PtServer ) )
					PtContainerGiveFocus( widget, aCbInfo->event );

        if( ptrev ) {
          ScreenToWidgetPos( ptrev->pos );

          if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
						InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_BUTTON_DOWN,
							       nsMouseEvent::eLeftButton);
          else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
						InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_BUTTON_DOWN,
	  						       nsMouseEvent::eRightButton);
          else // middle button
						InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_BUTTON_DOWN,
							       nsMouseEvent::eMiddleButton);

		  		result = DispatchMouseEvent(theMouseEvent);

					// if we're a right-button-up we're trying to popup a context menu. send that event to gecko also
					if( ptrev->buttons & Ph_BUTTON_MENU ) {
						nsMouseEvent contextMenuEvent(PR_TRUE, 0, nsnull,
                                                      nsMouseEvent::eReal);
						InitMouseEvent(ptrev, this, contextMenuEvent, NS_CONTEXTMENU,
							       nsMouseEvent::eRightButton);
						result = DispatchMouseEvent( contextMenuEvent );
						}
      	  }

      	 }
		break;		
		
		case Ph_EV_BUT_RELEASE:
		  {
			  PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
			  nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull,
                                         nsMouseEvent::eReal);
			  
			  // Update the current input group for clipboard mouse events
			  // (mozilla only). Note that for mozserver the mouse based
			  // (eg. Edit->Copy/Paste menu) events don't come through here.
			  // They are sent by the voyager client app via libPtWeb.so to
			  // do_command() in mozserver.cpp.
			  if (sClipboard)
			  	sClipboard->SetInputGroup(event->input_group);

			  if (event->subtype==Ph_EV_RELEASE_REAL || event->subtype==Ph_EV_RELEASE_PHANTOM) {
				  if (ptrev) {
					  ScreenToWidgetPos( ptrev->pos );
					  if ( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
						 InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_BUTTON_UP,
						 	        nsMouseEvent::eLeftButton);
					  else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
						 InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_BUTTON_UP,
						 	        nsMouseEvent::eRightButton);
					  else // middle button
						 InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE__BUTTON_UP,
						 	        nsMouseEvent::eMiddleButton);
					  
					  result = DispatchMouseEvent(theMouseEvent);
				  }
			  }
			  else if (event->subtype==Ph_EV_RELEASE_OUTBOUND) {
				  PhRect_t rect = {{0,0},{0,0}};
				  PhRect_t boundary = {{-10000,-10000},{10000,10000}};
				  PhInitDrag( PtWidgetRid(mWidget), ( Ph_DRAG_KEY_MOTION | Ph_DRAG_TRACK | Ph_TRACK_DRAG),&rect, &boundary, aCbInfo->event->input_group , NULL, NULL, NULL, NULL, NULL);
			  }
		  }
	    break;

		case Ph_EV_PTR_MOTION_BUTTON:
      	{
        PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    	nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);

        if( ptrev ) {

					if( ptrev->flags & Ph_PTR_FLAG_Z_ONLY ) break; // sometimes z presses come out of nowhere */

#ifdef PHOTON_DND
					if( sDragService ) {
						nsDragService *d;
						nsIDragService *s = sDragService;
						d = ( nsDragService * )s;
						d->SetNativeDndData( widget, event );
						}
#endif

          ScreenToWidgetPos( ptrev->pos );
 	      	InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
          result = DispatchMouseEvent(theMouseEvent);
        	}
      	}
      	break;

      case Ph_EV_KEY:
        // Update the current input group for clipboard key events. This
        // covers both mozserver and mozilla.
        if (sClipboard)
          sClipboard->SetInputGroup(event->input_group);
				result = DispatchKeyEvent( (PhKeyEvent_t*) PhGetData( event ) );
        break;

      case Ph_EV_DRAG:
	    	{
          nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);

          switch(event->subtype) {

		  			case Ph_EV_DRAG_COMPLETE: 
            	{  
 		      		nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull,
                                               nsMouseEvent::eReal);
              PhPointerEvent_t* ptrev2 = (PhPointerEvent_t*) PhGetData( event );
              ScreenToWidgetPos( ptrev2->pos );
              InitMouseEvent(ptrev2, this, theMouseEvent, NS_MOUSE_BUTTON_UP,
                             nsMouseEvent::eLeftButton);
              result = DispatchMouseEvent(theMouseEvent);
            	}
							break;
		  			case Ph_EV_DRAG_MOTION_EVENT: {
      		    PhPointerEvent_t* ptrev2 = (PhPointerEvent_t*) PhGetData( event );
      		    ScreenToWidgetPos( ptrev2->pos );
  	  		    InitMouseEvent(ptrev2, this, theMouseEvent, NS_MOUSE_MOVE );
      		    result = DispatchMouseEvent(theMouseEvent);
							}
							break;
		  			}
					}
        break;

      case Ph_EV_BOUNDARY:
				PRUint32 evtype;

        switch( event->subtype ) {
          case Ph_EV_PTR_ENTER:
					case Ph_EV_PTR_ENTER_FROM_CHILD:
						evtype = NS_MOUSE_ENTER;
            break;
					case Ph_EV_PTR_LEAVE_TO_CHILD:
          case Ph_EV_PTR_LEAVE:
						evtype = NS_MOUSE_EXIT;
            break;
          default:
						evtype = 0;
            break;
        	}

				if( evtype != 0 ) {
					PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
					nsMouseEvent theMouseEvent(PR_TRUE, 0, nsnull,
                                               nsMouseEvent::eReal);
					ScreenToWidgetPos( ptrev->pos );
					InitMouseEvent( ptrev, this, theMouseEvent, evtype );
					result = DispatchMouseEvent( theMouseEvent );
					}
        break;
    	}

  return result;
	}



//-------------------------------------------------------------------------
//
// the nsWidget raw event callback for all nsWidgets in this toolkit
//
//-------------------------------------------------------------------------
int nsWidget::RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {

  // Get the window which caused the event and ask it to process the message
  nsWidget *someWidget = (nsWidget*) data;

  if( someWidget && someWidget->mIsDestroying == PR_FALSE && someWidget->HandleEvent( widget, cbinfo ) )
		return Pt_END; // Event was consumed

  return Pt_CONTINUE;
	}


int nsWidget::GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) 
{
  nsWidget *pWidget = (nsWidget *) data;

	if( PtWidgetIsClass( widget, PtWindow ) ) {
		if( pWidget->mEventCallback ) {
			/* the WM_ACTIVATE code */
			pWidget->DispatchStandardEvent(NS_ACTIVATE);
			return Pt_CONTINUE;
			}
		}

	pWidget->DispatchStandardEvent(NS_GOTFOCUS);

  return Pt_CONTINUE;
}

int nsWidget::LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) 
{
  nsWidget *pWidget = (nsWidget *) data;
 	pWidget->DispatchStandardEvent(NS_LOSTFOCUS);
  return Pt_CONTINUE;
}

int nsWidget::DestroyedCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {
  nsWidget *pWidget = (nsWidget *) data;
  if( !pWidget->mIsDestroying ) pWidget->OnDestroy();
  return Pt_CONTINUE;
	}

#ifdef PHOTON_DND
void nsWidget::ProcessDrag( PhEvent_t *event, PRUint32 aEventType, PhPoint_t *pos ) {
	nsCOMPtr<nsIDragSession> currSession;
	sDragService->GetCurrentSession ( getter_AddRefs(currSession) );
	if( !currSession ) return;

	int action = nsIDragService::DRAGDROP_ACTION_NONE;
	nsDragService *d = ( nsDragService * ) sDragService;
	
  if( d->mActionType & nsIDragService::DRAGDROP_ACTION_MOVE )
    action = nsIDragService::DRAGDROP_ACTION_MOVE;
  else if( d->mActionType & nsIDragService::DRAGDROP_ACTION_LINK )
    action = nsIDragService::DRAGDROP_ACTION_LINK;
  else if( d->mActionType & nsIDragService::DRAGDROP_ACTION_COPY )
    action = nsIDragService::DRAGDROP_ACTION_COPY;

	currSession->SetDragAction( action );

	DispatchDragDropEvent( event, aEventType, pos );

	int old_subtype = event->subtype;
	event->subtype = Ph_EV_DND_ENTER;

	PRBool canDrop;
	currSession->GetCanDrop(&canDrop);
	if(!canDrop) {
		static PhCharacterCursorDescription_t nodrop_cursor = {  { Ph_CURSOR_NOINPUT, sizeof(PhCharacterCursorDescription_t) }, PgRGB( 255, 255, 224 ) };
		PhAckDnd( event, Ph_EV_DND_MOTION, ( PhCursorDescription_t * ) &nodrop_cursor );
		}
	else {
		static PhCharacterCursorDescription_t drop_cursor = { { Ph_CURSOR_PASTE, sizeof(PhCharacterCursorDescription_t) }, PgRGB( 255, 255, 224 ) };
		PhAckDnd( event, Ph_EV_DND_MOTION, ( PhCursorDescription_t * ) &drop_cursor );
		}

	event->subtype = old_subtype;

	// Clear the cached value
	currSession->SetCanDrop(PR_FALSE);
	}

void nsWidget::DispatchDragDropEvent( PhEvent_t *phevent, PRUint32 aEventType, PhPoint_t *pos ) {
  nsEventStatus status;
  nsMouseEvent event(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);

  InitEvent( event, aEventType );

  event.refPoint.x = pos->x;
  event.refPoint.y = pos->y;

	PhDndEvent_t *dnd = ( PhDndEvent_t * ) PhGetData( phevent );
  event.isControl = ( dnd->key_mods & Pk_KM_Ctrl ) ? PR_TRUE : PR_FALSE;
  event.isShift   = ( dnd->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
  event.isAlt     = ( dnd->key_mods & Pk_KM_Alt ) ? PR_TRUE : PR_FALSE;
  event.isMeta    = PR_FALSE;

	event.widget = this;

///* ATENTIE */ printf("DispatchDragDropEvent pos=%d %d widget=%p\n", event.refPoint.x, event.refPoint.y, this );

  DispatchEvent( &event, status );
	}

int nsWidget::DndCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {
	nsWidget *pWidget = (nsWidget *) data;
	PtDndCallbackInfo_t *cbdnd = (  PtDndCallbackInfo_t * ) cbinfo->cbdata;

	static PtDndFetch_t dnd_data_template = { "Mozilla", "dnddata", Ph_TRANSPORT_INLINE, Pt_DND_SELECT_MOTION,
                        NULL, NULL, NULL, NULL, NULL };

///* ATENTIE */ printf( "In nsWidget::DndCallback subtype=%d\n", cbinfo->reason_subtype );

	PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( cbinfo->event );
//printf("Enter pos=%d %d\n", ptrev->pos.x, ptrev->pos.y );
	pWidget->ScreenToWidgetPos( ptrev->pos );
//printf("After trans pos=%d %d pWidget=%p\n", ptrev->pos.x, ptrev->pos.y, pWidget );


	switch( cbinfo->reason_subtype ) {
		case Ph_EV_DND_ENTER: {
			sDragService->StartDragSession();
			pWidget->ProcessDrag( cbinfo->event, NS_DRAGDROP_ENTER, &ptrev->pos );

			PtDndSelect( widget, &dnd_data_template, 1, NULL, NULL, cbinfo );
			}
			break;

		case Ph_EV_DND_MOTION: {
			sDragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
			pWidget->ProcessDrag( cbinfo->event, NS_DRAGDROP_OVER, &ptrev->pos );
			}
			break;
		case Ph_EV_DND_DROP:
			nsDragService *d;
			d = ( nsDragService * )sDragService;
			if( d->SetDropData( (char*)cbdnd->data ) != NS_OK ) break;
			pWidget->ProcessDrag( cbinfo->event, NS_DRAGDROP_DROP, &ptrev->pos );
			sDragService->EndDragSession(PR_TRUE);
			((nsDragService*) sDragService)->SourceEndDrag();
			break;

		case Ph_EV_DND_LEAVE:
			pWidget->ProcessDrag( cbinfo->event, NS_DRAGDROP_EXIT, &ptrev->pos );
			sDragService->EndDragSession(PR_FALSE);
			break;

		case Ph_EV_DND_CANCEL:
			pWidget->ProcessDrag( cbinfo->event, NS_DRAGDROP_EXIT, &ptrev->pos );
			sDragService->EndDragSession(PR_TRUE);
			((nsDragService*) sDragService)->SourceEndDrag();
			break;
		}

	return Pt_CONTINUE;
	}
#endif /* PHOTON_DND */
