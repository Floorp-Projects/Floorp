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
#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
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

PRBool            nsWindow::mResizeQueueInited = PR_FALSE;
DamageQueueEntry  *nsWindow::mResizeQueue = nsnull;
PtWorkProcId_t    *nsWindow::mResizeProcID = nsnull;

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::nsWindow (%p)\n", this ));
  NS_INIT_REFCNT();

  mClientWidget    = nsnull;
  mShell           = nsnull;
  mFontMetrics     = nsnull;
  mClipChildren    = PR_TRUE;		/* This needs to be true for Photon */
  mClipSiblings    = PR_FALSE;		/* TRUE = Fixes Pop-Up Menus over animations */
  mBorderStyle     = eBorderStyle_default;
  mWindowType      = eWindowType_child;
  mIsResizing      = PR_FALSE;
  mFont            = nsnull;
  mMenuBar         = nsnull;
  mMenuBarVis      = PR_FALSE;
  mFrameLeft       = 0;
  mFrameRight      = 0;
  mFrameTop        = 0;
  mFrameBottom     = 0;

  mIsDestroyingWindow = PR_FALSE;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  border=%X, window=%X\n", mBorderStyle, mWindowType ));
}

ChildWindow::ChildWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("ChildWindow::ChildWindow this=(%p)\n", this ));
  mBorderStyle     = eBorderStyle_none;
  mWindowType      = eWindowType_child;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  border=%X, window=%X\n", mBorderStyle, mWindowType ));
}

ChildWindow::~ChildWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("ChildWindow::~ChildWindow this=(%p)\n", this ));
}

PRBool ChildWindow::IsChild() const
{
  return PR_TRUE;
}

NS_METHOD ChildWindow::Destroy()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("ChildWindow::Destroy this=(%p)\n", this ));

  RemoveResizeWidget(); 
  RemoveDamagedWidget( mWidget );

  return nsWidget::Destroy();
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::~nsWindow this=(%p)\n", this ));

  mIsDestroyingWindow = PR_TRUE;

  if ( (mWindowType == eWindowType_dialog) ||
       (mWindowType == eWindowType_popup) ||
	   (mWindowType == eWindowType_toplevel) )
  {
    Destroy();
  }
  NS_IF_RELEASE(mMenuBar);
}

NS_METHOD nsWindow::Destroy(void)
{
/* Pinkerton says this routine is garbage ands will be going away soon */
/* Removing the test for mIsDestroyingWindow forces the underlieing widget */
/* to be destroyed. This fixes the problem when the 2 toolbar is collapsed.*/

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Destroy (%p) mIsDestroyingWindow=<%d> mOnDestroyCalled=<%d> mRefCnt=<%d>\n", this, mIsDestroyingWindow, mOnDestroyCalled, mRefCnt));

  NS_IF_RELEASE(mMenuBar);

  // Call base class first... we need to ensure that upper management
  // knows about the close so that if this is the main application
  // window, for example, the application will exit as it should.

#if 0
//  if (mIsDestroyingWindow == PR_TRUE)
  {
    nsBaseWidget::Destroy();
    mEventCallback = nsnull;			/* adding this if close window */
    if (PR_FALSE == mOnDestroyCalled)
	{
        nsWidget::OnDestroy();
    }
  }
#else
  if (mIsDestroyingWindow == PR_TRUE)
  {
	NS_IF_RELEASE(mParent);
    nsBaseWidget::Destroy();
    mEventCallback = nsnull;			/* adding this if close window */
    if (PR_FALSE == mOnDestroyCalled)
	{
        nsWidget::OnDestroy();
    }
  }
#endif

  RemoveResizeWidget();
  RemoveDamagedWidget( mWidget );
  return NS_OK;
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


NS_METHOD nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PreCreateWidget\n"));

  if (nsnull != aInitData)
  {
    SetWindowType( aInitData->mWindowType );
    SetBorderStyle( aInitData->mBorderStyle );
//    mClipChildren = aInitData->clipChildren;
//    mClipSiblings = aInitData->clipSiblings;

//    if (mWindowType==1) mClipChildren = PR_FALSE; else mClipChildren = PR_TRUE;
//    if (mWindowType==2) mClipSiblings = PR_TRUE; else mClipSiblings = PR_FALSE;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PreCreateWidget mClipChildren=<%d> mClipSiblings=<%d> mBorderStyle=<%d> mWindowType=<%d>\n",
      mClipChildren, mClipSiblings, mBorderStyle, mWindowType));
//    printf("kedl: nsWindow::PreCreateWidget mClipChildren=<%d> mClipSiblings=<%d> mBorderStyle=<%d> mWindowType=<%d>\n",
//      mClipChildren, mClipSiblings, mBorderStyle, mWindowType);

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative(PtWidget_t *parentWidget)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative this=(%p) - PhotonParent=<%p> IsChild=<%d> mParent=<%p>\n", this, parentWidget, IsChild(), mParent));

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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - bounds = (%d,%d,%d,%d)\n", mBounds.x, mBounds.y, mBounds.width, mBounds.height ));

#ifdef DEBUG
  switch( mWindowType )
  {
  case eWindowType_popup :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("\twindow type = popup\n" ));
    mIsToplevel = PR_TRUE;
    break;
  case eWindowType_toplevel :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("\twindow type = toplevel\n" ));
    mIsToplevel = PR_TRUE;
    break;
  case eWindowType_dialog :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("\twindow type = dialog\n" ));
    mIsToplevel = PR_TRUE;
	break;
  case eWindowType_child :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("\twindow type = child\n" ));
    mIsToplevel = PR_FALSE;
	break;
  }
