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

// BGR, not RGB - REVISIT
#define NSCOLOR_TO_PHCOLOR(g,n) \
  g.red=NS_GET_B(n); \
  g.green=NS_GET_G(n); \
  g.blue=NS_GET_R(n);

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

//#define DBG 1

nsWidget::nsWidget()
{
  // XXX Shouldn't this be done in nsBaseWidget?
  NS_INIT_REFCNT();

  // get the proper color from the look and feel code
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground, mBackground);
  }
  NS_RELEASE(lookAndFeel);
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
  mMenuBar = nsnull;
}


nsWidget::~nsWidget()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::~nsWidget (%p).\n", this ));

  mIsDestroying = PR_TRUE;
  if (nsnull != mWidget) {
    Destroy();
  }
}


NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WidgetToScreen - Not Implemented.\n" ));
  return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ScreenToWidget - Not Implemented.\n" ));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Destroy(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Destroy (%p).\n", this ));

  if( !mIsDestroying )
  {
    nsBaseWidget::Destroy();
    NS_IF_RELEASE( mParent );
  }

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

// make sure that we clean up here

void nsWidget::OnDestroy()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnDestroy.\n" ));

  mOnDestroyCalled = PR_TRUE;
  // release references to children, device context, toolkit + app shell
  nsBaseWidget::OnDestroy();
  // dispatch the event
  if (!mIsDestroying) {
    // dispatching of the event may cause the reference count to drop to 0
    // and result in this object being destroyed. To avoid that, add a reference
    // and then release it after dispatching the event
    AddRef();
    DispatchStandardEvent(NS_DESTROY);
    Release();
  }
}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent(void)
{
  if( mParent )
  {
    NS_ADDREF( mParent );
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParent - mParent is NULL!\n" ));

  return mParent;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Show(PRBool bState)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show." ));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show - mWidget is NULL!\n" ));
    return NS_OK; // Will be null durring printing
  }

  if (bState)
  {
    if( !PtWidgetIsRealized( mWidget ))
      PtRealizeWidget(mWidget);
  }
  else
  {
    if( PtWidgetIsRealized( mWidget ))
      PtUnrealizeWidget(mWidget);
  }

  mShown = bState;

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
NS_METHOD nsWidget::Move(PRUint32 aX, PRUint32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move (%p) to (%ld,%ld)\n", this, aX, aY ));

  mBounds.x = aX;
  mBounds.y = aY;

  if (mWidget)
  {
    PtArg_t arg;
    PhPoint_t *oldpos;
    PhPoint_t pos = {aX, aY};

    PtSetArg( &arg, Pt_ARG_POS, &oldpos, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if(( oldpos->x != pos.x ) || ( oldpos->y != pos.y ))
      {
        PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
        PtSetResources( mWidget, 1, &arg );
      }
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move - mWidget is NULL!\n" ));

  return NS_OK;
}


NS_METHOD nsWidget::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize (%p)\n", this ));

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if( mWidget )
  {
    PtArg_t arg;
    int     *border;
    PhDim_t *olddim;
    PhDim_t dim = {aWidth, aHeight};

    PtSetArg( &arg, Pt_ARG_BORDER_WIDTH, &border, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      dim.w -= 2*(*border);
      dim.h -= 2*(*border);
    }

    PtSetArg( &arg, Pt_ARG_DIM, &olddim, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if(( olddim->w != dim.w ) || ( olddim->h != dim.h ))
      {
        PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
        PtSetResources( mWidget, 1, &arg );

        if (aRepaint)
        {
          // REVISIT - photon doesnt like being told to redraw...
        }
      }
    }
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize - mWidget is NULL!\n" ));

//  OnResize( mBounds );

  return NS_OK;
}


NS_METHOD nsWidget::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth,
                           PRUint32 aHeight, PRBool aRepaint)
{
  Resize(aWidth,aHeight,aRepaint);
  Move(aX,aY);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsRect &aRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnResize.\n" ));

  // call the event callback

  if (mEventCallback)
  {
    nsSizeEvent event;
    InitEvent(event, NS_SIZE);
    event.windowSize = &aRect;
    event.eventStructType = NS_SIZE_EVENT;
    event.mWinWidth = 0;
    event.mWinHeight = 0;
    event.point.x = 0; //mWidget->allocation.x;
    event.point.y = 0; //mWidget->allocation.y;
    event.time = 0;

    if (mWidget)
    {
      PhArea_t *area;
      PtArg_t arg;
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      if( PtGetResources( mWidget, 1, &arg ) == 0 )
      {
        event.mWinWidth = area->size.w;
        event.mWinHeight = area->size.h;
        event.point.x = area->pos.x;
        event.point.y = area->pos.y;
      }
    }

    PRBool result = DispatchWindowEvent(&event);

    // XXX why does this always crash?  maybe we need to add 
    // a ref in the dispatch code?  check the windows
    // code for a reference
    //NS_RELEASE(event.widget);

    return result;
  }
return PR_FALSE;
}

//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnMove.\n" ));

  nsGUIEvent event;

  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;
  PRBool result = DispatchWindowEvent(&event);
  // NS_RELEASE(event.widget);
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


NS_METHOD nsWidget::GetBounds(nsRect &aRect)
{
  ///////////////////////////////////////////////////////////
  // NOTE: The nsWidget::GetBounds() function does not work
  // for nsWindow objects. nsWindow MUST provide its own
  // GetBounds method.

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetBounds.\n" ));

  if( mWidget )
  {
    PtArg_t arg[2];
//    PhDim_t  *dim;
    PhArea_t  *area;
    int      *border;

//    PtSetArg( &arg[0], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[0], Pt_ARG_AREA, &area, 0 );
    PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, &border, 0 );
    if( PtGetResources( mWidget, 2, arg ) == 0 )
    {
      mBounds.x = area->pos.x - *border;
      mBounds.y = area->pos.y - *border;
      mBounds.width = area->size.w + 2*(*border);
      mBounds.height = area->size.h + 2*(*border);
/*
      mBounds.width = dim->w + 2*(*border);
      mBounds.height = dim->h + 2*(*border);

      short x,y,ox,oy;

      PtGetAbsPosition( mWidget, &x, &y );
      PtGetAbsPosition( mClient, &ox, &oy );
      mBounds.x = x - ox;
      mBounds.y = y - oy;
*/
    }
    else
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetBounds - GetResources FAILED!\n" ));
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetBounds - mWidget is NULL!\n" ));
 
  aRect = mBounds;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetBounds - bounds = (%ld,%ld,%ld,%ld)\n", aRect.x, aRect.y, aRect.width, aRect.height ));

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
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_north_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_south:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_south_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_east:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_east_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_west:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_west_plus:
    curs = Ph_CURSOR_POINTER;
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

  return NS_OK;
}


NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate %p\n", this ));

  if( mWidget )
  {
    PtArg_t  arg;
    int      *flags;
    
    PtSetArg( &arg, Pt_ARG_FLAGS, &flags, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if( *flags & Pt_DAMAGED )
        return NS_OK;
    }

    PtDamageWidget( mWidget );

    // REVISIT - PtFlush may be unstable & cause crashes...
    if( aIsSynchronous )
      PtFlush();
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
    PtArg_t  arg;
    int      *flags;
    
    PtSetArg( &arg, Pt_ARG_FLAGS, &flags, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if( *flags & Pt_DAMAGED )
        return NS_OK;
    }

    PhRect_t extent;
    PhArea_t area;

    PtWidgetArea( mWidget, &area );

    extent.ul.x = aRect.x + area.pos.x;
    extent.ul.y = aRect.y + area.pos.y;
    extent.lr.x = extent.ul.x + aRect.width - 1;
    extent.lr.y = extent.ul.y + aRect.height - 1;
 
    PtDamageExtent( mWidget, &extent );

    // REVISIT - PtFlush may be unstable & cause crashes...
    if( aIsSynchronous )
      PtFlush();
  }
  else
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate(2) - mWidget is NULL!\n" ));

  return NS_OK;
}


