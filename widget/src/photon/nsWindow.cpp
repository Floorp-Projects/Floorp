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


#include "nsPhWidgetLog.h"
#include <Pt.h>
#include <PtServer.h>
#include "PtRawDrawContainer.h"

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

PRBool             nsWindow::mResizeQueueInited = PR_FALSE;
DamageQueueEntry  *nsWindow::mResizeQueue = nsnull;
PtWorkProcId_t    *nsWindow::mResizeProcID = nsnull;

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  mClientWidget    = nsnull;
  mIsTooSmall      = PR_FALSE;
  mBlockFocusEvents = PR_FALSE;
  mBorderStyle     = eBorderStyle_default;
  mWindowType      = eWindowType_child;
  mIsResizing      = PR_FALSE;
}

ChildWindow::ChildWindow()
{
  mBorderStyle     = eBorderStyle_none;
  mWindowType      = eWindowType_child;
}

ChildWindow::~ChildWindow()
{
}

PRBool ChildWindow::IsChild() const
{
  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.
  Destroy();

  RemoveResizeWidget();
}

PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_IMETHODIMP nsWindow::WidgetToScreen( const nsRect& aOldRect, nsRect& aNewRect ) {

	PhPoint_t pos, offset, orig = nsToolkit::GetConsoleOffset();
	PtWidget_t *disjoint = PtFindDisjoint( mWidget );

	PtGetAbsPosition( disjoint, &pos.x, &pos.y );
	PtWidgetOffset( mWidget, &offset );
	aNewRect.x = pos.x + offset.x + aOldRect.x - orig.x;
	aNewRect.y = pos.y + offset.y + aOldRect.y - orig.y;

	aNewRect.width = aOldRect.width;
	aNewRect.height = aOldRect.height;

  return NS_OK;
	}

void nsWindow::DestroyNative(void)
{
  RemoveResizeWidget();
  if ( mClientWidget )
  {
    PtDestroyWidget(mClientWidget );  
    mClientWidget = nsnull;
  }
  // destroy all of the children that are nsWindow() classes
  // preempting the gdk destroy system.
  DestroyNativeChildren();

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

NS_IMETHODIMP nsWindow::Update(void)
{
  return nsWidget::Update();
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
  	}
	else {
    NS_IF_RELEASE(gRollupListener);
    gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
		gRollupWidget = nsnull;
  	}
  
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
	if (!mWidget) return NS_OK; // mWidget will be null during printing.
	if (!PtWidgetIsRealized(mWidget)) return NS_OK;
	nsWidget::Invalidate(aIsSynchronous);
	return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
	if (!mWidget) return NS_OK; // mWidget will be null during printing.
	if (!PtWidgetIsRealized(mWidget)) return NS_OK;
	nsWidget::Invalidate(aRect, aIsSynchronous);
	return NS_OK;
}

NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous)
{
	if (!mWidget) return NS_OK; // mWidget will be null during printing.
	if (!PtWidgetIsRealized(mWidget)) return NS_OK;
	nsWidget::InvalidateRegion(aRegion,aIsSynchronous);
	return NS_OK;
}

NS_IMETHODIMP nsWindow::SetBackgroundColor( const nscolor &aColor ) 
{
	return nsWidget::SetBackgroundColor(aColor);
}