#endif

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("\tborder style = %X\n", mBorderStyle ));

  if ( mWindowType == eWindowType_child )
  {
    arg_count = 0;
    PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0 /*Pt_HIGHLIGHTED*/, Pt_HIGHLIGHTED );
    PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0, 0 );
    //PtSetArg( &arg[arg_count++], Pt_ARG_TOP_BORDER_COLOR, Pg_RED, 0 );
    //PtSetArg( &arg[arg_count++], Pt_ARG_BOT_BORDER_COLOR, Pg_RED, 0 );
    PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
    mWidget = PtCreateWidget( PtRawDrawContainer, parentWidget, arg_count, arg );
  }
  else
  {
    // No border or decorations is the default
    render_flags = 0;

    if( mWindowType == eWindowType_popup )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Creating a pop-up (no decorations).\n" ));
    }
    else
    {
      #define PH_BORDER_STYLE_ALL  \
        Ph_WM_RENDER_TITLE | \
        Ph_WM_RENDER_CLOSE | \
        Ph_WM_RENDER_BORDER | \
        Ph_WM_RENDER_RESIZE | \
        Ph_WM_RENDER_MAX | \
        Ph_WM_RENDER_MIN | \
        Ph_WM_RENDER_MENU 


      if( mBorderStyle & eBorderStyle_all )
	  {
        render_flags = PH_BORDER_STYLE_ALL;
      }
      else
      {
        if( mBorderStyle & eBorderStyle_border )
          render_flags |= Ph_WM_RENDER_BORDER;

        if( mBorderStyle & eBorderStyle_title )
          render_flags |= ( Ph_WM_RENDER_TITLE | Ph_WM_RENDER_BORDER );

        if( mBorderStyle & eBorderStyle_close )
          render_flags |= Ph_WM_RENDER_CLOSE;

        if( mBorderStyle & eBorderStyle_menu )
          render_flags |= Ph_WM_RENDER_MENU;

        if( mBorderStyle & eBorderStyle_resizeh )
          render_flags |= Ph_WM_RENDER_RESIZE;

        if( mBorderStyle & eBorderStyle_minimize )
          render_flags |= Ph_WM_RENDER_MIN;

        if( mBorderStyle & eBorderStyle_maximize )
          render_flags |= Ph_WM_RENDER_MAX;
      }

      // Remember frame size for later use...
      PtFrameSize( render_flags, 0, &mFrameLeft, &mFrameTop, &mFrameRight, &mFrameBottom );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative Frame size L,T,R,B=(%d,%d,%d,%d)\n",
	      mFrameLeft, mFrameTop, mFrameRight, mFrameBottom ));
    }

    arg_count = 0;
    PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, 0xFFFFFFFF );

    if( mWindowType == eWindowType_popup )
    {
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DISJOINT, (Pt_HIGHLIGHTED | Pt_DISJOINT | Pt_GETS_FOCUS));
    }

    if( parentWidget )
	{
      mWidget = PtCreateWidget( PtWindow, parentWidget, arg_count, arg );
	}
    else
    {
      PtSetParentWidget( nsnull );
      mWidget = PtCreateWidget( PtWindow, nsnull, arg_count, arg );
    }
    
    // Must also create the client-area widget
    if( mWidget )
    {
      arg_count = 0;
      PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_FLAGS, Pt_LEFT_ANCHORED_LEFT |
                                   Pt_RIGHT_ANCHORED_RIGHT | Pt_TOP_ANCHORED_TOP | 
								   Pt_BOTTOM_ANCHORED_BOTTOM, 0xFFFFFFFF );
      PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_GREY, 0 );
      PhRect_t anch_offset = { 0, 0, 0, 0 };
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      if( mWindowType == eWindowType_popup )
	  {
        PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
        PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DISJOINT, (Pt_HIGHLIGHTED | Pt_DISJOINT | Pt_GETS_FOCUS));
        PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_MANAGED_FLAGS, Ph_WM_FFRONT, Ph_WM_FFRONT );
        mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, arg_count, arg );
	  }
	  else
	  {  
        PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, Pt_HIGHLIGHTED );
        mClientWidget = PtCreateWidget( PtContainer, mWidget,arg_count, arg );
	  }
	  
      // Create a region that is opaque to draw events and place behind
      // the client widget.
      if( !mClientWidget )
      {
        PtDestroyWidget( mWidget );
        mWidget = nsnull;
		NS_ASSERTION(0,"nsWindow::CreateNative Error creating Client Widget\n");
		abort();
      }
    }
  }

  if( mWidget )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - mWidget=%p, mClientWidget=%p\n", mWidget, mClientWidget ));

    SetInstance( mWidget, this );

    if( mClientWidget )
      SetInstance( mClientWidget, this );
 
    if (mWindowType == eWindowType_child)
    {
      PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
      PtAddEventHandler( mWidget,
        Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON |
        Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY
//		| Ph_EV_WM
//        | Ph_EV_EXPOSE
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
        Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY
//		| Ph_EV_WM
//        | Ph_EV_EXPOSE
        , RawEventHandler, this );

      PtArg_t arg;
      PtRawCallback_t callback;
		
		callback.event_mask = ( Ph_EV_KEY ) ;
		callback.event_f = RawEventHandler;
		callback.data = this;
		PtSetArg( &arg, Pt_CB_FILTER, &callback, 0 );
		PtSetResources( mClientWidget, 1, &arg );
			
      PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
      PtAddCallback(mWidget, Pt_CB_WINDOW_CLOSING, WindowCloseHandler, this );	
	}
    else if ( !parentWidget )
    {
      PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
      PtAddCallback(mWidget, Pt_CB_WINDOW_CLOSING, WindowCloseHandler, this ); 
    }

    // call the event callback to notify about creation
    DispatchStandardEvent( NS_CREATE );

    result = NS_OK;
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative - FAILED TO CREATE WIDGET!\n" ));

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
    if( !mWidget )
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_WINDOW ) - mWidget is NULL!\n"));
    return (void *)mWidget;

  case NS_NATIVE_WIDGET:
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetNativeData( NS_NATIVE_WIDGET ) - this=<%p> mWidget=<%p> mClientWidget=<%p>\n", this, mWidget, mClientWidget));
    if (mClientWidget)
	  return (void *) mClientWidget;
	else
	  return (void *) mWidget;
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetColorMap - Not Implemented.\n"));
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
// This routine is extra-complicated because Photon does not clip PhBlit
// calls correctly. Mozilla expects blits (and other draw commands) to be
// clipped around sibling widgets (and child widgets in some cases). Photon
// does not do this. So most of the grunge below achieves this "clipping"
// manually by breaking the scrollable rect down into smaller, unobscured
// rects that can be safely blitted. To make it worse, the invalidation rects
// must be manually calulated...
//
//   Ye have been warn'd -- enter at yer own risk.
//
// DVS
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll aDx=<%d aDy=<%d> aClipRect=<%p>.\n", aDx, aDy, aClipRect));

  PtWidget_t *widget;

