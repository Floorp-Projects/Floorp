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


#include <Pt.h>
#include <photon/PtServer.h>
#include "PtRawDrawContainer.h"
#include "nsCRT.h"

#include "nsWindow.h"
#include "nsWidgetsCID.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "nsIMenuBar.h"
#include "nsToolkit.h"
#include "nsIPref.h"

#include "nsClipboard.h"
#include "nsIRollupListener.h"

#include "nsIServiceManager.h"
#include "nsIAppShell.h"
#include "nsIDocShell.h"

#include "nsIViewManager.h"
#include "nsIXULWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWindowMediator.h"
#include "nsIPresShell.h"
#include "nsReadableUtils.h"

static PhTile_t *GetWindowClipping( PtWidget_t *aWidget );

nsIRollupListener *nsWindow::gRollupListener = nsnull;
nsIWidget         *nsWindow::gRollupWidget = nsnull;
static PtWidget_t	*gMenuRegion;

/* avoid making a costly PhWindowQueryVisible call */
static PhRect_t gConsoleRect;
static PRBool gConsoleRectValid = PR_FALSE;
#define QueryVisible( )	{\
													if( gConsoleRectValid == PR_FALSE ) { \
															PhWindowQueryVisible( Ph_QUERY_GRAPHICS, 0, 1, &gConsoleRect ); \
															gConsoleRectValid = PR_TRUE;\
															} \
													}

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  mClientWidget    = nsnull;
  mIsTooSmall      = PR_FALSE;
  mBorderStyle     = eBorderStyle_default;
  mWindowType      = eWindowType_child;
	mLastMenu				 = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
	nsWindow *p = (nsWindow*)mParent;
	if( p && p->mLastMenu == mWidget ) p->mLastMenu = nsnull;

  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.
  Destroy();
}

PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_IMETHODIMP nsWindow::WidgetToScreen( const nsRect& aOldRect, nsRect& aNewRect ) {

	PhPoint_t pos, offset;
	PtWidget_t *disjoint = PtFindDisjoint( mWidget );

	QueryVisible( );

	PtGetAbsPosition( disjoint, &pos.x, &pos.y );
	PtWidgetOffset( mWidget, &offset );
	aNewRect.x = pos.x + offset.x + aOldRect.x - gConsoleRect.ul.x;
	aNewRect.y = pos.y + offset.y + aOldRect.y - gConsoleRect.ul.y;

	aNewRect.width = aOldRect.width;
	aNewRect.height = aOldRect.height;

  return NS_OK;
	}

void nsWindow::DestroyNative(void)
{
  if ( mClientWidget )
  {
    PtDestroyWidget(mClientWidget );  
    mClientWidget = nsnull;
  }
  // destroy all of the children that are nsWindow() classes
  // preempting the gdk destroy system.
  if( mWidget ) DestroyNativeChildren();

  // Call the base class to actually PtDestroy mWidget.
  nsWidget::DestroyNative();
}

// this function will walk the list of children and destroy them.
// the reason why this is here is that because of the superwin code
// it's very likely that we will never get notification of the
// the destruction of the child windows.  so, we have to beat the
// layout engine to the punch.  CB 

void nsWindow::DestroyNativeChildren(void)
{
  for(PtWidget_t *w=PtWidgetChildFront( mWidget ); w; w=PtWidgetBrotherBehind( w )) 
  {
	  if( w->flags & Pt_DESTROYED ) continue;
	  nsWindow *childWindow = NS_STATIC_CAST(nsWindow *, GetInstance(w) );
		if( childWindow && !childWindow->mIsDestroying) childWindow->Destroy();
  }
}

