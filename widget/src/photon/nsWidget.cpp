/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *     Jerry.Kirk@Nexwarecorp.com
 *     Michael.Kedl@Nexwarecorp.com
 *	   Dale.Stansberry@Nexwarecorop.com
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

#include "nsIPref.h"
#include "nsPhWidgetLog.h"

#include <errno.h>

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

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
nsIRollupListener *nsWidget::gRollupListener = nsnull;
nsIWidget         *nsWidget::gRollupWidget = nsnull;

/* These are used to keep Popup Menus from drawing twice when they are put away */
/* because of extra EXPOSE events from Photon */

//
// Keep track of the last widget being "dragged"
//
#if 0
DamageQueueEntry   *nsWidget::mDmgQueue = nsnull;
PtWorkProcId_t     *nsWidget::mWorkProcID = nsnull;
PRBool              nsWidget::mDmgQueueInited = PR_FALSE;
#endif
nsILookAndFeel     *nsWidget::sLookAndFeel = nsnull;
PRUint32            nsWidget::sWidgetCount = 0;
nsWidget           *nsWidget::sFocusWidget = nsnull;
PRBool              nsWidget::sJustGotActivated = PR_FALSE;
PRBool              nsWidget::sJustGotDeactivated = PR_TRUE;

/* Enable this to queue widget damage, this should be ON by default */
#define ENABLE_DAMAGE_QUEUE

/* Enable this causing extra redraw when the RELEASE is called */
//#define ENABLE_DAMAGE_QUEUE_HOLDOFF


nsWidget::nsWidget()
{
  // XXX Shouldn't this be done in nsBaseWidget?
  // NS_INIT_REFCNT();

  if (!sLookAndFeel) {
    if (NS_OK != nsComponentManager::CreateInstance(kLookAndFeelCID,
                                                    nsnull,
                                                    NS_GET_IID(nsILookAndFeel),
                                                    (void**)&sLookAndFeel))
		sLookAndFeel = nsnull;
  	}

  if( sLookAndFeel )
    sLookAndFeel->GetColor( nsILookAndFeel::eColor_WindowBackground, mBackground );

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
  mIsDragDest = PR_FALSE;
  mIsToplevel = PR_FALSE;
  mListenForResizes = PR_FALSE;
  mHasFocus = PR_FALSE;
#if 0
  if (NS_OK == nsComponentManager::CreateInstance(kRegionCID,
                                                  nsnull,
                                                  NS_GET_IID(nsIRegion),
                                                  (void**)&mUpdateArea))
  {
    mUpdateArea->Init();
    mUpdateArea->SetTo(0, 0, 0, 0);
  }
#endif
  sWidgetCount++;
}


nsWidget::~nsWidget( ) {
#if 0
	NS_IF_RELEASE(mUpdateArea);
#endif
  // it's safe to always call Destroy() because it will only allow itself
  // to be called once
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
NS_IMPL_ISUPPORTS_INHERITED(nsWidget, nsBaseWidget, nsIKBStateControl)

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

	if( sFocusWidget == this ) sFocusWidget = nsnull;

  return NS_OK;
	}

// this is the function that will destroy the native windows for this widget.

/* virtual */
void nsWidget::DestroyNative( void ) {
  if( mWidget ) {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
#if 0
	  RemoveDamagedWidget(mWidget);
#endif
	  EnableDamage( mWidget, PR_FALSE );
	  PtDestroyWidget( mWidget );
	  EnableDamage( mWidget, PR_TRUE );
    mWidget = nsnull;
  	}
	}

// make sure that we clean up here

void nsWidget::OnDestroy( ) {
  mOnDestroyCalled = PR_TRUE;
  // release references to children, device context, toolkit + app shell
  nsBaseWidget::OnDestroy();

  nsCOMPtr<nsIWidget> kungFuDeathGrip = this;
  DispatchStandardEvent(NS_DESTROY);
	}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent( void ) {
  nsIWidget* result = mParent;
  if( mParent ) NS_ADDREF( result );
  return result;
	}


//////////////////////////////////////////////////////////////////////
//
// nsIKBStateControl Mehthods
//
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsWidget::ResetInputState( ) {
  return NS_OK;
	}

// to be implemented
NS_IMETHODIMP nsWidget::PasswordFieldInit( ) {
  return NS_OK;
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
		  EnableDamage( mWidget, PR_FALSE );
		  PtRealizeWidget(mWidget);


		  if( mWidget->rid == -1 ) {
			  EnableDamage( mWidget, PR_TRUE );
			  NS_ASSERTION(0,"nsWidget::Show mWidget's rid == -1\n");
			  mShown = PR_FALSE; 
			  return NS_ERROR_FAILURE;
		  	}
		  PtSetArg(&arg, Pt_ARG_FLAGS, 0, Pt_DELAY_REALIZE);
		  PtSetResources(mWidget, 1, &arg);
		  EnableDamage( mWidget, PR_TRUE );
		  PtDamageWidget(mWidget);
			}
		else {
			PtWidgetToFront( mWidget );
//			EnableDamage( mWidget, PR_FALSE );
			if( !mShown || !( mWidget->flags & Pt_REALIZED ) ) PtRealizeWidget( mWidget );
//			EnableDamage( mWidget, PR_TRUE );
			}
  	}
  else {
		if( mWindowType != eWindowType_child ) {
      EnableDamage( mWidget, PR_FALSE );
      PtUnrealizeWidget(mWidget);


      EnableDamage( mWidget, PR_TRUE );

      PtSetArg(&arg, Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
      PtSetResources(mWidget, 1, &arg);
			}
		else {
			EnableDamage( mWidget, PR_FALSE );
			PtWidgetToBack( mWidget );
			if( mShown ) PtUnrealizeWidget( mWidget );
			EnableDamage( mWidget, PR_TRUE );
			}
  	}

  mShown = bState;
  return NS_OK;
	}


/* This got moved to nsWindow.cpp */
NS_IMETHODIMP nsWidget::CaptureRollupEvents( nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent ) {
  return NS_OK;
	}

// nsWidget::SetModal - Not Implemented
NS_IMETHODIMP nsWidget::SetModal( PRBool aModal ) {
  return NS_ERROR_FAILURE;
	}

NS_METHOD nsWidget::IsVisible( PRBool &aState ) {
  /* Try a simpler algorithm */
  aState = mShown;
  return NS_OK;
	}