#if 1
  if (mClientWidget)
    widget = mClientWidget;
  else
    widget = mWidget;
#else
  if( IsChild() )
    widget = mWidget;
  else
    widget = mClientWidget;
#endif

  if( !aDx && !aDy )
    return NS_OK;

  if( widget )
  {
    PhRect_t    rect,clip;
    PhPoint_t   offset = { aDx, aDy };
    PhArea_t    area;
    PhRid_t     rid = PtWidgetRid( widget );
    PhTile_t    *clipped_tiles=nsnull, *sib_tiles=nsnull, *tile=nsnull;
    PhTile_t    *offset_tiles=nsnull, *intersection = nsnull;

//    int flux = PtStartFlux( widget ) - 1;

    // Manually move all the child-widgets

    PtWidget_t *w;
    PtArg_t    arg;
    PhPoint_t  *pos;
    PhPoint_t  p;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll  Moving children...\n" ));
    for( w=PtWidgetChildFront( widget ); w; w=PtWidgetBrotherBehind( w )) 
    { 
      PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
      PtGetResources( w, 1, &arg ) ;
      p = *pos;
      p.x += aDx;
      p.y += aDy;
      PtSetArg( &arg, Pt_ARG_POS, &p, 0 );
      PtSetResources( w, 1, &arg ) ;
    } 

//    PtEndFlux( widget );

//    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  flux count is now %i\n", flux ));

    // Take our nice, clean client-rect and shatter it into lots (maybe) of
    // unobscured tiles. sib_tiles represents the rects occupied by siblings
    // in front of our window - but its not needed here.

    if( GetSiblingClippedRegion( &clipped_tiles, &sib_tiles ) == NS_OK )
    {
      // Now we need to calc the actual blit tiles. We do this by making a copy
      // of the client-rect tiles (clipped_tiles) and offseting them by (-aDx,-aDy)
      // then intersecting them with the original clipped_tiles. These new tiles (there
      // may be none) can be safely blitted to the new location (+aDx,+aDy).

      offset_tiles = PhCopyTiles( clipped_tiles );
      offset.x = -aDx;
      offset.y = -aDy;
      PhTranslateTiles( offset_tiles, &offset );
      tile = PhCopyTiles( offset_tiles ); // Just a temp copy for next cmd
      if (( tile = PhClipTilings( tile, clipped_tiles, &intersection ) ) != NULL )
      {
         PhFreeTiles( tile );
      }

      // Apply passed-in clipping, if available
      // REVISIT - this wont work, PhBlits ignore clipping

      if( aClipRect )
      {
        clip.ul.x = aClipRect->x;
        clip.ul.y = aClipRect->y;
        clip.lr.x = clip.ul.x + aClipRect->width - 1;
        clip.lr.y = clip.ul.y + aClipRect->height - 1;
        PgSetUserClip( &clip );
      }

      // Make sure video buffer is up-to-date
      PgFlush();

      offset.x = aDx;
      offset.y = aDy;

      // Blit individual tiles
      tile = intersection;
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll  Bliting tiles...\n" ));
      while( tile )
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll tile (%i,%i,%i,%i)\n", tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y ));
        PhBlit( rid, &tile->rect, &offset );
        tile = tile->next;
      }

      PhFreeTiles( offset_tiles );

      if( aClipRect )
        PgSetUserClip( nsnull );

      // Now we must invalidate all of the exposed areas. This is similar to the
      // first processes: Make a copy of the clipped_tiles, offset by (+aDx,+aDy)
      // then clip (not intersect) these from the original clipped_tiles. This
      // results in the invalidated tile list.

      offset_tiles = PhCopyTiles( clipped_tiles );
      PhTranslateTiles( offset_tiles, &offset );
      clipped_tiles = PhClipTilings( clipped_tiles, offset_tiles, nsnull );
      tile = clipped_tiles;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll  Damaging tiles...\n" ));
      while( tile )
      {
        PtDamageExtent( widget, &(tile->rect));
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll tile (%i,%i,%i,%i)\n", tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y ));
        tile = tile->next;
      }

      PhFreeTiles( offset_tiles );
      PhFreeTiles( clipped_tiles );
      PhFreeTiles( sib_tiles );
      PtFlush();   /* This is really needed! */
    }
    else
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll clipped out!\n" ));
    }
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll  widget is NULL!\n" ));
  }
  
  return NS_OK;
}


