/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *     Jerry.Kirk@Nexwarecorp.com
 *	   Dale.Stansberry@Nexwarecorop.com
 */


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
#include "nsIAppShell.h"
#include "nsClipboard.h"
#include "nsIRollupListener.h"


// are we grabbing?
PRBool      nsWindow::mIsGrabbing = PR_FALSE;
nsWindow   *nsWindow::mGrabWindow = NULL;

// this is the last window that had a drag event happen on it.
nsWindow *nsWindow::mLastDragMotionWindow = NULL;
// we get our drop after the leave.
nsWindow *nsWindow::mLastLeaveWindow = NULL;

PRBool             nsWindow::mResizeQueueInited = PR_FALSE;
DamageQueueEntry  *nsWindow::mResizeQueue = nsnull;
PtWorkProcId_t    *nsWindow::mResizeProcID = nsnull;

/* Photon Event Timestamp */
unsigned long		IgnoreEvent = 0;

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  mClientWidget    = nsnull;
  mShell           = nsnull;
  mFontMetrics     = nsnull;
  mMenuRegion      = nsnull;
  mVisible         = PR_FALSE;
  mDisplayed       = PR_FALSE;
  mClipChildren    = PR_TRUE;		/* This needs to be true for Photon */
  mClipSiblings    = PR_FALSE;		/* TRUE = Fixes Pop-Up Menus over animations */
  mIsTooSmall      = PR_FALSE;
  mIsUpdating      = PR_FALSE;
  mBlockFocusEvents = PR_FALSE;
  mBorderStyle     = eBorderStyle_default;
  mWindowType      = eWindowType_child;
  mIsResizing      = PR_FALSE;
  mFont            = nsnull;
  mMenuBar         = nsnull;
  mMenuBarVis      = PR_FALSE;
  
  mIsDestroyingWindow = PR_FALSE;

  if( mLastLeaveWindow == this )				mLastLeaveWindow = NULL;
  if( mLastDragMotionWindow == this )		mLastDragMotionWindow = NULL;
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
  // make sure that we release the grab indicator here
  if (mGrabWindow == this) {
    mIsGrabbing = PR_FALSE;
    mGrabWindow = NULL;
  	}
  // make sure that we unset the lastDragMotionWindow if
  // we are it.
  if (mLastDragMotionWindow == this) mLastDragMotionWindow = NULL;

  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.
  Destroy();

  RemoveResizeWidget();
  RemoveDamagedWidget( mWidget );
}

PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  PhPoint_t p1;
  
	if( mWidget ) PtWidgetOffset(mWidget,&p1);

	aNewRect.x = p1.x + mBounds.x + aOldRect.x;
	aNewRect.y = p1.y + mBounds.y + aOldRect.y;

  return NS_OK;
}