//-------------------------------------------------------------------------
//
// Constrain a potential move to see if it fits onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::ConstrainPosition( PRInt32 *aX, PRInt32 *aY ) {
  return NS_OK;
	}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Move( PRInt32 aX, PRInt32 aY ) {

	if( ( mBounds.x == aX ) && ( mBounds.y == aY ) ) 
	   return NS_OK;

	mBounds.x = aX;
	mBounds.y = aY;
	
	if(mWidget) {
		PtArg_t arg;
		PhPoint_t *oldpos;
		PhPoint_t pos;
		
		pos.x = aX;
		pos.y = aY;
		
		//  EnableDamage( mWidget, PR_FALSE );
		 
		PtSetArg( &arg, Pt_ARG_POS, &oldpos, 0 );
		PtGetResources( mWidget, 1, &arg );
		
		
		if(( oldpos->x != pos.x ) || ( oldpos->y != pos.y )) {
			PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
			PtSetResources( mWidget, 1, &arg );
		}
		
		//  EnableDamage( mWidget, PR_TRUE );
	}
  return NS_OK;
}


NS_METHOD nsWidget::Resize( PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint ) {

	if( ( mBounds.width == aWidth ) && ( mBounds.height == aHeight ) ) 
	   return NS_OK;

    mBounds.width  = aWidth;
    mBounds.height = aHeight;

    if( mWidget ) {
		PtArg_t arg;
		
		/* Add the border to the size of the widget */
		PhDim_t dim = { aWidth, aHeight };

		EnableDamage( mWidget, PR_FALSE );
		
		PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
		PtSetResources( mWidget, 1, &arg );

		EnableDamage( mWidget, PR_TRUE );
	}
	return NS_OK;
}



NS_METHOD nsWidget::Resize( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint ) {
	Move(aX,aY);
	Resize(aWidth,aHeight,aRepaint);
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
		nsSizeEvent event;

		/* Stole this from GTK */
	    InitEvent(event, NS_SIZE);
		event.eventStructType = NS_SIZE_EVENT;

		nsRect *foo = new nsRect(0, 0, aRect.width, aRect.height);
		event.windowSize = foo;

		event.point.x = 0;
		event.point.y = 0;
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
  nsGUIEvent event;

  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;
  PRBool result = DispatchWindowEvent(&event);

  return result;
	}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Enable( PRBool bState ) {
	if( mWidget ) {
	  PtArg_t arg;
	  if( bState ) 
		   PtSetArg( &arg, Pt_ARG_FLAGS, 0, Pt_BLOCKED );
	  else 
		   PtSetArg( &arg, Pt_ARG_FLAGS, Pt_BLOCKED, Pt_BLOCKED );
		PtSetResources( mWidget, 1, &arg );
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFocus( PRBool aRaise ) {

  if(sFocusWidget == this)
    return NS_OK;

  // call this so that any cleanup will happen that needs to...
  if(sFocusWidget && mWidget->parent)
    sFocusWidget->LoseFocus();

  if(!mWidget->parent)
    return NS_OK;

  sFocusWidget = this;

  if( mWidget && mWidget->parent) {
       PtContainerGiveFocus( mWidget, NULL );
  	}

  DispatchStandardEvent(NS_GOTFOCUS);
  mHasFocus = PR_TRUE;
  return NS_OK;
}


void nsWidget::LoseFocus( void ) 
{

  // doesn't do anything.  needed for nsWindow housekeeping, really.
  if( mHasFocus == PR_FALSE ) return;
  
  mHasFocus = PR_FALSE;
  sFocusWidget = nsnull;
}


//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics *nsWidget::GetFont( void ) {
  NS_NOTYETIMPLEMENTED("nsWidget::GetFont");
  return nsnull;
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
		char *str = aString->ToNewCString();

		PtSetArg( &arg, Pt_ARG_TEXT_FONT, str, 0 );
		PtSetResources( mWidget, 1, &arg );

		delete [] str;
		NS_RELEASE(mFontMetrics);
		}
	return NS_OK;
	}

