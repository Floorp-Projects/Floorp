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


//PRBool  nsWindow::mIsResizing = PR_FALSE;
PRBool  nsWindow::mResizeQueueInited = PR_FALSE;
DamageQueueEntry *nsWindow::mResizeQueue = nsnull;
PtWorkProcId_t *nsWindow::mResizeProcID = nsnull;

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::nsWindow (%p)\n", this ));
  NS_INIT_REFCNT();

  mClientWidget    = nsnull;
  mFontMetrics     = nsnull;
  mClipChildren    = PR_FALSE;
  mClipSiblings    = PR_FALSE;
  mBorderStyle     = eBorderStyle_all;
  mWindowType      = eWindowType_toplevel;
  mIsResizing      = PR_FALSE;
  mFont            = nsnull;
  mMenuBar         = nsnull;
  mMenuBarVis      = PR_FALSE;
  mFrameLeft       = 0;
  mFrameRight      = 0;
  mFrameTop        = 0;
  mFrameBottom     = 0;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  border=%X, window=%X\n", mBorderStyle, mWindowType ));
}

ChildWindow::ChildWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("ChildWindow::ChildWindow (%p)\n", this ));
  mBorderStyle     = eBorderStyle_none;
  mWindowType      = eWindowType_child;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  border=%X, window=%X\n", mBorderStyle, mWindowType ));
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::~nsWindow (%p) - Not Implemented.\n", this ));

  if( mWidget )
  {
    RemoveResizeWidget();
    RemoveDamagedWidget( mWidget );
  }

  mIsDestroying = PR_TRUE;
}

NS_METHOD nsWindow::Destroy(void)
{
  RemoveResizeWidget();
  nsWidget::Destroy();
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

  mClipChildren = aInitData->clipChildren;
  mClipSiblings = aInitData->clipSiblings;

  SetWindowType( aInitData->mWindowType );
  SetBorderStyle( aInitData->mBorderStyle );

//  mBorderStyle = aInitData->mBorderStyle;
//  mWindowType = aInitData->mWindowType;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PreCreateWidget mClipChildren=<%d> mClipSiblings=<%d> mBorderStyle=<%d> mWindowType=<%d>\n",
    mClipChildren, mClipSiblings, mBorderStyle, mWindowType));

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

  switch( mWindowType )
  {
  case eWindowType_toplevel :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  window type = toplevel\n" ));
    break;
  case eWindowType_dialog :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  window type = dialog\n" ));
    break;
  case eWindowType_popup :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  window type = popup\n" ));
    break;
  case eWindowType_child :
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  window type = child\n" ));
    break;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  border style = %X\n", mBorderStyle ));

  if( IsChild() )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Child window class\n" ));

    if(( mWindowType == eWindowType_dialog ) || ( mWindowType == eWindowType_toplevel ))
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Trying to creata a child window w/ wrong window type.\n" ));
      return result;
    }

    PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[2], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[3], Pt_ARG_FLAGS, 0 /*Pt_HIGHLIGHTED*/, Pt_HIGHLIGHTED );
    PtSetArg( &arg[4], Pt_ARG_BORDER_WIDTH, 0, 0 );
    PtSetArg( &arg[5], Pt_ARG_TOP_BORDER_COLOR, Pg_RED, 0 );
    PtSetArg( &arg[6], Pt_ARG_BOT_BORDER_COLOR, Pg_RED, 0 );
    PtSetArg( &arg[7], RDC_DRAW_FUNC, RawDrawFunc, 0 );
//    PtStartFlux( parentWidget );
    mWidget = PtCreateWidget( PtRawDrawContainer, parentWidget, 8, arg );
