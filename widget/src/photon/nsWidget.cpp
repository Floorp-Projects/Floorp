/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsWidget.h"
#include "nsWindow.h"
#include "nsIDeviceContext.h"
#include "nsIAppShell.h"
#include "nsGfxCIID.h"
#include "nsIComponentManager.h"
#include "nsIFontMetrics.h"
#include <Pt.h>
#include "PtRawDrawContainer.h"

// BGR, not RGB - REVISIT
#define NSCOLOR_TO_PHCOLOR(g,n) \
  g.red=NS_GET_B(n); \
  g.green=NS_GET_G(n); \
  g.blue=NS_GET_R(n);

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

nsILookAndFeel *nsWidget::sLookAndFeel = nsnull;
PRUint32 nsWidget::sWidgetCount = 0;

//#define DBG 1

DamageQueueEntry *nsWidget::mDmgQueue = nsnull;
PtWorkProcId_t *nsWidget::mWorkProcID = nsnull;
PRBool nsWidget::mDmgQueueInited = PR_FALSE;

nsWidget::nsWidget()
{
  // XXX Shouldn't this be done in nsBaseWidget?
  NS_INIT_REFCNT();

  if (!sLookAndFeel) {
    if (NS_OK != nsComponentManager::CreateInstance(kLookAndFeelCID,
                                                    nsnull,
                                                    NS_GET_IID(nsILookAndFeel),
                                                    (void**)&sLookAndFeel))
      sLookAndFeel = nsnull;
  }

  if (sLookAndFeel)
    sLookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground,
                           mBackground);

  mWidget = nsnull;
  mParent = nsnull;
  mClient = nsnull;
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
  mUpdateArea.SetRect(0, 0, 0, 0);
//  mCreateHold = PR_TRUE;
//  mHold = PR_FALSE;
  sWidgetCount++;

}


nsWidget::~nsWidget()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::~nsWidget this=(%p) mWidget=<%p>\n", this, mWidget));

  mIsDestroying = PR_TRUE;
  if (nsnull != mWidget)
  {
    Destroy();
  }
  if (!sWidgetCount--) {
    NS_IF_RELEASE(sLookAndFeel);
  }
}


NS_METHOD nsWidget::SetBackgroundColor( const nscolor &aColor )
{
  nsBaseWidget::SetBackgroundColor( aColor );

  if( mWidget )
  {
    PtArg_t   arg;
    PgColor_t color = NS_TO_PH_RGB( aColor );

    PtSetArg( &arg, Pt_ARG_FILL_COLOR, color, 0 );
    PtSetResources( mWidget, 1, &arg );
  }

  return NS_OK;
}


NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WidgetToScreen - Not Implemented.\n" ));
  NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
  return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ScreenToWidget - Not Implemented.\n" ));
  NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Destroy(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Destroy this=<%p> mRefCnt=<%d> mWidget=<%p> mIsDestroying=<%d>\n",this,mRefCnt, mWidget, mIsDestroying));

  if( !mIsDestroying )
  {
    nsBaseWidget::Destroy();
    NS_IF_RELEASE( mParent );
  }

  if( mWidget )
  {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    RemoveDamagedWidget(mWidget);
    PtDestroyWidget( mWidget );
    mWidget = nsnull;

    if( PR_FALSE == mOnDestroyCalled )
      OnDestroy();
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Destroy the end this=<%p> mRefCnt=<%d>\n", this, mRefCnt));

  return NS_OK;
}


// make sure that we clean up here

void nsWidget::OnDestroy()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnDestroy this=<%p> mRefCnt=<%d>\n", this, mRefCnt ));

  mOnDestroyCalled = PR_TRUE;

  // release references to children, device context, toolkit + app shell
  nsBaseWidget::OnDestroy();

  // dispatch the event
  if (!mIsDestroying) {
    // dispatching of the event may cause the reference count to drop to 0
    // and result in this object being destroyed. To avoid that, add a reference
    // and then release it after dispatching the event
#if 1
/* Code stolen from GTK */
    // dispatching of the event may cause the reference count to drop
    // to 0 and result in this object being destroyed. To avoid that,
    // add a reference and then release it after dispatching the event
    nsrefcnt old = mRefCnt;
    mRefCnt = 99;
    DispatchStandardEvent(NS_DESTROY);
    mRefCnt = old;
#else
    AddRef();
    DispatchStandardEvent(NS_DESTROY);
    Release();
#endif
  }
}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent(void)
{
#if 1
/* Stolen from GTK */
  if (mParent) {
    NS_ADDREF(mParent);
  }
  return mParent;
#else
  nsIWidget *theParent = nsnull;
  
  if( mParent )
  {
    /* I know this is really a nsWidget so I am type casting to */
    /* improve preformance since this function is used a lot. (kirkj) */
    if ( ! (((nsWidget *)mParent)->mIsDestroying))
    {
      NS_ADDREF( mParent );
      theParent = mParent;
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParent - parent = %X\n", mParent ));
    }
    else
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParent - mParent is being destroyed so return NULL!\n"));
    }
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParent - mParent is NULL!\n" ));
  }
  
  return theParent;