NS_METHOD nsWindow::SetTitle(const nsString& aTitle)
{
  nsresult res = NS_ERROR_FAILURE;
  char * title = aTitle.ToNewUTF8String();
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetTitle this=<%p> mWidget=<%p> title=<%s>\n", this, mWidget, title));

  if( mWidget )
  {
    PtArg_t  arg;

    PtSetArg( &arg, Pt_ARG_WINDOW_TITLE, title, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
    {
      res = NS_OK;
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetTitle - mWidget is NULL!\n"));

  if (title)
    nsCRT::free(title);

  if (res != NS_OK)
  {
	NS_ASSERTION(0,"nsWindow::SetTitle Error Setting page Title\n");
  }
  	
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
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::EndResizingChildren.\n"));
  return NS_OK;
}



PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, (" nsWindow::OnKey - mEventCallback=<%p>\n", mEventCallback));
    return DispatchWindowEvent(&aEvent);
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, (" nsWindow::OnKey - mEventCallback=<%p> Discarding Event!\n", mEventCallback));
    printf("nsWindow::OnKey Discarding Event, no mEventCallback\n");  
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


NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  PtArg_t  arg;
  PhDim_t  dim = { aWidth, aHeight };

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize this=<%p> w/h=(%i,%i) Repaint=<%i> mWindowType=<%d>\n", this, aWidth, aHeight, aRepaint, mWindowType ));

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if( mWidget )
  {
#if 0
    /* This breaks the dialog size for Viewer */
    if (mWindowType == eWindowType_dialog)
    {
      dim.w -= mFrameLeft + mFrameRight;
      dim.h -= mFrameTop + mFrameBottom;
    }
#endif

    EnableDamage( mWidget, PR_FALSE );

    PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
    PtSetResources( mWidget, 1, &arg );

	/* This fixes XUL dialogs */
    if (mClientWidget)
	{
      PtWidget_t *theClientChild = PtWidgetChildBack(mClientWidget);
	  if (theClientChild)
	  {
        nsWindow * pWin = (nsWindow*) GetInstance( theClientChild );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize  Resizing mClientWidget->Child this=<%p> mClientWidget=<%p> ChildBack=<%p>\n", this, mClientWidget, pWin));
	    pWin->Resize(aWidth, aHeight, aRepaint);
      }
	}
	
    EnableDamage( mWidget, PR_TRUE );

    Invalidate( aRepaint );

    if (aRepaint)
    {
      // REVISIT - Do nothing, resize handler will cause a redraw

      // Hack - Added this to see if it helps
//      Invalidate( aRepaint );
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize - mWidget is NULL!\n" ));

  return NS_OK;
}

NS_METHOD nsWindow::SetMenuBar( nsIMenuBar * aMenuBar )
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow(%p)::SetMenuBar(%p)\n", this, aMenuBar ));

  if  ( (mWindowType == eWindowType_toplevel) ||
        (mWindowType == eWindowType_dialog) )
  {
    mMenuBar = aMenuBar;
    res = NS_OK;
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetMenuBar - ERROR! Trying to set a menu for a ChildWindow!\n"));
  }

  return res;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


int nsWindow::WindowCloseHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WindowCloseHandler this=(%p)\n", data));

  nsWindow * pWin = (nsWindow*) data;

/* this isn't complete yet! :) */

#if 1
  /* I had to add this check for to not do this for pop-up but I wonder */
  /* is this is valid at all!   I really don't think so from looking some more... */  
  /* 11/15/99 */
  if ( pWin->mWindowType != eWindowType_popup )
  {
    if ( pWin )
	{
      NS_ADDREF(pWin);
      pWin->SetIsDestroying ( PR_TRUE );
      pWin->Destroy();
      NS_RELEASE(pWin);
    }
  }
#endif
  
  return Pt_CONTINUE;
}


NS_METHOD nsWindow::ShowMenuBar( PRBool aShow)
{
 nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ShowMenuBar  aShow=<%d>\n", aShow));

  mMenuBarVis = aShow;

  if( mWidget && mClientWidget && mMenuBar)
  {
    PtArg_t    arg[2];
    PhPoint_t  client_pos = { 0, 0 };
    PhDim_t    client_dim;
    PhDim_t    win_dim;
    PtWidget_t *menubar;

    mMenuBar->GetNativeData( menubar );

    if( mMenuBarVis )
    {
      client_pos.y = GetMenuBarHeight();
      PtRealizeWidget( menubar );
    }
    else
    {
      PtUnrealizeWidget( menubar );
    }

    win_dim.w = mBounds.width - mFrameLeft - mFrameRight;
    win_dim.h = mBounds.height - mFrameTop - mFrameBottom;

    PtSetArg( &arg[0], Pt_ARG_DIM, &win_dim, 0 );
    if( PtSetResources( mWidget, 1, arg ) == 0 )
    {
      client_dim.w = mBounds.width - mFrameLeft - mFrameRight;
      client_dim.h = mBounds.height - mFrameTop - mFrameBottom - client_pos.y;

      PtSetArg( &arg[0], Pt_ARG_POS, &client_pos, 0 );
      PtSetArg( &arg[1], Pt_ARG_DIM, &client_dim, 0 );
      PtSetResources( mClientWidget, 2, arg );
    }
 
    res = NS_OK;
  }

  return res;
}

	
//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::HandleEvent( PtCallbackInfo_t* aCbInfo )
{
  PRBool     result = PR_FALSE; // call the default nsWindow proc
  PhEvent_t* event = aCbInfo->event;

    switch ( event->type )
    {
	default:
	  result = nsWidget::HandleEvent(aCbInfo);
	  break;
	}
	
	return result;
}


void nsWindow::RawDrawFunc( PtWidget_t * pWidget, PhTile_t * damage )
{
  nsWindow * pWin = (nsWindow*) GetInstance( pWidget );
  nsresult   result;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc for mWidget=<%p> this=<%p> this->mContext=<%p> pWin->mParent=<%p>\n", pWidget, pWin, pWin->mContext, pWin->mParent));

#if 1
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = damage;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Damage Tiles List:\n"));
  do {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  } while (top);
}
#endif
  
  if ( !pWin )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  aborted because instance is NULL!\n"));
    NS_ASSERTION(pWin, "nsWindow::RawDrawFunc  aborted because instance is NULL!");
    return;
  }

  if ( !pWin->mContext )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  aborted because pWin->mContext is NULL!\n"));
    NS_ASSERTION(pWin->mContext, "nsWindow::RawDrawFunc  aborted because pWin->mContext is NULL!");
    return;
  }

#if 1
  if (PtWidgetBrotherInFront(pWidget) != NULL)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Brother In Front is not NULL\n"));
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Brother In Front is NULL\n"));
  }
#endif

#if 1
    if ( PtWidgetIsClass(pWidget, PtRawDrawContainer))
	{
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc pWidget is a RawDrawContainer\n"));
	}
    else
	{
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc aborted because pWidget is not a RawDrawContainer\n"));
	  return;  
    }
#endif


  
#if 1
  // This prevents redraws while any window is resizing, ie there are
  //   windows in the resize queue
  if ( pWin->mIsResizing )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  aborted due to hold-off!\n"));
    //printf("nsWindow::RawDrawFunc aborted due to Resize holdoff\n");
    return;
  }
