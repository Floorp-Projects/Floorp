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
int                nsWindow::mModalCount = -1;

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::nsWindow (%p)\n", this ));
  //NS_INIT_REFCNT();

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
  mFrameLeft       = 0;
  mFrameRight      = 0;
  mFrameTop        = 0;
  mFrameBottom     = 0;
  
  mIsDestroyingWindow = PR_FALSE;

  if (mLastLeaveWindow == this)
    mLastLeaveWindow = NULL;
  if (mLastDragMotionWindow == this)
    mLastDragMotionWindow = NULL;
	  
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

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::~nsWindow this=(%p)\n", this ));

  // make sure that we release the grab indicator here
  if (mGrabWindow == this) {
    mIsGrabbing = PR_FALSE;
    mGrabWindow = NULL;
  }
  // make sure that we unset the lastDragMotionWindow if
  // we are it.
  if (mLastDragMotionWindow == this) {
    mLastDragMotionWindow = NULL;
  }

#if 0
  // make sure to release our focus window
  if (mHasFocus == PR_TRUE) {
    focusWindow = NULL;
  }
#endif

  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.

  Destroy();

  if ( mMenuRegion )
  {
    PtDestroyWidget(mMenuRegion );  
    mMenuRegion = nsnull;
  }
  
  RemoveResizeWidget();
  RemoveDamagedWidget( mWidget );
}

PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WidgetToScreen - Not Implemented this=<%p> mBounds=<%d,%d,%d,%d> aOldRect=<%d,%d>\n", this, mBounds.x, mBounds.y, mBounds.width, mBounds.height,aOldRect.x,aOldRect.y ));
  PhPoint_t p1;

  if (mWidget && PtWidgetOffset(mWidget,&p1))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WidgetToScreen - Not Implemented mWidget=<%d,%d>\n", p1.x, p1.y));
  }

  if (mClientWidget && PtWidgetOffset(mClientWidget,&p1))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WidgetToScreen - Not Implemented mClientWidget=<%d,%d>\n", p1.x, p1.y));
  }  

/* I don't know which is "right"... I see no difference between these... */
#if 0
   aNewRect.x = p1.x + aOldRect.x;
   aNewRect.y = p1.y + aOldRect.y;
#endif

#if 0
   aNewRect.x = aOldRect.x;
   aNewRect.y = aOldRect.y;
#endif

#if 1
   aNewRect.x = mBounds.x + aOldRect.x;
   aNewRect.y = mBounds.y + aOldRect.y;
#endif

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
}

// this function will walk the list of children and destroy them.
// the reason why this is here is that because of the superwin code
// it's very likely that we will never get notification of the
// the destruction of the child windows.  so, we have to beat the
// layout engine to the punch.  CB 

void nsWindow::DestroyNativeChildren(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::DestroyNativeChildren this=(%p)\n"));

  nsCOMPtr <nsIEnumerator> children (getter_AddRefs(GetChildren()));
  
  if (children) {
    children->First();
    do {
      nsISupports *child;
      if (NS_SUCCEEDED(children->CurrentItem(&child))) {
        nsIWidget *childWindow = NS_STATIC_CAST(nsIWidget *, child);
        NS_RELEASE(child);
        childWindow->Destroy();
      }
    } while(NS_SUCCEEDED(children->Next()));
  }
}