NS_METHOD nsWidget::SetBackgroundColor( const nscolor &aColor ) {
  nsBaseWidget::SetBackgroundColor( aColor );

	if( mWidget ) {
		PtArg_t   arg;
		PgColor_t color = NS_TO_PH_RGB( aColor );
		
		PtSetArg( &arg, Pt_ARG_FILL_COLOR, color, 0 );
		PtSetResources( mWidget, 1, &arg );
  	}
  return NS_OK;
	}

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetCursor( nsCursor aCursor ) {
  unsigned short curs = 0;
  PgColor_t color = Ph_CURSOR_DEFAULT_COLOR;

  // Only change cursor if it's changing
  if( aCursor != mCursor ) {
    switch( aCursor ) {
			case eCursor_sizeNW:
				curs = Ph_CURSOR_DRAG_TL;
				break;
			case eCursor_sizeSE:
				curs = Ph_CURSOR_DRAG_BR;
				break;
			case eCursor_sizeNE:
				curs = Ph_CURSOR_DRAG_TL;
				break;
			case eCursor_sizeSW:
				curs = Ph_CURSOR_DRAG_BL;
				break;

			case eCursor_crosshair:
				curs = Ph_CURSOR_CROSSHAIR;
				break;

			case eCursor_copy:
			case eCursor_alias:
			case eCursor_context_menu:
				break;

			case eCursor_cell:
				break;

			case eCursor_spinning:
				break;

			case eCursor_count_up:
			case eCursor_count_down:
			case eCursor_count_up_down:
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

  		case eCursor_sizeWE:
  		  curs = Ph_CURSOR_DRAG_HORIZONTAL;
  		  break;

  		case eCursor_sizeNS:
  		  curs = Ph_CURSOR_DRAG_VERTICAL;
  		  break;

  		// REVISIT - Photon does not have the following cursor types...
  		case eCursor_arrow_north:
  		case eCursor_arrow_north_plus:
  		  curs = Ph_CURSOR_DRAG_TOP;
  		  break;

  		case eCursor_arrow_south:
  		case eCursor_arrow_south_plus:
  		  curs = Ph_CURSOR_DRAG_BOTTOM;
  		  break;

  		case eCursor_arrow_east:
  		case eCursor_arrow_east_plus:
  		  curs = Ph_CURSOR_DRAG_RIGHT;
  		  break;

  		case eCursor_arrow_west:
  		case eCursor_arrow_west_plus:
  		  curs = Ph_CURSOR_DRAG_LEFT;
  		  break;

  		default:
  		  NS_ASSERTION(0, "Invalid cursor type");
  		  break;
  		}

  	if( mWidget && curs ) {
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

	if( !mWidget ) return NS_OK; // mWidget will be null during printing
	if( !PtWidgetIsRealized( mWidget ) ) return NS_OK;

	PtWidget_t *aWidget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);

#if 0
	nsRect   rect = mBounds;
	
	/* Damage has to be relative Widget coords */
	mUpdateArea->SetTo( rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height );

	if( aIsSynchronous ) UpdateWidgetDamage();
	else QueueWidgetDamage();
#else
	PtDamageWidget(aWidget);
	if (aIsSynchronous)
	   PtFlush();
#endif	
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

/* Returns only the rect that is inside of all my parents, problem on test 9 */
/* Traverse parent heirarchy and clip the passed-in rect bounds */
PRBool nsWidget::GetParentClippedArea( nsRect &rect ) {

  PtArg_t    arg;
  PhArea_t   *area;
  PtWidget_t *parent;
  PhPoint_t  offset;
  nsRect     rect2;
  PtWidget_t *disjoint = PtFindDisjoint( mWidget );

  // convert passed-in rect to absolute window coords first...
  PtWidgetOffset( mWidget, &offset );
  rect.x += offset.x;
  rect.y += offset.y;


  parent = PtWidgetParent( mWidget );
  while( parent ) {

    PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
    PtGetResources( parent, 1, &arg );

		rect2.width = area->size.w;
		rect2.height = area->size.h;

    if( (parent == disjoint) || (PtWidgetIsClass(parent, PtWindow)) || (PtWidgetIsClass(parent, PtRegion)) )
      rect2.x = rect2.y = 0;
	  else {
       PtWidgetOffset( parent, &offset );
       rect2.x = area->pos.x + offset.x;
       rect2.y = area->pos.y + offset.y;
     	}

    if( ( rect.x >= ( rect2.x + rect2.width )) ||   // rect is out of bounds to right
        (( rect.x + rect.width ) <= rect2.x ) ||   // rect is out of bounds to left
        ( rect.y >= ( rect2.y + rect2.height )) ||  // rect is out of bounds to bottom
        (( rect.y + rect.height ) <= rect2.y ) )   // rect is out of bounds to top
    			{
       		rect.width = 0;
      	  rect.height = 0;
      	  //printf( "  Out of bounds !\n" );
      	  break;
      	}
     else {
			if( rect.x < rect2.x ) {
				rect.width -= ( rect2.x - rect.x );
				rect.x = rect2.x;
				}
			if( rect.y < rect2.y ) {
				rect.height -= ( rect2.y - rect.y );
				rect.y = rect2.y;
				}

			if(( rect.x + rect.width ) > ( rect2.x + rect2.width ))
			rect.width = (rect2.x + rect2.width) - rect.x;

			if(( rect.y + rect.height ) > ( rect2.y + rect2.height ))
			rect.height = (rect2.y + rect2.height) - rect.y;
			}

		parent = PtWidgetParent( parent );
		}


  // convert rect back to widget coords...
  PtWidgetOffset( mWidget, &offset );
  rect.x -= offset.x;
  rect.y -= offset.y;

  if( rect.width && rect.height )
    return PR_TRUE;
  else return PR_FALSE;
	}



NS_METHOD nsWidget::Update( void ) {

	/* if the widget has been invalidated or damaged then re-draw it */
//	UpdateWidgetDamage();
PtFlush();
  return NS_OK;
	}


//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWidget::GetNativeData( PRUint32 aDataType ) {

  switch( aDataType ) {
  	case NS_NATIVE_WINDOW:
  	case NS_NATIVE_WIDGET:
  	case NS_NATIVE_PLUGIN_PORT:
  	  return (void *)mWidget;

  	case NS_NATIVE_DISPLAY:
  	  return nsnull;
	
  	case NS_NATIVE_GRAPHIC:
  	  return nsnull;

  	default:
  	  break;
  	}

  return nsnull;
	}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetColorMap( nsColorMap *aColorMap ) {
  return NS_OK;
	}

NS_METHOD nsWidget::ScrollWidgets( PRInt32 aDx, PRInt32 aDy ) {
  return NS_OK;   
	}

NS_METHOD nsWidget::Scroll( PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect ) {
  return NS_OK;
	}

NS_METHOD nsWidget::BeginResizingChildren( void ) 
{
	PtHold();
	return NS_OK;
}

NS_METHOD nsWidget::EndResizingChildren( void ) 
{
	PtRelease();
	return NS_OK;
}

NS_METHOD nsWidget::GetPreferredSize( PRInt32& aWidth, PRInt32& aHeight ) {
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
	}

NS_METHOD nsWidget::SetPreferredSize( PRInt32 aWidth, PRInt32 aHeight ) {
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
	}

NS_IMETHODIMP nsWidget::SetTitle( const nsString &aTitle ) {
  return NS_OK;
	}

// nsWidget::SetMenuBar - Not Implemented
NS_METHOD nsWidget::SetMenuBar( nsIMenuBar * aMenuBar ) {
  return NS_ERROR_FAILURE;
	}


NS_METHOD nsWidget::ShowMenuBar( PRBool aShow) {
  return NS_ERROR_FAILURE;
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

	sJustGotActivated = PR_TRUE;

  nsIWidget *baseParent = aInitData && (aInitData->mWindowType == eWindowType_dialog ||
    	aInitData->mWindowType == eWindowType_toplevel ) ?  nsnull : aParent;

  BaseCreate( baseParent, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData );

  mParent = aParent;

  if( aNativeParent ) {
    parentWidget = (PtWidget_t*)aNativeParent;
    // we've got a native parent so listen for resizes
    mListenForResizes = PR_TRUE;
  	}
  else if( aParent ) {
    parentWidget = (PtWidget_t*) (aParent->GetNativeData(NS_NATIVE_WIDGET));
  	}

  mBounds = aRect;

  CreateNative (parentWidget);

	if( aRect.width > 1 && aRect.height > 1 ) Resize(aRect.width, aRect.height, PR_FALSE);

  if( mWidget ) {
    SetInstance(mWidget, this);
    PtAddCallback( mWidget, Pt_CB_GOT_FOCUS, GotFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_LOST_FOCUS, LostFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_IS_DESTROYED, DestroyedCallback, this );
  	}

  DispatchStandardEvent(NS_CREATE);

  InitCallbacks();

  return NS_OK;
	}


//-------------------------------------------------------------------------
//
// create with nsIWidget parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    return(CreateWidget(aParent, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData,
                        nsnull));
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    return(CreateWidget(nsnull, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData,
                        aParent));
}


// nsWidget::ConvertToDeviceCoordinates - Not Implemented
void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
}


void nsWidget::InitEvent( nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint ) {
	event.widget = this;

	if( aPoint == nsnull ) {
		event.point.x = 0;
		event.point.y = 0;
		}    
	else {
		event.point.x = aPoint->x;
		event.point.y = aPoint->y;
		}

	event.time = PR_IntervalNow();
	event.message = aEventType;
	}


PRBool nsWidget::ConvertStatus( nsEventStatus aStatus ) {
  switch(aStatus) {
    case nsEventStatus_eIgnore:
      return(PR_FALSE);
    case nsEventStatus_eConsumeNoDefault:
      return(PR_TRUE);
    case nsEventStatus_eConsumeDoDefault:
      return(PR_FALSE);
    default:
      NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
      break;
  	}
  return PR_FALSE;
	}