NS_IMETHODIMP nsWindow::SetFocus( PRBool aRaise ) {
  return nsWidget::SetFocus(aRaise);
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
  PtArg_t   arg[20];
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
    render_flags = Ph_WM_RENDER_INLINE;

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
      PhPoint_t p = nsToolkit::GetConsoleOffset();
      pos.x += p.x;
      pos.y += p.y;
    	PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    }

    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );

    /* Create Pop-up Menus as a PtRegion */

    if (!parentWidget)
      PtSetParentWidget( nsnull );
		
    if( mWindowType == eWindowType_popup ) {
	  	int	fields = Ph_REGION_PARENT|Ph_REGION_HANDLE| Ph_REGION_FLAGS|Ph_REGION_ORIGIN|Ph_REGION_EV_SENSE|Ph_REGION_EV_OPAQUE|Ph_REGION_RECT;
	  	int sense =  Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE | Ph_EV_BUT_REPEAT;

      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FIELDS,   fields, fields );
      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_PARENT,   Ph_ROOT_RID, 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_SENSE,    sense | Ph_EV_DRAG|Ph_EV_EXPOSE, sense | Ph_EV_DRAG|Ph_EV_EXPOSE);
      PtSetArg( &arg[arg_count++], Pt_ARG_REGION_OPAQUE,   sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW, sense |Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW);
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_GETS_FOCUS | Pt_DELAY_REALIZE);
	  PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0);
      mWidget = PtCreateWidget( PtRegion, parentWidget, arg_count, arg);

    	// Must also create the client-area widget
      arg_count = 0;
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_FLAGS, Pt_ANCHOR_ALL, ~0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
       PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_GREEN, 0 );
      PhRect_t anch_offset = {{0, 0},{0, 0}};
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, (Pt_HIGHLIGHTED | Pt_GETS_FOCUS));
      mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, arg_count, arg );
     }
	else {
		PtSetParentWidget( NULL );

		/* Dialog and TopLevel Windows */
		PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
		PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, -1 );
		PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );

		PtRawCallback_t cb_raw = { Ph_EV_INFO, EvInfo, NULL };
		PtCallback_t cb_resize = { ResizeHandler, NULL };
		PtSetArg( &arg[arg_count++], Pt_CB_RESIZE, &cb_resize, NULL );
		PtSetArg( &arg[arg_count++], Pt_CB_RAW, &cb_raw, NULL );
		mWidget = PtCreateWidget( PtWindow, NULL, arg_count, arg );
	}
  }

  if( mWidget ) {
	  SetInstance( mWidget, this );
	  if( mClientWidget ) SetInstance( mClientWidget, this );
	  if( mWindowType == eWindowType_child ) {
      	PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
      	PtAddEventHandler( mWidget,
      	  Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON |
      	  Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY|Ph_EV_DRAG /* | Ph_EV_WM | Ph_EV_EXPOSE */
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
      		  	Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY | Ph_EV_WM /*| Ph_EV_EXPOSE*/,
      			  RawEventHandler, this );

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
       else {
#if 0
		   PtRawCallback_t filter_cb;
		   
		   filter_cb.event_mask =  Ph_EV_EXPOSE ;
		   filter_cb.event_f = RawEventHandler;
		   filter_cb.data = this;
		   PtSetResource(mWidget, Pt_CB_FILTER, &filter_cb,1);
#endif		  
          	PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 

           	PtCallback_t callback;
		    		callback.event_f = WindowWMHandler;
            callback.data = this;

            PtSetArg( &arg[0], Pt_CB_WINDOW, &callback, 0 );
		   PtSetArg( &arg[1], Pt_ARG_WINDOW_NOTIFY_FLAGS, 0xFFFFFFFF, 0XFFFFFFFF );
            PtSetResources( mWidget, 2, arg );
       			}
    		}

    // call the event callback to notify about creation
    DispatchStandardEvent( NS_CREATE );

    result = NS_OK;
  	}

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
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
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

NS_IMETHODIMP nsWindow::ScrollRect( nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy ) {
	NS_WARNING("nsWindow::ScrollRect Not Implemented\n");
	return NS_OK;
	}

NS_METHOD nsWindow::SetTitle( const nsString& aTitle ) {
  nsresult res = NS_ERROR_FAILURE;
  char * title = ToNewUTF8String(aTitle);
  
  if( mWidget ) {
    PtSetResource( mWidget, Pt_ARG_WINDOW_TITLE, title, 0 );
		res = NS_OK;
  	}

  if (title) nsCRT::free(title);

  return res;
}


/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
  return NS_OK;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
	PtHold();
	return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
	PtRelease();
	return NS_OK;
}



PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if( mEventCallback ) return DispatchWindowEvent(&aEvent);
  return PR_FALSE;
}


// nsWindow::DispatchFocus - Not Implemented
PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
  return PR_FALSE;
}


// nsWindow::OnScroll - Not Implemented
PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
  return PR_FALSE;
}