#endif
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Show(PRBool bState)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show. mRefCnt=<%d>", mRefCnt));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show - mWidget is NULL!\n" ));
    return NS_OK; // Will be null durring printing
  }

  if (bState)
  {
    if( !PtWidgetIsRealized( mWidget ))
    {
      EnableDamage( mWidget, PR_FALSE );
      PtRealizeWidget(mWidget);
      EnableDamage( mWidget, PR_TRUE );

      Invalidate( PR_TRUE );
    }
  }
  else
  {
    if( PtWidgetIsRealized( mWidget ))
      PtUnrealizeWidget(mWidget);
  }

  mShown = bState;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show end mRefCnt=<%d>", mRefCnt));

  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetModal(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetModal - Not Implemented\n"));

  if (!mWidget)
	return NS_ERROR_FAILURE;

  PtWidget_t *toplevel = PtFindDisjoint(mWidget);
  if (!toplevel)
	return NS_ERROR_FAILURE;
	
  //gtk_window_set_modal(toplevel, PR_TRUE);

  return NS_OK;
}

NS_METHOD nsWidget::IsVisible(PRBool &aState)
{
  if( mWidget )
  {
    if( PtWidgetIsRealized( mWidget ))
      mShown = PR_TRUE;
    else
      mShown = PR_FALSE;

    aState = mShown;
  }
  else
    aState = PR_FALSE;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Move(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move (%p) to (%ld,%ld)\n", this, aX, aY ));

  if(( mBounds.x == aX ) && ( mBounds.y == aY ))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  already there.\n" ));
    return NS_OK;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  was at (%i,%i)\n", mBounds.x, mBounds.y ));

  mBounds.x = aX;
  mBounds.y = aY;

  if (mWidget)
  {
    PtArg_t arg;
    PhPoint_t *oldpos;
    PhPoint_t pos = {aX, aY};

    EnableDamage( mWidget, PR_FALSE );

    PtSetArg( &arg, Pt_ARG_POS, &oldpos, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if(( oldpos->x != pos.x ) || ( oldpos->y != pos.y ))
      {
        PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
        PtSetResources( mWidget, 1, &arg );
      }
    }

    EnableDamage( mWidget, PR_TRUE );
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move - mWidget is NULL!\n" ));

  return NS_OK;
}


NS_METHOD nsWidget::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize (%p) to <%d, %d>\n", this, aWidth, aHeight ));

  if(( mBounds.width != aWidth ) || ( mBounds.height != aHeight ))
  {
    mBounds.width  = aWidth;
    mBounds.height = aHeight;

    if( mWidget )
    {
      PtArg_t arg;
      int     *border;

      PtSetArg( &arg, Pt_ARG_BORDER_WIDTH, &border, 0 );
      if( PtGetResources( mWidget, 1, &arg ) == 0 )
      {
		/* Add the border to the size of the widget */
        PhDim_t dim = {aWidth - 2*(*border), aHeight - 2*(*border)};

        EnableDamage( mWidget, PR_FALSE );

        PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
        PtSetResources( mWidget, 1, &arg );

        EnableDamage( mWidget, PR_TRUE );
      }

      Invalidate( aRepaint );
    }
    else
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize - mWidget is NULL!\n" ));
  }

  return NS_OK;
}


NS_METHOD nsWidget::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX,aY);
  Resize(aWidth,aHeight,aRepaint);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsRect &aRect)
{
  PRBool result = PR_FALSE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnResize\n"));

  // call the event callback
  if (mEventCallback)
  {
    nsSizeEvent event;

#if 1
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
#else
    InitEvent(event, NS_SIZE);
    event.windowSize = &aRect;
    event.eventStructType = NS_SIZE_EVENT;
    event.mWinWidth = mBounds.width;
    event.mWinHeight = mBounds.height;
    event.point.x = mBounds.x; //mWidget->allocation.x;
    event.point.y = mBounds.y; //mWidget->allocation.y;
    event.time = 0;

    result = DispatchWindowEvent(&event);

    /* Not sure if this is really needed, kirkj */
    NS_IF_RELEASE(event.widget);
#endif
  }

  return result;
}

//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnMove\n" ));

  nsGUIEvent event;

  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;
  PRBool result = DispatchWindowEvent(&event);