NS_IMETHODIMP nsWindow::Update(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Update this=<%p> mUpdateArea=<%p> mUpdateArea->IsEmpty()=<%d>\n", this, mUpdateArea, mUpdateArea->IsEmpty() ));

#if 1
  return nsWidget::Update();
#else
  printf("nsWindow::Update this=<%p> mUpdateArea=<%p> mUpdateArea->IsEmpty()=<%d>\n", this, mUpdateArea, mUpdateArea->IsEmpty());

//  if (mIsUpdating)
//    UnqueueDraw();

   if (mWidget == NULL)
    {
      printf("nsWindow::Update mWidget is NULL\n");
      NS_ASSERTION(0, "nsWidget::UpdateWidgetDamaged mWidget is NULL");
      return;
	}

    if( !PtWidgetIsRealized( mWidget ))	
    {
      //NS_ASSERTION(0, "nsWidget::UpdateWidgetDamaged skipping update because widget is not Realized");
      printf("nsWindow::Update mWidget is not realized\n");
      return;
    }
	
  if ((mUpdateArea) && (!mUpdateArea->IsEmpty()))
  {
#if 1

      PhTile_t * nativeRegion = nsnull;

	  mUpdateArea->GetNativeRegion((void *&) nativeRegion );

      if (nativeRegion)
	  {
        printf("nsWindow::Update mWidget before RawDrawFunc\n");
        RawDrawFunc(mWidget, PhCopyTiles(nativeRegion));
        printf("nsWindow::Update mWidget after RawDrawFunc\n");
      }
      else
	  {
        printf("nsWindow::Update mWidget has NULL nativeRegion\n");
	  }
#else
    nsRegionRectSet *regionRectSet = nsnull;

    if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
      return NS_ERROR_FAILURE;

    PRUint32 len;
    PRUint32 i;

    len = regionRectSet->mRectsLen;

    for (i=0;i<len;++i)
    {
      nsRegionRect *r = &(regionRectSet->mRects[i]);
      //DoPaint (r->x, r->y, r->width, r->height, mUpdateArea);
    }
    mUpdateArea->FreeRects(regionRectSet);
#endif

    mUpdateArea->SetTo(0, 0, 0, 0);
    return NS_OK;
  }
 
  // While I'd think you should NS_RELEASE(aPaintEvent.widget) here,
  // if you do, it is a NULL pointer.  Not sure where it is getting
  // released.
  return NS_OK;
#endif
}

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener,
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CaptureRollupEvents this=<%p> doCapture=<%i> aConsumeRollupEvent=<%d>\n", this, aDoCapture, aConsumeRollupEvent));

  PtWidget_t *grabWidget;

  grabWidget = mWidget;

  if (aDoCapture)
  {
    printf("grabbing widget\n");
    
	/* Create a pointer region */
     mIsGrabbing = PR_TRUE;
     mGrabWindow = this;

    if ((mMenuRegion) && (!PtWidgetIsRealized(mMenuRegion)))
	{
  	  PtRealizeWidget(mMenuRegion);
	}
  }
  else
  {
    printf("ungrabbing widget\n");

    // make sure that the grab window is marked as released
    if (mGrabWindow == this)
	{
      mGrabWindow = NULL;
    }
    mIsGrabbing = PR_FALSE;

	/* Release the pointer region */
	if ((mMenuRegion) && (PtWidgetIsRealized(mMenuRegion)))
	{
	  PtUnrealizeWidget(mMenuRegion);
	}
  }
  
  if (aDoCapture) {
    //    gtk_grab_add(mWidget);
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupConsumeRollupEvent = PR_TRUE;
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  } else {
    //    gtk_grab_remove(mWidget);
    NS_IF_RELEASE(gRollupListener);
    //gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 1 this=<%p> IsSynch=<%d> mBounds=(%d,%d,%d,%d)\n", this, aIsSynchronous,
    mBounds.x, mBounds.y, mBounds.width, mBounds.height));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 1 - mWidget is NULL!\n" ));
    return NS_OK; // mWidget will be null during printing. 
  }
  
  if (!PtWidgetIsRealized(mWidget))
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 1 - mWidget is not realized\n"));
      return NS_OK;
  }

  /* Damage has to be relative Widget coords */
//  mUpdateArea->SetTo( mBounds.x, mBounds.y, mBounds.width, mBounds.height );
  mUpdateArea->SetTo( 0,0, mBounds.width, mBounds.height );

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

NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 2 this=<%p> IsSynch=<%d> mBounds=(%d,%d,%d,%d) aRect=(%d,%d,%d,%d)\n", this, aIsSynchronous,
    mBounds.x, mBounds.y, mBounds.width, mBounds.height,aRect.x, aRect.y, aRect.width, aRect.height));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 2 - mWidget is NULL!\n" ));
    return NS_OK; // mWidget will be null during printing. 
  }
  
  if (!PtWidgetIsRealized(mWidget))
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 2 - mWidget is not realized\n"));
      return NS_OK;
  }
  
  mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 3 this=<%p> IsSynch=<%d> mBounds=(%d,%d,%d,%d)\n", this, aIsSynchronous,
    mBounds.x, mBounds.y, mBounds.width, mBounds.height));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 3 - mWidget is NULL!\n" ));
    return NS_OK; // mWidget will be null during printing. 
  }
  
  if (!PtWidgetIsRealized(mWidget))
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 3 - mWidget is not realized\n"));
      return NS_OK;
  }
  
#if defined(DEBUG)
{
  PRUint32 len;
  PRUint32 i;
  nsRegionRectSet *regionRectSet = nsnull;

  aRegion->GetRects(&regionRectSet);
  len = regionRectSet->mRectsLen;
  for (i=0;i<len;++i)
  {
    nsRegionRect *r = &(regionRectSet->mRects[i]);
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Invalidate 3 rect %d is <%d,%d,%d,%d>\n", i, r->x, r->y, r->width, r->height));
  }

  aRegion->FreeRects(regionRectSet);
}
#endif


  mUpdateArea->Union(*aRegion);

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

NS_IMETHODIMP nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetBackgroundColor - Not Implemented this=<%p> color(RGB)=(%d,%d,%d)\n", this, NS_GET_R(aColor),NS_GET_G(aColor),NS_GET_B(aColor)));

  //nsBaseWidget::SetBackgroundColor(aColor);

  return nsWidget::SetBackgroundColor(aColor);
}