#endif

  if ( pWin->mEventCallback )
  {
    PhRect_t   rect;
    PhArea_t   area = {{0,0},{0,0}};
    PhPoint_t  offset = {0,0};
    nsRect     nsDmg;

    // Ok...  I ~think~ the damage rect is in window coordinates and is not neccessarily clipped to
    // the widgets canvas. Mozilla wants the paint coords relative to the parent widget, not the window.

    PtWidgetArea( pWidget, &area );
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc area=<%d,%d,%d,%d>\n", area.pos.x, area.pos.y, area.size.w, area.size.h));
    PtWidgetOffset( pWidget, &offset );
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc offset=<%d,%d>\n", offset.x, offset.y));
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc mBounds=<%d,%d,%d,%d>\n", pWin->mBounds.x, pWin->mBounds.y, pWin->mBounds.width, pWin->mBounds.height));

    offset.x += area.pos.x;  
    offset.y += area.pos.y;  
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc offset+area =<%d,%d>\n", offset.x, offset.y));


#if 0
    /* Use the first photon damage tile which is supposed to be a bounding box */
    rect = damage->rect;
#else
    /* Create our own bounding box of all the photon damage */
    PhTile_t *top = damage->next;
    rect = top->rect;	
    top=top->next;
    while (top)
	{
      PhRect_t tmp_rect = top->rect;
	  rect.ul.x = PR_MIN(rect.ul.x, tmp_rect.ul.x);
	  rect.ul.y = PR_MIN(rect.ul.y, tmp_rect.ul.y);
	  rect.lr.x = PR_MAX(rect.lr.x, tmp_rect.lr.x);
	  rect.lr.y = PR_MAX(rect.lr.y, tmp_rect.lr.y);
      top=top->next;
	}
#endif

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage rect = <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y));

    // Convert damage rect to widget's coordinates...
    rect.ul.x -= offset.x;
    rect.ul.y -= offset.y;
    rect.lr.x -= offset.x;
    rect.lr.y -= offset.y;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage rect-offset <%d,%d,%d,%d> next=<%p>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, damage->next));

    // If the damage tile is not within our bounds, do nothing
    if(( rect.ul.x >= area.size.w ) || ( rect.ul.y >= area.size.h ) || ( rect.lr.x < 0 ) || ( rect.lr.y < 0 ))
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage tile is not within our bounds, do nothing\n"));
      return;
	}

    // clip damage to widgets bounds...
    if( rect.ul.x < 0 ) rect.ul.x = 0;
    if( rect.ul.y < 0 ) rect.ul.y = 0;
    if( rect.lr.x >= area.size.w ) rect.lr.x = area.size.w - 1;
    if( rect.lr.y >= area.size.h ) rect.lr.y = area.size.h - 1;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc clipped damage <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y));

    nsDmg.x = rect.ul.x;
    nsDmg.y = rect.ul.y;
    nsDmg.width = rect.lr.x - rect.ul.x + 1;
    nsDmg.height = rect.lr.y - rect.ul.y + 1;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc nsDmg <%d,%d,%d,%d>\n", nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height));

    if ( (nsDmg.width <= 0 ) || (nsDmg.height <= 0 ) )
    {
      return;
    }

    nsPaintEvent pev;
    pWin->InitEvent(pev, NS_PAINT);

    pev.rect = &nsDmg;
    pev.eventStructType = NS_PAINT_EVENT;

	pev.point.x = nsDmg.x;
	pev.point.y = nsDmg.y;

    PRInt32 x,y,w,h;
	pWin->mUpdateArea->GetBoundingBox(&x,&y,&w,&h);

    pev.rect = new nsRect(nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height);
	
    // call the event callback
    if (pWin->mEventCallback) 
    {
      PRBool aClipState;
      pev.renderingContext = nsnull;
      pev.renderingContext = pWin->GetRenderingContext();

      if (pev.renderingContext)
      {
        pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(pWin->mUpdateArea)),
                                            nsClipCombine_kReplace, aClipState);

#if 1
   /* Not doing this causes many extra rips */
        if( pWin->SetWindowClipping( damage, offset ) == NS_OK )
        {
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ( "nsWindow::RawDrawFunc Dispatching paint event (area=%ld,%ld,%ld,%ld).\n",nsDmg.x,nsDmg.y,nsDmg.width,nsDmg.height ));
          result = pWin->DispatchWindowEvent(&pev);
        }
 	    else
	    {
           printf("nsWindow::RawDrawFunc SetWindowClipping resulted in nothing to draw\n");
           PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc SetWindowClipping resulted in nothing to draw this=<%p>\n", pWin));
	    }
#else
        PhRect_t r = {nsDmg.x, nsDmg.y, nsDmg.x+nsDmg.width-1,nsDmg.y+nsDmg.height-1};
        PgSetClipping( 1, &r );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Dispatching paint event (area=%ld,%ld,%ld,%ld).\n",nsDmg.x,nsDmg.y,nsDmg.width,nsDmg.height ));
        result = pWin->DispatchWindowEvent(&pev);
#endif	  
	    NS_RELEASE(pev.renderingContext);
      }
    }
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  aborted due to no event callback!\n"));
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  End of RawDrawFunc\n"));
}