NS_IMETHODIMP nsWindow::CaptureRollupEvents( nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent ) {
  PtWidget_t *grabWidget;
  grabWidget = mWidget;

  if (aDoCapture) {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
	
		/* Realize the gMenuRegion to capture events outside of the application's canvas when menus are displayed */
		/* Menus+submenus have the same parent. If the parent has mLastMenu set and realized, then use its region as "infront" */
		/* If not, then this widget ( mWidget ) becomes the mLastMenu and gets recorded into the parent */
		/* Different windows have different mLastMenu's */
		if( mWindowType == eWindowType_popup && !( PtWidgetFlags( gMenuRegion ) & Pt_REALIZED ) ) {

			PtWidget_t *pw = ((nsWindow*)mParent)->mLastMenu;

			if( pw && ( PtWidgetFlags( pw ) & Pt_REALIZED ) && PtWidgetRid( pw ) > 0 )
				PtSetResource( gMenuRegion, Pt_ARG_REGION_INFRONT, PtWidgetRid( pw ), 0 );
			else {
				if( !PtWidgetIsRealized( mWidget ) ) PtRealizeWidget( mWidget );
				PtSetResource( gMenuRegion, Pt_ARG_REGION_INFRONT, PtWidgetRid( mWidget ), 0 );
				((nsWindow*)mParent)->mLastMenu = mWidget;
				}

			PtRealizeWidget( gMenuRegion );
			}
  	}
	else {
    NS_IF_RELEASE(gRollupListener);
    gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
		gRollupWidget = nsnull;

		if( mWindowType == eWindowType_popup && ( PtWidgetFlags( gMenuRegion ) & Pt_REALIZED ) )
			PtUnrealizeWidget( gMenuRegion );
  	}
  
  return NS_OK;
}

NS_METHOD nsWindow::PreCreateWidget( nsWidgetInitData *aInitData ) {
  if (nsnull != aInitData) {
    SetWindowType( aInitData->mWindowType );
    SetBorderStyle( aInitData->mBorderStyle );
    return NS_OK;
  	}
  return NS_ERROR_FAILURE;
	}