// GTK doesn't do this anymore
//  NS_RELEASE(event.widget);

  return result;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Enable(PRBool bState)
{
  if (mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Enable being set to %d\n", bState));

    PtArg_t arg;
    if( bState )
      PtSetArg( &arg, Pt_ARG_FLAGS, 0, Pt_BLOCKED );
    else
      PtSetArg( &arg, Pt_ARG_FLAGS, Pt_BLOCKED, Pt_BLOCKED );
    PtSetResources( mWidget, 1, &arg );
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Enable - mWidget is NULL!\n" ));

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFocus(void)
{
  if (mWidget)
    PtContainerGiveFocus( mWidget, NULL );
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFocus - mWidget is NULL!\n" ));

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics *nsWidget::GetFont(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetFont - Not Implemented.\n" ));
  NS_NOTYETIMPLEMENTED("nsWidget::GetFont");
  return nsnull;
}


//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFont(const nsFont &aFont)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFont\n" ));
#if 1
{
  char *str = nsnull;
  str = aFont.name.ToNewCString();
  if (str)
  {
   PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFont aFont.name=<%s>\n",str));
   delete [] str;  
  }
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFont aFont.size=<%d>\n",aFont.size));

}
#endif

  nsIFontMetrics* mFontMetrics;
  mContext->GetMetricsFor(aFont, mFontMetrics);

  if (mFontMetrics)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFont Get a FontMetrics\n" ));
    PtArg_t arg;

    nsFontHandle aFontHandle;
	mFontMetrics->GetFontHandle(aFontHandle);
	nsString *aString;
	aString = (nsString *) aFontHandle;
    char *str = aString->ToNewCString();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFont Get a FontMetrics font name=<%s>\n",str ));

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
NS_METHOD nsWidget::SetCursor(nsCursor aCursor)
{
  unsigned short curs;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetCursor to <%d> was <%d>\n", aCursor, mCursor));

  // Only change cursor if it's changing
  if (aCursor != mCursor) {
    switch( aCursor )
    {
  case eCursor_select:
    curs = Ph_CURSOR_INSERT;
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

  if( mWidget )
  {
    PtArg_t arg;

    PtSetArg( &arg, Pt_ARG_CURSOR_TYPE, curs, 0 );
    PtSetResources( mWidget, 1, &arg );
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetCursor - mWidget is NULL!\n" ));

  mCursor = aCursor;

  }
  return NS_OK;
}


NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate %p\n", this ));

  if( mWidget )
  {
    nsRect   rect;

    rect = mBounds;
    GetParentClippedArea( rect );

    if( rect.width && rect.height )
    {
	  /* Damage has to be releative Parent */
      mUpdateArea.SetRect( rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  rect=(%i,%i,%i,%i)\n", rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height  ));

      if (aIsSynchronous)
      {
        UpdateWidgetDamage();
      }
      else
      {
        QueueWidgetDamage();
      }
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate - mWidget is NULL!\n" ));

  return NS_OK;
}


NS_METHOD nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate %p (%ld,%ld,%ld,%ld)\n", this, aRect.x, aRect.y, aRect.width, aRect.height ));

  if( mWidget )
  {
//    PtArg_t  arg;
//    PhArea_t *area;
    nsRect   rect = aRect;

#if 0
    // Just to be safe, get the real widget coords
    PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      mBounds.x = area->pos.x;
      mBounds.y = area->pos.y;
      mBounds.width  = area->size.w;
      mBounds.height = area->size.h;
    }
#endif

    rect.x += mBounds.x;
    rect.y += mBounds.y;

    GetParentClippedArea( rect );

    if( rect.width && rect.height )
    {
      rect.x -= mBounds.x;
      rect.y -= mBounds.y;

      mUpdateArea.UnionRect( mUpdateArea, rect );

      if( aIsSynchronous)
      {
        UpdateWidgetDamage();
      }
      else
      {
        QueueWidgetDamage();
      }
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate(2) - mWidget is NULL!\n" ));

  return NS_OK;
}


PRBool nsWidget::GetParentClippedArea( nsRect &rect )
{
// Traverse parent heirarchy and clip the passed-in rect bounds

  PtArg_t    arg;
  PhArea_t   *area;
  PtWidget_t *parent;
  PhPoint_t  offset;
  nsRect     rect2;
  PtWidget_t *disjoint = PtFindDisjoint( mWidget );

//printf( "Clipping widget (%p) rect: %d,%d,%d,%d\n", this, rect.x, rect.y, rect.width, rect.height );

  // convert passed-in rect to absolute window coords first...
  PtWidgetOffset( mWidget, &offset );
  rect.x += offset.x;
  rect.y += offset.y;


//printf( "  screen coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height );

  parent = PtWidgetParent( mWidget );
  while( parent )
  {
    PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
    if( PtGetResources( parent, 1, &arg ) == 0 )
    {
      rect2.width = area->size.w;
      rect2.height = area->size.h;

      if( parent != disjoint )
      {
        PtWidgetOffset( parent, &offset );
        rect2.x = area->pos.x + offset.x;
        rect2.y = area->pos.y + offset.y;
      }
      else
      {
        rect2.x = rect2.y = 0;
      }

//printf( "  parent at: %d,%d,%d,%d\n", rect2.x, rect2.y, rect2.width, rect2.height );

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
      else
      {
        if( rect.x < rect2.x )
        {
          rect.width -= ( rect2.x - rect.x );
          rect.x = rect2.x;
        }

        if( rect.y < rect2.y )
        {
          rect.height -= ( rect2.y - rect.y );
          rect.y = rect2.y;
        }

        if(( rect.x + rect.width ) > ( rect2.x + rect2.width ))
        {
          rect.width = (rect2.x + rect2.width) - rect.x;
        }

        if(( rect.y + rect.height ) > ( rect2.y + rect2.height ))
        {
          rect.height = (rect2.y + rect2.height) - rect.y;
        }

//printf( "  new widget coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height );

      }
    }

    parent = PtWidgetParent( parent );
  }


  // convert rect back to widget coords...
  PtWidgetOffset( mWidget, &offset );
  rect.x -= offset.x;
  rect.y -= offset.y;

//printf( "  final widget coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height );

  if( rect.width && rect.height )
    return PR_TRUE;
  else
    return PR_FALSE;
}



NS_METHOD nsWidget::Update(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Update\n" ));

  UpdateWidgetDamage();

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWidget::GetNativeData(PRUint32 aDataType)
{
  switch(aDataType)
  {
  case NS_NATIVE_WINDOW:
  case NS_NATIVE_WIDGET:
  case NS_NATIVE_PLUGIN_PORT:
    if( !mWidget )
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetNativeData(mWidget) - mWidget is NULL!\n" ));
    return (void *)mWidget;
  case NS_NATIVE_DISPLAY:
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetNativeData(NS_NATIVE_DISPLAY) - Not Implemented.\n" ));
    return nsnull;
  case NS_NATIVE_GRAPHIC:
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetNativeData(NS_NATIVE_GRAPHIC) - Not Implemented.\n" ));
    return nsnull;
  default:
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetNativeData() - ERROR Bad ID.\n" ));
    break;
  }
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetColorMap(nsColorMap *aColorMap)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetColorMap - Not Implemented.\n" ));
  return NS_OK;
}

NS_METHOD nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Scroll - Not Implemented.\n" ));
  return NS_OK;
}

NS_METHOD nsWidget::BeginResizingChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::BeginResizingChildren - Not Implemented.\n" ));
  return NS_OK;
}

NS_METHOD nsWidget::EndResizingChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::EndResizingChildren - Not Implemented.\n" ));
  return NS_OK;
}

NS_METHOD nsWidget::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}


NS_METHOD nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetMenuBar - Not Implemented.\n" ));
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::ShowMenuBar( PRBool aShow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ShowMenuBar aShow=<%d> - Not Implemented.\n",aShow));
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget this=<%p> mRefCnt=<%d>\n", this, mRefCnt));
  if (aParent)
     PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget (%p) nsIWidget parent\n", this));
  else if (aNativeParent)
     PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget (%p) native parent\n",this));
  else if(aAppShell)
     PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::CreateWidget (%p) nsAppShell parent\n",this));
			
  PtWidget_t *parentWidget = nsnull;

#if 1
 nsIWidget *baseParent = aInitData &&
    (aInitData->mWindowType == eWindowType_dialog ||
     aInitData->mWindowType == eWindowType_toplevel) ?
    nsnull : aParent;

  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  mParent = aParent;
  NS_IF_ADDREF(mParent);


#else			 

  BaseCreate(aParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);
#endif


  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget after BaseCreate  mRefCnt=<%d>\n", mRefCnt));


  if( aNativeParent )
  {
    parentWidget = (PtWidget_t*)aNativeParent;
//    mParent = GetInstance( (PtWidget_t*)aNativeParent );
  }
  else if( aParent )
  {
    parentWidget = (PtWidget_t*) (aParent->GetNativeData(NS_NATIVE_WIDGET));
//    mParent = aParent;
  }
  else if(aAppShell)
  {
    nsNativeWidget shellWidget = aAppShell->GetNativeData(NS_NATIVE_SHELL);
    if (shellWidget)
	{
      parentWidget = (PtWidget_t*) shellWidget;
    }
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget - No parent specified!\n" ));
  }


#if 1
  mBounds = aRect;
  CreateNative (parentWidget);

  Resize(aRect.width, aRect.height, PR_FALSE);

  /* place the widget in its parent if it isn't a toplevel window*/
  if (mIsToplevel)
  {
    if (parentWidget)
    {
      // set transient properties
    }
  }
  else
  {
    // Find the native client widget and store for ALL non-toplevel widgets
    if( parentWidget )
    {
      PtWidget_t *pTop = PtFindDisjoint( parentWidget );
      nsWindow * pWin = (nsWindow *) GetInstance( pTop );
      if( pWin )
      {
        mClient = (PtWidget_t*) pWin->GetNativeData( NS_NATIVE_WIDGET );
      }
    }
  }
  
#else

  // REVISIT - Are we really supposed to addref our parent ???
  NS_IF_ADDREF( mParent );

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget before GetInstance mRefCnt=<%d>\n", mRefCnt));

  // Find the native client widget and store for ALL non-toplevel widgets
  if( parentWidget )
  {
    PtWidget_t *pTop = PtFindDisjoint( parentWidget );
    nsWindow * pWin = (nsWindow *) GetInstance( pTop );
    if( pWin )
    {
      mClient = (PtWidget_t*) pWin->GetNativeData( NS_NATIVE_WIDGET );
    }
  }

  mBounds = aRect;

#endif

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget - bounds=(%i,%i,%i,%i) mRefCnt=<%d>\n", mBounds.x, mBounds.y, mBounds.width, mBounds.height, mRefCnt));

#if 0
  if (aInitData)
    PreCreateWidget(aInitData);
	
  CreateNative (parentWidget);

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget after CreateNative mRefCnt=<%d>\n", mRefCnt));
#endif

  if( mWidget )
  {
    SetInstance(mWidget, this);
    PtAddCallback( mWidget, Pt_CB_GOT_FOCUS, GotFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_LOST_FOCUS, LostFocusCallback, this );
  }

  DispatchStandardEvent(NS_CREATE);

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget end mRefCnt=<%d>\n", mRefCnt));

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


void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ConvertToDeviceCoordinates - Not Implemented.\n" ));
}


void nsWidget::InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint)
{
    event.widget = this;
//    NS_IF_ADDREF(event.widget);

    if (aPoint == nsnull)
    {
      event.point.x = 0;
      event.point.y = 0;
    }    
    else
    {
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;
    }

    event.time = PR_IntervalNow();
    event.message = aEventType;
}


PRBool nsWidget::ConvertStatus(nsEventStatus aStatus)
{
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
  return(PR_FALSE);
}

PRBool nsWidget::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  PRBool ret;
  
  DispatchEvent(event, status);

//  printf("nsWidget::DispatchWindowEvent  status=<%d> convtered=<%d>\n", status, ConvertStatus(status) );

  ret = ConvertStatus(status);
  return ret;
}


//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event, aMsg);

  PRBool result = DispatchWindowEvent(&event);
  NS_IF_RELEASE(event.widget);

  return result;
}


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *aEvent,
                                      nsEventStatus &aStatus)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget(%p)::DispatchEvent\n", this));