PRBool nsWidget::DispatchWindowEvent( nsGUIEvent* event ) {
  nsEventStatus status;
  PRBool ret;

  DispatchEvent(event, status);

  ret = ConvertStatus(status);
  return ret;
	}


//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchStandardEvent( PRUint32 aMsg ) {
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event, aMsg);
  PRBool result = DispatchWindowEvent(&event);
  return result;
	}


//-------------------------------------------------------------------------
//
// Invokes callback and ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent( nsGUIEvent *aEvent, nsEventStatus &aStatus ) {

	/* Stolen from GTK */

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

   NS_RELEASE(aEvent->widget);

  return NS_OK;
	}

//==============================================================
void nsWidget::InitMouseEvent(PhPointerEvent_t *aPhButtonEvent,
                              nsWidget * aWidget,
                              nsMouseEvent &anEvent,
                              PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = aWidget;
  anEvent.eventStructType = NS_MOUSE_EVENT;

  if (aPhButtonEvent != nsnull) {
    anEvent.time =      PR_IntervalNow();
    anEvent.isShift =   ( aPhButtonEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhButtonEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhButtonEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
		anEvent.isMeta =		PR_FALSE;
    anEvent.point.x =   aPhButtonEvent->pos.x; 
    anEvent.point.y =   aPhButtonEvent->pos.y;
    anEvent.clickCount = aPhButtonEvent->click_count;
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
      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(aEvent));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
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

//-------------------------------------------------------------------------
// Old icky code I am trying to replace!
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchMouseEvent( PhPoint_t &aPos, PRUint32 aEvent ) {

  PRBool result = PR_FALSE;

  if( nsnull == mEventCallback && nsnull == mMouseListener ) return result;

  nsMouseEvent event;

  InitEvent( event, aEvent );
  event.eventStructType = NS_MOUSE_EVENT;
  event.point.x = aPos.x;
  event.point.y = aPos.y;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;       /* hack  makes the mouse not work */
  
  // call the event callback
  if( nsnull != mEventCallback ) {
    result = DispatchWindowEvent( &event );
    NS_IF_RELEASE(event.widget);
    return result;
  	}

  if( nsnull != mMouseListener ) { 
    switch( aEvent ) {
      case NS_MOUSE_MOVE:
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        break;

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    	} // switch
  	}
  return result;
	}

void nsWidget::InitCallbacks( char * aName ) {
	}


PRBool nsWidget::SetInstance( PtWidget_t * pWidget, nsWidget * inst ) {
  PRBool res = PR_FALSE;

  if( pWidget ) {
    PtArg_t arg;
    void *data = (void *)inst;

    PtSetArg(&arg, Pt_ARG_USER_DATA, &data, sizeof(void *) );
    if( PtSetResources( pWidget, 1, &arg) == 0 )
      res = PR_TRUE;
  	}
  return res;
	}


nsWidget* nsWidget::GetInstance( PtWidget_t * pWidget ) {
  if( pWidget ) {
    PtArg_t  arg;
    void     **data;
    
    PtSetArg( &arg, Pt_ARG_USER_DATA, &data, 0 );
    if( PtGetResources( pWidget, 1, &arg ) == 0 ) {
      if( data )
        return (nsWidget *) *data;
    	}
  	}
  return NULL;
	}


// Input keysym is in gtk format; output is in NS_VK format
PRUint32 nsWidget::nsConvertKey(unsigned long keysym, PRBool *aIsChar ) {

  struct nsKeyConverter {
    PRUint32       vkCode; // Platform independent key code
    unsigned long  keysym; // Photon key_sym key code
    PRBool         isChar;
  };

 struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     Pk_Cancel, PR_FALSE },
  { NS_VK_BACK,       Pk_BackSpace, PR_FALSE },
  { NS_VK_TAB,        Pk_Tab, PR_FALSE },
  { NS_VK_CLEAR,      Pk_Clear, PR_FALSE },
  { NS_VK_RETURN,     Pk_Return, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_L, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_R, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_L, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_R, PR_FALSE },
  { NS_VK_CONTROL,    Pk_Control_L, PR_FALSE },
  { NS_VK_CONTROL,    Pk_Control_R, PR_FALSE },
  { NS_VK_ALT,        Pk_Alt_L, PR_FALSE },
  { NS_VK_ALT,        Pk_Alt_R, PR_FALSE },
  { NS_VK_PAUSE,      Pk_Pause, PR_FALSE },
  { NS_VK_CAPS_LOCK,  Pk_Caps_Lock, PR_FALSE },
  { NS_VK_ESCAPE,     Pk_Escape, PR_FALSE },
  { NS_VK_SPACE,      Pk_space, PR_TRUE },
  { NS_VK_PAGE_UP,    Pk_Pg_Up, PR_FALSE },
  { NS_VK_PAGE_DOWN,  Pk_Pg_Down, PR_FALSE },
  { NS_VK_END,        Pk_End, PR_FALSE },
  { NS_VK_HOME,       Pk_Home, PR_FALSE },
  { NS_VK_LEFT,       Pk_Left, PR_FALSE },
  { NS_VK_UP,         Pk_Up, PR_FALSE },
  { NS_VK_RIGHT,      Pk_Right, PR_FALSE },
  { NS_VK_DOWN,       Pk_Down, PR_FALSE },
  { NS_VK_PRINTSCREEN, Pk_Print, PR_FALSE },
  { NS_VK_INSERT,     Pk_Insert, PR_FALSE },
  { NS_VK_DELETE,     Pk_Delete, PR_FALSE },
  { NS_VK_COMMA,      Pk_comma, PR_TRUE },
  { NS_VK_PERIOD,     Pk_period, PR_TRUE },
  { NS_VK_SLASH,      Pk_slash, PR_TRUE },
  { NS_VK_OPEN_BRACKET,  Pk_bracketleft, PR_TRUE },
  { NS_VK_CLOSE_BRACKET, Pk_bracketright, PR_TRUE },
  { NS_VK_QUOTE,         Pk_quotedbl, PR_TRUE },
  { NS_VK_MULTIPLY,      Pk_KP_Multiply, PR_TRUE },
  { NS_VK_ADD,           Pk_KP_Add, PR_TRUE },
  { NS_VK_COMMA,         Pk_KP_Separator, PR_FALSE },
  { NS_VK_SUBTRACT,      Pk_KP_Subtract, PR_TRUE },
  { NS_VK_PERIOD,        Pk_KP_Decimal, PR_TRUE },
  { NS_VK_DIVIDE,        Pk_KP_Divide, PR_TRUE },
  { NS_VK_RETURN,        Pk_KP_Enter, PR_FALSE },
  { NS_VK_INSERT,        Pk_KP_0, PR_FALSE },
  { NS_VK_END,           Pk_KP_1, PR_FALSE },
  { NS_VK_DOWN,          Pk_KP_2, PR_FALSE },
  { NS_VK_PAGE_DOWN,     Pk_KP_3, PR_FALSE },
  { NS_VK_LEFT,          Pk_KP_4, PR_FALSE },
  { NS_VK_NUMPAD5,       Pk_KP_5, PR_FALSE },
  { NS_VK_RIGHT,         Pk_KP_6, PR_FALSE },
  { NS_VK_HOME,          Pk_KP_7, PR_FALSE },
  { NS_VK_UP,            Pk_KP_8, PR_FALSE },
  { NS_VK_PAGE_UP,       Pk_KP_9, PR_FALSE }
  };

  const int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  if (aIsChar) {
	/* Default this to TRUE */
    *aIsChar = PR_TRUE;
  	}
	
  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  if (keysym >= Pk_a && keysym <= Pk_z)
     return keysym - Pk_a + NS_VK_A;

  if (keysym >= Pk_A && keysym <= Pk_Z)
     return keysym - Pk_A + NS_VK_A;

  if (keysym >= Pk_0 && keysym <= Pk_9)
     return keysym - Pk_0 + NS_VK_0;
		  
  if (keysym >= Pk_F1 && keysym <= Pk_F24) {
     *aIsChar = PR_FALSE;
     return keysym - Pk_F1 + NS_VK_F1;
  	}

  for (int i = 0; i < length; i++) {
    if( nsKeycodes[i].keysym == keysym ) {
      if( aIsChar ) *aIsChar = (nsKeycodes[i].isChar);
      return (nsKeycodes[i].vkCode);
    	}
  	}

  return((int) 0);
	}