NS_IMETHODIMP nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	PRBool nNeedToShow = PR_FALSE;
	
	if ( aWidth == mBounds.width && aHeight == mBounds.height)
		return NS_OK;
	
	mBounds.width  = aWidth;
	mBounds.height = aHeight;
	
	// code to keep the window from showing before it has been moved or resized
	// if we are resized to 1x1 or less, we will hide the window.  Show(TRUE) will be ignored until a
	// larger resize has happened
	if (aWidth <= 1 || aHeight <= 1)
	{
		if ( mIsToplevel )
		{
			aWidth = 1;
			aHeight = 1;
			mIsTooSmall = PR_TRUE;
//			if( mShown ) Show(PR_FALSE);
		}
		else
		{
			aWidth = 1;
			aHeight = 1;
			mIsTooSmall = PR_TRUE;
//			if( mShown ) Show(PR_FALSE);
		}
	}
	else
	{
		if (mIsTooSmall)
		{
			// if we are not shown, we don't want to force a show here, so check and see if Show(TRUE) has been called
			nNeedToShow = mShown;
			mIsTooSmall = PR_FALSE;
		}
	}
	
	PtArg_t  arg;
	PhDim_t  dim = { aWidth, aHeight };
	
	if( mWidget ) {
		//		EnableDamage( mWidget, PR_FALSE );
		
		// center the dialog
		if( mWindowType == eWindowType_dialog ) {
			PhPoint_t p, pos = nsToolkit::GetConsoleOffset( );
			PtCalcAbsPosition(NULL, NULL, &dim, &p);
			p.x -= pos.x;
			p.y -= pos.y;
			Move(p.x, p.y); // the move should be in coordinates assuming the console is 0, 0
		}
		if ( aRepaint == PR_FALSE )
			PtStartFlux(mWidget);
		PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
		PtSetResources( mWidget, 1, &arg );
		if ( aRepaint == PR_FALSE )
			PtEndFlux(mWidget);
		
		/* This fixes XUL dialogs */
#if 0
		if (mClientWidget) {
			printf("resize!!!\n");
			PtWidget_t *theClientChild = PtWidgetChildBack(mClientWidget);
			if (theClientChild) {
				nsWindow * pWin = (nsWindow*) GetInstance( theClientChild );
				pWin->Resize(aWidth, aHeight, aRepaint);
			}
		}
#endif	
	}
	if (mIsToplevel || mListenForResizes) {
		nsSizeEvent sevent;
		sevent.message = NS_SIZE;
		sevent.widget = this;
		sevent.eventStructType = NS_SIZE_EVENT;
		
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

NS_IMETHODIMP nsWindow::Resize( PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint ) {
  Move(aX,aY);
  // resize can cause a show to happen, so do this last
  Resize(aWidth,aHeight,aRepaint);
  return NS_OK;
	}


int nsWindow::WindowWMHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) 
{
	PhWindowEvent_t *we = (PhWindowEvent_t *) cbinfo->cbdata;
	nsWindow * win = (nsWindow*) data;
	
	switch( we->event_f ) {
		case Ph_WM_CLOSE:
		  {
			  NS_ADDREF(win);
			  
			  // dispatch an "onclose" event. to delete immediately, call win->Destroy()
			  nsGUIEvent event;
			  nsEventStatus status;
			  
			  event.message = NS_XUL_CLOSE;
			  event.widget  = win;
			  event.eventStructType = NS_GUI_EVENT;
			  
			  event.time = 0;
			  event.point.x = 0;
			  event.point.y = 0;
			  
			  win->DispatchEvent(&event, status);
			  
			  NS_RELEASE(win);
		  }
		break;
		
		default:
		if( we->event_f == Ph_WM_TOFRONT || ( we->event_f == Ph_WM_FOCUS && we->state_f == Ph_WM_EVSTATE_FOCUS ) )
		   return Pt_CONTINUE;

		/* destroy any pop up widgets ( menus ) */
		if( gRollupWidget && gRollupListener ) gRollupListener->Rollup();
	}
	
	return Pt_CONTINUE;
}

//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::HandleEvent( PtWidget_t *widget, PtCallbackInfo_t* aCbInfo ) {
/* if in the future you add code here, modify nsWidget::HandleEvent() to allow you this call */
	return nsWidget::HandleEvent( widget, aCbInfo);
	}