//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative( PtWidget_t *parentWidget ) {
  PtArg_t   arg[25];
  int       arg_count = 0;
  PhPoint_t pos;
  PhDim_t   dim;
  unsigned  long render_flags;
  nsresult  result = NS_ERROR_FAILURE;

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width;
  dim.h = mBounds.height;

  switch( mWindowType )
  {
  case eWindowType_popup :
    mIsToplevel = PR_TRUE;
    break;
  case eWindowType_toplevel :
  case eWindowType_invisible :
    mIsToplevel = PR_TRUE;
    break;
  case eWindowType_dialog :
    mIsToplevel = PR_TRUE;
    break;
  case eWindowType_child :
    mIsToplevel = PR_FALSE;
    break;
  }
	
  if ( mWindowType == eWindowType_child )
  {
	arg_count = 0;
    PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0 , Pt_HIGHLIGHTED|Pt_GETS_FOCUS );
    PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_RED, 0 );
    PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
    mWidget = PtCreateWidget( PtRawDrawContainer, parentWidget, arg_count, arg );
  }
  else
  {
    // No border or decorations is the default
    render_flags = 0; // Ph_WM_RENDER_RESIZE;

    if( mWindowType != eWindowType_popup ) {

      #define PH_BORDER_STYLE_ALL  \
        Ph_WM_RENDER_TITLE | \
        Ph_WM_RENDER_CLOSE | \
        Ph_WM_RENDER_BORDER | \
        Ph_WM_RENDER_RESIZE | \
        Ph_WM_RENDER_MAX | \
        Ph_WM_RENDER_MIN | \
        Ph_WM_RENDER_MENU


      if( mBorderStyle & eBorderStyle_all )	render_flags |= PH_BORDER_STYLE_ALL;
      else
      {
        if( mBorderStyle & eBorderStyle_border )		render_flags |= Ph_WM_RENDER_BORDER;
        if( mBorderStyle & eBorderStyle_title )			render_flags |= ( Ph_WM_RENDER_TITLE | Ph_WM_RENDER_BORDER );
        if( mBorderStyle & eBorderStyle_close )			render_flags |= Ph_WM_RENDER_CLOSE;
        if( mBorderStyle & eBorderStyle_menu )			render_flags |= Ph_WM_RENDER_MENU;
        if( mBorderStyle & eBorderStyle_resizeh )		render_flags |= Ph_WM_RENDER_RESIZE;
        if( mBorderStyle & eBorderStyle_minimize )		render_flags |= Ph_WM_RENDER_MIN;
        if( mBorderStyle & eBorderStyle_maximize )		render_flags |= Ph_WM_RENDER_MAX;
      }
    }
	arg_count = 0;
    if (mWindowType == eWindowType_dialog)
    {
    	// center on parent
    	if (parentWidget)
    	{
    		PtCalcAbsPosition(parentWidget, NULL, &dim, &pos);
	    	PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    	}
	}
    else if ((mWindowType == eWindowType_toplevel) && parentWidget)
    {
			QueryVisible( );
      pos.x += gConsoleRect.ul.x;
      pos.y += gConsoleRect.ul.y;
    	PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    }

    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );

    /* Create Pop-up Menus as a PtRegion */

    if (!parentWidget)
      PtSetParentWidget( nsnull );
		
    if( mWindowType == eWindowType_popup ) {

			/* the gMenuRegion is used to capture events outside of the application's canvas when menus are displayed */
			if( !gMenuRegion ) {
				PtArg_t args[10];
				PtSetParentWidget( NULL );
				PtSetArg( &args[0], Pt_ARG_REGION_PARENT, 0, 0 );
	  		PtSetArg( &args[1], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0);
				PhArea_t area = { { 0, 0 }, { SHRT_MAX, SHRT_MAX } };
				PtSetArg( &args[2], Pt_ARG_AREA, &area, 0 );
				PtSetArg( &args[3], Pt_ARG_REGION_SENSE, Ph_EV_BUT_PRESS, Ph_EV_BUT_PRESS );
				PtSetArg( &args[4], Pt_ARG_REGION_FLAGS, Ph_FORCE_BOUNDARY, Ph_FORCE_BOUNDARY );
				PtSetArg( &args[5], Pt_ARG_CURSOR_TYPE, Ph_CURSOR_POINTER, 0 );
				gMenuRegion = PtCreateWidget( PtRegion, NULL, 6, args );
				PtAddEventHandler( gMenuRegion, Ph_EV_BUT_PRESS, MenuRegionCallback, this );
				}

	  	int	fields = Ph_REGION_PARENT|Ph_REGION_HANDLE| Ph_REGION_FLAGS|Ph_REGION_ORIGIN|Ph_REGION_EV_SENSE|Ph_REGION_EV_OPAQUE|Ph_REGION_RECT;
	  	int sense =  Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE | Ph_EV_BUT_REPEAT;
			PtCallback_t cb_destroyed = { MenuRegionDestroyed, NULL };

      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FIELDS,   fields, fields );
      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_PARENT,   Ph_ROOT_RID, 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_SENSE,    sense | Ph_EV_DRAG|Ph_EV_EXPOSE, sense | Ph_EV_DRAG|Ph_EV_EXPOSE);
      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_OPAQUE,   sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW|Ph_EV_BLIT, sense |Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW|Ph_EV_BLIT);
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_GETS_FOCUS | Pt_DELAY_REALIZE);
			PtSetArg( &arg[arg_count++], Pt_CB_DESTROYED, &cb_destroyed, 0 );
      mWidget = PtCreateWidget( PtRegion, parentWidget, arg_count, arg);

    	// Must also create the client-area widget
      arg_count = 0;
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_FLAGS, Pt_ANCHOR_ALL, ~0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PhRect_t anch_offset = {{0, 0},{0, 0}};
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, (Pt_HIGHLIGHTED | Pt_GETS_FOCUS));
      mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, arg_count, arg );
     }
	else {
		/* Dialog and TopLevel Windows */
		PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
		PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, -1 );
		PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_MANAGED_FLAGS, 0, Ph_WM_CLOSE );
		PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_NOTIFY_FLAGS, Ph_WM_CLOSE|Ph_WM_CONSWITCH|Ph_WM_FOCUS, ~0 );
		PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );

		PtRawCallback_t cb_raw = { Ph_EV_INFO, EvInfo, NULL };
		PtCallback_t cb_resize = { ResizeHandler, NULL };
		PtCallback_t cb_window = { WindowWMHandler, this };

		PtSetArg( &arg[arg_count++], Pt_CB_RESIZE, &cb_resize, NULL );
		PtSetArg( &arg[arg_count++], Pt_CB_RAW, &cb_raw, NULL );
		PtSetArg( &arg[arg_count++], Pt_CB_WINDOW, &cb_window, 0 );
		mWidget = PtCreateWidget( PtWindow, parentWidget, arg_count, arg );
		}
  }

  if( mWidget ) {
	  SetInstance( mWidget, this );
	  if( mClientWidget ) SetInstance( mClientWidget, this );
	  if( mWindowType == eWindowType_child ) {
      	PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
      	PtAddEventHandler( mWidget,
      	  Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON |
      	  Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY|Ph_EV_DRAG
      	  , RawEventHandler, this );
	
    		PtArg_t arg;
    		PtRawCallback_t callback;
		
				callback.event_mask = ( Ph_EV_KEY ) ;
				callback.event_f = RawEventHandler;
				callback.data = this;
				PtSetArg( &arg, Pt_CB_FILTER, &callback, 0 );
				PtSetResources( mWidget, 1, &arg );
    		}
    else if( mWindowType == eWindowType_popup ) {
      		PtAddEventHandler( mClientWidget,
      			 	Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON | 
      		  	Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY,
      			  RawEventHandler, this );

      		PtAddEventHandler( mWidget, Ph_EV_DRAG, RawEventHandler, this );

      		PtArg_t arg;
      		PtRawCallback_t callback;
		
					callback.event_mask = ( Ph_EV_KEY ) ;
					callback.event_f = RawEventHandler;
					callback.data = this;
					PtSetArg( &arg, Pt_CB_FILTER, &callback, 0 );
					PtSetResources( mClientWidget, 1, &arg );
			
    		  PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
					}
    else if( !parentWidget ) {
       if( mClientWidget ) PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull );
    	}

    // call the event callback to notify about creation
    DispatchStandardEvent( NS_CREATE );

    result = NS_OK;
  	}

  /* force SetCursor to actually set the cursor, even though our internal state indicates that we already
     have the standard cursor */
  mCursor = eCursor_wait;
  SetCursor( mCursor );

  return result;
	}