int nsWindow::GetMenuBarHeight()
{
  int h = 0;

  if(( mMenuBar != nsnull ) && ( mMenuBarVis == PR_TRUE ))
  {
    void * menubar;

    mMenuBar->GetNativeData( menubar );

    if( menubar )
    {
      PtArg_t arg[2];
      PhDim_t *mb_dim;
      int     *mb_border;

      PtSetArg( &arg[0], Pt_ARG_DIM, &mb_dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, &mb_border, 0 );
      if( PtGetResources(( PtWidget_t* ) menubar, 2, arg ) == 0 )
      {
        h = mb_dim->h + 2*(*mb_border);
      }
    }
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetMenuBarHeight = <%d>\n", h));

  return h;
}


void nsWindow::ScreenToWidget( PhPoint_t &pt )
{
  // pt is in screen coordinates
  // convert it to be relative to ~this~ widgets origin
  short x=0,y=0;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ScreenToWidget 1 pt=(%d,%d)\n", pt.x, pt.y));

  if( mWindowType == eWindowType_child )
  {
    PtGetAbsPosition( mWidget, &x, &y );
  }
  else
  {
    PtGetAbsPosition( mClientWidget, &x, &y );
  }

  pt.x -= x;
  pt.y -= y;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ScreenToWidget 2 pt=(%d,%d)\n", pt.x, pt.y));
}


NS_METHOD nsWindow::GetFrameSize(int *FrameLeft, int *FrameRight, int *FrameTop, int *FrameBottom) const
{

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetFrameSize \n"));

  if (FrameLeft)
    *FrameLeft = mFrameLeft;
  if (FrameRight)
    *FrameRight = mFrameRight;
  if (FrameTop)
    *FrameTop = mFrameTop;
  if (FrameBottom)
    *FrameBottom = mFrameBottom;


  return NS_OK;
}



NS_METHOD nsWindow::GetSiblingClippedRegion( PhTile_t **btiles, PhTile_t **ctiles )
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion 0\n"));

  if(( btiles ) && ( ctiles ))
  {
    *btiles = PhGetTile();
    if( *btiles )
    {
      PhTile_t   *tile, *last;
      PtWidget_t *w;
      PhArea_t   *area;
      PtArg_t    arg;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion 1\n"));

      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      if( PtGetResources( mWidget, 1, &arg ) == 0 )
      {
        nsRect rect( area->pos.x, area->pos.x, area->size.w, area->size.h );
        GetParentClippedArea( rect );

        (*btiles)->rect.ul.x = rect.x;
        (*btiles)->rect.ul.y = rect.y;
        (*btiles)->rect.lr.x = rect.x + rect.width - 1;
        (*btiles)->rect.lr.y = rect.y + rect.height - 1;

        (*btiles)->next = nsnull;

        *ctiles = last = nsnull;

        for( w=PtWidgetBrotherInFront( mWidget ); w; w=PtWidgetBrotherInFront( w )) 
        { 
          PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
          PtGetResources( w, 1, &arg );
          tile = PhGetTile();
          if( tile )
          {
            tile->rect.ul.x = area->pos.x;
            tile->rect.ul.y = area->pos.y;
            tile->rect.lr.x = area->pos.x + area->size.w - 1;
            tile->rect.lr.y = area->pos.y + area->size.h - 1;
            tile->next = NULL;
            if( !*ctiles )
              *ctiles = tile;
            if( last )
              last->next = tile;
            last = tile;
          }
        }

        if( *ctiles )
        {
          // We have siblings... now clip'em
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion 5\n"));

          *btiles = PhClipTilings( *btiles, *ctiles, nsnull );
        }
        res = NS_OK;
      }
    }
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion error failure \n"));

  return res;
}


NS_METHOD nsWindow::SetWindowClipping( PhTile_t *damage, PhPoint_t &offset )
{
  nsresult res = NS_ERROR_FAILURE;

  PhTile_t   *tile, *last, *clip_tiles;
  PtWidget_t *w;
  PhArea_t   *area;
  PtArg_t    arg;
  PtWidget_t *aWidget = GetNativeData(NS_NATIVE_WIDGET);
  
  clip_tiles = last = nsnull;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  this=<%p> damage=<%p> offset=(%d,%d) mClipChildren=<%d> mClipSiblings=<%d>\n", this, damage, offset.x, offset.y, mClipChildren, mClipSiblings));

  if ( mClipChildren )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clipping children...\n"));

    for( w=PtWidgetChildFront( aWidget ); w; w=PtWidgetBrotherBehind( w )) 
    { 
      if( PtWidgetIsRealized( w ))
      {
        PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
        PtGetResources( w, 1, &arg );

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  clipping children w=<%p> area=<%d,%d,%d,%d>\n", w, area->pos.x, area->pos.y, area->size.w, area->size.h));

        tile = PhGetTile();
        if( tile )
        {
          tile->rect.ul.x = area->pos.x;
          tile->rect.ul.y = area->pos.y;
          tile->rect.lr.x = area->pos.x + area->size.w - 1;
          tile->rect.lr.y = area->pos.y + area->size.h - 1;
          tile->next = NULL;
          if( !clip_tiles )
            clip_tiles = tile;
          if( last )
            last->next = tile;
          last = tile;
        }
      }
	  else
      {
	      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping Widget isn't realized %p\n", w));
      }
    }
  }

  if ( mClipSiblings )
  {
    PtWidget_t *node = aWidget;
    PhPoint_t  origin = { 0, 0 };

   PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clipping siblings...\n"));
    
    while( node )
    {
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( node, 1, &arg );
      origin.x += area->pos.x;
      origin.y += area->pos.y;
      //printf ("parent: %p: %d %d %d %d\n",node,area->pos.x,area->pos.y,area->size.w,area->size.h);

      for( w=PtWidgetBrotherInFront( node ); w; w=PtWidgetBrotherInFront( w ))
      {
        if( PtWidgetIsRealized( w ))
        {
          PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
          PtGetResources( w, 1, &arg );
	  //printf ("sib: %p: %d %d %d %d\n",w,area->pos.x,area->pos.y,area->size.w,area->size.h);
          tile = PhGetTile();
	  if (area->size.w != 43 && area->size.h != 43)  // oh god another HACK
          if( tile )
          {
            tile->rect.ul.x = area->pos.x - origin.x;
            tile->rect.ul.y = area->pos.y - origin.y;
            tile->rect.lr.x = tile->rect.ul.x + area->size.w - 1;
            tile->rect.lr.y = tile->rect.ul.y + area->size.h - 1;
            tile->next = NULL;
            if( !clip_tiles )
              clip_tiles = tile;
            if( last )
              last->next = tile;
            last = tile;
          }
        }
      }
      node = PtWidgetParent( node );
    }
  }

  int rect_count;
  PhRect_t *rects;
  PhTile_t *dmg;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping damage=<%p> damage->next=<%p>  clip_tiles=<%p>\n", damage, damage->next,  clip_tiles));

  if( damage->next )
    dmg = PhCopyTiles( damage->next );
  else
    dmg = PhCopyTiles( damage );

  PhDeTranslateTiles( dmg, &offset );

  if( clip_tiles )
  {
    // We have chiluns... now clip'em
    dmg = PhClipTilings( dmg, clip_tiles, nsnull );

    PhFreeTiles( clip_tiles );
  }  

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping after children clipping dmg=<%p>\n", dmg ));

  if( dmg )
  {
    rects = PhTilesToRects( dmg, &rect_count );
#if 1
    PgSetClipping( rect_count, rects );
#else
    //PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  damage clipped to:\n"));
    //printf("  damage clipped to:\n");


    int bX=0, bY=0;
    int aX=32767, aY=32767;

    for(int i=0;i<rect_count;i++)
	{
	  //PR_LOG(PhWidLog, PR_LOG_DEBUG, ("    (%i,%i,%i,%i)\n", rects[i].ul.x, rects[i].ul.y, rects[i].lr.x, rects[i].lr.y));
      //printf("    (%i,%i,%i,%i)\n", rects[i].ul.x, rects[i].ul.y, rects[i].lr.x, rects[i].lr.y);

      aX = min(aX, rects[i].ul.x);
      aY = min(aY, rects[i].ul.y);
      bX = max(bX, rects[i].lr.x);
      bY = max(bY, rects[i].lr.y);	 
	}

   PhRect_t r = {aX, aY, bX, bY}; 
   PgSetClipping( 1, &r );

#endif
    free( rects );
    PhFreeTiles( dmg );
    res = NS_OK;
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  no valid damage.\n"));
  }

  return res;
}


