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

#include "nsPhWidgetLog.h"
#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIRenderingContextPh.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsStringUtil.h"
#include <Pt.h>
#include "PtRawDrawContainer.h"
#include "nsGfxCIID.h"
#include "prtime.h"

#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"


static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
static NS_DEFINE_IID(kRenderingContextPhIID, NS_IRENDERING_CONTEXT_PH_IID);


// for nsISupports
NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)


/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 *
*/
nsresult nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kCWindow, NS_WINDOW_CID);
    if (aIID.Equals(kCWindow)) {
        *aInstancePtr = (void*) ((nsWindow*)this);
        AddRef();
        return NS_OK;
    }
    return nsWidget::QueryInterface(aIID,aInstancePtr);
}



//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  NS_INIT_REFCNT();
//  strcpy(gInstanceClassName, "nsWindow");
  mClientWidget = nsnull;
  mRawDraw = nsnull;
  mFontMetrics = nsnull;
//  mShell = nsnull;
//  mVBox = nsnull;
  mResized = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
//  mBorderStyle = GTK_WINDOW_TOPLEVEL;
  mIsDestroying = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mFont = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::~nsWindow (%p) - Not Implemented.\n", this ));

  mIsDestroying = PR_TRUE;

//  if (mShell)
//  {
//    if (GTK_IS_WIDGET(mShell))
//      gtk_widget_destroy(mShell);
//    mShell = nsnull;
//  }

}

//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ConvertToDeviceCoordinates - Not Implemented.\n"));
}

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetTooltips - Not Implemented.\n"));

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::UpdateTooltips - Not Implemented.\n"));

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RemoveTooltips - Not Implemented.\n"));
  return NS_OK;
}


#if 0
NS_METHOD nsWindow::Destroy()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Destroy\n"));

  // disconnect from the parent

  if( !mIsDestroying )
    nsBaseWidget::Destroy();

  if( mWidget )
  {
    mEventCallback = nsnull;
    PtDestroyWidget( mWidget );
    mWidget = nsnull;

    if( PR_FALSE == mOnDestroyCalled )
      OnDestroy();
  }

  return NS_OK;
}
#endif

#if 0
void nsWindow::OnDestroy()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::OnDestroy\n"));

  mOnDestroyCalled = PR_TRUE;

  // release references to children, device context, toolkit, and app shell
  nsBaseWidget::OnDestroy();

  // dispatch the event
  if( !mIsDestroying )
  {
    // dispatching of the event may cause the reference count to drop to 0
    // and result in this object being destroyed. To avoid that, add a reference
    // and then release it after dispatching the event
    AddRef();
    DispatchStandardEvent(NS_DESTROY);
    Release();
  }
}
#endif


NS_METHOD nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PreCreateWidget - Not Implemented.\n"));

/*
  if (nsnull != aInitData) {
    switch(aInitData->mBorderStyle)
    {
      case eBorderStyle_none:
        break;
      case eBorderStyle_dialog:
        mBorderStyle = GTK_WINDOW_DIALOG;
        break;
      case eBorderStyle_window:
        mBorderStyle = GTK_WINDOW_TOPLEVEL;
        break;
      case eBorderStyle_3DChildWindow:
        break;
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
*/
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative(PtWidget_t *parentWidget)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative (%p) - parent = %p.\n", this, parentWidget));

  PtArg_t arg[20];
  PhPoint_t pos;
  PhDim_t dim;
  unsigned long render_flags;
  nsresult result = NS_ERROR_FAILURE;

  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
  // REVISIT
  //printf( "Must check thread here...\n" );  

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width;
  dim.h = mBounds.height;

  //PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - bounds = %lu,%lu.\n", mBounds.width, mBounds.height ));

#define MY_FIELDS Ph_REGION_EV_SENSE |\
                     Ph_REGION_FLAGS |\
                     Ph_REGION_EV_OPAQUE

  if( IsChild() )
  {
    PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[2], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[3], Pt_ARG_FLAGS, 0 /*Pt_HIGHLIGHTED*/, Pt_HIGHLIGHTED );
    PtSetArg( &arg[4], Pt_ARG_BORDER_WIDTH, 0, 0 );
    PtSetArg( &arg[5], Pt_ARG_TOP_BORDER_COLOR, Pg_RED, 0 );
    PtSetArg( &arg[6], Pt_ARG_BOT_BORDER_COLOR, Pg_RED, 0 );