void nsWindow::RawDrawFunc( PtWidget_t * pWidget, PhTile_t * damage )
{
  nsWindow  * pWin = (nsWindow*) GetInstance( pWidget );
  nsresult    result;
  PhTile_t  * dmg = NULL;
  PRBool      aClipState;
  nsPaintEvent pev;
  PhRect_t   extent;

#if 0
  PgSetFillColor(Pg_WHITE);
  PgSetDrawMode(Pg_DRAWMODE_XOR);
  PtWidgetExtent(pWidget, &extent);
  int i;
  for (i = 0; i < 5 * 2; i++)
  {
	  PgDrawRect(&extent, Pg_DRAW_FILL);
	  PgFlush();
	  delay(1);
  }
  PgSetDrawMode(Pg_DRAWMODE_OPAQUE);
#endif	
  if( !pWin || !pWin->mContext ) return;

  // This prevents redraws while any window is resizing, ie there are
  //   windows in the resize queue
  if( pWin->mIsResizing ) return;

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
		clip_tiles = pWin->GetWindowClipping();
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
		pev.eventStructType = NS_PAINT_EVENT;
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
			pev.eventStructType = NS_PAINT_EVENT;
			pev.point.x = nsDmg.x;
			pev.point.y = nsDmg.y;
			pev.rect = &nsDmg;

			if( pev.renderingContext ) {
				nsIRegion *ClipRegion = pWin->GetRegion( );
				ClipRegion->SetTo( nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height );
				pev.renderingContext->SetClipRegion( NS_STATIC_CAST(const nsIRegion &, *(ClipRegion)), nsClipCombine_kReplace, aClipState );

				NS_RELEASE( ClipRegion );
				
				/* You can turn off most drawing if you take this out */
				result = pWin->DispatchWindowEvent(&pev);
			}
		}
		NS_RELEASE(pev.renderingContext);
		PhFreeTiles( new_damage );
	}
}


void nsWindow::ScreenToWidget( PhPoint_t &pt ) {
  // pt is in screen coordinates
  // convert it to be relative to ~this~ widgets origin
  short x,y;

  if( mWindowType == eWindowType_child )
    PtGetAbsPosition( mWidget, &x, &y );
  else PtGetAbsPosition( mClientWidget, &x, &y );

  pt.x -= x;
  pt.y -= y;
	}


PhTile_t *nsWindow::GetWindowClipping( ) {
	PtWidget_t *w, *aWidget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);
	PhTile_t *clip_tiles = NULL, *last = NULL;
	PhRect_t w_extent;

	PtWidgetExtent( aWidget, &w_extent);
	
	for( w = PtWidgetChildFront( aWidget ); w; w=PtWidgetBrotherBehind( w ) ) {
		long flags = PtWidgetFlags( w );
		if( (flags & (Pt_OPAQUE|Pt_REALIZED)) && !(PtIsDisjoint(w))) {
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
  PtContainerCallback_t *cb = (PtContainerCallback_t *) cbinfo->cbdata;
  PhRect_t *extents = &cb->new_size;
  nsWindow *someWindow = (nsWindow *) GetInstance(widget);
  nsRect rect;

  if( someWindow && extents)	// kedl
  {
	rect.x = extents->ul.x;
    rect.y = extents->ul.y;
    rect.width = extents->lr.x - rect.x + 1;
    rect.height = extents->lr.y - rect.y + 1;
    if ( someWindow->mBounds.width == rect.width && someWindow->mBounds.height == rect.height)
		  return (Pt_CONTINUE);
    someWindow->mBounds.width  = rect.width;	// kedl, fix main window not resizing!!!!
    someWindow->mBounds.height = rect.height;
    /* This enables the resize holdoff */
    if (PtWidgetIsRealized(widget)) {
//      someWindow->ResizeHoldOff();
      someWindow->OnResize( rect );
    }
  }
	return( Pt_CONTINUE );
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

			nsCOMPtr<nsIViewManager> viewManager;
			presShell->GetViewManager(getter_AddRefs(viewManager));
			NS_ENSURE_TRUE(viewManager, NS_ERROR_FAILURE);

			windowEnumerator->HasMoreElements(&more);
			}

		PtDamageWidget( widget );
		}
	return Pt_CONTINUE;
	}

void nsWindow::ResizeHoldOff() {

  if( !mWidget ) return;

  if( PR_FALSE == mResizeQueueInited ) {
    // This is to guarantee that the Invalidation work-proc is in place prior to the resize work-proc.
//    if( !mDmgQueueInited ) Invalidate( PR_FALSE );

    PtWidget_t *top = PtFindDisjoint( mWidget );

		if( (mResizeProcID = PtAppAddWorkProc( nsnull, ResizeWorkProc, top )) != nsnull ) {
			PtHold();
			mResizeQueueInited = PR_TRUE;
			}
		}

  if( PR_TRUE == mResizeQueueInited ) {
    DamageQueueEntry *dqe;
    PRBool           found = PR_FALSE;
  
		for( dqe = mResizeQueue; dqe; dqe = dqe->next ) {
      if( dqe->widget == mWidget ) {
        found = PR_TRUE;
        break;
      	}
    	}

    if( !found ) {
      dqe = new DamageQueueEntry;
      if( dqe ) {
        mIsResizing = PR_TRUE;
        dqe->widget = mWidget;
        dqe->inst = this;
        dqe->next = mResizeQueue;
        mResizeQueue = dqe;
      	}
    	}
  	}
	}