//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent 1 mRefCnt=<%d>\n", mRefCnt));

#if 1
/* Stolen from GTK */

  NS_ADDREF(aEvent->widget);

  if (nsnull != mMenuListener) {
    if (NS_MENU_EVENT == aEvent->eventStructType)
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *aEvent));
  }

  aStatus = nsEventStatus_eIgnore;
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(aEvent);
  }

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*aEvent);
  }

// This causes the App to die in a bad way... The widget must be getting released
//  somewhere else... This is a Memory Leak! 
   NS_RELEASE(aEvent->widget);

  return NS_OK;
#endif

#if 0
#if 1

  aStatus = nsEventStatus_eIgnore;
  
  //if (nsnull != mMenuListener)
  //	aStatus = mMenuListener->MenuSelected(*event);
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  }

    // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }

  // the window can be destroyed during processing of seemingly innocuous events like, say,
  // mousedowns due to the magic of scripting. mousedowns will return nsEventStatus_eIgnore,
  // which causes problems with the deleted window. therefore:
  if (mOnDestroyCalled)
    aStatus = nsEventStatus_eConsumeNoDefault;

#else

//  printf( ">>> nsWidget::DispatchEvent\n" ); fflush(stdout);

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent 1 mRefCnt=<%d>\n", mRefCnt));

  if( !( event ) || !( event->widget ))
  {
    return NS_ERROR_FAILURE;
  }