//    PtEndFlux( parentWidget );
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Top-level window class\n" ));

    // No border or decorations is the default
    render_flags = 0;

    if( mWindowType == eWindowType_popup )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Creating a pop-up (no decorations).\n" ));
    }
    else if( mWindowType == eWindowType_child )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Trying to creata a normal window as a child.\n" ));
      return result;
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
        render_flags = PH_BORDER_STYLE_ALL;
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
    }

    PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[2], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[3], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, 0xFFFFFFFF );

    // Remember frame size for later use...
    PtFrameSize( render_flags, 0, &mFrameLeft, &mFrameTop, &mFrameRight, &mFrameBottom );

    if( parentWidget )
      mWidget = PtCreateWidget( PtWindow, parentWidget, 4, arg );
    else
    {
      PtSetParentWidget( nsnull );
      mWidget = PtCreateWidget( PtWindow, nsnull, 4, arg );
    }
    
    // Must also create the client-area widget
    if( mWidget )
    {
#if 0
      PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_ANCHOR_FLAGS, Pt_LEFT_ANCHORED_LEFT |
        Pt_RIGHT_ANCHORED_RIGHT | Pt_TOP_ANCHORED_TOP | Pt_BOTTOM_ANCHORED_BOTTOM, 0xFFFFFFFF );
      PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[3], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PtSetArg( &arg[4], Pt_ARG_FLAGS, 0, Pt_HIGHLIGHTED );
      PtSetArg( &arg[5], Pt_ARG_TOP_BORDER_COLOR, Pg_BLUE, 0 );
      PtSetArg( &arg[6], Pt_ARG_BOT_BORDER_COLOR, Pg_BLUE, 0 );
      PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_GREY, 0 );
//      PtSetArg( &arg[7], Pt_ARG_FILL_COLOR, Pg_BLUE, 0 );

      PhRect_t anch_offset = { 0, 0, 0, 0 };
      PtSetArg( &arg[8], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      //#define REGION_FIELDS  Ph_REGION_EV_SENSE | Ph_REGION_EV_OPAQUE

      //PtSetArg( &arg[9], Pt_ARG_REGION_FIELDS, Ph_REGION_EV_OPAQUE, Ph_REGION_EV_OPAQUE );
      //PtSetArg( &arg[10], Pt_ARG_REGION_FLAGS, Ph_EV_WIN_OPAQUE, Ph_EV_WIN_OPAQUE );
      //PtSetArg( &arg[10], Pt_ARG_REGION_SENSE, Ph_EV_WIN_OPAQUE, Ph_EV_WIN_OPAQUE );
      //PtSetArg( &arg[10], Pt_ARG_REGION_OPAQUE, Ph_EV_DRAW, Ph_EV_DRAW );

      mClientWidget = PtCreateWidget( PtContainer, mWidget, 9, arg );
//      mClientWidget = PtCreateWidget( PtRegion, mWidget, 9, arg );
//      mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, 4, arg );
#else
      PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
      PtSetArg( &arg[1], Pt_ARG_ANCHOR_FLAGS, Pt_LEFT_ANCHORED_LEFT |
        Pt_RIGHT_ANCHORED_RIGHT | Pt_TOP_ANCHORED_TOP | Pt_BOTTOM_ANCHORED_BOTTOM, 0xFFFFFFFF );
#if 0
      // Anchoring doesnt seem to work with the RawDdraw widget
      // - cant use it as a client area...

      PtSetArg( &arg[2], RDC_DRAW_FUNC, RawDrawFunc, 0 );

      mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, 3, arg );