NS_IMETHODIMP nsWindow::SetFocus(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetFocus - Not Implemented this=<%p>\n", this));

#if 1
 return nsWidget::SetFocus();
#else

  GtkWidget *top_mozarea = GetMozArea();
  
  if (top_mozarea)
  {
    if (!GTK_WIDGET_HAS_FOCUS(top_mozarea))
      gtk_widget_grab_focus(top_mozarea);
  }
  
  // check to see if we need to send a focus out event for the old window
  if (focusWindow)
  {
    if(mIMEEnable == PR_FALSE)
    {
#ifdef NOISY_XIM
      printf("  IME is not usable on this window\n");
#endif
    }
    if (mIC)
    {
      GdkWindow *gdkWindow = (GdkWindow*) focusWindow->GetNativeData(NS_NATIVE_WINDOW);
      focusWindow->KillICSpotTimer();
      if (gdkWindow)
      {
        gdk_im_end();
      }
      else
      {
#ifdef NOISY_XIM
        printf("gdkWindow is not usable\n");
#endif
      }
    }
    else
    {
#ifdef NOISY_XIM
      printf("mIC isn't created yet\n");
#endif
    }

    // let the current window loose its focus
    focusWindow->LooseFocus();
  }

  // set the focus window to this window

  focusWindow = this;
  mHasFocus = PR_TRUE;

  // don't recurse
  if (mBlockFocusEvents)
  {
    return NS_OK;
  }
  
  mBlockFocusEvents = PR_TRUE;
  
  nsGUIEvent event;
  
  event.message = NS_GOTFOCUS;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  AddRef();
  
  DispatchFocus(event);
  
  Release();

  mBlockFocusEvents = PR_FALSE;

  if(mIMEEnable == PR_FALSE)
  {
#ifdef NOISY_XIM
    printf("  IME is not usable on this window\n");
#endif
    return NS_OK;
  }

  if (!mIC)
    GetXIC();

  if (mIC)
  {
    GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
    if (gdkWindow)
    {
      gdk_im_begin ((GdkIC*)mIC, gdkWindow);
      UpdateICSpot();
      PrimeICSpotTimer();
    }
    else
    {
#ifdef NOISY_XIM
      printf("gdkWindow is not usable\n");
#endif
    }
  }
  else
  {
#ifdef NOISY_XIM
    printf("mIC can't created yet\n");
#endif
  }

  return NS_OK;
#endif
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

#if 0
    /* Create Pop-ups as a PtWindow, this has many problems and is */
	/* only being kept here for a little longer to make sure there are */
	/* no problems in the PtRegion code. */

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
#else
    /* Create Pop-up Menus as a PtRegion */

    if (!parentWidget)
      PtSetParentWidget( nsnull );
		
    if( mWindowType == eWindowType_popup )
    {
	  int	fields = Ph_REGION_PARENT|Ph_REGION_HANDLE|
					 Ph_REGION_FLAGS|Ph_REGION_ORIGIN|
					 Ph_REGION_EV_SENSE|Ph_REGION_EV_OPAQUE
					 |Ph_REGION_RECT;

	  int   sense =  Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE | Ph_EV_BUT_REPEAT;

      PtRawCallback_t callback;
        callback.event_mask = sense;
        callback.event_f = (void *) MenuRegionCallback;
        callback.data = (ApInfo_t *) this;
	  
      PhRid_t    rid;
      PhRegion_t region;
	  PhRect_t   rect;	  

	    PhQueryRids( 0, 0, 0, Ph_INPUTGROUP_REGION | Ph_QUERY_IG_POINTER, 0, 0, 0, &rid, 1);
  	    PhRegionQuery( rid, &region, &rect, NULL, 0 );

      PhArea_t area;
        area.pos = region.origin;
        area.size.w = rect.lr.x - rect.ul.x + 1;
        area.size.h = rect.lr.y - rect.ul.y + 1;

      PtArg_t   args[20];  
      int arg_count2 = 0;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative parentWidget = <%p>\n", parentWidget));


        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_PARENT,   Ph_ROOT_RID, 0 );
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_FIELDS,   fields, fields );
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_FLAGS,    Ph_FORCE_FRONT | Ph_FORCE_BOUNDARY, Ph_FORCE_FRONT | Ph_FORCE_BOUNDARY);
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_SENSE,    sense, sense );
        PtSetArg( &args[arg_count2++], Pt_ARG_REGION_OPAQUE,   Ph_EV_BOUNDARY, Ph_EV_BOUNDARY );
        PtSetArg( &args[arg_count2++], Pt_ARG_AREA,            &area, 0 );
        PtSetArg( &args[arg_count2++], Pt_ARG_FILL_COLOR,      Pg_TRANSPARENT, 0 );
        PtSetArg( &args[arg_count2++], Pt_ARG_RAW_CALLBACKS,   &callback, 1 );
        //PtSetArg( &args[arg_count2++], Pt_ARG_CURSOR_TYPE,     cursor_type, 1 );
        //PtSetArg( &args[arg_count2++], Pt_ARG_CURSOR_COLOR,    cursor_color, 1 );
        mMenuRegion = PtCreateWidget( PtRegion, parentWidget, arg_count2, args );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative mMenuRegion = <%p>\n", mMenuRegion));

        //arg_count = 0;
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_PARENT,   mMenuRegion->rid, 0 );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FIELDS,   fields, fields );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FLAGS,    Ph_FORCE_FRONT, Ph_FORCE_FRONT );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_SENSE,    sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_BUT_RELEASE, sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_BUT_RELEASE );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_OPAQUE,   sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_BUT_RELEASE|Ph_EV_DRAW, sense |Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_BUT_RELEASE|Ph_EV_DRAW);
//HACK        PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR,      Pg_TRANSPARENT, 0 );
        mWidget = PtCreateWidget( PtRegion, parentWidget, arg_count, arg);

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative PtRegion = <%p>\n", mWidget));

    }
	else
	{
      mWidget = PtCreateWidget( PtWindow, parentWidget, arg_count, arg );
	}
	