void nsWindow::DestroyNative(void)
{
  RemoveResizeWidget();
  RemoveDamagedWidget( mWidget );
  
  if ( mMenuRegion )
  {
    PtDestroyWidget(mMenuRegion );  
    mMenuRegion = nsnull;
  }
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

  if (aDoCapture) { /* Create a pointer region */
     mIsGrabbing = PR_TRUE;
     mGrabWindow = this;
  	}
  else {
    // make sure that the grab window is marked as released
    if (mGrabWindow == this) mGrabWindow = NULL;
    mIsGrabbing = PR_FALSE;
  	}
  
  if (aDoCapture) {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupConsumeRollupEvent = PR_TRUE;
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  	}
	else {
    NS_IF_RELEASE(gRollupListener);
    //gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
  	}
  
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
  if (!mWidget) return NS_OK; // mWidget will be null during printing.
  if (!PtWidgetIsRealized(mWidget)) return NS_OK;

  /* Damage has to be relative Widget coords */
  mUpdateArea->SetTo( 0,0, mBounds.width, mBounds.height );

  if (aIsSynchronous) UpdateWidgetDamage();
  else QueueWidgetDamage();

  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
  if (!mWidget) return NS_OK; // mWidget will be null during printing.
  if (!PtWidgetIsRealized(mWidget)) return NS_OK;
  
  /* Offset the rect by the mBounds X,Y to fix damaging inside widget in test14 */
  if (mWindowType == eWindowType_popup) mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);
	else mUpdateArea->Union((aRect.x+mBounds.x), (aRect.y+mBounds.y), aRect.width, aRect.height);

  if (aIsSynchronous)
  {
    UpdateWidgetDamage();
  }
  else
  {
    QueueWidgetDamage();
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous)
{
  nsIRegion *newRegion;
  
  if (!mWidget) return NS_OK; // mWidget will be null during printing.
  if (!PtWidgetIsRealized(mWidget)) return NS_OK;

  newRegion = GetRegion( );
  newRegion->SetTo( *aRegion );
     
  if (mWindowType != eWindowType_popup)
    newRegion->Offset(mBounds.x, mBounds.y);
  mUpdateArea->Union(*newRegion);

	NS_RELEASE( newRegion );

  if (aIsSynchronous)	UpdateWidgetDamage();
  else								QueueWidgetDamage();

  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetBackgroundColor( const nscolor &aColor ) {
  return nsWidget::SetBackgroundColor(aColor);
	}


NS_IMETHODIMP nsWindow::SetFocus(PRBool aRaise) {
  return nsWidget::SetFocus();
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
    PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0 /*Pt_HIGHLIGHTED*/, Pt_HIGHLIGHTED|Pt_GETS_FOCUS );
    PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_RED, 0 );
    PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
    mWidget = PtCreateWidget( PtRawDrawContainer, parentWidget, arg_count, arg );
  }
  else
  {
    // No border or decorations is the default
    render_flags = 0;

    if( mWindowType != eWindowType_popup ) {

      #define PH_BORDER_STYLE_ALL  \
        Ph_WM_RENDER_TITLE | \
        Ph_WM_RENDER_CLOSE | \
        Ph_WM_RENDER_BORDER | \
        Ph_WM_RENDER_RESIZE | \
        Ph_WM_RENDER_MAX | \
        Ph_WM_RENDER_MIN | \
        Ph_WM_RENDER_MENU 


      if( mBorderStyle & eBorderStyle_all )	render_flags = PH_BORDER_STYLE_ALL;
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
		
    if( mWindowType == eWindowType_popup )
    {
	  int	fields = Ph_REGION_PARENT|Ph_REGION_HANDLE| Ph_REGION_FLAGS|Ph_REGION_ORIGIN| 
					 				Ph_REGION_EV_SENSE|Ph_REGION_EV_OPAQUE|Ph_REGION_RECT;
	  int   sense =  Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE | Ph_EV_BUT_REPEAT;

      PtRawCallback_t callback;
        callback.event_mask = sense;
        callback.event_f = MenuRegionCallback;
        callback.data = this;
	  
      PhRid_t    rid;
      PhRegion_t region;
	  	PhRect_t   rect;	  
      PhArea_t   area;
      PtArg_t    args[20];  
      int        arg_count2 = 0;
            
      /* if no RollupListener has been added then add a full screen region to catch mounse and key */
      /* events that are outside of the application */
      if ( mMenuRegion == nsnull)
      {
	    PhQueryRids( 0, 0, 0, Ph_INPUTGROUP_REGION | Ph_QUERY_IG_POINTER, 0, 0, 0, &rid, 1);
  	    PhRegionQuery( rid, &region, &rect, NULL, 0 );
        area.pos = region.origin;
        area.size.w = rect.lr.x - rect.ul.x + 1;
        area.size.h = rect.lr.y - rect.ul.y + 1;

        PtSetArg( &args[arg_count2++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE|Pt_GETS_FOCUS);
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_PARENT,   Ph_ROOT_RID, 0 );
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_FIELDS,   fields, fields );
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_FLAGS,    Ph_FORCE_FRONT | Ph_FORCE_BOUNDARY, Ph_FORCE_FRONT | Ph_FORCE_BOUNDARY);
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_SENSE,    sense, sense );
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_OPAQUE,   Ph_EV_BOUNDARY, Ph_EV_BOUNDARY);
        PtSetArg( &args[arg_count2++], Pt_ARG_AREA,            &area, 0 );
        PtSetArg( &args[arg_count2++], Pt_ARG_FILL_COLOR,      Pg_TRANSPARENT, 0 );
        PtSetArg( &args[arg_count2++], Pt_ARG_RAW_CALLBACKS,   &callback, 1 );
        mMenuRegion = PtCreateWidget( PtRegion, parentWidget, arg_count2, args );
      }

        callback.event_f = PopupMenuRegionCallback;
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FIELDS,   fields, fields );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FLAGS,    Ph_FORCE_FRONT, Ph_FORCE_FRONT );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_SENSE,    sense | Ph_EV_DRAG|Ph_EV_EXPOSE, sense | Ph_EV_DRAG|Ph_EV_EXPOSE);
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_OPAQUE,   sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW, sense |Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW);
        PtSetArg( &arg[arg_count++], Pt_ARG_RAW_CALLBACKS,   &callback, 1 );
        PtSetArg( &args[arg_count++], Pt_ARG_FLAGS, 0, Pt_GETS_FOCUS);
        mWidget = PtCreateWidget( PtRegion, parentWidget, arg_count, arg);
     }
	else
	{
      PtSetParentWidget( nsnull );
      
      /* Dialog and TopLevel Windows */
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
      PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, 0xFFFFFFFF );
      PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_MANAGED_FLAGS, 0, Ph_WM_CLOSE );
      PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_BLUE, 0 );

      mWidget = PtCreateWidget( PtWindow, NULL, arg_count, arg );
      PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
	}
	
    // Must also create the client-area widget
    if( mWidget )
    {
      arg_count = 0;
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_FLAGS, Pt_LEFT_ANCHORED_LEFT |
                                   Pt_RIGHT_ANCHORED_RIGHT | Pt_TOP_ANCHORED_TOP | 
								   Pt_BOTTOM_ANCHORED_BOTTOM, 0xFFFFFFFF );
      PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_GREEN, 0 );
      //PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );
      PhRect_t anch_offset = { 0, 0, 0, 0 };
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      if( mWindowType == eWindowType_popup )
	  {
        PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
        PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, (Pt_HIGHLIGHTED | Pt_GETS_FOCUS));
        mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, arg_count, arg );
	  }
	  else
	  {  
        PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, Pt_HIGHLIGHTED | Pt_GETS_FOCUS );
        PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_YELLOW, 0 );
	  }
    }
  }

  if( mWidget )
  {
    SetInstance( mWidget, this );

    if( mClientWidget )
      SetInstance( mClientWidget, this );
 
    if (mWindowType == eWindowType_child)
    {
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
    else if (mWindowType == eWindowType_popup)
	{
      PtAddEventHandler( mClientWidget,
       	Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON | 
        	Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY | Ph_EV_WM | Ph_EV_EXPOSE,
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
    else if ( !parentWidget )
    {
       if (mClientWidget) PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull );
       else
       {
 	     PtRawCallback_t filter_cb;
		
		  filter_cb.event_mask =  Ph_EV_EXPOSE ;
          filter_cb.event_f = RawEventHandler;
          filter_cb.data = this;
		  PtSetResource(mWidget, Pt_CB_FILTER, &filter_cb,1);
		  
          PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 

           PtArg_t arg;
           PtCallback_t callback;
		
		    callback.event_f = WindowCloseHandler;
            callback.data = this;
            PtSetArg( &arg,Pt_CB_WINDOW, &callback, 0 );
            PtSetResources( mWidget, 1, &arg );
			
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

	PhRect_t source = { 0, 0, widget->area.size.w, widget->area.size.h };
	PhPoint_t point = { aDx, aDy };
	PtBlit( widget, &source, &point );
  
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
  char * title = aTitle.ToNewUTF8String();
  
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
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
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
//      if(mShown) Show(PR_FALSE);
    }
    else
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;
			//Show(PR_FALSE);
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
		EnableDamage( mWidget, PR_FALSE );

		// center the dialog
		if( mWindowType == eWindowType_dialog ) {
			PhPoint_t p, pos = nsToolkit::GetConsoleOffset( );
			PtCalcAbsPosition(NULL, NULL, &dim, &p);
			p.x -= pos.x;
			p.y -= pos.y;
			Move(p.x, p.y); // the move should be in coordinates assuming the console is 0, 0
			}

    PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
    PtSetResources( mWidget, 1, &arg );
		/* This fixes XUL dialogs */
    if (mClientWidget) {
      PtWidget_t *theClientChild = PtWidgetChildBack(mClientWidget);
	  	if (theClientChild) {
        nsWindow * pWin = (nsWindow*) GetInstance( theClientChild );
	    	pWin->Resize(aWidth, aHeight, aRepaint);
      	}
			}
	
    EnableDamage( mWidget, PR_TRUE );
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


int nsWindow::WindowCloseHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo ) {
  PhWindowEvent_t *we = (PhWindowEvent_t *) cbinfo->cbdata;
  nsWindow * win = (nsWindow*) data;

  if( we->event_f == Ph_WM_CLOSE ) {
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
  return Pt_CONTINUE;
	}

NS_METHOD nsWindow::Show( PRBool bState ) {

  if (!mWidget) return NS_OK; // Will be null durring printing
  EnableDamage(mWidget, PR_FALSE);

  if (bState) {
     if (mWindowType == eWindowType_popup) {
          short arg_count=0;
          PtArg_t   arg[2];
          PhPoint_t pos = nsToolkit::GetConsoleOffset();

          /* Position the region on the current console*/
          PtSetArg( &arg[0],  Pt_ARG_POS,  &pos, 0 );
          PtSetResources( mMenuRegion, 1, arg );

          PtRealizeWidget(mMenuRegion);

          /* Now that the MenuRegion is Realized make the popup menu a child in front of the big menu */
          PtSetArg( &arg[arg_count++], Pt_ARG_REGION_PARENT, mMenuRegion->rid, 0 );
          PtSetResources(mWidget, arg_count, arg);           
					}
			}
  else {
     if( mWindowType == eWindowType_popup ) PtUnrealizeWidget(mMenuRegion);
		}

  EnableDamage(mWidget, PR_TRUE);
  return nsWidget::Show(bState);
	}

//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::HandleEvent( PtCallbackInfo_t* aCbInfo ) {
	return nsWidget::HandleEvent(aCbInfo);
	}

void nsWindow::RawDrawFunc( PtWidget_t * pWidget, PhTile_t * damage )
{
  nsWindow  * pWin = (nsWindow*) GetInstance( pWidget );
  nsresult    result;
  PhTile_t  * dmg = NULL;
  PRBool      aClipState;
  nsPaintEvent pev;

  if( !PtWidgetIsRealized( pWidget ) ) return;

  if( !pWin || !pWin->mContext ) return;

  // This prevents redraws while any window is resizing, ie there are
  //   windows in the resize queue
  if( pWin->mIsResizing ) return;

	if ( pWin->mEventCallback ) {
		PhRect_t   rect;
		PhArea_t   area;
		PhPoint_t  offset;
		nsRect     nsDmg;

		// Ok...  I ~think~ the damage rect is in window coordinates and is not neccessarily clipped to
		// the widgets canvas. Mozilla wants the paint coords relative to the parent widget, not the window.
		PtWidgetArea( pWidget, &area );
		PtWidgetOffset( pWidget, &offset );

		/* Build a List of Tiles that might be in front of me.... */
		PhTile_t *intersection = NULL, *clip_tiles = pWin->GetWindowClipping( );

		/* Intersect the Damage tile list w/ the clipped out list and see whats left! */
		PhTile_t *new_damage, *tiles;

		if( damage->next ) tiles = PhCopyTiles( damage->next );
		else tiles = PhRectsToTiles( &damage->rect, 1 );

		/* new_damage is the same as tiles2... I need to release it later */
		new_damage = PhClipTilings( tiles, clip_tiles, &intersection );

		PhFreeTiles( clip_tiles );
		if( intersection ) PhFreeTiles( intersection );
		if( new_damage == nsnull ) return; /* tiles and clip_tiles have been released */

		PhDeTranslateTiles( new_damage, &offset );

		pWin->InitEvent(pev, NS_PAINT);
		pev.eventStructType = NS_PAINT_EVENT;
		pev.renderingContext = nsnull;
		pev.renderingContext = pWin->GetRenderingContext();

		for( dmg = new_damage; dmg; dmg = dmg->next ) {

			/* Copy the Damage Rect - Do I need to Translate it?? If so by what? offset? offset+area? */
      nsDmg.x = dmg->rect.ul.x - area.pos.x;
      nsDmg.y = dmg->rect.ul.y - area.pos.y;
      nsDmg.width = dmg->rect.lr.x - dmg->rect.ul.x + 1;
      nsDmg.height = dmg->rect.lr.y - dmg->rect.ul.y + 1;

			if( (nsDmg.width <= 0 ) || (nsDmg.height <= 0 ) ) { /* Move to the next Damage Tile */
				continue;
				}

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
	PhPoint_t w_offset;

	PtWidgetOffset( aWidget, &w_offset );

	for( w = PtWidgetChildFront( aWidget ); w; w=PtWidgetBrotherBehind( w ) ) {
		long flags = PtWidgetFlags( w );

//		if( (flags & Pt_OPAQUE) && (w != PtFindDisjoint(w)) && PtWidgetIsRealized( w ) ) {
		if( (flags & (Pt_OPAQUE|Pt_REALIZED)) && (w != PtFindDisjoint(w)) ) {
			PhTile_t *tile = PhGetTile( );
			if( !tile ) return NULL;

			tile->rect.ul.x = w->area.pos.x + w_offset.x;
			tile->rect.ul.y = w->area.pos.y + w_offset.y;
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
  PhRect_t *extents = (PhRect_t *)cbinfo->cbdata; 
  nsWindow *someWindow = (nsWindow *) GetInstance(widget);
  nsRect rect;

  if( someWindow && extents)	// kedl
  {
    rect.x = extents->ul.x;
    rect.y = extents->ul.y;
    rect.width = extents->lr.x - rect.x + 1;
    rect.height = extents->lr.y - rect.y + 1;
    someWindow->mBounds.width  = rect.width;	// kedl, fix main window not resizing!!!!
    someWindow->mBounds.height = rect.height;

    /* This enables the resize holdoff */
    if (PtWidgetIsRealized(widget)) {
      someWindow->ResizeHoldOff();
      someWindow->OnResize( rect );
    }
  }
	return( Pt_CONTINUE );
}

void nsWindow::ResizeHoldOff() {

  if( !mWidget ) return;

  if( PR_FALSE == mResizeQueueInited ) {
    // This is to guarantee that the Invalidation work-proc is in place prior to the resize work-proc.
    if( !mDmgQueueInited ) Invalidate( PR_FALSE );

    PtWidget_t *top = PtFindDisjoint( mWidget );

		if( (mResizeProcID = PtAppAddWorkProc( nsnull, ResizeWorkProc, top )) != nsnull ) {
			int Global_Widget_Hold_Count;
			Global_Widget_Hold_Count =  PtHold();
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
      PtWidget_t *top = PtFindDisjoint( mWidget );
      int Global_Widget_Hold_Count = PtRelease( );
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

    int Global_Widget_Hold_Count;
    Global_Widget_Hold_Count =  PtRelease();
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
    
			PtWidget_t *parent, *disjoint = PtFindDisjoint( mWidget );
			disjoint = PtFindDisjoint( PtWidgetParent( disjoint ) );

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
			parent = PtWidgetParent( mWidget );
			PtWidgetOffset( parent, &offset );
			aX += offset.x;
			aY += offset.y;

			offset = nsToolkit::GetConsoleOffset();
			aX -= offset.x;
			aY -= offset.y;
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

  nsWindow      *pWin = (nsWindow*) data;
  nsWindow      *pWin2 = (nsWindow*) GetInstance(widget);
  nsresult      result;
  PhEvent_t	    *event = cbinfo->event;

	if( event->type == Ph_EV_BUT_PRESS ) {
		if( pWin->gRollupWidget && pWin->gRollupListener ) {
			/* Close the Window */
			pWin->gRollupListener->Rollup();
			IgnoreEvent = event->timestamp;
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


int nsWindow::PopupMenuRegionCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
  return RawEventHandler(widget, data, cbinfo);
}