void nsWindow::RemoveResizeWidget( ) {
  if( mIsResizing ) {
    DamageQueueEntry *dqe;
    DamageQueueEntry *last_dqe = nsnull;
  
    dqe = mResizeQueue;

    // If this widget is in the queue, remove it
    while( dqe ) {
      if( dqe->widget == mWidget ) {
        if( last_dqe ) last_dqe->next = dqe->next;
        else mResizeQueue = dqe->next;

        delete dqe;
        mIsResizing = PR_FALSE;
        break;
      	}
      last_dqe = dqe;
      dqe = dqe->next;
    	}

    if( nsnull == mResizeQueue ) {
      mResizeQueueInited = PR_FALSE;
      PtRelease( );
      if( mResizeProcID ) PtAppRemoveWorkProc( nsnull, mResizeProcID );
    	}
  	}
	}


int nsWindow::ResizeWorkProc( void *data ) {

  if ( mResizeQueueInited ) {
    DamageQueueEntry *dqe = nsWindow::mResizeQueue;
    DamageQueueEntry *last_dqe;

    while( dqe ) {
      ((nsWindow*)dqe->inst)->mIsResizing = PR_FALSE;
      dqe->inst->Invalidate( PR_FALSE );
      last_dqe = dqe;
      dqe = dqe->next;
      delete last_dqe;
    	}

    nsWindow::mResizeQueue = nsnull;
    nsWindow::mResizeQueueInited = PR_FALSE;

    PtRelease();
  	}

  return Pt_END;
	}


NS_METHOD nsWindow::GetClientBounds( nsRect &aRect ) {
  aRect.x = 0;
  aRect.y = 0;
  aRect.width = mBounds.width;
  aRect.height = mBounds.height;
  return NS_OK;
	}

//-------------------------------------------------------------------------
//
// grab mouse events for this widget
// CaptureMouse Not Implemented
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureMouse( PRBool aCapture ) {
  return NS_OK;
	}

NS_METHOD nsWindow::ConstrainPosition( PRInt32 *aX, PRInt32 *aY ) {
  return NS_OK;
	}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move( PRInt32 aX, PRInt32 aY ) {
	PRInt32 origX, origY;

	/* Keep a untouched version of the coordinates laying around for comparison */
	origX=aX;
	origY=aY;

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
			PhPoint_t offset = nsToolkit::GetConsoleOffset();
			aX += offset.x;
			aY += offset.y;
			break;
		}

	/* Call my base class */
	nsresult res = nsWidget::Move(aX, aY);

	/* If I am a top-level window my origin should always be 0,0 */
	if( mWindowType != eWindowType_child ) {
		mBounds.x = origX;
		mBounds.y = origY;
		}

	return res;
	}

//-------------------------------------------------------------------------
//
// Determine whether an event should be processed, assuming we're
// a modal window.
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent, PRBool *aForWindow) {
	*aForWindow = PR_TRUE;
	return NS_OK;
	}

int nsWindow::MenuRegionCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {

	if( cbinfo->event->type == Ph_EV_BUT_PRESS ) {
  	nsWindow      *pWin = (nsWindow*) data;
		if( pWin->gRollupWidget && pWin->gRollupListener ) {
			/* Close the Window */
			pWin->gRollupListener->Rollup();
			}
		}
	return Pt_CONTINUE;
	}

NS_METHOD nsWindow::GetAttention( ) {
  if( mWidget && mWidget->parent && !PtIsFocused( mWidget ) ) {
    /* Raise to the top */
    PtWidgetToFront(mWidget);  
  	}
  return NS_OK;
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


nsIRegion *nsWindow::GetRegion()
{
  nsIRegion *region = NULL;
  nsresult res;

  static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

  res = nsComponentManager::CreateInstance( kRegionCID, nsnull, NS_GET_IID(nsIRegion), (void **)&region );
  if (NS_OK == res) region->Init();

  NS_ASSERTION(NULL != region, "Null region context");
  
  return region;  
}