#endif
    
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
        Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY|Ph_EV_DRAG
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
//      | Ph_EV_EXPOSE
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

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy)
{
  NS_WARNING("nsWindow::ScrollRect Not Implemented\n");
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


NS_IMETHODIMP nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  PRBool nNeedToShow = PR_FALSE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize this=<%p> w/h=(%i,%i) Repaint=<%i> mWindowType=<%d>\n", this, aWidth, aHeight, aRepaint, mWindowType ));
  //printf("nsWindow::Resize  (%p) to %d %d\n", this, aWidth, aHeight);

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
      if (mShown)
      {
        printf("nsWindow::Resize Forcing small toplevel window window to Hide\n");
		Show(PR_FALSE);
      }
    }
    else
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;
      printf("nsWindow::Resize Forcing small non-toplevel window to Hide\n");
	  Show(PR_FALSE);
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

  if( mWidget )
  {
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
#if 0
    if (mShown)
      Invalidate( aRepaint );
#endif

  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize - mWidget is NULL!\n" ));
  }

  if (mIsToplevel || mListenForResizes)
  {
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

  if (nNeedToShow)
  {
    printf("nsWindow::Resize Forcing window to Show\n");
    Show(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                               PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX,aY);

  // resize can cause a show to happen, so do this last
  Resize(aWidth,aHeight,aRepaint);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


int nsWindow::WindowCloseHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WindowCloseHandler this=(%p)\n", data));

  nsWindow * pWin = (nsWindow*) data;

  /* I had to add this check for to not do this for pop-up but I wonder */
  /* is this is valid at all!   I really don't think so from looking some more... */  
  /* 11/15/99 */
  if ( pWin->mWindowType != eWindowType_popup )
  {
    if ( pWin )
	{
      NS_ADDREF(pWin);
      pWin->mIsDestroying = PR_TRUE;
      pWin->Destroy();
      NS_RELEASE(pWin);
    }
  }
  
  return Pt_CONTINUE;
}

NS_METHOD nsWindow::Show(PRBool bState)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Show this=<%p> State=<%d> mRefCnt=<%d> mWidget=<%p>\n", this, bState, mRefCnt, mWidget));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Show - mWidget is NULL!\n" ));
    return NS_OK; // Will be null durring printing
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Show this=<%p> IsRealized=<%d>\n", this, PtWidgetIsRealized( mWidget )));

  EnableDamage(mMenuRegion, PR_FALSE);
  
  if (bState)
  {
	if ((mMenuRegion) && (!PtWidgetIsRealized(mMenuRegion)))
	  PtRealizeWidget(mMenuRegion);
  }
  else
  {
	if ((mMenuRegion) && (PtWidgetIsRealized(mMenuRegion)))
	  PtUnrealizeWidget(mMenuRegion);  
  }
  
  EnableDamage(mMenuRegion, PR_TRUE);

#if 0
  // don't show if we are too small
  if (mIsTooSmall)
    return NS_OK;