//kedl, this is it!!!
//    PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );
//    PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_BLUE, 0 );
//    PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_GREY, 0 );
//extern int raw_container_color;
//    PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, raw_container_color, 0 );
//kedl, but falls thru to red.....
//    mWidget = PtCreateWidget( PtContainer, parentWidget, 8, arg );
//    mWidget = PtCreateWidget( PtRaw, parentWidget, 8, arg );
    PtSetArg( &arg[7], RDC_DRAW_FUNC, RawDrawFunc, 0 );
    mWidget = PtCreateWidget( PtRawDrawContainer, parentWidget, 8, arg );
//printf ("kedl: raw parent:%lu\n",mWidget);
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - Is a Child\n" ));

#if 0
    if( mWidget )
    {
      PhDim_t rawDim = { 20, 20 };
      PtSetArg( &arg[0], Pt_ARG_DIM, &rawDim, 0 );
      PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[2], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PtSetArg( &arg[3], Pt_ARG_RAW_DRAW_F, RawDrawFunc, 0 );
      PtSetArg( &arg[4], Pt_ARG_TOP_BORDER_COLOR, Pg_GREEN, 0 );
      PtSetArg( &arg[5], Pt_ARG_BOT_BORDER_COLOR, Pg_GREEN, 0 );
      PtSetArg( &arg[6], Pt_ARG_FILL_COLOR, /*Pg_GRAY*/ Pg_GREEN, 0 );
      PtSetArg( &arg[7], Pt_ARG_FLAGS, 0 ,Pt_HIGHLIGHTED );
      mRawDraw = PtCreateWidget( PtRaw, mWidget, 8, arg );
//printf ("kedl: raw widget: %lu\n",mRawDraw);
    }
#endif
  }
  else
  {
    render_flags = Ph_WM_RENDER_TITLE | Ph_WM_RENDER_CLOSE |
                   Ph_WM_RENDER_BORDER | Ph_WM_RENDER_RESIZE |
                   Ph_WM_RENDER_MAX | Ph_WM_RENDER_MIN | Ph_WM_RENDER_MENU;

    PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[2], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[3], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, 0xFFFFFFFF );
    mWidget = PtCreateWidget( PtWindow, parentWidget, 4, arg );
    
    // Must also create the client-area widget
    if( mWidget )
    {
      PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_ANCHOR_FLAGS, Pt_LEFT_ANCHORED_LEFT |
        Pt_RIGHT_ANCHORED_RIGHT | Pt_TOP_ANCHORED_TOP | Pt_BOTTOM_ANCHORED_BOTTOM, 0xFFFFFFFF );
      PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[3], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PtSetArg( &arg[4], Pt_ARG_FLAGS, 0 , Pt_HIGHLIGHTED|Pt_OPAQUE );
      PtSetArg( &arg[5], Pt_ARG_TOP_BORDER_COLOR, Pg_BLUE, 0 );
      PtSetArg( &arg[6], Pt_ARG_BOT_BORDER_COLOR, Pg_BLUE, 0 );
// kedl, falls thru to backdrop...
      PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );
      PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_RED, 0 );
      PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_GREY, 0 );
      PtSetArg( &arg[8], Pt_ARG_REGION_FIELDS, MY_FIELDS, MY_FIELDS);
      PtSetArg( &arg[9], Pt_ARG_REGION_SENSE, Ph_EV_WIDGET_SENSE, Ph_EV_WIDGET_SENSE );
      PtSetArg( &arg[9], Pt_ARG_REGION_SENSE, Ph_EV_WIN_OPAQUE, Ph_EV_WIN_OPAQUE );
//      mClientWidget = PtCreateWidget( PtRegion, mWidget, 10, arg );
      mClientWidget = PtCreateWidget( PtContainer, mWidget, 8, arg );
      if( !mClientWidget )
      {
        PtDestroyWidget( mWidget );
        mWidget = nsnull;
      }
    }
  }

  if( mWidget )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - mWidget=%p, mClientWidget=%p\n", mWidget, mClientWidget ));

    SetInstance( mWidget, this );

//    if( mRawDraw )
//      SetInstance( mRawDraw, this );

    if( mClientWidget )
      SetInstance( mClientWidget, this );

    // Attach event handler
    if( IsChild())
    {
 	PhDim_t    *dim; 
 	dim = (PhDim_t *)malloc(sizeof(PhDim_t)); 
 	PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, dim); 

        PtAddEventHandler( mWidget,
          Ph_EV_KEY | Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON |
          Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY,
          RawEventHandler, this );
    }
    else
    {
 	PhDim_t    *dim; 
 	dim = (PhDim_t *)malloc(sizeof(PhDim_t)); 
 	PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, dim); 