#else
      PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 0 , 0 );
      PtSetArg( &arg[3], Pt_ARG_MARGIN_WIDTH, 0 , 0 );
      PtSetArg( &arg[4], Pt_ARG_FLAGS, 0, Pt_HIGHLIGHTED );
      PtSetArg( &arg[5], Pt_ARG_FILL_COLOR, Pg_GREY, 0 );
      PhRect_t anch_offset = { 0, 0, 0, 0 };
      PtSetArg( &arg[6], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      mClientWidget = PtCreateWidget( PtContainer, mWidget, 7, arg );
#endif
#endif
      // Create a region that is opaque to draw events and place behind
      // the client widget.
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

    if( mClientWidget )
      SetInstance( mClientWidget, this );

    mIsToplevel = PR_FALSE;

    if( IsChild())
    {
      PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
      PtAddEventHandler( mWidget,
        Ph_EV_KEY | Ph_EV_PTR_MOTION_BUTTON | Ph_EV_PTR_MOTION_NOBUTTON |
        Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY,
        RawEventHandler, this );
    }
    else if( !parentWidget )
    {
      mIsToplevel = PR_TRUE;
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

  if( IsChild() )
    widget = mWidget;
  else
    widget = mClientWidget;

  if( !aDx && !aDy )
    return NS_OK;

  if( widget )
  {
    PhRect_t    rect,clip;
    PhPoint_t   offset = { aDx, aDy };
    PhArea_t    area;
    PhRid_t     rid = PtWidgetRid( widget );
    PhTile_t    *clipped_tiles, *sib_tiles, *tile;
    PhTile_t    *offset_tiles, *intersection = nsnull;

//    int flux = PtStartFlux( widget ) - 1;

    // Manually move all the child-widgets

    PtWidget_t *w;
    PtArg_t    arg;
    PhPoint_t  *pos;
    PhPoint_t  p;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Moving children...\n" ));
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
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Blitng tiles...\n" ));
      while( tile )
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("    tile (%i,%i,%i,%i)\n", tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y ));
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

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  Damaging tiles...\n" ));
      while( tile )
      {
        PtDamageExtent( widget, &(tile->rect));
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("    tile (%i,%i,%i,%i)\n", tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y ));
        tile = tile->next;
      }

      PhFreeTiles( offset_tiles );
      PhFreeTiles( clipped_tiles );
      PhFreeTiles( sib_tiles );
      PtFlush();
    }
    else
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clipped out!\n" ));
    }
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  widget is NULL!\n" ));
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

//  PtStartFlux( mWidget );
//  mHold = PR_TRUE;

  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::EndResizingChildren.\n"));

//  PtEndFlux( mWidget );
//  mHold = PR_FALSE;

  return NS_OK;
}



PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback)
  {
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


NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  PtArg_t  arg;
  PhDim_t  dim = { aWidth, aHeight };

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow(%p)::Resize (%i,%i,%i)\n", this, aWidth, aHeight, aRepaint ));

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if( mWidget )
  {
    if( !IsChild() )
    {
      dim.w -= mFrameLeft + mFrameRight;
      dim.h -= mFrameTop + mFrameBottom;
    }

    EnableDamage( mWidget, PR_FALSE );

    PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
    PtSetResources( mWidget, 1, &arg );

    EnableDamage( mWidget, PR_TRUE );

    Invalidate( aRepaint );

    if (aRepaint)
    {
      // REVISIT - Do nothing, resize handler will cause a redraw
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize - mWidget is NULL!\n" ));

  return NS_OK;
}


#if 0
NS_METHOD nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                           PRInt32 aHeight, PRBool aRepaint)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize Combination Resize and Move\n"));

  Resize(aWidth,aHeight,aRepaint);
  Move(aX,aY);
  return NS_OK;
}
#endif


NS_METHOD nsWindow::SetMenuBar( nsIMenuBar * aMenuBar )
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow(%p)::SetMenuBar(%p)\n", this, aMenuBar ));

  if( !IsChild() )
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
	PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WindowCloseHandler (%p)\n", data));

  if( data )
    ((nsWindow *) data)->Destroy();

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
//        printf( "Window mouse click: (%ld,%ld)\n", ptrev->pos.x, ptrev->pos.y );
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
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  aborted because instance is NULL!\n"));
    return;
  }

#if 1
  if( /*pWin->mCreateHold || pWin->mHold ||*/ pWin->mIsResizing )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  aborted due to hold-off!\n"));
    return;
  }