//==============================================================
void nsWidget::InitKeyEvent(PhKeyEvent_t *aPhKeyEvent,
                            nsWidget * aWidget,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType) {

  if( aPhKeyEvent != nsnull ) {
  
    anEvent.message = aEventType;
    anEvent.widget  = aWidget;
    anEvent.eventStructType = NS_KEY_EVENT;
    anEvent.nativeMsg = (void *)aPhKeyEvent;
    anEvent.time =      PR_IntervalNow();
    anEvent.point.x = 0; 
    anEvent.point.y = 0;


    PRBool IsChar;
    unsigned long keysym;
	
    if (Pk_KF_Sym_Valid & aPhKeyEvent->key_flags)
        keysym = nsConvertKey(aPhKeyEvent->key_sym, &IsChar);
    else
		/* Need this to support key release events on numeric key pad */
    	keysym = nsConvertKey(aPhKeyEvent->key_cap, &IsChar);



    anEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
    anEvent.isMeta =    PR_FALSE;

    if ((aEventType == NS_KEY_PRESS) && (IsChar == PR_TRUE)) {
      anEvent.charCode = aPhKeyEvent->key_sym;
      anEvent.keyCode =  0;  /* I think the spec says this should be 0 */
      //printf("nsWidget::InitKeyEvent charCode=<%d>\n", anEvent.charCode);

      if ((anEvent.isControl) || (anEvent.isAlt))
        anEvent.charCode = aPhKeyEvent->key_cap;
	  	else
	  	  anEvent.isShift = anEvent.isControl = anEvent.isAlt = anEvent.isMeta = PR_FALSE;
    	}
		else {
 	    anEvent.charCode = 0; 
 	    anEvent.keyCode  =  (keysym  & 0x00FF);
  	  }
  	}
	}


PRBool  nsWidget::DispatchKeyEvent( PhKeyEvent_t *aPhKeyEvent ) {
  NS_ASSERTION(aPhKeyEvent, "nsWidget::DispatchKeyEvent a NULL PhKeyEvent was passed in");

  nsKeyEvent keyEvent;
  PRBool result = PR_FALSE;

  if ( (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid) == 0) {
		//printf("nsWidget::DispatchKeyEvent throwing away invalid key: Modifiers Valid=<%d,%d,%d> this=<%p>\n",
		//(aPhKeyEvent->key_flags & Pk_KF_Scan_Valid), (aPhKeyEvent->key_flags & Pk_KF_Sym_Valid), (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid), this );
		return PR_TRUE;
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
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_DOWN);
    result = w->OnKey(keyEvent); 

    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_PRESS);
    result = w->OnKey(keyEvent); 
  	}
  else if (aPhKeyEvent->key_flags & Pk_KF_Key_Repeat) {
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_PRESS);
    result = w->OnKey(keyEvent);   
  	}
  else if (PkIsKeyDown(aPhKeyEvent->key_flags) == 0) {
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_UP);
    result = w->OnKey(keyEvent); 
  	}

  w->Release();

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

///* ATENTIE */ printf( "!!!!!!!!!!! nsWidget::RawEventHandler widget=%p\n", widget );

  // kedl, shouldn't handle events if the window is being destroyed
  if ( (someWidget) &&
        (someWidget->mIsDestroying == PR_FALSE) &&
        (someWidget->nsWidget::HandleEvent( cbinfo ))
/* because nsWindow::HandleEvent() is simply calling nsWidget::HandleEvent(), try to save a call - it was (someWidget->HandleEvent( cbinfo )) */
      )
        return( Pt_END ); // Event was consumed

  return( Pt_CONTINUE );
	}