//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWindow::GetNativeData(PRUint32 aDataType)
{
  switch(aDataType)
  {
  case NS_NATIVE_WINDOW:
    if( !mWidget )	return (void *)mWidget;

  case NS_NATIVE_WIDGET:
		if (mClientWidget)	return (void *) mClientWidget;
		else	return (void *) mWidget;
  }
  	  	
  return nsWidget::GetNativeData(aDataType);
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll( PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect ) {
	PtWidget_t  *widget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);

	/* If aDx and aDy are 0 then skip it or if widget == null */
	if( ( !aDx && !aDy ) || (!widget )) return NS_OK;

	PtStartFlux( widget );

	PtWidget_t *w;
	for( w=PtWidgetChildFront( widget ); w; w=PtWidgetBrotherBehind( w )) {
		PtArg_t arg;
		PhPoint_t  p;
		p.x = w->area.pos.x + aDx;
		p.y = w->area.pos.y + aDy;

		PtSetArg( &arg, Pt_ARG_POS, &p, 0 );
		PtSetResources( w, 1, &arg ) ;

		nsWindow *pWin = (nsWindow *) GetInstance(w);
		if (pWin) {
			pWin->mBounds.x += aDx;
			pWin->mBounds.y += aDy;
			}
		}

	PtEndFlux( widget);

	PhRect_t source = {{widget->area.pos.x, widget->area.pos.y},{widget->area.pos.x+ widget->area.size.w-1, widget->area.pos.y + widget->area.size.h-1}};
	PhPoint_t point = { aDx, aDy };

	if( !widget->damage_list )
		PtBlit( widget, &source, &point );
	else {
		/* first noticed as a scrolling problem in netscape email */
		/* the scrolling should be clipped out by the rectangles given by Invalidate(). These are accumulated in widget->damage_list */
		PhTile_t original = { source, NULL }, *clip;

		clip = PhGetTile();
		clip->rect = source;
		clip->next = NULL;
		clip = PhClipTilings( clip, widget->damage_list, NULL );

		if( clip ) {
			PtClippedBlit( widget, &original, &point, clip );
			PhFreeTiles( clip );
			}
		}
  
	return NS_OK;
	}