#endif

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

    nsDmg.x = rect.ul.x;
    nsDmg.y = rect.ul.y;
    nsDmg.width = rect.lr.x - rect.ul.x + 1;
    nsDmg.height = rect.lr.y - rect.ul.y + 1;

    if(( nsDmg.width <= 0 ) || ( nsDmg.height <= 0 ))
      return;

    nsPaintEvent pev;
    pWin->InitEvent(pev, NS_PAINT);

    pev.rect = &nsDmg;
    pev.eventStructType = NS_PAINT_EVENT;

    PtHold();

    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&pev.renderingContext))
    {
      pev.renderingContext->Init( pWin->mContext, pWin );

      if( pWin->SetWindowClipping( damage, offset ) == NS_OK )
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ( "Dispatching paint event (area=%ld,%ld,%ld,%ld).\n",nsDmg.x,nsDmg.y,nsDmg.width,nsDmg.height ));
        pWin->DispatchWindowEvent(&pev);
      }

      NS_RELEASE(pev.renderingContext);
    }

    PtRelease();
    NS_RELEASE(pev.widget);
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  aborted due to no event callback!\n"));
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  End of RawDrawFunc\n"));
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


NS_METHOD nsWindow::GetSiblingClippedRegion( PhTile_t **btiles, PhTile_t **ctiles )
{
  nsresult res = NS_ERROR_FAILURE;

  if(( btiles ) && ( ctiles ))
  {
    *btiles = PhGetTile();
    if( *btiles )
    {
      PhTile_t   *tile, *last;
      PtWidget_t *w;
      PhArea_t   *area;
      PtArg_t    arg;

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
          *btiles = PhClipTilings( *btiles, *ctiles, nsnull );
          res = NS_OK;
        }
      }
    }
  }

  return res;
}