//        PtAddEventHandler( mWidget, Ph_EV_WM, RawEventHandler, this );
//        PtAddEventHandler( mClientWidget, Ph_EV_PTR_ALL | Ph_EV_KEY, RawEventHandler, this );
    }
    
    // call the event callback to notify about creation
    DispatchStandardEvent(NS_CREATE);

    result = NS_OK;
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - FAILED TO CREATE WIDGET!\n" ));

  mIsToplevel = PR_TRUE;
  mCursor = eCursor_select;
  SetCursor(eCursor_standard);

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
    if( !mWidget )
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_WINDOW ) - mWidget is NULL!\n"));
    return (void *)mWidget;
  case NS_NATIVE_DISPLAY:
//    return (void *)GDK_DISPLAY();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_DISPLAY ) - Not Implemented.\n"));
    return nsnull;
  case NS_NATIVE_WIDGET:
    if( IsChild() )
    {
      if( !mWidget )
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_WIDGET ) this=%p IsChild=TRUE - mWidget is NULL!\n", this ));
      return (void *)mWidget;
    }
    else
    {
      if( !mClientWidget )
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_WIDGET ) this=%p IsChild=FALSE - mClientWidget is NULL!\n", this ));
      return (void *)mClientWidget;
    }

  case NS_NATIVE_GRAPHIC:
//    return (void *)((nsToolkit *)mToolkit)->GetSharedGC();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_GRAPHIC ) - Not Implemented.\n"));
    return nsnull;
  default:
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData - Wierd value.\n"));
    break;
  }
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetColorMap - Not Implemented.\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll aDx=<%d aDy=<%d> aClipRect=<%p> - Not Implemented.\n", aDx, aDy, aClipRect));

  PtWidget_t *widget;
  PhRid_t     rid;

  if( IsChild() )
  {
//    widget = mRawDraw;
    widget = mWidget;
    rid = PtWidgetRid( mWidget );
  }
  else
  {
    widget = mClientWidget;
    rid = PtWidgetRid( mClientWidget );
  }

  if( widget )
  {
    PhRect_t    rect;
    PhPoint_t   offset = { aDx, aDy };
    PhArea_t    area;

    PtWidgetArea( widget, &area );

    // Ignore clipping for now...

    rect.ul.x = /*origin.x +*/ area.pos.x;
    rect.ul.y = /*origin.y +*/ area.pos.y;
    rect.lr.x = rect.ul.x + area.size.w - 1;
    rect.lr.y = rect.ul.y + area.size.h - 1;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  blit rect = %ld,%ld,%ld,%ld\n", rect.ul.x, rect.ul.y, rect.lr.x, rect.lr.y ));

    if( aClipRect )
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clip rect = %ld,%ld,%ld,%ld\n", aClipRect->x, aClipRect->y, aClipRect->width, aClipRect->height ));
    else
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  no clip rect\n" ));

    PgFlush();
    PhBlit( rid, &rect, &offset );

    // Invalidate exposed area...

    if( aDx > 0 )
      rect.lr.x = rect.ul.x + aDx;
    else if( aDx < 0 )
      rect.ul.x = rect.lr.x + aDx;

    if( aDy > 0 )
      rect.lr.y = rect.ul.y + aDy;
    else if( aDy < 0 )
      rect.ul.y = rect.lr.y + aDy;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  dmg rect = %ld,%ld,%ld,%ld\n", rect.ul.x, rect.ul.y, rect.lr.x, rect.lr.y ));
  
    PtDamageExtent( widget, &rect );

	// move all the children
PtWidget_t *w;
PtArg_t  arg;
PhPoint_t *pos;
PhPoint_t p;

	for( w = PtWidgetChildFront( mWidget ); 
	     w; 
	     w = PtWidgetBrotherBehind( w ) ) 
	{ 
//		printf ("move: %lu\n",w);
		PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
		PtGetResources( w, 1, &arg ) ;
//		printf ("old pos: %d %d\n",pos->x,pos->y);
		p = *pos;
		p.x += aDx;
		p.y += aDy;
//		printf ("new pos: %d %d\n",p.x,p.y);
		PtSetArg( &arg, Pt_ARG_POS, &p, 0 );
		PtSetResources( w, 1, &arg ) ;
		PtDamageWidget(w);
	} 
  }
  
  return NS_OK;
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetTitle.\n"));

  if( mWidget )
  {
    PtArg_t  arg;
    char     *title = aTitle.ToNewCString();

    PtSetArg( &arg, Pt_ARG_WINDOW_TITLE, title, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
      res = NS_OK;
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetTitle - mWidget is NULL!\n"));

  return res;
}


/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::OnPaint - Not Implemented.\n"));
  return NS_OK;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::BeginResizingChildren.\n"));

//  PtHold();

  PtStartFlux( mWidget );
#if 0
  if( IsChild() )
    PtContainerHold( mWidget );
  else
    PtContainerHold( mClientWidget );
#endif
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::EndResizingChildren.\n"));

//  PtRelease();

  PtEndFlux( mWidget );
#if 0
  if( IsChild() )
    PtContainerRelease( mWidget );
  else
    PtContainerRelease( mClientWidget );
#endif
  return NS_OK;
}