//kirkj  NS_ADDREF(event->widget);

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent 2  mRefCnt=<%d>\n", mRefCnt));

  if( nsnull != mMenuListener )
  {
    if( NS_MENU_EVENT == event->eventStructType )
    {
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *event));
    }
  }

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent 3 mRefCnt=<%d>\n", mRefCnt));

  aStatus = nsEventStatus_eIgnore;
  if( nsnull != mEventCallback )
  {
    aStatus = (*mEventCallback)(event);
  }

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent 4 mRefCnt=<%d>\n", mRefCnt));

  // Dispatch to event listener if event was not consumed
  if(( aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener))
  {
    aStatus = mEventListener->ProcessEvent(*event);
  }

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent 5  mRefCnt=<%d>\n", mRefCnt));

//kirkj  NS_RELEASE(event->widget);
#endif

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent end  mRefCnt=<%d>\n", mRefCnt));

  return NS_OK;
#endif
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
//PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent)
PRBool nsWidget::DispatchMouseEvent( PhPoint_t &aPos, PRUint32 aEvent )
{
//  printf( ">>> nsWidget::DispatchMouseEvent\n" ); fflush( stdout );

  PRBool result = PR_FALSE;
  if( nsnull == mEventCallback && nsnull == mMouseListener )
  {
    return result;
  }

  nsMouseEvent event;

  InitEvent( event, aEvent );
  event.eventStructType = NS_MOUSE_EVENT;
  event.point.x = aPos.x;
  event.point.y = aPos.y;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;

  // call the event callback
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent( &event );
    NS_IF_RELEASE(event.widget);

    return result;
  }

  if (nsnull != mMouseListener)
  {
    switch (aEvent)
    {
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

void nsWidget::InitCallbacks( char * aName )
{
}


PRBool nsWidget::SetInstance( PtWidget_t * pWidget, nsWidget * inst )
{
  PRBool res = PR_FALSE;

  if( pWidget )
  {
    PtArg_t arg;
    void *data = (void *)inst;

    PtSetArg(&arg, Pt_ARG_USER_DATA, &data, sizeof(void *) );
    if( PtSetResources( pWidget, 1, &arg) == 0 )
      res = PR_TRUE;
  }

  return res;
}


nsWidget* nsWidget::GetInstance( PtWidget_t * pWidget )
{
  if( pWidget )
  {
    PtArg_t  arg;
    void     **data;
    
    PtSetArg( &arg, Pt_ARG_USER_DATA, &data, 0 );
    if( PtGetResources( pWidget, 1, &arg ) == 0 )
    {
      if( data )
        return (nsWidget *) *data;
    }
  }

  return NULL;
}


// Input keysym is in gtk format; output is in NS_VK format
PRUint32 nsWidget::nsConvertKey(unsigned long keysym)
{

  struct nsKeyConverter {
    PRUint32       vkCode; // Platform independent key code
    unsigned long  keysym; // Photon key_sym key code
  };

 struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     Pk_Cancel },
  { NS_VK_BACK,       Pk_BackSpace },
  { NS_VK_TAB,        Pk_Tab },
  { NS_VK_CLEAR,      Pk_Clear },
  { NS_VK_RETURN,     Pk_Return },
  { NS_VK_SHIFT,      Pk_Shift_L },
  { NS_VK_SHIFT,      Pk_Shift_R },
  { NS_VK_CONTROL,    Pk_Control_L },
  { NS_VK_CONTROL,    Pk_Control_R },
  { NS_VK_ALT,        Pk_Alt_L },
  { NS_VK_ALT,        Pk_Alt_R },
  { NS_VK_PAUSE,      Pk_Pause },
  { NS_VK_CAPS_LOCK,  Pk_Caps_Lock },
  { NS_VK_ESCAPE,     Pk_Escape },
  { NS_VK_SPACE,      Pk_space },
  { NS_VK_PAGE_UP,    Pk_Pg_Up },
  { NS_VK_PAGE_DOWN,  Pk_Pg_Down },
  { NS_VK_END,        Pk_End },
  { NS_VK_HOME,       Pk_Home },
  { NS_VK_LEFT,       Pk_Left },
  { NS_VK_UP,         Pk_Up },
  { NS_VK_RIGHT,      Pk_Right },
  { NS_VK_DOWN,       Pk_Down },
  { NS_VK_PRINTSCREEN, Pk_Print },
  { NS_VK_INSERT,     Pk_Insert },
  { NS_VK_DELETE,     Pk_Delete },
  { NS_VK_MULTIPLY,   Pk_KP_Multiply },
  { NS_VK_ADD,        Pk_KP_Add },
  { NS_VK_SEPARATOR,  Pk_KP_Separator },
  { NS_VK_SUBTRACT,   Pk_KP_Subtract },
  { NS_VK_DECIMAL,    Pk_KP_Decimal },
  { NS_VK_DIVIDE,     Pk_KP_Divide },
  { NS_VK_RETURN,     Pk_KP_Enter },
  { NS_VK_COMMA,      Pk_comma },
  { NS_VK_PERIOD,     Pk_period },
  { NS_VK_SLASH,      Pk_slash },
  { NS_VK_OPEN_BRACKET, Pk_bracketleft },
  { NS_VK_CLOSE_BRACKET, Pk_bracketright },
  { NS_VK_QUOTE, Pk_quotedbl }
  };

  const int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  if (keysym >= Pk_a && keysym <= Pk_z)
     return keysym - Pk_a + NS_VK_A;
		
  if (keysym >= Pk_A && keysym <= Pk_Z)
     return keysym - Pk_A + NS_VK_A;
			  
  if (keysym >= Pk_0 && keysym <= Pk_9)
     return keysym - Pk_0 + NS_VK_0;

  if (keysym >= Pk_KP_0 && keysym <= Pk_KP_9)
     return keysym - Pk_KP_0 + NS_VK_NUMPAD0;
						  
  if (keysym >= Pk_F1 && keysym <= Pk_F24)
     return keysym - Pk_F1 + NS_VK_F1;

  for (int i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
    {
      printf("nsWidget::nsConvertKey - Converted <%x> to <%x>\n", keysym, nsKeycodes[i].vkCode);
      return(nsKeycodes[i].vkCode);
    }
  }

  NS_ASSERTION(0,"nsWidget::nsConvertKey - Did not Find Key! - Not Implemented\n");

  return((int) 0);
}