NS_METHOD nsWidget::Update(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Update\n" ));

  PtFlush();

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
  return NS_OK;
}

NS_METHOD nsWidget::ShowMenuBar( PRBool aShow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ShowMenuBar aShow=<%d> - Not Implemented.\n",aShow));
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::IsMenuBarVisible( PRBool *aVisible )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::IsMenuBarVisible - Not Implemented \n"));
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget (%p).\n", this ));

  PtWidget_t *parentWidget = nsnull;

  BaseCreate(aParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  mParent = aParent;
  NS_IF_ADDREF( mParent );

  if( aNativeParent )
  {
    parentWidget = (PtWidget_t*)aNativeParent;
  }
  else if( aParent )
  {
    parentWidget = (PtWidget_t*) (aParent->GetNativeData(NS_NATIVE_WIDGET));
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget - No parent specified!\n" ));
  }
//  else if(aAppShell) {
//    nsNativeWidget shellWidget = aAppShell->GetNativeData(NS_NATIVE_SHELL);
//    if (shellWidget)
//      parentWidget = GTK_WIDGET(shellWidget);
//  }

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget - bounds=(%i,%i,%i,%i)\n", mBounds.x, mBounds.y, mBounds.width, mBounds.height ));

  CreateNative (parentWidget);

  DispatchStandardEvent(NS_CREATE);

//  InitCallbacks();

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
    NS_IF_ADDREF(event.widget);

    if (aPoint == nsnull) {     // use the point from the event
      // get the message position in client coordinates and in twips

//      if (ge != nsnull) {
//        event.point.x = PRInt32(ge->x);
//        event.point.y = PRInt32(ge->y);
//      } else { 
        event.point.x = 0;
        event.point.y = 0;
//      }  
    }    
    else {                      // use the point override if provided
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;
    }

    event.time = 0; //gdk_event_get_time((GdkEvent*)ge);
    event.message = aEventType;

//    mLastPoint.x = event.point.x;
//    mLastPoint.y = event.point.y;
}

PRBool nsWidget::ConvertStatus(nsEventStatus aStatus)
{
//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ConvertStatus - aStatus=<%d>\n",aStatus));

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
//  printf( ">>> nsWidget::DispatchWindowEvent\n" ); fflush( stdout );
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWidget::DispatchStandardEvent(PRUint32 aMsg)
{
//  printf( ">>> nsWidget::DispatchStandardEvent\n" ); fflush( stdout );

  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event, aMsg);

  PRBool result = DispatchWindowEvent(&event);
//  NS_IF_RELEASE(event.widget);
  return result;
}


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *event,
                                      nsEventStatus &aStatus)
{
//  printf( ">>> nsWidget::DispatchEvent\n" ); fflush(stdout);

  if( !( event ) || !( event->widget ))
  {
    printf( "  event or event->widget is NULL!\n" ); fflush(stdout);
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(event->widget);

  if( nsnull != mMenuListener )
  {
    if( NS_MENU_EVENT == event->eventStructType )
    {
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *event));
    }
  }

  aStatus = nsEventStatus_eIgnore;
  if( nsnull != mEventCallback )
  {
//printf ("kedl: real callback....\n");
    aStatus = (*mEventCallback)(event);
  }

  // Dispatch to event listener if event was not consumed
  if(( aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener))
  {
    aStatus = mEventListener->ProcessEvent(*event);
  }

  NS_RELEASE(event->widget);

  return NS_OK;
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