NS_METHOD nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize (%p) from (%ld,%ld) to (%ld,%ld).\n", this,
  mBounds.width, mBounds.height, aWidth, aHeight ));

  nsWidget::Resize( aWidth, aHeight, aRepaint );

#if 0
  if( IsChild() )
  {
    PtArg_t arg[5];
    PhDim_t dim = { aWidth, aHeight };

    PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
    PtSetResources( mRawDraw, 1, arg );
  }
#endif

  return NS_OK;
}


NS_METHOD nsWindow::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth,
                           PRUint32 aHeight, PRBool aRepaint)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize (cover).\n" ));

  Resize(aWidth,aHeight,aRepaint);
  Move(aX,aY);
  return NS_OK;
}


PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback) {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, (" nsWindow::OnKey - mEventCallback=<%p>\n", mEventCallback));
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}


PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::DispatchFocus - Not Implemented.\n"));

  if( mEventCallback )
  {
//    return DispatchWindowEvent(&aEvent);
//    return DispatchStandardEvent(&aEvent);
  }

  return PR_FALSE;
}

PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::OnScroll - Not Implemented.\n"));
  return PR_FALSE;
}


NS_METHOD nsWindow::GetBounds( nsRect &aRect )
{
  nsresult   res = NS_ERROR_FAILURE;

  if( IsChild() )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetBounds for child window (%p).\n", this ));

    // A child window is just another widget, call base class GetBounds
    res = nsWidget::GetBounds( aRect );
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetBounds - not a child (%p).\n", this ));

    if( mWidget )
    {
      PtArg_t        arg[5];
      PhDim_t        *dim;
      int            *border;
      unsigned short *render;

      PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, &border, 0 );
      PtSetArg( &arg[2], Pt_ARG_WINDOW_RENDER_FLAGS, &render, 0 );

      if( PtGetResources( mWidget, 3, arg ) == 0 )
      {
        short x,y;
        int l,r,t,b;

        // Get position

        PtGetAbsPosition( mWidget, &x, &y );
        
        // Get Frame dimensions

        PtFrameSize( *render, 0, &l, &t, &r, &b );
        
        aRect.x = x - l;
        aRect.y = y - t;
        aRect.width = dim->w + 2*(*border) + r;
        aRect.height = dim->h + 2*(*border) + b;

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetBounds = %ld,%ld,%ld,%ld\n", aRect.x, aRect.y, aRect.width, aRect.height ));
      }
    }
  }

  return res;
}


NS_METHOD nsWindow::GetClientBounds( nsRect &aRect )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetClientBounds (%p).\n", this ));

  nsresult   res = NS_ERROR_FAILURE;

  if( IsChild() )
  {
    // A child window is just another widget, call base class GetBounds
    res = nsWidget::GetBounds( aRect );
  }
  else
  {
    // All we care about here are the dimensions of our mClientWidget.
    // The origin is ALWAYS 0,0
    if( mClientWidget )
    {
      PtArg_t  arg[5];
      PhDim_t  *dim;
      int      *border;

      PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, &border, 0 );
      if( PtGetResources( mClientWidget, 2, arg ) == 0 )
      {
        aRect.x = 0;
        aRect.y = 0;
        aRect.width = dim->w + 2*(*border);
        aRect.height = dim->h + 2*(*border);
        res = NS_OK;
      }
    }
  }
  
  return res;
}