//==============================================================
void nsWidget::InitKeyEvent(PhKeyEvent_t *aPhKeyEvent,
                            nsWidget * aWidget,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::InitKeyEvent\n"));

  anEvent.message = aEventType;
  anEvent.widget  = aWidget;
  anEvent.eventStructType = NS_KEY_EVENT;

  if (aPhKeyEvent != nsnull)
  {
    anEvent.keyCode =  (nsConvertKey(aPhKeyEvent->key_cap)  & 0x00FF);
    if (aEventType == NS_KEY_PRESS)
	{
      printf("nsWidget::InitKeyEvent key_sym=<%lu> converted=<%lu>\n",
	     aPhKeyEvent->key_sym, nsConvertKey(aPhKeyEvent->key_cap));

	  anEvent.charCode = aPhKeyEvent->key_sym;
    }
	else
      anEvent.charCode = 0; 

    anEvent.time =      PR_IntervalNow();
    anEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
    anEvent.point.x = 0; 
    anEvent.point.y = 0;
  }
}

PRBool  nsWidget::DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchEvent Got a Key Event aPhEkyEvent->key_mods:<%x> aPhEkyEvent->key_flags:<%x> aPhEkyEvent->key_sym=<%x> aPhEkyEvent->key_caps=<%x>\n",aPhKeyEvent->key_mods, aPhKeyEvent->key_flags, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap));
  static int counter=0;
  
  NS_ASSERTION(aPhKeyEvent, "nsWidget::DispatchKeyEvent a NULL PhKeyEvent was passed in");

  nsKeyEvent keyEvent;
  PRBool result = PR_FALSE;
   
  if ( (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid) == 0)
  {
     printf("nsWidget::DispatchKeyEvent throwing away invalid key\n");
     return PR_TRUE;
  }

  if ( ( aPhKeyEvent->key_cap == Pk_Shift_L ) ||
       ( aPhKeyEvent->key_cap == Pk_Shift_R ) ||	  
       ( aPhKeyEvent->key_cap == Pk_Control_L ) ||	  
       ( aPhKeyEvent->key_cap == Pk_Control_R ) ||	  
       ( aPhKeyEvent->key_cap == Pk_Alt_L ) ||	  
       ( aPhKeyEvent->key_cap == Pk_Alt_R )	  
     )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchKeyEvent Ignoring SHIFT, CONTROL and ALT keypress\n"));
    return PR_TRUE;
  }

  nsWindow *w = (nsWindow *) this;
  