NS_METHOD nsWindow::ScrollWidgets( PRInt32 aDx, PRInt32 aDy ) {
	PtWidget_t  *widget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);

	if( ( !aDx && !aDy ) || (!widget )) return NS_OK;

	PtStartFlux( widget );

	PtWidget_t *w;
	for( w=PtWidgetChildFront( widget ); w; w=PtWidgetBrotherBehind( w )) {
		PtArg_t arg;
		PhPoint_t  p;
		p.x = w->area.pos.x + aDx;
		p.y = w->area.pos.y + aDy;
		PtSetArg( &arg, Pt_ARG_POS, &p, 0 );
		PtSetResources( w, 1, &arg ) ;

		nsWindow *pWin = (nsWindow *) GetInstance(w);
		if (pWin) {
			pWin->mBounds.x += aDx;
			pWin->mBounds.y += aDy;
			}
		}

	PtEndFlux( widget);
	return NS_OK;    
	}

NS_METHOD nsWindow::SetTitle( const nsAString& aTitle ) {
  if( mWidget ) {
  	char * title = ToNewUTF8String(aTitle);
    PtSetResource( mWidget, Pt_ARG_WINDOW_TITLE, title, 0 );
  	if (title) nsCRT::free(title);
		}
  return NS_OK;
}


NS_IMETHODIMP nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	PRBool nNeedToShow = PR_FALSE;
	
	if( aWidth == mBounds.width && aHeight == mBounds.height ) return NS_OK;
	
	mBounds.width  = aWidth;
	mBounds.height = aHeight;
	
	// code to keep the window from showing before it has been moved or resized
	// if we are resized to 1x1 or less, we will hide the window.  Show(TRUE) will be ignored until a larger resize has happened
	if( aWidth <= 1 || aHeight <= 1 ) {
		aWidth = aHeight = 1;
		mIsTooSmall = PR_TRUE;
		}
	else {
		if( mIsTooSmall ) {
			// if we are not shown, we don't want to force a show here, so check and see if Show(TRUE) has been called
			nNeedToShow = mShown;
			mIsTooSmall = PR_FALSE;
			}
		}
	
	PhDim_t  dim = { aWidth, aHeight };
	
	if( mWidget ) {
		// center the dialog
		if( mWindowType == eWindowType_dialog ) {
			PhPoint_t p;
			QueryVisible( );
			PtCalcAbsPosition( NULL, NULL, &dim, &p );
			p.x -= gConsoleRect.ul.x;
			p.y -= gConsoleRect.ul.y;
			Move(p.x, p.y); // the move should be in coordinates assuming the console is 0, 0
			}
		if( aRepaint == PR_FALSE )  PtStartFlux(mWidget);
		PtSetResource( mWidget, Pt_ARG_DIM, &dim, 0 );
		if( aRepaint == PR_FALSE ) PtEndFlux(mWidget);
		}

	if( mIsToplevel || mListenForResizes ) {
		nsSizeEvent sevent;
		sevent.message = NS_SIZE;
		sevent.widget = this;
		
		sevent.windowSize = new nsRect (0, 0, aWidth, aHeight); 	
		
		sevent.point.x = 0;
		sevent.point.y = 0;
		sevent.mWinWidth = aWidth;
		sevent.mWinHeight = aHeight;
		// XXX fix this
		sevent.time = 0;
		AddRef();
		DispatchWindowEvent(&sevent);
		Release();
		delete sevent.windowSize;
		}
	if( nNeedToShow ) Show(PR_TRUE);
	return NS_OK;
}