NS_METHOD nsWindow::SetMenuBar( nsIMenuBar * aMenuBar )
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetMenuBar\n"));

  nsresult res = NS_ERROR_FAILURE;

  if( IsChild() ) // Child windows can't have menus!
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetMenuBar - ERROR! Trying to set a menu for a ChildWindow!\n"));
    return res;
  }

  mMenuBar = aMenuBar;

  PtArg_t   arg[5];
  int       menu_h = GetMenuBarHeight();
  PhArea_t  *win_area,new_area;
  PhArea_t  old_area;

  // Now, get the dimensions of the real window

  PtSetArg( &arg[0], Pt_ARG_AREA, &win_area, 0 );
  if( PtGetResources( mWidget, 1, arg ) == 0 )
  {
    // Resize the window
    old_area = *win_area;
    new_area = *win_area;
    new_area.size.h += menu_h;

    PtSetArg( &arg[0], Pt_ARG_AREA, &new_area, 0 );
    if( PtSetResources( mWidget, 1, arg ) == 0 )
    {
      int *ca_border;

      // Get the client widgets current area
      PtSetArg( &arg[0], Pt_ARG_BORDER_WIDTH, &ca_border, 0 );
      if( PtGetResources( mClientWidget, 1, arg ) == 0 )
      {
        // Now move the client area below the menu bar

        // New position is just below the menubar
        old_area.pos.x = 0;
        old_area.pos.y = menu_h;

        // New size is the orig. window size minus border width
        old_area.size.w -= 2*(*ca_border);
        old_area.size.h -= 2*(*ca_border);

        PtSetArg( &arg[0], Pt_ARG_AREA, &old_area, 0 );
        if( PtSetResources( mClientWidget, 1, arg ) == 0 )
        {
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetMenuBar - client shifted down by %i\n", menu_h ));
          res = NS_OK;
        }
      }
    }
  }

  return res;
}

NS_METHOD nsWindow::ShowMenuBar( PRBool aShow)
{
  return NS_ERROR_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


int nsWindow::ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
PhRect_t *extents = (PhRect_t *)cbinfo->cbdata; 
nsWindow *someWindow = (nsWindow *) GetInstance(widget);
nsRect rect;

	rect.x = extents->ul.x;
	rect.y = extents->ul.y;
	rect.width = extents->lr.x - rect.x;
	rect.height = extents->lr.y - rect.y - someWindow->GetMenuBarHeight();

//	printf ("doing resize %d %d %d %d\n",rect.x,rect.y,rect.width,rect.height);
	someWindow->OnResize( rect );
	return( Pt_CONTINUE );
}


#if 0
//-------------------------------------------------------------------------
//
// the nsWindow raw event callback for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
int nsWindow::RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  // Get the window which caused the event and ask it to process the message
  nsWindow *someWindow = (nsWindow*) data;

  if( nsnull != someWindow )
  {
//printf ("kedl: nswindow raweventhandler %x\n",someWindow);
    if( someWindow->HandleEvent( cbinfo ))
      return( Pt_END ); // Event was consumed
  }

  return( Pt_CONTINUE );
}
#endif


//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::HandleEvent( PtCallbackInfo_t* aCbInfo )
{
  PRBool  result = PR_FALSE; // call the default nsWindow proc

  // When we get menu selections, dispatch the menu command (event) AND IF there is
  // a menu listener, call "listener->MenuSelected(event)"


  if( aCbInfo->reason == Pt_CB_RAW )
  {
    PhEvent_t* event = aCbInfo->event;

    switch( event->type )
    {
    case Ph_EV_KEY:
      {
        PhKeyEvent_t* keyev = (PhKeyEvent_t*) PhGetData( event );
		result = DispatchKeyEvent(keyev);
      }
      break;

    case Ph_EV_PTR_MOTION_BUTTON:
    case Ph_EV_PTR_MOTION_NOBUTTON:
      {
      PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );

//      printf( "  Mouse move event.\n" );

      if( ptrev )
      {
//        ptrev->pos.y -= GetMenuBarHeight();
        ScreenToWidget( ptrev->pos );
        result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_MOVE );
      }
      }
      break;

    case Ph_EV_BUT_PRESS:
      {
      PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );

//      printf( "  Mouse button-press event.\n" );

      if( ptrev )
      {
//        ptrev->pos.y -= GetMenuBarHeight();
        ScreenToWidget( ptrev->pos );
        if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
        {
        printf( "Window mouse click: (%ld,%ld)\n", ptrev->pos.x, ptrev->pos.y );
          if( ptrev->click_count == 2 )
            result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_LEFT_DOUBLECLICK );
          else
            result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_LEFT_BUTTON_DOWN );
        }
        else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
        {
          if( ptrev->click_count == 2 )
            result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_RIGHT_DOUBLECLICK );
          else
            result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_RIGHT_BUTTON_DOWN );
        }
        else // middle button
        {
          if( ptrev->click_count == 2 )
            result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_MIDDLE_DOUBLECLICK );
          else
            result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_MIDDLE_BUTTON_DOWN );
        }
      }
      }
      break;

    case Ph_EV_BUT_RELEASE:
      {
      PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );

// printf( "  Mouse button-release event.\n" );

      if (event->subtype==Ph_EV_RELEASE_REAL)
      if( ptrev )
      {
//        ptrev->pos.y -= GetMenuBarHeight();
        ScreenToWidget( ptrev->pos );
        if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
          result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_LEFT_BUTTON_UP );
        else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
          result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_RIGHT_BUTTON_UP );
        else // middle button
          result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_MIDDLE_BUTTON_UP );
      }
      }
      break;

    case Ph_EV_BOUNDARY:
      switch( event->subtype )
      {
      case Ph_EV_PTR_ENTER:
        result = DispatchStandardEvent( NS_MOUSE_ENTER );
        break;
      case Ph_EV_PTR_LEAVE:
        result = DispatchStandardEvent( NS_MOUSE_EXIT );
        break;
      }
      break;

    case Ph_EV_WM:
      {
      PhWindowEvent_t* wmev = (PhWindowEvent_t*) PhGetData( event );

//      printf( "  WM event.\n" );

      switch( wmev->event_f )
      {
      case Ph_WM_FOCUS:
        {
        if( wmev->event_state == Ph_WM_EVSTATE_FOCUS )
          result = DispatchStandardEvent(NS_GOTFOCUS);
        else
          result = DispatchStandardEvent(NS_LOSTFOCUS);
        }
        break;

      case Ph_WM_RESIZE:
        {
          nsRect rect;
          PtArg_t arg[5];

          rect.x = wmev->pos.x;
          rect.y = wmev->pos.y;
          rect.width = wmev->size.w;
          rect.height = wmev->size.h - GetMenuBarHeight();

          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow (%p) Resize Event\n", this ));

          OnResize( rect );
        }
        break;
      }
      }
      break;
    }
  }

  return result;
}


void nsWindow::RawDrawFunc( PtWidget_t * pWidget, PhTile_t * damage )
{
  nsWindow * pWin = (nsWindow*) GetInstance( pWidget );

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc for %p\n", pWin ));

  if( !pWin )
    return;

  if( pWin->mEventCallback )
  {
    PhRect_t   rect;
    PhArea_t   area;
    PhPoint_t  offset;
    nsRect     nsDmg;

    // Ok...  I ~think~ the damage rect is in window coordinates and is not neccessarily clipped to
    // the widgets canvas. Mozilla wants the paint coords relative to the parent widget, not the window.

    PtWidgetArea( pWidget, &area );
    PtWidgetOffset( pWidget, &offset );
    offset.x += area.pos.x;  
    offset.y += area.pos.y;  

    // Convert damage rect to widget's coordinates...
    rect = damage->rect;
    rect.ul.x -= offset.x;
    rect.ul.y -= offset.y;
    rect.lr.x -= offset.x;
    rect.lr.y -= offset.y;

    // If the damage tile is not within our bounds, do nothing
    if(( rect.ul.x >= area.size.w ) || ( rect.ul.y >= area.size.h ) || ( rect.lr.x < 0 ) || ( rect.lr.y < 0 ))
      return;

    // clip damage to widgets bounds...
    if( rect.ul.x < 0 ) rect.ul.x = 0;
    if( rect.ul.y < 0 ) rect.ul.y = 0;
    if( rect.lr.x >= area.size.w ) rect.lr.x = area.size.w - 1;
    if( rect.lr.y >= area.size.h ) rect.lr.y = area.size.h - 1;

    // Make damage relative to widgets parent
    nsDmg.x = rect.ul.x + area.pos.x;
    nsDmg.y = rect.ul.y + area.pos.y;

    // Get the position of the child window instead of pWidgets pos since it's always 0,0.

//    PtWidget_t * parent = PtWidgetParent( pWidget );
//    if( parent )
//    {
//      PtWidgetArea( parent, &area );
//      nsDmg.x = area.pos.x + rect.ul.x;
//      nsDmg.y = area.pos.y + rect.ul.y;
//    }
//    else
//      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  No parent!\n" ));

    nsDmg.width = rect.lr.x - rect.ul.x + 1;
    nsDmg.height = rect.lr.y - rect.ul.y + 1;

    if( !nsDmg.width || !nsDmg.height )
      return;

    nsPaintEvent pev;
    pWin->InitEvent(pev, NS_PAINT);

    pev.rect = &nsDmg;
    pev.eventStructType = NS_PAINT_EVENT;

    PtHold();

    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&pev.renderingContext))
    {
      PhRect_t  *rects;
      int       rect_count;
      int       i;

      pev.renderingContext->Init( pWin->mContext, pWin );

      rects = PhTilesToRects( damage->next, &rect_count );

      for(i=0;i<rect_count;i++)
      {
        rects[i].ul.x -= offset.x;
        rects[i].ul.y -= offset.y;
        rects[i].lr.x -= offset.x;
        rects[i].lr.y -= offset.y;
      }

      PgSetClipping( rect_count, rects );

      for(i=0;i<rect_count;i++)
      {
        rects[i].ul.x += offset.x;
        rects[i].ul.y += offset.y;
        rects[i].lr.x += offset.x;
        rects[i].lr.y += offset.y;
      }

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ( "Dispatching paint event (area=%ld,%ld,%ld,%ld).\n",nsDmg.x,nsDmg.y,nsDmg.width,nsDmg.height ));
      pWin->DispatchWindowEvent(&pev);
      NS_RELEASE(pev.renderingContext);
    }

    PtRelease();