printf("nsWidget::DispatchKeyEvent KeyEvent Info: key_flags=<%lu> key_mods=<%lu>  key_sym=<%lu> key_cap=<%lu> key_scan=<%d>\n",
	aPhKeyEvent->key_flags, aPhKeyEvent->key_mods, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap, aPhKeyEvent->key_scan);
	

  if (PkIsFirstDown(aPhKeyEvent->key_flags))
  {
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_DOWN);
    result = w->OnKey(keyEvent); 

  printf("nsWidget::DispatchKeyEvent Key Down counter=<%d>\n", counter++);

//    if (aPhKeyEvent->key_cap < 0xF000)
    {
      InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_PRESS);
      result = w->OnKey(keyEvent); 
      printf("nsWidget::DispatchKeyEvent Key Press counter=<%d>\n", counter++);
    }
  }
  else if (PkIsKeyDown(aPhKeyEvent->key_flags) == 0)
  {
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_UP);
    result = w->OnKey(keyEvent); 
  printf("nsWidget::DispatchKeyEvent Key Up counter=<%d>\n", counter++);
  }

  printf("nsWidget::DispatchKeyEvent after events result=<%d>\n", result);
  

//  result = PR_FALSE;	/* HACK! */

  return result;
}



//-------------------------------------------------------------------------
//
// the nsWidget raw event callback for all nsWidgets in this toolkit
//
//-------------------------------------------------------------------------
int nsWidget::RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  // Get the window which caused the event and ask it to process the message
  nsWidget *someWidget = (nsWidget*) data;

  if( nsnull != someWidget )
  {
    if( someWidget->HandleEvent( cbinfo ))
      return( Pt_END ); // Event was consumed
  }

  return( Pt_CONTINUE );
}