int nsWindow::WindowWMHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) 
{
	PhWindowEvent_t *we = (PhWindowEvent_t *) cbinfo->cbdata;
	switch( we->event_f ) {
		case Ph_WM_CLOSE:
		  {
				nsWindow * win = (nsWindow*) data;
			  NS_ADDREF(win);
			  
			  // dispatch an "onclose" event. to delete immediately, call win->Destroy()
			  nsGUIEvent event;
			  nsEventStatus status;
			  
			  event.message = NS_XUL_CLOSE;
			  event.widget  = win;
			  
			  event.time = 0;
			  event.point.x = 0;
			  event.point.y = 0;
			  
			  win->DispatchEvent(&event, status);
			  
			  NS_RELEASE(win);
		  }
		break;

		case Ph_WM_CONSWITCH:
			gConsoleRectValid = PR_FALSE; /* force a call tp PhWindowQueryVisible() next time, since we might have moved this window into a different console */
      /* rollup the menus */
      if( gRollupWidget && gRollupListener ) gRollupListener->Rollup();
			break;

		case Ph_WM_FOCUS:
			if( we->event_state == Ph_WM_EVSTATE_FOCUSLOST ) {
      	/* rollup the menus */
      	if( gRollupWidget && gRollupListener ) gRollupListener->Rollup();

				if( sFocusWidget ) sFocusWidget->DispatchStandardEvent(NS_DEACTIVATE);
				}
			break;
	}
	
	return Pt_CONTINUE;
}

void nsWindow::RawDrawFunc( PtWidget_t * pWidget, PhTile_t * damage )
{
  nsWindow  * pWin = (nsWindow*) GetInstance( pWidget );
  nsresult    result;
  PhTile_t  * dmg = NULL;
  nsPaintEvent pev;
  PhRect_t   extent;

  if( !pWin || !pWin->mContext ) return;

	if ( pWin->mEventCallback ) {
		PhPoint_t  offset;
		nsRect     nsDmg;

		// Ok...  The damage rect is in window coordinates and is not neccessarily clipped to
		// the widgets canvas. Mozilla wants the paint coords relative to the parent widget, not the window.
		PtWidgetExtent(pWidget, &extent);
		PtWidgetOffset(pWidget, &offset);
		/* Build a List of Tiles that might be in front of me.... */
		PhTile_t *new_damage, *clip_tiles, *intersect;
		/* Intersect the Damage tile list w/ the clipped out list and see whats left! */
		new_damage = PhRectsToTiles(&damage->rect, 1);
		PhDeTranslateTiles(new_damage, &offset);
		clip_tiles = GetWindowClipping( pWidget );
		if (clip_tiles) {
			new_damage = PhClipTilings( new_damage, clip_tiles, NULL);
			PhFreeTiles(clip_tiles);
		}
		clip_tiles = PhRectsToTiles(&extent, 1);
		intersect = PhIntersectTilings( new_damage, clip_tiles, NULL);
		if ( intersect == NULL ) return;
		PhDeTranslateTiles(intersect, &extent.ul);
		PhFreeTiles(clip_tiles);
		PhFreeTiles(new_damage);
		new_damage = intersect;
		
		pWin->InitEvent(pev, NS_PAINT);
		pev.region = nsnull;
		pev.renderingContext = nsnull;
		pev.renderingContext = pWin->GetRenderingContext();
		for( dmg = new_damage; dmg; dmg = dmg->next ) {
			nsDmg.x = dmg->rect.ul.x;
			nsDmg.y = dmg->rect.ul.y;
			nsDmg.width = dmg->rect.lr.x - dmg->rect.ul.x + 1;
			nsDmg.height = dmg->rect.lr.y - dmg->rect.ul.y + 1;

			if( (nsDmg.width <= 0 ) || (nsDmg.height <= 0 ) ) /* Move to the next Damage Tile */
				continue;

			/* Re-Setup Paint Event */
			pWin->InitEvent(pev, NS_PAINT);
			pev.point.x = nsDmg.x;
			pev.point.y = nsDmg.y;
			pev.rect = &nsDmg;
			pev.region = nsnull;

			if( pev.renderingContext ) {
				nsIRegion *ClipRegion = pWin->GetRegion( );
				ClipRegion->SetTo( nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height );
				pev.renderingContext->SetClipRegion( NS_STATIC_CAST(const nsIRegion &, *(ClipRegion)), nsClipCombine_kReplace );

				NS_RELEASE( ClipRegion );
				
				/* You can turn off most drawing if you take this out */
				result = pWin->DispatchWindowEvent(&pev);
			}
		}
		NS_RELEASE(pev.renderingContext);
		PhFreeTiles( new_damage );
	}
}