NS_METHOD nsWindow::SetWindowClipping( PhTile_t *damage, PhPoint_t &offset )
{
  nsresult res = NS_ERROR_FAILURE;

  PhTile_t   *tile, *last, *clip_tiles;
  PtWidget_t *w;
  PhArea_t   *area;
  PtArg_t    arg;

  clip_tiles = last = nsnull;

  if( mClipChildren )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clipping children...\n"));

    for( w=PtWidgetChildFront( mWidget ); w; w=PtWidgetBrotherBehind( w )) 
    { 
      if( PtWidgetIsRealized( w ))
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
          if( !clip_tiles )
            clip_tiles = tile;
          if( last )
            last->next = tile;
          last = tile;
        }
      }
    }
  }

  #if 0
  if( mClipSiblings )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clipping siblings...\n"));

    for( w=PtWidgetBrotherInFront( mWidget ); w; w=PtWidgetBrotherInFront( w ))
    {
      if( PtWidgetIsRealized( w ))
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
          if( !clip_tiles )
            clip_tiles = tile;
          if( last )
            last->next = tile;
          last = tile;
        }
      }
    }
  }
  #endif
  
  if( mClipSiblings )
  {
    PtWidget_t *node = mWidget;
    PhPoint_t  origin = { 0, 0 };
    
    while( node )
    {
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( node, 1, &arg );
      origin.x += area->pos.x;
      origin.y += area->pos.y;

      for( w=PtWidgetBrotherInFront( node ); w; w=PtWidgetBrotherInFront( w ))
      {
        if( PtWidgetIsRealized( w ))
        {
          PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
          PtGetResources( w, 1, &arg );
          tile = PhGetTile();
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

  if( dmg )
  {
    rects = PhTilesToRects( dmg, &rect_count );
    PgSetClipping( rect_count, rects );

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  damage clipped to:\n"));

    int i;
    for(i=0;i<rect_count;i++)
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("    (%i,%i,%i,%i)\n", rects[i].ul.x, rects[i].ul.y, rects[i].lr.x, rects[i].lr.y));

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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHandler for %p\n", someWindow ));

  if( someWindow )
  {
    rect.x = extents->ul.x;
    rect.y = extents->ul.y;
    rect.width = extents->lr.x - rect.x + 1;
    rect.height = extents->lr.y - rect.y + 1;

    someWindow->ResizeHoldOff();

  	someWindow->OnResize( rect );
  }
	return( Pt_CONTINUE );
}


//void nsWindow::StartResizeHoldOff( PtWidget_t *widget )

void nsWindow::ResizeHoldOff()
{
  if( !mWidget )
  {
    return;
  }

  if( PR_FALSE == mResizeQueueInited )
  {
    // This is to guarantee that the Invalidation work-proc is in place prior to the
    // Resize work-proc.

    if( !mDmgQueueInited )
    {
      Invalidate( PR_FALSE );
    }

    PtWidget_t *top = PtFindDisjoint( mWidget );

//    if( PtAppAddWorkProc( nsnull, ResizeWorkProc, top ))
    if(( mResizeProcID = PtAppAddWorkProc( nsnull, ResizeWorkProc, top )) != nsnull )
    {
//      PtStartFlux( top );
//      PtStartFlux( top );
      PtHold();
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
        mIsResizing = PR_TRUE;
        dqe->widget = mWidget;
        dqe->inst = this;
        dqe->next = mResizeQueue;
        mResizeQueue = dqe;
//        NS_ADDREF_THIS();
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
//      PtEndFlux( top );
      PtRelease();
      if( mResizeProcID )
        PtAppRemoveWorkProc( nsnull, mResizeProcID );
    }
  }
}


int nsWindow::ResizeWorkProc( void *data )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeWorkProc\n" ));

  if( mResizeQueueInited )
  {
    DamageQueueEntry *dqe = nsWindow::mResizeQueue;
    DamageQueueEntry *last_dqe;

    while( dqe )
    {
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


NS_METHOD nsWindow::GetClientBounds( nsRect &aRect )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetClientBounds (%p).\n", this ));

  aRect.x = 0;
  aRect.y = 0;
  aRect.width = mBounds.width;
  aRect.height = mBounds.height;

  if( !IsChild() )
  {
    int  h = GetMenuBarHeight();

    aRect.width -= (mFrameRight + mFrameLeft);
    aRect.height -= (h + mFrameTop + mFrameBottom);
  }

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move (%p) to (%ld,%ld) mClipChildren=<%d> mClipSiblings=<%d> mBorderStyle=<%d> mWindowType=<%d>\n", this, aX, aY, mClipChildren, mClipSiblings, mBorderStyle, mWindowType));
  
  if ( (mWindowType == eWindowType_popup) && (mParent == NULL))
  {
    /* The coordinates are offset from my parent */
    /* Don't have a parent so offset from the cursor position */
    /* This is bug 11088 */
	
   	nsRect parentRect;
    PRInt32 newX, newY;
    PhCursorInfo_t aCursorInfo;
    int err;	
    err = PhQueryCursor(1, &aCursorInfo);

	if (err == 0)
	{
		printf("nsWindow::Move cursor at <%d,%d>\n", aCursorInfo.pos.x, aCursorInfo.pos.y);
		newX = aX + aCursorInfo.pos.x;
		newY = aY + aCursorInfo.pos.y;
		
		return this->nsWidget::Move(newX, newY);
	}
	else
	  printf("nsWindow::Move PhQueryCursor failed err=<%d>\n", err);

    return NS_ERROR_FAILURE;
	
  }
  else  
    return this->nsWidget::Move(aX, aY);
}

#if 0
void nsWindow::NativePaint( PhRect_t &extent )
{
//  PtDamageWidget( mWidget, &extent );
  PtDamageExtent( mWidget, &extent );
/*
  if( IsChild() && mWidget )
  {
    PhTile_t  damage;
    PhPoint_t offset;

    PtWidgetOffset( mWidget, &offset );
    damage.rect.ul.x = offset.x + extent.ul.x;
    damage.rect.ul.y = offset.y + extent.ul.y;
    damage.rect.lr.x = offset.x + extent.lr.x;
    damage.rect.lr.y = offset.y + extent.lr.y;
    damage.next = nsnull;

    RawDrawFunc( mWidget, &damage );
  }
*/
}
#endif