int nsWindow::ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  PhRect_t *extents = (PhRect_t *)cbinfo->cbdata; 
  nsWindow *someWindow = (nsWindow *) GetInstance(widget);
  nsRect rect;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHandler for someWindow=<%p>\n", someWindow));

  if( someWindow )
  {
    rect.x = extents->ul.x;
    rect.y = extents->ul.y;
    rect.width = extents->lr.x - rect.x + 1;
    rect.height = extents->lr.y - rect.y + 1;

    /* This enables the resize holdoff */
    someWindow->ResizeHoldOff();  /* commenting this out sometimes makes pref. dlg draw */

  	someWindow->OnResize( rect );
  }
	return( Pt_CONTINUE );
}

void nsWindow::ResizeHoldOff()
{
  if( !mWidget )
  {
    return;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHoldOff Entering this=<%p>\n", this ));

  if( PR_FALSE == mResizeQueueInited )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHoldOff Initing Queue this=<%p>\n", this ));

    // This is to guarantee that the Invalidation work-proc is in place prior to the
    // Resize work-proc.
    if( !mDmgQueueInited )
    {
      Invalidate( PR_FALSE );
    }

    PtWidget_t *top = PtFindDisjoint( mWidget );

    if ( (mResizeProcID = PtAppAddWorkProc( nsnull, ResizeWorkProc, top )) != nsnull )
    {
#if 1
      int Global_Widget_Hold_Count;
        Global_Widget_Hold_Count =  PtHold();
        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWindow::ResizeHoldOff PtHold Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, this));
#endif
        mResizeQueueInited = PR_TRUE;
    }
    else
    {
      printf( "*********** resize work proc failed to init. ***********\n" );
    }
  }

  if( PR_TRUE == mResizeQueueInited )
  {
    DamageQueueEntry *dqe;
    PRBool           found = PR_FALSE;
  
    dqe = mResizeQueue;

    while( dqe )
    {
      if( dqe->widget == mWidget )
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHoldOff Widget already in Queue this=<%p>\n", this ));

        found = PR_TRUE;
        break;
      }
      dqe = dqe->next;
    }

    if( !found )
    {
      dqe = new DamageQueueEntry;
      if( dqe )
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHoldOff Adding widget to Queue this=<%p> dqe=<%p> dqe->inst=<%p>\n", this, dqe, this ));

        mIsResizing = PR_TRUE;
        dqe->widget = mWidget;
        dqe->inst = this;
        dqe->next = mResizeQueue;
        mResizeQueue = dqe;
      }
    }
  }
}


void nsWindow::RemoveResizeWidget()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWindow::RemoveResizeWidget (%p)\n", this));

  if( mIsResizing )
  {
    DamageQueueEntry *dqe;
    DamageQueueEntry *last_dqe = nsnull;
  
    dqe = mResizeQueue;

    // If this widget is in the queue, remove it
    while( dqe )
    {
      if( dqe->widget == mWidget )
      {
        if( last_dqe )
          last_dqe->next = dqe->next;
        else
          mResizeQueue = dqe->next;

//        NS_RELEASE( dqe->inst );

        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWindow::RemoveResizeWidget this=(%p) dqe=<%p>\n", this, dqe));

        delete dqe;
        mIsResizing = PR_FALSE;
        break;
      }
      last_dqe = dqe;
      dqe = dqe->next;
    }

    if( nsnull == mResizeQueue )
    {
      mResizeQueueInited = PR_FALSE;
      PtWidget_t *top = PtFindDisjoint( mWidget );

#if 1
      int Global_Widget_Hold_Count;
      Global_Widget_Hold_Count =  PtRelease();
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWindow::RemoveResizeWidget PtHold/PtRelease Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, this));
#endif

      if( mResizeProcID )
        PtAppRemoveWorkProc( nsnull, mResizeProcID );
    }
  }
}


static int count=100;
int nsWindow::ResizeWorkProc( void *data )
{

  /* Awful HACK that needs to be removed.. */
//  if (count) {count--; return Pt_CONTINUE;}
//  count=100;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeWorkProc data=<%p> mResizeQueueInited=<%d>\n", data, mResizeQueueInited ));
  
  if ( mResizeQueueInited )
  {
    DamageQueueEntry *dqe = nsWindow::mResizeQueue;
    DamageQueueEntry *last_dqe;

    while( dqe )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeWorkProc before Invalidate dqe=<%p> dqe->inst=<%p>\n", dqe, dqe->inst));

      ((nsWindow*)dqe->inst)->mIsResizing = PR_FALSE;
      dqe->inst->Invalidate( PR_FALSE );
      last_dqe = dqe;
      dqe = dqe->next;
      delete last_dqe;
    }

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeWorkProc after while loop\n"));

    nsWindow::mResizeQueue = nsnull;
    nsWindow::mResizeQueueInited = PR_FALSE;

#if 1
    int Global_Widget_Hold_Count;
      Global_Widget_Hold_Count =  PtRelease();
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWindow::ResizeWorkProc PtHold/PtRelease Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, NULL));
#endif

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeWorkProc after PtRelease\n"));

  }

  return Pt_END;
}