static PhTile_t *GetWindowClipping( PtWidget_t *aWidget ) {
	PtWidget_t *w;
	PhTile_t *clip_tiles = NULL, *last = NULL;
	PhRect_t w_extent;

	PtWidgetExtent( aWidget, &w_extent);
	
	for( w = PtWidgetChildFront( aWidget ); w; w=PtWidgetBrotherBehind( w ) ) {
		long flags = PtWidgetFlags( w );
		if( (flags & Pt_REALIZED) && (flags & Pt_OPAQUE) && !PtIsDisjoint(w) ) {
			PhTile_t *tile = PhGetTile( );
			if( !tile ) return NULL;

			tile->rect.ul.x = w->area.pos.x + w_extent.ul.x;
			tile->rect.ul.y = w->area.pos.y + w_extent.ul.y;
			tile->rect.lr.x = tile->rect.ul.x + w->area.size.w - 1;
			tile->rect.lr.y = tile->rect.ul.y + w->area.size.h - 1;

			tile->next = NULL;
			if( !clip_tiles ) clip_tiles = tile;
			if( last ) last->next = tile;
			last = tile;
			}
		}
	return clip_tiles;
	}

int nsWindow::ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWindow *someWindow = (nsWindow *) GetInstance(widget);
  if( someWindow ) {
  	PtContainerCallback_t *cb = (PtContainerCallback_t *) cbinfo->cbdata;
  	PhRect_t *extents = &cb->new_size;
  	nsRect rect;
		rect.x = extents->ul.x;
    rect.y = extents->ul.y;
    rect.width = extents->lr.x - rect.x + 1;
    rect.height = extents->lr.y - rect.y + 1;

    if( someWindow->mBounds.width == rect.width && someWindow->mBounds.height == rect.height )
		  return Pt_CONTINUE;

    someWindow->mBounds.width  = rect.width;
    someWindow->mBounds.height = rect.height;

    /* This enables the resize holdoff */
    if( PtWidgetIsRealized( widget ) ) someWindow->OnResize( rect );
  	}
	return Pt_CONTINUE;
}


static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