#endif

  return nsWidget::Show(bState);
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
  nsWindow  * pWin = (nsWindow*) GetInstance( pWidget );
  nsresult    result;
  PhTile_t  * dmg;
  nsIRegion * ClipRegion;
  PRBool      aClipState;
  nsPaintEvent pev;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc for mWidget=<%p> this=<%p> this->mContext=<%p> pWin->mParent=<%p> mClientWidget=<%p>\n", 
    pWidget, pWin, pWin->mContext, pWin->mParent, pWin->mClientWidget));

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

  /* Why did I think this was important? */
  if ( !pWin->mContext )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  pWin->mContext is NULL!\n"));
    NS_ASSERTION(pWin->mContext, "nsWindow::RawDrawFunc  pWin->mContext is NULL!");
    //return;
  }

  // This prevents redraws while any window is resizing, ie there are
  //   windows in the resize queue
  if ( pWin->mIsResizing )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  aborted due to hold-off!\n"));
    //printf("nsWindow::RawDrawFunc aborted due to Resize holdoff\n");
    return;
  }

  if ( pWin->mEventCallback )
  {
    PhRect_t   rect;
    PhArea_t   area = {{0,0},{0,0}};
    PhPoint_t  offset = {0,0};
    nsRect     nsDmg;

    // Ok...  I ~think~ the damage rect is in window coordinates and is not neccessarily clipped to
    // the widgets canvas. Mozilla wants the paint coords relative to the parent widget, not the window.
    PtWidgetArea( pWidget, &area );
    PtWidgetOffset( pWidget, &offset );

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc area=<%d,%d,%d,%d>\n", area.pos.x, area.pos.y, area.size.w, area.size.h));
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc offset=<%d,%d>\n", offset.x, offset.y));
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc mBounds=<%d,%d,%d,%d>\n", pWin->mBounds.x, pWin->mBounds.y, pWin->mBounds.width, pWin->mBounds.height));
//    offset.x += area.pos.x;  
//    offset.y += area.pos.y;  
//    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc offset+area =<%d,%d>\n", offset.x, offset.y));

    /* Build a List of Tiles that might be in front of me.... */
    PhTile_t *clip_tiles=pWin->GetWindowClipping(offset);

#if 0
    /* Build a clipping regions */
    if (clip_tiles)
	{
      ClipRegion = pWin->GetRegion();
      PhTile_t *tip = clip_tiles;
        while(tip)
		{
          ClipRegion->Union(tip->rect.ul.x, tip->rect.ul.y,
		                    (tip->rect.lr.x - tip->rect.ul.x + 1),
			                (tip->rect.lr.y - tip->rect.ul.y + 1)  );
          tip=tip->next;		  
		}
    }
#endif

#if 1
    /* Intersect the Damage tile list w/ the clipped out list and */
	/* see whats left! */
	PhTile_t *new_damage;

  // The upper left X seems to be off by 1 but this didn't help...
  // This appears to be a PHOTON BUG....
  if (damage->next->rect.ul.x != 0)
  {
    damage->next->rect.ul.x--;
  }

    /* This could be leaking some memory */
	new_damage = PhClipTilings(PhCopyTiles(damage->next), PhCopyTiles(clip_tiles), NULL);

    /* Doing this breaks text selection but speeds things up */
    new_damage = PhCoalesceTiles( PhMergeTiles (PhSortTiles( new_damage )));

    if (new_damage == nsnull)
    { 
      /* Nothing to Draw */
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Nothing to Draw damage is NULL\n"));
      return;	
    }

#if 1
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = new_damage;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc New Damage Tiles List <%p>:\n", new_damage));
  while (top)
  {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  }
}
#endif
#endif


#if 0
  /* Create a Bounding Damage Tile */
  PhTile_t   * BoundingDmg;
  BoundingDmg = PhGetTile();
  BoundingDmg->next = nsnull;
  BoundingDmg->rect = new_damage->rect;
  
  PhTile_t *top = new_damage->next;
  while(top)
  {
    PhRect_t tmp_rect = top->rect;
    BoundingDmg->rect.ul.x = PR_MIN(BoundingDmg->rect.ul.x, tmp_rect.ul.x);
    BoundingDmg->rect.ul.y = PR_MIN(BoundingDmg->rect.ul.y, tmp_rect.ul.y);
    BoundingDmg->rect.lr.x = PR_MAX(BoundingDmg->rect.lr.x, tmp_rect.lr.x);
	BoundingDmg->rect.lr.y = PR_MAX(BoundingDmg->rect.lr.y, tmp_rect.lr.y);      
    top=top->next;  
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Bounding Damage=<%d,%d,%d,%d> next=<%p>\n",
   BoundingDmg->rect.ul.x,BoundingDmg->rect.ul.y,BoundingDmg->rect.lr.x,BoundingDmg->rect.lr.y));

#endif


    pWin->InitEvent(pev, NS_PAINT);
    pev.eventStructType = NS_PAINT_EVENT;
    pev.renderingContext = nsnull;
    pev.renderingContext = pWin->GetRenderingContext();
    if (pev.renderingContext == NULL)
	{
      printf("nsWindow::RawDrawFunc error getting RenderingContext\n");
      abort();	
	}

    /* Choose one... Usually the first rect is a bounding box and can */
	/* be ignored, but my special doPaint forgets the bounding box so .. */
	/* this is a quick hack.. */
    //dmg = damage->next;
	//dmg = damage;		/* if using my DoPaint that call RawDrawFunc directly */
    dmg = new_damage;
    //dmg = BoundingDmg;
		
    while(dmg)
	{
      /* Copy the Damage Rect */
      rect.ul.x = dmg->rect.ul.x;
      rect.ul.y = dmg->rect.ul.y;
      rect.lr.x = dmg->rect.lr.x;
      rect.lr.y = dmg->rect.lr.y;
  
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage rect = <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y));

#if 1
      /* Do I need to Translate it?? If so by what? offset? offset+area?*/
      //PtTranslateRect(&rect, &offset);
      PtDeTranslateRect(&rect, &offset);
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage rect after translate = <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y));
#endif

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

    /* Setup Paint Event */