//printf ("kedl: in dispatchmouse %lu\n",aEvent);
  // call the event callback
  if (nsnull != mEventCallback) {
//	printf ("kedl: dispatchwindow!!!\n");
    result = DispatchWindowEvent( &event );

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

  for (int i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
    {
      return(nsKeycodes[i].vkCode);
    }
  }

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

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::nsConvertKey - Did not Find Key! - Not Implemented\n"));

  return(keysym & 0x00FF);
}

PRBool  nsWidget::DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchEvent Got a Key Event aPhEkyEvent->key_mods:<%x> aPhEkyEvent->key_flags:<%x> aPhEkyEvent->key_sym=<%x> aPhEkyEvent->key_caps=<%x>\n",aPhKeyEvent->key_mods, aPhKeyEvent->key_flags, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap));

  NS_ASSERTION(aPhKeyEvent, "nsWidget::DispatchKeyEvent a NULL PhKeyEvent was passed in");

  nsKeyEvent keyEvent;
  PRBool result = PR_FALSE;
   
  if (mEventCallback == NULL)
    return PR_TRUE;
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
	  
  //mIsShiftDown   = ( aPhEkyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
  //mIsControlDown = ( aPhEkyEvent->key_mods & Pk_KM_Ctrl ) ? PR_TRUE : PR_FALSE;
  //mIsAltDown     = ( aPhKeyEvent->key_mods & Pk_KM_Alt ) ? PR_TRUE : PR_FALSE;

  if (PkIsKeyDown(aPhKeyEvent->key_flags))
  {
	keyEvent.message = NS_KEY_DOWN;
  }
  else
  {
  	keyEvent.message = NS_KEY_UP;
  }

  keyEvent.widget = this;
  NS_ADDREF(keyEvent.widget);
  keyEvent.eventStructType = NS_KEY_EVENT;	  


  /* this should be keyCode  = nsConvertKey(aPhKeyEvent->key_cap); */
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::HandleEvent key_cap converted to NS_VK = <%x>\n", nsConvertKey(aPhKeyEvent->key_cap)));

  keyEvent.keyCode =  nsConvertKey(aPhKeyEvent->key_cap);
  keyEvent.charCode =  (aPhKeyEvent->key_cap & 0x00FF);  /* this should be UNICODE */
  keyEvent.time = 0; // event->timestamp;

  keyEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
  keyEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
  keyEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
  keyEvent.point.x = 0; // aPhKeyEvent->pos.x;
  keyEvent.point.y = 0; // aPhKeyEvent->pos.y;


  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::HandleEvent Shift=<%d> Control=<%d> Alt=<%d>\n", keyEvent.isShift, keyEvent.isControl, keyEvent.isAlt));
  //PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::HandleEvent keyCode=<%d> charCode=<%d>\n", keyEvent.keyCode, keyEvent.charCode));

  result = DispatchWindowEvent(&keyEvent);
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::HandleEvent result=<%d>\n", result));
  result = PR_TRUE;	/* HACK! */

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