/* catch an Ph_EV_INFO event when the graphics mode has changed */
int nsWindow::EvInfo( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {

	if( cbinfo->event && cbinfo->event->type == Ph_EV_INFO && cbinfo->event->subtype == Ph_OFFSCREEN_INVALID ) {
		nsresult rv;
		
		nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
		if (NS_SUCCEEDED(rv)) {
			 PRBool displayInternalChange = PR_FALSE;
			 pPrefs->GetBoolPref("browser.display.internaluse.graphics_changed", &displayInternalChange);
			 pPrefs->SetBoolPref("browser.display.internaluse.graphics_changed", !displayInternalChange);
		 }
		nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
		NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

		nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
		NS_ENSURE_SUCCESS(windowMediator->GetXULWindowEnumerator(nsnull, getter_AddRefs(windowEnumerator)), NS_ERROR_FAILURE);

		PRBool more;
		windowEnumerator->HasMoreElements(&more);
		while(more) {
			nsCOMPtr<nsISupports> nextWindow = nsnull;
			windowEnumerator->GetNext(getter_AddRefs(nextWindow));
			nsCOMPtr<nsIXULWindow> xulWindow(do_QueryInterface(nextWindow));
			NS_ENSURE_TRUE(xulWindow, NS_ERROR_FAILURE);
	
			nsCOMPtr<nsIDocShell> docShell;
			xulWindow->GetDocShell(getter_AddRefs(docShell));

			nsCOMPtr<nsIPresShell> presShell;
			docShell->GetPresShell( getter_AddRefs(presShell) );

			nsIViewManager* viewManager = presShell->GetViewManager();
			NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

			windowEnumerator->HasMoreElements(&more);
			}

		PtDamageWidget( widget );
		}
	return Pt_CONTINUE;
	}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move( PRInt32 aX, PRInt32 aY ) {

	if( mWindowType != eWindowType_popup && (mBounds.x == aX) && (mBounds.y == aY) )
		return NS_OK;

	mBounds.x = aX;
	mBounds.y = aY;

	switch( mWindowType ) {
		case eWindowType_popup:
			{
			PhPoint_t offset, total_offset = { 0, 0 };
    
			PtWidget_t *parent, *disjoint = PtFindDisjoint( mWidget->parent );

			while( disjoint ) {
				PtGetAbsPosition( disjoint, &offset.x, &offset.y );
				total_offset.x += offset.x;
				total_offset.y += offset.y;
				if( PtWidgetIsClass( disjoint, PtWindow ) || PtWidgetIsClass( disjoint, PtServer ) ) break; /* Stop at the first PtWindow */
				parent = PtWidgetParent(disjoint);
				if( parent ) disjoint = PtFindDisjoint( parent );
				else {
					disjoint = parent;
					break;
					}           
				}

			aX += total_offset.x;
			aY += total_offset.y;

			/* Add the Offset if the widget is offset from its parent.. */
			PtWidgetOffset( mWidget->parent, &offset );
			aX += offset.x;
			aY += offset.y;
			}
			break;

		case eWindowType_dialog:
		case eWindowType_toplevel:
			/* Offset to the current virtual console */
			QueryVisible( );
			aX += gConsoleRect.ul.x;
			aY += gConsoleRect.ul.y;
			break;
		}

  if( mWidget ) {
    if(( mWidget->area.pos.x != aX ) || ( mWidget->area.pos.y != aY )) {
      PhPoint_t pos = { aX, aY };
      PtSetResource( mWidget, Pt_ARG_POS, &pos, 0 );
    	}
  	}

	return NS_OK;
	}

int nsWindow::MenuRegionCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {
	if( gRollupWidget && gRollupListener ) {
		/* rollup the menu */
		gRollupListener->Rollup();
		}
	return Pt_CONTINUE;
	}

NS_IMETHODIMP nsWindow::SetModal( PRBool aModal ) {
  nsresult res = NS_ERROR_FAILURE;
 
  if (!mWidget)
	return NS_ERROR_FAILURE;

  PtWidget_t *toplevel = PtFindDisjoint(mWidget);
  if( !toplevel ) return NS_ERROR_FAILURE;

  if( aModal ) {
	  PtModalStart();
	  res = NS_OK;
  	}
  else {
		PtModalEnd();
    res = NS_OK;
  	}

  return res;
	}


inline nsIRegion *nsWindow::GetRegion()
{
  nsIRegion *region = NULL;
  nsresult res;

  static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

  res = nsComponentManager::CreateInstance( kRegionCID, nsnull, NS_GET_IID(nsIRegion), (void **)&region );
  if (NS_OK == res) region->Init();

  NS_ASSERTION(NULL != region, "Null region context");
  
  return region;  
}

/*
	widget is a PtRegion representing the native widget for a menu - reset the mParent->mLastMenu
	since it points to an widget being destroyed
*/
int nsWindow::MenuRegionDestroyed( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
	nsWindow *pWin = (nsWindow *) GetInstance( widget );
	if( pWin ) {
		nsWindow *parent = ( nsWindow * ) pWin->mParent;
		if( parent && parent->mLastMenu == widget )
			parent->mLastMenu = nsnull;
		}
	return Pt_CONTINUE;
}

NS_IMETHODIMP nsWindow::SetFocus(PRBool aRaise)
{
	sFocusWidget = this;

	if( PtIsFocused( mWidget ) == 2 ) return NS_OK;

	if( mWidget ) {
		PtWidget_t *disjoint;
		disjoint = PtFindDisjoint( mWidget );
		if( PtWidgetIsClass( disjoint, PtWindow ) ) {
			if( !( PtWindowGetState( disjoint ) & Ph_WM_STATE_ISFOCUS ) ) {
				nsWindow *pWin = (nsWindow *) GetInstance( disjoint );
				pWin->GetAttention( -1 );
				}
			}
		PtContainerGiveFocus( mWidget, NULL );
		}
	return NS_OK;
}