PRBool nsWidget::HandleEvent( PtCallbackInfo_t* aCbInfo ) {
  PRBool  result = PR_TRUE; // call the default nsWindow proc
  PhEvent_t* event = aCbInfo->event;

	/* Photon 2 added a Consumed flag which indicates a  previous receiver of the */
	/* event has processed it */
	if (event->processing_flags & Ph_CONSUMED) return PR_TRUE;

	switch ( event->type ) {

      case Ph_EV_PTR_MOTION_NOBUTTON:
       	{
	    	PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    	nsMouseEvent   theMouseEvent;

        if( ptrev ) {

///* ATENTIE */ printf( "Ph_EV_PTR_MOTION_NOBUTTON (%d %d)\n", ptrev->pos.x, ptrev->pos.y );

        	ScreenToWidget( ptrev->pos );
 	      	InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
        	result = DispatchMouseEvent(theMouseEvent);
        	}
       	}
	    	break;

      case Ph_EV_BUT_PRESS:
       {
	    	PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
        nsMouseEvent   theMouseEvent;

				/* there is no big region to capture the button press events outside the menu's area - this will be checked here, and if the click was not on a menu item, close the menu, if any */
				nsWidget *parent = (nsWidget*)mParent;
				if( !parent || ( parent->mWindowType != eWindowType_popup ) ) {
					if( gRollupWidget && gRollupListener ) {
						gRollupListener->Rollup();
						break;
						}
					}

        if( ptrev ) {
          ScreenToWidget( ptrev->pos );

          if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
						InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_DOWN );
          else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
						InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_RIGHT_BUTTON_DOWN );
          else // middle button
						InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MIDDLE_BUTTON_DOWN );

		  		result = DispatchMouseEvent(theMouseEvent);

					// if we're a right-button-up we're trying to popup a context menu. send that event to gecko also
					if( ptrev->buttons & Ph_BUTTON_MENU ) {
						InitMouseEvent( ptrev, this, theMouseEvent, NS_CONTEXTMENU );
						result = DispatchMouseEvent( theMouseEvent );
						}
      	  }
      	 }
		break;		
		
		case Ph_EV_BUT_RELEASE:
		  {
			  PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
			  nsMouseEvent      theMouseEvent;
			  
			  if (event->subtype==Ph_EV_RELEASE_REAL || event->subtype==Ph_EV_RELEASE_PHANTOM) {
				  if (ptrev) {
					  ScreenToWidget( ptrev->pos );
					  if ( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
						 InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_UP );
					  else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
						 InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_RIGHT_BUTTON_UP );
					  else // middle button
						 InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MIDDLE_BUTTON_UP );
					  
					  result = DispatchMouseEvent(theMouseEvent);
				  }
			  }
			  else if (event->subtype==Ph_EV_RELEASE_OUTBOUND) {
				  PhRect_t rect = {{0,0},{0,0}};
				  PhRect_t boundary = {{-10000,-10000},{10000,10000}};
				  PhInitDrag( PtWidgetRid(mWidget), ( Ph_DRAG_KEY_MOTION | Ph_DRAG_TRACK ),&rect, &boundary, aCbInfo->event->input_group , NULL, NULL, NULL, NULL, NULL);
			  }
		  }
	    break;

		case Ph_EV_PTR_MOTION_BUTTON:
      	{
        PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    	nsMouseEvent   theMouseEvent;


        if( ptrev ) {
          ScreenToWidget( ptrev->pos );
 	      	InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
          result = DispatchMouseEvent(theMouseEvent);
        	}
      	}
      	break;

      case Ph_EV_KEY:
       	{
	    	PhKeyEvent_t* keyev = (PhKeyEvent_t*) PhGetData( event );
				result = DispatchKeyEvent(keyev);
       	}
        break;

      case Ph_EV_DRAG:
	    	{
          nsMouseEvent   theMouseEvent;

          switch(event->subtype) {

		  			case Ph_EV_DRAG_COMPLETE: 
            	{  
 		      		nsMouseEvent      theMouseEvent;
              PhPointerEvent_t* ptrev2 = (PhPointerEvent_t*) PhGetData( event );
              ScreenToWidget( ptrev2->pos );
              InitMouseEvent(ptrev2, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_UP );
              result = DispatchMouseEvent(theMouseEvent);
            	}
							break;
		  			case Ph_EV_DRAG_MOTION_EVENT: {
      		    PhPointerEvent_t* ptrev2 = (PhPointerEvent_t*) PhGetData( event );
      		    ScreenToWidget( ptrev2->pos );
  	  		    InitMouseEvent(ptrev2, this, theMouseEvent, NS_MOUSE_MOVE );
      		    result = DispatchMouseEvent(theMouseEvent);
							}
							break;
		  			}
					}
        break;

      case Ph_EV_BOUNDARY:
        switch( event->subtype ) {
          case Ph_EV_PTR_ENTER:
						result = DispatchStandardEvent( NS_MOUSE_ENTER );
            break;
					case Ph_EV_PTR_LEAVE_TO_CHILD:
          case Ph_EV_PTR_LEAVE:
						result = DispatchStandardEvent( NS_MOUSE_EXIT );
            break;
          default:
            break;
        	}
        break;
    	}

  return result;
	}


void nsWidget::ScreenToWidget( PhPoint_t &pt ) {
  // pt is in screen coordinates
  // convert it to be relative to ~this~ widgets origin
  short x=0,y=0;

  PtGetAbsPosition( mWidget, &x, &y );

  pt.x -= x;
  pt.y -= y;
	}


//---------------------------------------------------------------------------
// nsWidget::InitDamageQueue()
//
// Starts a Photon background task that will flush widget damage when the
// app goes idle.
//---------------------------------------------------------------------------
#if 0
void nsWidget::InitDamageQueue( ) {

  mDmgQueue = nsnull;

  mWorkProcID = PtAppAddWorkProc( nsnull, WorkProc, &mDmgQueue );
  if( mWorkProcID ) {
    mDmgQueueInited = PR_TRUE;

#ifdef ENABLE_DAMAGE_QUEUE_HOLDOFF
		PtHold();
#endif

	  }
	}

//---------------------------------------------------------------------------
// nsWidget::QueueWidgetDamage()
//
// Adds this widget to the damage queue. The damage is accumulated in
// mUpdateArea.
//---------------------------------------------------------------------------
void nsWidget::QueueWidgetDamage( ) {
  if( !mDmgQueueInited ) InitDamageQueue();

  if( mWidget && mDmgQueueInited ) {
	  
    // See if we're already in the queue, if so don't add us again
    DamageQueueEntry *dqe;
    PRBool           found = PR_FALSE;
  
    dqe = mDmgQueue;

    while( dqe ) {
      if( dqe->widget == mWidget ) {
        found = PR_TRUE;
        break;
      	}
      dqe = dqe->next;
    	}

    if( !found ) {
      dqe = new DamageQueueEntry;
      if( dqe ) {
        dqe->widget = mWidget;
        dqe->inst = this;
        dqe->next = mDmgQueue;
        mDmgQueue = dqe;
      	}
    	}
  	}
	}


//---------------------------------------------------------------------------
// nsWidget::UpdateWidgetDamage()
//
// Cause the current widget damage to be repaired now. If this widget was
// queued, remove it from the queue.
//---------------------------------------------------------------------------
void nsWidget::UpdateWidgetDamage( ) {

	PhRect_t         extent;
	PhArea_t         area;
	nsRegionRectSet *regionRectSet = nsnull;
	PRUint32         len;
	PRUint32         i;
	nsRect           temp_rect;

	if( mWidget == NULL ) return;
	if( !PtWidgetIsRealized( mWidget ) ) return;
#if 0
	RemoveDamagedWidget( mWidget );
#endif
	if( mUpdateArea->IsEmpty( ) ) return;



	PtWidgetArea( mWidget, &area );
	
	if( PtWidgetIsClass(mWidget, PtWindow ) || PtWidgetIsClass( mWidget, PtRegion ) ) {
		area.pos.x = area.pos.y = 0;  
	}

	if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet))) {
		NS_ASSERTION(0,"nsWidget::UpdateWidgetDamaged Error mUpdateArea->GetRects returned NULL");
		return;
	}


	len = regionRectSet->mRectsLen;

	for( i=0; i<len; ++i ) {
		nsRegionRect *r = &(regionRectSet->mRects[i]);
		temp_rect.SetRect(r->x, r->y, r->width, r->height);
		
		if( GetParentClippedArea( temp_rect ) ) {
			extent.ul.x = temp_rect.x + area.pos.x;
			extent.ul.y = temp_rect.y + area.pos.y;
			extent.lr.x = extent.ul.x + temp_rect.width - 1;
			extent.lr.y = extent.ul.y + temp_rect.height - 1;
			
			PtDamageExtent( mWidget, &extent );
		}
	}
	
	// drop the const.. whats the right thing to do here?
	mUpdateArea->FreeRects(regionRectSet);
	
	//PtFlush();  
	mUpdateArea->SetTo(0,0,0,0);
}