//  NS_RELEASE(pev.widget);
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("*  widget released (not really)\n"));

  }
}


int nsWindow::GetMenuBarHeight()
{
  int h = 0;

  if( mMenuBar )
  {
    void * menubar;
//    PtWidget_t * menubar;

    mMenuBar->GetNativeData( menubar );

    if( menubar )
    {
      PtArg_t arg[2];
      PhDim_t *mb_dim;
      int     *mb_border;

      // Resize the window to accomodate the menubar
      // First, get the menubars dimensions and border size

      PtSetArg( &arg[0], Pt_ARG_DIM, &mb_dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, &mb_border, 0 );
      if( PtGetResources(( PtWidget_t* ) menubar, 2, arg ) == 0 )
      {
        h = mb_dim->h + 2*(*mb_border);
      }
    }
  }

  return h;
}


void nsWindow::ScreenToWidget( PhPoint_t &pt )
{
  // pt is in screen coordinates
  // convert it to be relative to ~this~ widgets origin
  short x=0,y=0;

  if( IsChild())
  {
    PtGetAbsPosition( mWidget, &x, &y );
  }
  else
  {
    PtGetAbsPosition( mClientWidget, &x, &y );
  }

  pt.x -= x;
  pt.y -= y;
}

#if 0

NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
  return nsWidget::Invalidate( aIsSynchronous );
}


NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate %p (%ld,%ld,%ld,%ld)\n", this, aRect.x, aRect.y, aRect.width, aRect.height ));

  if( mWidget )
  {
    PhRect_t extent;
    PhArea_t area;

    PtWidgetArea( mWidget, &area );

    extent.ul.x = aRect.x + area.pos.x;
    extent.ul.y = aRect.y + area.pos.y;
    extent.lr.x = extent.ul.x + aRect.width - 1;
    extent.lr.y = extent.ul.y + aRect.height - 1;
 

    PtDamageExtent( mWidget, &extent );

/*
if (PtWidgetChildFront(mWidget))
    PtDamageExtent( PtWidgetChildFront(mWidget), &extent );
else
    PtDamageExtent( mWidget, &extent );
*/

#if 0
  PtWidget_t *w;

	for( w = PtWidgetChildFront( mWidget ); 
	     w; 
	     w = PtWidgetBrotherBehind( w ) ) 
	{ 
//		printf ("damage: %lu\n",w);
		PtDamageExtent(w,&extent);
	} 
#endif

    // REVISIT - PtFlush may be unstable & cause crashes...
    if( aIsSynchronous )
      PtFlush();
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate - mWidget is NULL!\n" ));

  return NS_OK;
}
#endif