//    nsPaintEvent pev;
    pWin->InitEvent(pev, NS_PAINT);
    pev.eventStructType = NS_PAINT_EVENT;
	pev.point.x = nsDmg.x;
	pev.point.y = nsDmg.y;
    pev.rect = new nsRect(nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height);
	
//      pev.renderingContext = nsnull;
//      pev.renderingContext = pWin->GetRenderingContext();

      if (pev.renderingContext)
      {
        ClipRegion = pWin->GetRegion();
        ClipRegion->SetTo(nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height);
        pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(ClipRegion)),
                                            nsClipCombine_kReplace, aClipState);
		
         /* Not sure WHAT this sould be, probably nsDmg rect... */
         // pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(ClipRegion)),
         // pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(pWin->mUpdateArea)),
         //                                     nsClipCombine_kReplace, aClipState);

         result = pWin->DispatchWindowEvent(&pev);

//	    NS_RELEASE(pev.renderingContext);
      }

      /* Move to the next Damage Tile */
      dmg = dmg->next;
	}

    NS_RELEASE(pev.renderingContext);

  }
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  End of RawDrawFunc\n"));
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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion this=<%p> btiles=<%p> ctiles=<%p> mWidget=<%p>\n", this, btiles, ctiles, mWidget));

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

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion 2\n"));

        (*btiles)->rect.ul.x = rect.x;
        (*btiles)->rect.ul.y = rect.y;
        (*btiles)->rect.lr.x = rect.x + rect.width - 1;
        (*btiles)->rect.lr.y = rect.y + rect.height - 1;

        (*btiles)->next = nsnull;

        *ctiles = last = nsnull;

        for( w=PtWidgetBrotherInFront( mWidget ); w; w=PtWidgetBrotherInFront( w )) 
        { 
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion 3\n"));

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
      else
      {
	    NS_ASSERTION(0,"nsWindow::GetSiblingClippedRegion Pt_ARG_AREA failed!");
	    abort();
	  }
    }
    else
	{
	  NS_ASSERTION(0,"nsWindow::GetSiblingClippedRegion PhGetTile failed!");
	  abort();
	}
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion the end res=<%d>\n", res));

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
  PhTile_t   *new_damage = nsnull;
  
  clip_tiles = last = nsnull;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  this=<%p> damage=<%p> offset=(%d,%d) mClipChildren=<%d> mClipSiblings=<%d>\n", this, damage, offset.x, offset.y, mClipChildren, mClipSiblings));

#if 0
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = damage;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping 1 Damage Tiles List:\n"));
  do {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  } while (top);
}
#endif

#if 0
  tile = PhGetTile();
  if( tile )
  {
    PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
    PtGetResources( aWidget, 1, &arg );

    tile->rect.ul.x = offset.x;
    tile->rect.ul.y = offset.y;
    tile->rect.lr.x = area->pos.x + area->size.w-1;
    tile->rect.lr.y = area->pos.y + area->size.h-1;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  Adding Initial widget=<%d,%d,%d,%d>\n", tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y ));

    tile->next = NULL;

	damage = PhIntersectTilings(PhCopyTiles(damage), PhCopyTiles(tile), NULL);
  }
#endif

#if 0
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = damage;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping 2 Damage Tiles List: top=<%p>\n", top));
  while(top)
  {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  };
}
#endif
  			  
  /* No damage left, nothing to draw */
  if (damage == nsnull)
    return NS_ERROR_FAILURE;

  if ( mClipChildren )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  clipping children...\n"));

    for( w=PtWidgetChildFront( aWidget ); w; w=PtWidgetBrotherBehind( w )) 
    { 
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( w, 1, &arg );

      long flags = PtWidgetFlags(w);

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  clipping children w=<%p> area=<%d,%d,%d,%d> flags=<%ld>\n", w, area->pos.x, area->pos.y, area->size.w, area->size.h, flags));

	  if (flags & Pt_OPAQUE)
	  {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  widget w=<%p> is opaque IsRealized=<%d>\n", w, PtWidgetIsRealized(w) ));

        if ( PtWidgetIsRealized( w ))
        {
 
          tile = PhGetTile();
          if( tile )
          {
// HACK tried to offset the children...
            tile->rect.ul.x = area->pos.x; // + offset.x;
            tile->rect.ul.y = area->pos.y; // + offset.y;
            tile->rect.lr.x = area->pos.x + area->size.w - 1;
            tile->rect.lr.y = area->pos.y + area->size.h - 1;
            tile->next = NULL;
            if( !clip_tiles )
              clip_tiles = tile;
            if( last )
              last->next = tile;
            last = tile;
          }
          else
		  {
		   printf("Error failed to GetTile\n");
		   abort();
		  }
        }
   	    else
        {
	      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping Widget isn't realized %p\n", w));
        }
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

#if 1
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = damage;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping 3 Damage Tiles List:\n"));
  while(top)
  {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping  photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  }
}
#endif

  /* Skip the first bounding tile if possible */
  if (damage->next)
    dmg = PhCopyTiles( damage->next );
  else
    dmg = PhCopyTiles( damage );

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping after PhCopyTiles\n"));

  PhDeTranslateTiles( dmg, &offset );

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping after PhDeTranslateTiles\n"));

  if( clip_tiles )
  {
    // We have chiluns... now clip'em
    dmg = PhClipTilings( dmg, clip_tiles, nsnull );

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping after  PhClipTilings\n"));

    PhFreeTiles( clip_tiles );
  }  

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping after children clipping dmg=<%p>\n", dmg ));

  if( dmg )
  {
    rects = PhTilesToRects( dmg, &rect_count );
#if 1
#ifdef DEBUG
  /* Debug print out the tile list! */
  int index;
  PhRect_t *tip = rects;
  for(index=0; index < rect_count; index++)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetWindowClipping %d rect = <%d,%d,%d,%d>\n",index,
	  tip->ul.x, tip->ul.y, tip->lr.x, tip->lr.y));
    tip++;
  }