void nsWidget::RemoveDamagedWidget( PtWidget_t *aWidget ) {

	if( mDmgQueueInited ) {
		DamageQueueEntry *dqe;
		DamageQueueEntry *last_dqe = nsnull;
  
		dqe = mDmgQueue;

		// If this widget is in the queue, remove it
		while( dqe ) {
			if( dqe->widget == aWidget ) {
				if( last_dqe ) last_dqe->next = dqe->next;
				else mDmgQueue = dqe->next;

				delete dqe;
				break;
				}
			last_dqe = dqe;
			dqe = dqe->next;
			}

		/* If removing the item empties the queue */
		if( nsnull == mDmgQueue ) {
			mDmgQueueInited = PR_FALSE;

#ifdef ENABLE_DAMAGE_QUEUE_HOLDOFF
			PtRelease();
#endif

			if( mWorkProcID ) PtAppRemoveWorkProc( nsnull, mWorkProcID );
			}
		}
	}


int nsWidget::WorkProc( void *data )
{
  DamageQueueEntry **dq = (DamageQueueEntry **) data;

  if( dq && (*dq) ) {
    DamageQueueEntry *dqe = *dq;
    DamageQueueEntry *last_dqe;
    PhRect_t extent;
    PhArea_t area;
    PtWidget_t *w;

/* This uses the new mUpdateRect */

    while( dqe ) {
      if( PtWidgetIsRealized( dqe->widget ) ) {
        nsRegionRectSet *regionRectSet = nsnull;
        PRUint32         len;
        PRUint32         i;
        nsRect           temp_rect;

        PtWidgetArea( dqe->widget, &area ); // parent coords

				if( dqe->inst->mUpdateArea->IsEmpty( ) ) {
          extent.ul.x = 0; //area.pos.x; // convert widget coords to parent
          extent.ul.y = 0; //area.pos.y;
          extent.lr.x = extent.ul.x + area.size.w - 1;
          extent.lr.y = extent.ul.y + area.size.h - 1;

          PtWidget_t *aPtWidget;
		  		nsWidget   *aWidget = GetInstance( (PtWidget_t *) dqe->widget );
          aPtWidget = (PtWidget_t *)aWidget->GetNativeData(NS_NATIVE_WIDGET);		  
          PtDamageExtent( aPtWidget, &extent);
					}
				else {
          dqe->inst->mUpdateArea->GetRects(&regionRectSet);

          len = regionRectSet->mRectsLen;
          for( i=0; i<len; ++i ) {
            nsRegionRect *r = &(regionRectSet->mRects[i]);

            // convert widget coords to parent
            temp_rect.SetRect(r->x, r->y, r->width, r->height);
            extent.ul.x = temp_rect.x; // + area.pos.x;
            extent.ul.y = temp_rect.y; // + area.pos.y;
            extent.lr.x = extent.ul.x + temp_rect.width - 1;
            extent.lr.y = extent.ul.y + temp_rect.height - 1;
		
            PtWidget_t *aPtWidget;
		    		nsWidget   *aWidget = GetInstance( (PtWidget_t *) dqe->widget );
            aPtWidget = (PtWidget_t *)aWidget->GetNativeData(NS_NATIVE_WIDGET);		  
            PtDamageExtent( aPtWidget, &extent);
          	}
  
          dqe->inst->mUpdateArea->FreeRects(regionRectSet);
          dqe->inst->mUpdateArea->SetTo(0,0,0,0);
				}
      }

      last_dqe = dqe;
      dqe = dqe->next;
      delete last_dqe;
    }

    *dq = nsnull;
    mDmgQueueInited = PR_FALSE;

#ifdef ENABLE_DAMAGE_QUEUE_HOLDOFF
		/* The matching PtHold is in nsWidget::InitDamageQueue */
		PtRelease();
#endif
	  }

  return Pt_END;
	}
#endif
int nsWidget::GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) 
{
  nsWidget *pWidget = (nsWidget *) data;

  if(!widget->parent)
  {
    	pWidget->DispatchStandardEvent(NS_GOTFOCUS);
  }

  if( !widget->parent || PtIsFocused(widget) != 2 ) return Pt_CONTINUE;

  pWidget->DispatchStandardEvent(NS_GOTFOCUS);

  if( !sJustGotActivated )
  {
		sJustGotActivated = PR_TRUE;
    sJustGotDeactivated = PR_FALSE;
    pWidget->DispatchStandardEvent(NS_ACTIVATE);
  }
	else sJustGotActivated = PR_FALSE;

  return Pt_CONTINUE;
}

int nsWidget::LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) 
{
//  nsWidget *pWidget = (nsWidget *) data;

  if(!widget->parent)
  {
    if(sFocusWidget)
    {
      if(!sJustGotDeactivated)
      {
        sFocusWidget->DispatchStandardEvent(NS_DEACTIVATE);
        sJustGotDeactivated = PR_TRUE;
      }
    }
 }

  if (!widget->parent || PtIsFocused(widget) != 2) return Pt_CONTINUE;

//  pWidget->DispatchStandardEvent(NS_LOSTFOCUS);

	sJustGotActivated = PR_FALSE;

  return Pt_CONTINUE;
}

int nsWidget::DestroyedCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {
  nsWidget *pWidget = (nsWidget *) data;

  if( !pWidget->mIsDestroying ) {
#if 0
	  pWidget->RemoveDamagedWidget(pWidget->mWidget);
#endif
	  pWidget->OnDestroy();
  	}
  return Pt_CONTINUE;
	}

void nsWidget::EnableDamage( PtWidget_t *widget, PRBool enable ) {

  PtWidget_t *top = PtFindDisjoint( widget );

  if( top ) {
    if( PR_TRUE == enable )
      PtEndFlux( top );
    else
      PtStartFlux( top );
  	}
	}