PRBool nsWidget::HandleEvent( PtCallbackInfo_t* aCbInfo )
{
#if 1
  NS_ASSERTION(0, "nsWidget::HandleEvent Somebody got here by mistake");
  return PR_FALSE;
#else  
  PRBool  result = PR_FALSE; // call the default nsWindow proc
  PtWidget_t *parent = PtFindContainer( mWidget );
  nsWidget * parentWidget = (nsWidget *) GetInstance( parent );


  // When we get menu selections, dispatch the menu command (event) AND IF there is
  // a menu listener, call "listener->MenuSelected(event)"

  if( aCbInfo->reason == Pt_CB_RAW )
  {
    PhEvent_t* event = aCbInfo->event;

    switch( event->type )
    {
    case Ph_EV_BUT_PRESS:
      {
      PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );

      if( ptrev )
      {
        ptrev->pos.x = 1;
        ptrev->pos.y = 1;
        if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
        {
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

      if (event->subtype==Ph_EV_RELEASE_REAL)
      if( ptrev )
      {
        ptrev->pos.x = 1;
        ptrev->pos.y = 1;
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
    }
  }

//  return result;
  return PR_TRUE;
#endif
}


void nsWidget::ScreenToWidget( PhPoint_t &pt )
{
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
void nsWidget::InitDamageQueue()
{
  mDmgQueue = nsnull;

  mWorkProcID = PtAppAddWorkProc( nsnull, WorkProc, &mDmgQueue );
  if( mWorkProcID )
  {
    mDmgQueueInited = PR_TRUE;
    PtHold();
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("*********** damage queue failed to init. ***********\n" ));
  }
}


//---------------------------------------------------------------------------
// nsWidget::QueueWidgetDamage()
//
// Adds this widget to the damage queue. The damage is accumulated in
// mUpdateArea.
//---------------------------------------------------------------------------
void nsWidget::QueueWidgetDamage()
{
  if( !mDmgQueueInited )
    InitDamageQueue();

  if( mWidget && mDmgQueueInited )
  {
    // See if we're already in the queue, if so don't add us again
    DamageQueueEntry *dqe;
    PRBool           found = PR_FALSE;
  
    dqe = mDmgQueue;

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
void nsWidget::UpdateWidgetDamage()
{
  if( mWidget )
  {
    RemoveDamagedWidget( mWidget );

    // If we have damage, cause it to be repaired
    if( mUpdateArea.width && mUpdateArea.height )
    {
      PhRect_t extent;
      PhArea_t area;

//      if( mCreateHold )
//        mCreateHold = PR_FALSE;

      PtWidgetArea( mWidget, &area );

      if( !mUpdateArea.width || !mUpdateArea.height )
      {
        mUpdateArea.x = area.pos.x;
        mUpdateArea.y = area.pos.y;
        mUpdateArea.width = area.size.w;
        mUpdateArea.height = area.size.h;
      }

      if( GetParentClippedArea( mUpdateArea ))
      {
        extent.ul.x = mUpdateArea.x + area.pos.x;
        extent.ul.y = mUpdateArea.y + area.pos.y;
        extent.lr.x = extent.ul.x + mUpdateArea.width - 1;
        extent.lr.y = extent.ul.y + mUpdateArea.height - 1;
        PtDamageExtent( mWidget, &extent );
//        NativePaint( extent );
      }

      PtFlush();
      mUpdateArea.SetRect(0,0,0,0);
    }
  }
}


void nsWidget::RemoveDamagedWidget(PtWidget_t *aWidget)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::RemoveDamagedWidget Photon Widget=<%p>\n", aWidget));

    if( mDmgQueueInited )
    {
      DamageQueueEntry *dqe;
      DamageQueueEntry *last_dqe = nsnull;
  
      dqe = mDmgQueue;

      // If this widget is in the queue, remove it
      while( dqe )
      {
        if( dqe->widget == aWidget )
        {
          if( last_dqe )
            last_dqe->next = dqe->next;
          else
            mDmgQueue = dqe->next;

          delete dqe;
          break;
        }
        last_dqe = dqe;
        dqe = dqe->next;
      }

      if( nsnull == mDmgQueue )
      {
        mDmgQueueInited = PR_FALSE;
        PtRelease();
        if( mWorkProcID )
          PtAppRemoveWorkProc( nsnull, mWorkProcID );
      }
    }
}


int nsWidget::WorkProc( void *data )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::WorkProc\n"));

  DamageQueueEntry **dq = (DamageQueueEntry **) data;

  if( dq && (*dq))
  {
    DamageQueueEntry *dqe = *dq;
    DamageQueueEntry *last_dqe;
    PhRect_t extent;
    PhArea_t area;
    PtWidget_t *w;

    while( dqe )
    {
      if( PtWidgetIsRealized( dqe->widget ))
      {
        PtWidgetArea( dqe->widget, &area );

        extent.ul.x = dqe->inst->mUpdateArea.x + area.pos.x;
        extent.ul.y = dqe->inst->mUpdateArea.y + area.pos.y;
        extent.lr.x = extent.ul.x + dqe->inst->mUpdateArea.width - 1;
        extent.lr.y = extent.ul.y + dqe->inst->mUpdateArea.height - 1;

//        dqe->inst->mCreateHold = PR_FALSE;
//        dqe->inst->NativePaint( extent );
        PtDamageExtent( dqe->widget, &extent );
      }

      dqe->inst->mUpdateArea.SetRect(0,0,0,0);

      last_dqe = dqe;
      dqe = dqe->next;
      delete last_dqe;
    }

    *dq = nsnull;
    mDmgQueueInited = PR_FALSE;
    PtRelease();
    PtFlush();
  }

  return Pt_END;
}


int nsWidget::GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWidget *pWidget = (nsWidget *) data;

  pWidget->DispatchStandardEvent(NS_GOTFOCUS);

  return Pt_CONTINUE;
}


int nsWidget::LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWidget *pWidget = (nsWidget *) data;

  pWidget->DispatchStandardEvent(NS_LOSTFOCUS);

  return Pt_CONTINUE;
}

void nsWidget::EnableDamage( PtWidget_t *widget, PRBool enable )
{
  PtWidget_t *top = PtFindDisjoint( widget );

  if( top )
  {
    if( PR_TRUE == enable )
      PtEndFlux( top );
    else
      PtStartFlux( top );
  }
}