#endif  

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

PhTile_t *nsWindow::GetWindowClipping(PhPoint_t &offset)
{
  PhTile_t   *tile, *last, *clip_tiles;
  PtWidget_t *w;
  PhArea_t   *area;
  PtArg_t    arg;
  PtWidget_t *aWidget = GetNativeData(NS_NATIVE_WIDGET);

  PhPoint_t w_offset;
  PhArea_t w_area;
  PhDim_t w_dim;
  short abs_pos_x, abs_pos_y;
	    
  clip_tiles = last = nsnull;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  this=<%p> offset=<%d,%d> mClipChildren=<%d> mClipSiblings=<%d>\n", this, offset.x, offset.y, mClipChildren, mClipSiblings));

/* This this routine on and off */
#if 1

//if ( mClipChildren )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping    clipping children...\n"));

    for( w=PtWidgetChildFront( aWidget ); w; w=PtWidgetBrotherBehind( w )) 
    { 
      long flags = PtWidgetFlags(w);

      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( w, 1, &arg );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  clipping children w=<%p> area=<%d,%d,%d,%d> flags=<%ld>\n", w, area->pos.x, area->pos.y, area->size.w, area->size.h, flags));

      PtWidgetArea(w, &w_area);
      PtWidgetDim(w, &w_dim);

	  PtWidgetOffset(w, &w_offset);
      PtGetAbsPosition(w, &abs_pos_x, &abs_pos_y);

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  clipping children w=<%p> offset=<%d,%d>  Abs Pos=<%d,%d> w_area=<%d,%d,%d,%d>\n", w, w_offset.x, w_offset.y, abs_pos_x, abs_pos_y, w_area.pos.x, w_area.pos.y, w_area.size.w, w_area.size.h));

	  if (flags & Pt_OPAQUE)
	  {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  widget w=<%p> is opaque IsRealized=<%d>\n", w, PtWidgetIsRealized(w)));
        if ( PtWidgetIsRealized( w ))
        {
 
          tile = PhGetTile();
          if( tile )
          {

#if 0
            tile->rect.ul.x = abs_pos_x;
            tile->rect.ul.y = abs_pos_y;
            tile->rect.lr.x = abs_pos_x + w_dim.w - 1;
            tile->rect.lr.y = abs_pos_y + w_dim.h - 1;
#endif

#if 1
            tile->rect.ul.x = area->pos.x;
            tile->rect.ul.y = area->pos.y;
            tile->rect.lr.x = area->pos.x + area->size.w - 1;
            tile->rect.lr.y = area->pos.y + area->size.h - 1;
#endif
            tile->next = NULL;
            if( !clip_tiles )
              clip_tiles = tile;
            if( last )
              last->next = tile;
            last = tile;
          }
          else
		  {
		   printf("Error failed to GetTile\n");
		   abort();
		  }
        }
   	    else
        {
	      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping Widget isn't realized %p\n", w));
        }
      }
    }
  }

  if ( mClipSiblings )
  {
    PtWidget_t *node = aWidget;
    PhPoint_t  origin = { 0, 0 };

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping    clipping siblings...\n"));
    
    while( node )
    {
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( node, 1, &arg );
      origin.x += area->pos.x;
      origin.y += area->pos.y;
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping parent: %p: %d %d %d %d\n",node,area->pos.x,area->pos.y,area->size.w,area->size.h));

      for( w=PtWidgetBrotherInFront( node ); w; w=PtWidgetBrotherInFront( w ))
      {
        PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
        PtGetResources( w, 1, &arg );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping BrotherInFront: %p: area=<%d,%d,%d,%d> IsRealized=<%d>\n",w,area->pos.x,area->pos.y,area->size.w,area->size.h, PtWidgetIsRealized(w)));

        if( PtWidgetIsRealized( w ))
        {
          //PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping sib: %p: %d %d %d %d\n",w,area->pos.x,area->pos.y,area->size.w,area->size.h));
          tile = PhGetTile();
          if( tile )
          {
#if 1
            tile->rect.ul.x = area->pos.x;
            tile->rect.ul.y = area->pos.y;
            tile->rect.lr.x = area->pos.x + area->size.w - 1;
            tile->rect.lr.y = area->pos.y + area->size.h - 1;
#else
            tile->rect.ul.x = area->pos.x - origin.x;
            tile->rect.ul.y = area->pos.y - origin.y;
            tile->rect.lr.x = tile->rect.ul.x + area->size.w - 1;
            tile->rect.lr.y = tile->rect.ul.y + area->size.h - 1;
#endif
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

#if 1
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = clip_tiles;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping Child+Sibling Tiles List:\n"));
  while(top)
  {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping tiles to clip %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  }
}
#endif

#endif

  return clip_tiles;
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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetClientBounds the end bounds=(%ld,%ld,%ld,%ld)\n", aRect.x, aRect.y, aRect.width, aRect.height ));

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// grab mouse events for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureMouse(PRBool aCapture)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CaptureMouse this=<%p> aCapture=<%d>\n",this, aCapture ));

  NS_WARNING("nsWindow::CaptureMouse Not Implemented\n");
  
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

   *aForWindow = PR_TRUE;
    return NS_OK;
}

int nsWindow::MenuRegionCallback( PtWidget_t *widget, ApInfo_t *apinfo, PtCallbackInfo_t *cbinfo)
{
  nsWindow      *pWin = (nsWindow*) apinfo;
  nsWindow      *pWin2 = (nsWindow*) GetInstance(widget);
  nsresult      result;
  PhEvent_t	    *event = cbinfo->event;

  /* eliminate 'unreferenced' warnings */
  widget = widget, apinfo = apinfo, cbinfo = cbinfo;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback this=<%p> widget=<%p> apinfo=<%p>\n", pWin, widget, apinfo));
  printf("nsWindow::MenuRegionCallback this=<%p> widget=<%p> apinfo=<%p> pWin2=<%p>\n", pWin, widget, apinfo, pWin2);
//  printf("nsWindow::MenuRegionCallback cbinfo->reason=<%d>\n", cbinfo->reason);
//  printf("nsWindow::MenuRegionCallback cbinfo->subtype=<%d>\n", cbinfo->reason_subtype);
//  printf("nsWindow::MenuRegionCallback cbinfo->cbdata=<%d>\n", cbinfo->cbdata);
//  printf("nsWindow::MenuRegionCallback event->type=<%d>\n", event->type);

  if ( (event->type == Ph_EV_BUT_PRESS) || (event->type == Ph_EV_KEY) )
  {
    //printf("nsWindow::MenuRegionCallback event is Ph_EV_BUT_PRESS | Ph_EV_KEY\n");
    //printf("nsWindow::MenuRegionCallback pWin->gRollupWidget=<%p> pWin->gRollupListener=<%p>\n", pWin->gRollupWidget, pWin->gRollupListener);

    if (pWin->gRollupWidget && pWin->gRollupListener)
    {
      /* Close the Window */
      //pWin->gRollupListener->Rollup();
    }
  }

  return (Pt_CONTINUE);
}

NS_METHOD nsWindow::GetAttention()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetAttention this=(%p)\n", this ));
  
  NS_WARNING("nsWindow::GetAttention Called...\n");
  
  if ((mWidget) && (mWidget->parent) && (!PtIsFocused(mWidget)))
  {
    /* Raise to the top */
    PtWidgetToFront(mWidget);  
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetModal(PRBool aModal)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetModal this=<%p> mModalCount=<%d> mWidget=<%p>\n", this, mModalCount, mWidget));
  nsresult res = NS_ERROR_FAILURE;
  
  if (!mWidget)
	return NS_ERROR_FAILURE;

  PtWidget_t *toplevel = PtFindDisjoint(mWidget);
  if (!toplevel)
	return NS_ERROR_FAILURE;
	
  if (aModal)
  {
    if (mModalCount != -1)
	{
	  NS_ASSERTION(0, "nsWindow::SetModalAlready have a Modal Window, Can't have 2");	
	}
	else
    {
	  mModalCount = PtModalStart();
	  res = NS_OK;
    }

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetModal after call to PtModalStart mModalCount=<%d>\n", mModalCount));
  }
  else
  {
	PtModalEnd( mModalCount );
	mModalCount = -1;  
    res = NS_OK;
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetModal after call to PtModalEnd mModalCount=<%d>\n", mModalCount));
  }

  return res;
}


nsIRegion *nsWindow::GetRegion()
{
  nsIRegion *region = NULL;
  nsresult res;

  static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

  res = nsComponentManager::CreateInstance(kRegionCID, nsnull,
                                           NS_GET_IID(nsIRegion),
                                           (void **)&region);

  if (NS_OK == res)
    region->Init();

  NS_ASSERTION(NULL != region, "Null region context");
  
  return region;  
}