//#if defined(DEBUG)
/**************************************************************/
/* This was stolen from widget/src/xpwidgets/nsBaseWidget.cpp */
nsAutoString nsWidget::GuiEventToString(nsGUIEvent * aGuiEvent)
{
  NS_ASSERTION(nsnull != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName; eventName.AssignWithConversion("UNKNOWN");

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName = (const PRUnichar *)  _name ; break

  switch(aGuiEvent->message)
  {
    _ASSIGN_eventName(NS_BLUR_CONTENT,"NS_BLUR_CONTENT");
    _ASSIGN_eventName(NS_CONTROL_CHANGE,"NS_CONTROL_CHANGE");
    _ASSIGN_eventName(NS_CREATE,"NS_CREATE");
    _ASSIGN_eventName(NS_DESTROY,"NS_DESTROY");
    _ASSIGN_eventName(NS_DRAGDROP_GESTURE,"NS_DND_GESTURE");
    _ASSIGN_eventName(NS_DRAGDROP_DROP,"NS_DND_DROP");
    _ASSIGN_eventName(NS_DRAGDROP_ENTER,"NS_DND_ENTER");
    _ASSIGN_eventName(NS_DRAGDROP_EXIT,"NS_DND_EXIT");
    _ASSIGN_eventName(NS_DRAGDROP_OVER,"NS_DND_OVER");
    _ASSIGN_eventName(NS_FOCUS_CONTENT,"NS_FOCUS_CONTENT");
    _ASSIGN_eventName(NS_FORM_SELECTED,"NS_FORM_SELECTED");
    _ASSIGN_eventName(NS_FORM_CHANGE,"NS_FORM_CHANGE");
    _ASSIGN_eventName(NS_FORM_INPUT,"NS_FORM_INPUT");
    _ASSIGN_eventName(NS_FORM_RESET,"NS_FORM_RESET");
    _ASSIGN_eventName(NS_FORM_SUBMIT,"NS_FORM_SUBMIT");
    _ASSIGN_eventName(NS_GOTFOCUS,"NS_GOTFOCUS");
    _ASSIGN_eventName(NS_IMAGE_ABORT,"NS_IMAGE_ABORT");
    _ASSIGN_eventName(NS_IMAGE_ERROR,"NS_IMAGE_ERROR");
    _ASSIGN_eventName(NS_IMAGE_LOAD,"NS_IMAGE_LOAD");
    _ASSIGN_eventName(NS_KEY_DOWN,"NS_KEY_DOWN");
    _ASSIGN_eventName(NS_KEY_PRESS,"NS_KEY_PRESS");
    _ASSIGN_eventName(NS_KEY_UP,"NS_KEY_UP");
    _ASSIGN_eventName(NS_LOSTFOCUS,"NS_LOSTFOCUS");
    _ASSIGN_eventName(NS_MENU_SELECTED,"NS_MENU_SELECTED");
    _ASSIGN_eventName(NS_MOUSE_ENTER,"NS_MOUSE_ENTER");
    _ASSIGN_eventName(NS_MOUSE_EXIT,"NS_MOUSE_EXIT");
    _ASSIGN_eventName(NS_MOUSE_LEFT_BUTTON_DOWN,"NS_MOUSE_LEFT_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_LEFT_BUTTON_UP,"NS_MOUSE_LEFT_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_LEFT_CLICK,"NS_MOUSE_LEFT_CLICK");
    _ASSIGN_eventName(NS_MOUSE_LEFT_DOUBLECLICK,"NS_MOUSE_LEFT_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_BUTTON_DOWN,"NS_MOUSE_MIDDLE_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_BUTTON_UP,"NS_MOUSE_MIDDLE_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_CLICK,"NS_MOUSE_MIDDLE_CLICK");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_DOUBLECLICK,"NS_MOUSE_MIDDLE_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MOVE,"NS_MOUSE_MOVE");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_BUTTON_DOWN,"NS_MOUSE_RIGHT_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_BUTTON_UP,"NS_MOUSE_RIGHT_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_CLICK,"NS_MOUSE_RIGHT_CLICK");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_DOUBLECLICK,"NS_MOUSE_RIGHT_DBLCLICK");
    _ASSIGN_eventName(NS_MOVE,"NS_MOVE");
    _ASSIGN_eventName(NS_PAGE_LOAD,"NS_PAGE_LOAD");
    _ASSIGN_eventName(NS_PAGE_UNLOAD,"NS_PAGE_UNLOAD");
    _ASSIGN_eventName(NS_PAINT,"NS_PAINT");
    _ASSIGN_eventName(NS_XUL_POPUP_SHOWING,"NS_XUL_POPUP_SHOWING");
    _ASSIGN_eventName(NS_XUL_POPUP_SHOWN,"NS_XUL_POPUP_SHOWN");
    _ASSIGN_eventName(NS_XUL_POPUP_HIDING,"NS_XUL_POPUP_HIDING");
    _ASSIGN_eventName(NS_XUL_POPUP_HIDDEN,"NS_XUL_POPUP_HIDDEN");
    _ASSIGN_eventName(NS_XUL_COMMAND, "NS_XUL_COMMAND");
    _ASSIGN_eventName(NS_XUL_BROADCAST, "NS_XUL_BROADCAST");
    _ASSIGN_eventName(NS_XUL_COMMAND_UPDATE, "NS_XUL_COMMAND_UPDATE");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_NEXT,"NS_SB_LINE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_PREV,"NS_SB_LINE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_NEXT,"NS_SB_PAGE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_PREV,"NS_SB_PAGE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_POS,"NS_SB_POS");
    _ASSIGN_eventName(NS_SIZE,"NS_SIZE");

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %d",aGuiEvent->message);
      
      eventName = (const PRUnichar *) buf;
    }
    break;
  }
  
  return nsAutoString(eventName);
}



nsAutoString nsWidget::PhotonEventToString(PhEvent_t * aPhEvent)
{
  NS_ASSERTION(nsnull != aPhEvent,"cmon, null photon gui event.");

  nsAutoString eventName; eventName.AssignWithConversion("UNKNOWN");

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName = (const PRUnichar *) _name ; break
 
   switch ( aPhEvent->type )
    {
	  _ASSIGN_eventName(Ph_EV_KEY, "Ph_EV_KEY");
	  _ASSIGN_eventName(Ph_EV_DRAG, "Ph_EV_DRAG");
	  _ASSIGN_eventName(Ph_EV_PTR_MOTION_BUTTON, "Ph_EV_PTR_MOTION_BUTTON");
	  _ASSIGN_eventName(Ph_EV_PTR_MOTION_NOBUTTON, "Ph_EV_PTR_MOTION_NOBUTTON");
	  _ASSIGN_eventName(Ph_EV_BUT_PRESS, "Ph_EV_BUT_PRESS");
	  _ASSIGN_eventName(Ph_EV_BUT_RELEASE, "Ph_EV_BUT_RELEASE");
	  _ASSIGN_eventName(Ph_EV_BOUNDARY, "Ph_EV_BOUNDARY");
	  _ASSIGN_eventName(Ph_EV_WM, "Ph_EV_WM");
	  _ASSIGN_eventName(Ph_EV_EXPOSE, "Ph_EV_EXPOSE");	  

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %ld",aPhEvent->type);
      
      eventName = (const PRUnichar *) buf;
    }
    break;
  }
  
  return nsAutoString(eventName);
}
//#endif