NS_METHOD nsWindow::GetClientBounds( nsRect &aRect )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetClientBounds (%p) aRect=(%d,%d,%d,%d)\n", this, aRect.x, aRect.y, aRect.width, aRect.height));

  aRect.x = 0;
  aRect.y = 0;
  aRect.width = mBounds.width;
  aRect.height = mBounds.height;

/* kirkj took this out to fix sizing errors on pref. dialog. */
#if 0
  if  ( (mWindowType == eWindowType_toplevel) ||
        (mWindowType == eWindowType_dialog) )
  {
    int  h = GetMenuBarHeight();

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetClientBounds h=<%d> mFrameRight=<%d> mFrameLeft=<%d> mFrameTop=<%d> mFrameBottom=<%d>\n",
      h, mFrameRight, mFrameLeft, mFrameTop, mFrameBottom));


    aRect.width -= (mFrameRight + mFrameLeft);
    aRect.height -= (h + mFrameTop + mFrameBottom);
  }
#endif

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  bounds = %ld,%ld,%ld,%ld\n", aRect.x, aRect.y, aRect.width, aRect.height ));

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move this=(%p) to (%ld,%ld) mClipChildren=<%d> mClipSiblings=<%d> mBorderStyle=<%d> mWindowType=<%d> mParent=<%p>\n", 
    this, aX, aY, mClipChildren, mClipSiblings, mBorderStyle, mWindowType, mParent));

  if (mWindowType==eWindowType_popup)
  {
    PtWidget_t *top, *last = mWidget;
      while (top=PtWidgetParent(last))
      {
        last = top;  
      }
  
    nsWindow *myWindowParent = (nsWindow *) GetInstance(last);
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move last=<%p> myWindowParent=<%p>\n", last, myWindowParent));

#if 0
    PhPoint_t p;
	if ( PtWidgetOffset(mWidget,&p) )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move mWidget=<%p> WidgetOffset=<%d,%d)\n",mWidget,  p.x,p.y));
    }
	
    top=PtWidgetParent(mWidget);
	if (top)
	{
  	  if ( PtWidgetOffset(top,&p) )
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move mWidget parent=<%p> WidgetOffset=<%d,%d)\n",top,  p.x,p.y));
  	}	

    top=PtWidgetParent(top);
	if (top)
	{
  	  if ( PtWidgetOffset(top,&p) )
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move mWidget parent parent=<%p> WidgetOffset=<%d,%d)\n",top,  p.x,p.y));
  	}	

    if (mParent)
	{
	  nsRect r;

	  mParent->GetBounds(r);
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move mParent=<%p> GetBounds rect=<%d,%d,%d,%d>\n", mParent, r.x,r.y,r.width,r.height));

	  mParent->GetClientBounds(r);
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move mParent GetClientBounds rect=<%d,%d,%d,%d>\n", r.x,r.y,r.width,r.height));
	}
#endif	
 
      if (myWindowParent)
      {
        int FrameLeft, FrameTop, FrameBottom;
        PhArea_t   area;
          myWindowParent->GetFrameSize(&FrameLeft, NULL, &FrameTop, &FrameBottom);
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move parent frame size left=<%d> top=<%d>\n", FrameLeft, FrameTop));
          PtWidgetArea(last, &area);
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move parent area=<%d,%d,%d,%d>\n", area.pos.x,area.pos.y,area.size.w,area.size.h));
		  
          aX = aX + FrameLeft + area.pos.x;
		  /* Subtract FrameBottom for Augusta/Photon 2 */
          aY = aY + FrameTop + area.pos.y - FrameBottom;	
      }
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move this=(%p) to (%ld,%ld) \n", this, aX, aY ));

  /* Call my base class */
  nsresult res = nsWidget::Move(aX, aY);

  /* If I am a top-level window my origin should always be 0,0 */
  if ( (mWindowType == eWindowType_dialog) ||
//       (mWindowType == eWindowType_popup) ||
	   (mWindowType == eWindowType_toplevel) )
  {
    //printf("HACK HACK: forcing bounds to 0,0 for toplevel window\n");
    mBounds.x = mBounds.y = 0;
  }

  return res;
}

//-------------------------------------------------------------------------
//
// Determine whether an event should be processed, assuming we're
// a modal window.
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                     PRBool *aForWindow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ModalEventFilter aEvent=<%p> - Not Implemented.\n", aEvent));

#if 1
    *aForWindow = PR_FALSE;
    return NS_OK;
#else
  PRBool isInWindow, isMouseEvent;
  PhEvent_t *msg = (PhEvent_t *) aEvent;

  if (aRealEvent == PR_FALSE)
  {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }

  isInWindow = PR_FALSE;

  // Get native window
  PtWidget_t *win;
  win = (PtWidget_t *)GetNativeData(NS_NATIVE_WINDOW);
  PtWidget_t *eWin = (PtWidget_t *) msg->collector.handle;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ModalEventFilter win=<%p> eWin=<%p>\n",win, eWin));

  if (nsnull != eWin) {
    if (win == eWin) {
      isInWindow = PR_TRUE;
    }
  }
  
  switch(msg->type)
  {
    case Ph_EV_BUT_PRESS:
    case Ph_EV_BUT_RELEASE:
    case Ph_EV_BUT_REPEAT:
    case Ph_EV_PTR_MOTION_BUTTON:
    case Ph_EV_PTR_MOTION_NOBUTTON:
       isMouseEvent = PR_TRUE;
       break;
    default:
       isMouseEvent = PR_FALSE;
       break;
  }

  *aForWindow = isInWindow == PR_TRUE || isMouseEvent == PR_FALSE ? PR_TRUE : PR_FALSE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ModalEventFilter isInWindow=<%d> isMouseEvent=<%d> aForWindow=<%d>\n", isInWindow, isMouseEvent, *aForWindow));
  return NS_OK;
#endif
}
