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


//#undef DEBUG

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

/* Photon Event Timestamp */
unsigned long		IgnoreEvent = 0;

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

  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.
  Destroy();

  if ( mMenuRegion )
  {
    if (mMenuRegion ==  gRollupScreenRegion)
       gRollupScreenRegion = nsnull;
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WidgetToScreen  this=<%p> mBounds=<%d,%d,%d,%d> aOldRect=<%d,%d>\n", this, mBounds.x, mBounds.y, mBounds.width, mBounds.height,aOldRect.x,aOldRect.y ));
  PhPoint_t p1;
  
  if (mWidget)
  {
    PtWidgetOffset(mWidget,&p1);
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::WidgetToScreen  mWidget offset <%d,%d>\n", p1.x, p1.y));
  }


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
    if (mMenuRegion ==  gRollupScreenRegion)
       gRollupScreenRegion = nsnull;
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::DestroyNativeChildren this=(%p)\n", this));

  for(PtWidget_t *w=PtWidgetChildFront( mWidget ); w; w=PtWidgetBrotherBehind( w )) 
  {
    nsWindow *childWindow = NS_STATIC_CAST(nsWindow *, GetInstance(w) );
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::DestroyNativeChildren this=(%p) childWindow=<%p> photon_widget=<%p>\n", this, childWindow, w));
    if (childWindow)
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::DestroyNativeChildren \t mIsDestroying=<%d>  mOnDestroyCalled=<%d> \n", childWindow->mIsDestroying, childWindow->mOnDestroyCalled));
      if (!childWindow->mIsDestroying)
      {
        childWindow->Destroy();
      }
    }
  }

// PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::DestroyNativeChildren exiting  this=(%p)\n", this));
}

NS_IMETHODIMP nsWindow::Update(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Update this=<%p> mUpdateArea=<%p> mUpdateArea->IsEmpty()=<%d>\n", this, mUpdateArea, mUpdateArea->IsEmpty() ));

#if 1
  return nsWidget::Update();
#else

//  if (mIsUpdating)
//    UnqueueDraw();

   if (mWidget == NULL)
    {
//      printf("nsWindow::Update mWidget is NULL\n");
      NS_ASSERTION(0, "nsWidget::UpdateWidgetDamaged mWidget is NULL");
      return;
	}

    if( !PtWidgetIsRealized( mWidget ))	
    {
      //NS_ASSERTION(0, "nsWidget::UpdateWidgetDamaged skipping update because widget is not Realized");
//      printf("nsWindow::Update mWidget is not realized\n");
      return;
    }
	
  if ((mUpdateArea) && (!mUpdateArea->IsEmpty()))
  {
#if 1

      PhTile_t * nativeRegion = nsnull;

	  mUpdateArea->GetNativeRegion((void *&) nativeRegion );

      if (nativeRegion)
	  {
        RawDrawFunc(mWidget, PhCopyTiles(nativeRegion));
      }
      else
	  {
        //printf("nsWindow::Update mWidget has NULL nativeRegion\n");
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
    //printf("nsWindow::CaptureRollupEvents CAPTURE widget this=<%p> gRollupScreenRegion=<%p> mMenuRegion=<%p>\n", this, gRollupScreenRegion, mMenuRegion);
    
	/* Create a pointer region */
     mIsGrabbing = PR_TRUE;
     mGrabWindow = this;
  }
  else
  {
    //printf("nsWindow::CaptureRollupEvents UN-CAPTURE widget this=<%p> gRollupScreenRegion=<%p> mMenuRegion=<%p>\n", this, gRollupScreenRegion, mMenuRegion);

    // make sure that the grab window is marked as released
    if (mGrabWindow == this)
	{
      mGrabWindow = NULL;
    }

    mIsGrabbing = PR_FALSE;
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
  
  /* Offset the rect by the mBounds X,Y to fix damaging inside widget in test14 */
  if (mWindowType == eWindowType_popup)
     mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);
  else
     mUpdateArea->Union((aRect.x+mBounds.x), (aRect.y+mBounds.y), aRect.width, aRect.height);

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
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::InvalidateRegion this=<%p> IsSynch=<%d> mBounds=(%d,%d,%d,%d)\n", this, aIsSynchronous,
    mBounds.x, mBounds.y, mBounds.width, mBounds.height));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::InvalidateRegion - mWidget is NULL!\n" ));
    return NS_OK; // mWidget will be null during printing. 
  }
  
  if (!PtWidgetIsRealized(mWidget))
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::InvalidateRegion - mWidget is not realized\n"));
      return NS_OK;
  }

  newRegion = GetRegion();
  newRegion->SetTo(*aRegion);
     
#if defined(DEBUG) && 1
{
  PRUint32 len;
  PRUint32 i;
  nsRegionRectSet *regionRectSet = nsnull;

  newRegion->GetRects(&regionRectSet);
  len = regionRectSet->mRectsLen;
  for (i=0;i<len;++i)
  {
    nsRegionRect *r = &(regionRectSet->mRects[i]);
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::InvalidateRegion rect %d is <%d,%d,%d,%d>\n", i, r->x, r->y, r->width, r->height));
  }

  newRegion->FreeRects(regionRectSet);
}
#endif

 
  if (mWindowType != eWindowType_popup)
    newRegion->Offset(mBounds.x, mBounds.y);
  mUpdateArea->Union(*newRegion);

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetBackgroundColor this=<%p> color(RGB)=(%d,%d,%d)\n", this, NS_GET_R(aColor),NS_GET_G(aColor),NS_GET_B(aColor)));
  return nsWidget::SetBackgroundColor(aColor);
}


NS_IMETHODIMP nsWindow::SetFocus(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetFocus this=<%p>\n", this));
  return nsWidget::SetFocus();
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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("\tborder style = %X\n", mBorderStyle ));

  if ( mWindowType == eWindowType_child )
  {
    arg_count = 0;
    PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );
    PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0 /*Pt_HIGHLIGHTED*/, Pt_HIGHLIGHTED );
    PtSetArg( &arg[arg_count++], Pt_ARG_BORDER_WIDTH, 0, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_RED, 0 );
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

      /* Remove data members: mFrameLeft, mFrameTop, mFrameRight, mFrameBottom ? */ 
    }

    arg_count = 0;
    PtSetArg( &arg[arg_count++], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[arg_count++], Pt_ARG_RESIZE_FLAGS, 0, Pt_RESIZE_XY_BITS );

    /* Create Pop-up Menus as a PtRegion */

    if (!parentWidget)
      PtSetParentWidget( nsnull );
		
    if( mWindowType == eWindowType_popup )
    {
	  int	fields = Ph_REGION_PARENT|Ph_REGION_HANDLE|
                     Ph_REGION_FLAGS|Ph_REGION_ORIGIN|
					 Ph_REGION_EV_SENSE|Ph_REGION_EV_OPAQUE|
					 Ph_REGION_RECT;

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
      if ( gRollupScreenRegion== nsnull)
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
        mMenuRegion = gRollupScreenRegion = PtCreateWidget( PtRegion, parentWidget, arg_count2, args );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::CreateNative  creating gRollupScreenRegion = <%p>\n",  gRollupScreenRegion));
      }

        callback.event_f = PopupMenuRegionCallback;
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_PARENT,  gRollupScreenRegion->rid, 0 );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FIELDS,   fields, fields );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_FLAGS,    Ph_FORCE_FRONT, Ph_FORCE_FRONT );
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_SENSE,    sense | Ph_EV_DRAG|Ph_EV_EXPOSE, sense | Ph_EV_DRAG|Ph_EV_EXPOSE);
        PtSetArg( &arg[arg_count++], Pt_ARG_REGION_OPAQUE,   sense | Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW, sense |Ph_EV_DRAG|Ph_EV_EXPOSE|Ph_EV_DRAW);
        PtSetArg( &arg[arg_count++], Pt_ARG_RAW_CALLBACKS,   &callback, 1 );
        PtSetArg( &args[arg_count++], Pt_ARG_FLAGS, 0, Pt_GETS_FOCUS);
        // briane
        //mWidget = PtCreateWidget( PtRegion, parentWidget, arg_count, arg);
        mWidget = mMenuRegion = PtCreateWidget( PtRegion, parentWidget, arg_count, arg);
     }
	else
	{
      PtSetParentWidget( nsnull );
      
      /* Dialog and TopLevel Windows */
      PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
      PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, 0xFFFFFFFF );
      PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_MANAGED_FLAGS, 0, Ph_WM_CLOSE );
      PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_BLUE, 0 );
      //PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR,      Pg_TRANSPARENT, 0 );

      //mWidget = PtCreateWidget( PtWindow, parentWidget, arg_count, arg );
      mWidget = PtCreateWidget( PtWindow, NULL, arg_count, arg );
      PtAddCallback(mWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
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
      PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_GREEN, 0 );
      //PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );
      PhRect_t anch_offset = { 0, 0, 0, 0 };
      PtSetArg( &arg[arg_count++], Pt_ARG_ANCHOR_OFFSETS, &anch_offset, 0 );

      if( mWindowType == eWindowType_popup )
	  {
        PtSetArg( &arg[arg_count++], RDC_DRAW_FUNC, RawDrawFunc, 0 );
        PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, (Pt_HIGHLIGHTED | Pt_GETS_FOCUS));
        PtSetArg( &arg[arg_count++], Pt_ARG_WINDOW_MANAGED_FLAGS, Ph_WM_FFRONT, Ph_WM_FFRONT );
        mClientWidget = PtCreateWidget( PtRawDrawContainer, mWidget, arg_count, arg );
	  }
	  else
	  {  
        PtSetArg( &arg[arg_count++], Pt_ARG_FLAGS, 0, Pt_HIGHLIGHTED );
        PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_YELLOW, 0 );
        //PtSetArg( &arg[arg_count++], Pt_ARG_FILL_COLOR, Pg_TRANSPARENT, 0 );
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
//      | Ph_EV_EXPOSE
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
		| Ph_EV_WM
        | Ph_EV_EXPOSE
        , RawEventHandler, this );

      PtArg_t arg;
      PtRawCallback_t callback;
		
		callback.event_mask = ( Ph_EV_KEY ) ;
		callback.event_f = RawEventHandler;
		callback.data = this;
		PtSetArg( &arg, Pt_CB_FILTER, &callback, 0 );
		PtSetResources( mClientWidget, 1, &arg );
			
      PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
//      PtAddCallback(mWidget, Pt_CB_WINDOW_CLOSING, WindowCloseHandler, this );	
	}
    else if ( !parentWidget )
    {
       if (mClientWidget)
       {
          PtAddCallback(mClientWidget, Pt_CB_RESIZE, ResizeHandler, nsnull ); 
       }
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll this=<%p> mWidget=<%p> mClientWidget=<%p> aDx=<%d aDy=<%d> aClipRect=<%p>\n",
    this, mWidget, mClientWidget, aDx, aDy, aClipRect));

  short        count = 0;
  PhRect_t     rect,clip;
  PhPoint_t    offset = { aDx, aDy };
  PhArea_t     area;
  PhTile_t    *clipped_tiles=nsnull, *sib_tiles=nsnull, *tile=nsnull;
  PhTile_t    *offset_tiles=nsnull, *intersection = nsnull;

  PtWidget_t  *widget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);
  PhRid_t      rid = PtWidgetRid( widget );

  /* If aDx and aDy are 0 then skip it or if widget == null */
  if ( ( !aDx && !aDy ) || (!widget ))
  {
    return NS_OK;
  }
  
#if defined(DEBUG) && 0
{
  PtWidget_t *w;

   PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll Children of w=<%p>\n", widget));
    for(count=0, w=PtWidgetChildFront( widget ); w; w=PtWidgetBrotherBehind( w ),count++) 
    { 
      PhArea_t  area;
        PtWidgetArea(w, &area);
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  nsWindow::Scroll Child %d is <%p> at <%d,%d,%d,%d>\n", count, w, area.pos.x, area.pos.y, area.size.w, area.size.h));

      nsWindow *win = (nsWindow *) GetInstance(w);
      if (win)
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("    Moz ptr=<%p> window_type=<%d>\n", win, win->mWindowType));
      }
    }

   PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll Brothers in front of w=<%p>\n", widget));
    for(count=0, w=PtWidgetBrotherInFront( widget ); w; w=PtWidgetBrotherInFront(w), count++) 
    { 
      PhArea_t  area;
        PtWidgetArea(w, &area);
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  nsWindow::Scroll Brother %d is <%p> at <%d,%d,%d,%d>\n", count, w, area.pos.x, area.pos.y, area.size.w, area.size.h));

      nsWindow *win = (nsWindow *) GetInstance(w);
      if (win)
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("    Moz ptr=<%p> window_type=<%d>\n", win, win->mWindowType));
      }
    }
}
#endif


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

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll  Moving children PtWidget=<%p> to (%d,%d)\n", w, p.x, p.y));

      PtSetArg( &arg, Pt_ARG_POS, &p, 0 );
      PtSetResources( w, 1, &arg ) ;

      nsWindow *pWin = (nsWindow *) GetInstance(w);
      if (pWin)
      {
         pWin->mBounds.x += aDx;
         pWin->mBounds.y += aDy;
      }
    } 

    // Take our nice, clean client-rect and shatter it into lots (maybe) of
    // unobscured tiles. sib_tiles represents the rects occupied by siblings
    // in front of our window - but its not needed here.
    if ( GetSiblingClippedRegion( &clipped_tiles, &sib_tiles ) == NS_OK )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll GetSiblingClippedRegion returned OK\n"));

#if defined(DEBUG) && 0
{
  PhTile_t *top = clipped_tiles;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll Damage clipped_tiles Tiles List:\n"));
  while (top)
  {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  nsWindow::Scroll tile %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  } 
}
#endif

#if defined(DEBUG) && 0
{
  PhTile_t *top = sib_tiles;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll Damage sib_tiles Tiles List:\n"));
  while (top)
  {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("  nsWindow::Scroll tile %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    top=top->next;
  } while (top);
}
#endif

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
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Scroll blit tile (%i,%i,%i,%i) offset=<%d,%d>\n",
           tile->rect.ul.x, tile->rect.ul.y, tile->rect.lr.x, tile->rect.lr.y, offset.x, offset.y));
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
  
  
  return NS_OK;
}

NS_METHOD nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ScrollWidgets - Not Implemented  this=<%p> mWidget=<%p> mClientWidget=<%p> aDx=<%d aDy=<%d>\n",
    this, mWidget, mClientWidget, aDx, aDy));

   PtWidget_t  *widget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);

    // Manually move all the child-widgets
    PtWidget_t *w;
    PtArg_t    arg;
    PhPoint_t  *pos;
    PhPoint_t  p;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ScrollWidgets  Moving children...\n" ));
    for( w=PtWidgetChildFront( widget ); w; w=PtWidgetBrotherBehind( w )) 
    { 
      PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
      PtGetResources( w, 1, &arg ) ;
      p = *pos;
      p.x += aDx;
      p.y += aDy;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ScrollWidgets  Moving children PtWidhet=<%p> to (%d,%d)\n", w, p.x, p.y));

      PtSetArg( &arg, Pt_ARG_POS, &p, 0 );
      PtSetResources( w, 1, &arg ) ;

      nsWindow *pWin = (nsWindow *) GetInstance(w);
      if (pWin)
      {
         pWin->mBounds.x += aDx;
         pWin->mBounds.y += aDy;
      }
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
    //printf("nsWindow::OnKey Discarding Event, no mEventCallback\n");  
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
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize Forcing small toplevel window window to Hide\n"));
		//Show(PR_FALSE);
      }
    }
    else
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Resize Forcing small non-toplevel window to Hide\n"));
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

#if 0
     /* KEDL: fix so throbber area is right size; only works first time though... */
     sevent.windowSize = new nsRect (0, 0, aWidth+2, aHeight); 	
#else
     sevent.windowSize = new nsRect (0, 0, aWidth, aHeight); 	
#endif

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
    //printf("nsWindow::Resize Forcing window to Show\n");
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

  PhWindowEvent_t *we = (PhWindowEvent_t *) cbinfo->cbdata;
  nsWindow * win = (nsWindow*) data;

  if (we->event_f == Ph_WM_CLOSE)
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


  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Show this=<%p> IsRealized=<%d> mIsTooSmall=<%d>\n", this, PtWidgetIsRealized( mWidget ), mIsTooSmall));
  //printf ("nsWindow::Show this=<%p> State=<%d> IsRealized=<%d> mIsTooSmall=<%d> gRollupScreenRegion=<%p>\n", this, bState, PtWidgetIsRealized( mWidget ), mIsTooSmall, gRollupScreenRegion);

#if 0
  // don't show if we are too small
  if (mIsTooSmall)
    return NS_OK;
#endif

  EnableDamage(mMenuRegion, PR_FALSE);

  if (bState)
  {
     //printf("nsWindow::Show Show WindowIsPopup=<%d> gRollupScreenRegion=<%p> mMenuRegion=<%p>\n",   (mWindowType == eWindowType_popup), gRollupScreenRegion, mMenuRegion);

     if (mWindowType == eWindowType_popup)
     {
        if ((!gRollupScreenRegion) && (mMenuRegion))
          gRollupScreenRegion = mMenuRegion;

        if (gRollupScreenRegion)
        {
          short arg_count=0;
          PtArg_t   arg[2];
          PhPoint_t pos = nsToolkit::GetConsoleOffset();

          /* Position the region on the current console*/
          PtSetArg( &arg[0],  Pt_ARG_POS,  &pos, 0 );
          PtSetResources( gRollupScreenRegion, 1, arg );

          PtRealizeWidget(gRollupScreenRegion);

           /* Now that the MenuRegion is Realized make the popup menu a child in front of the big menu */
           PtSetArg( &arg[arg_count++], Pt_ARG_REGION_PARENT,   gRollupScreenRegion->rid, 0 );
           PtSetResources(mWidget, arg_count, arg);           
       }
     }
  }
  else
  {
     //printf("nsWindow::Show Un-Show WindowIsPopup=<%d> gRollupScreenRegion=<%p> mMenuRegion=<%p>\n",  (mWindowType == eWindowType_popup), gRollupScreenRegion, mMenuRegion);
   
     if ( (mWindowType == eWindowType_popup))

     {
        if ((gRollupScreenRegion) && (mMenuRegion==gRollupScreenRegion))
        {
             PtUnrealizeWidget(gRollupScreenRegion);  
             gRollupScreenRegion = nsnull;
        }
     }
  }

  EnableDamage(mMenuRegion, PR_TRUE);

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
  PhTile_t  * dmg = NULL;
  nsIRegion * ClipRegion;
  PRBool      aClipState;
  nsPaintEvent pev;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc for mWidget=<%p> this=<%p> this->mContext=<%p> pWin->mParent=<%p> mClientWidget=<%p>\n", 
    pWidget, pWin, pWin->mContext, pWin->mParent, pWin->mClientWidget));

#if defined(DEBUG) && 1
{
  /* Print out the Photon Damage tiles */
  PhTile_t *top = damage;
  int index=0;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Damage Tiles List:\n"));
  do {
    PhRect_t   rect = top->rect;    
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next));
    //printf("nsWindow::%p RawDrawFunc photon damage %d rect=<%d,%d,%d,%d> next=<%p>\n", pWidget,index++,rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y, top->next);
    top=top->next;
  } while (top);
}
#endif
    
  if ( !PtWidgetIsRealized( pWidget ))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc  aborted because pWidget was not realized!\n"));
    NS_ASSERTION(pWin, "nsWindow::RawDrawFunc  aborted because pWidget was not realized!");
    abort();
    return; 
  }

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
    return;
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

    /* Add the X,Y of area to offset to fix the throbber drawing in Viewer */
    //offset.x += area.pos.x;  
    //offset.y += area.pos.y;  

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc offset+area =<%d,%d>\n", offset.x, offset.y));

    /* Build a List of Tiles that might be in front of me.... */
    PhTile_t *clip_tiles=pWin->GetWindowClipping(offset);

    /* Intersect the Damage tile list w/ the clipped out list and */
	/* see whats left! */
	PhTile_t *new_damage;

   /* This could be leaking some memory */
   PhTile_t *tiles2, *tile3;
   if (damage->next)
   {
    tiles2 = PhCopyTiles(damage->next);
    tile3 = PhRectsToTiles(&damage->rect, 1);
    tiles2 = PhIntersectTilings(tiles2, tile3, NULL);
   }
   else
   {
	tiles2 = PhRectsToTiles(&damage->rect, 1);
   }

   /* Change to window relative coordinates */
    PhDeTranslateTiles(tiles2,&offset);

    /* new_damage is the same as tiles2... I need to release it later */
    new_damage = PhClipTilings(tiles2, PhCopyTiles(clip_tiles), NULL);

#if 0
/* There is a bug in merge or coalesce that causes rips when you move a window */
/* down and to the right over Mozilla, these should be readded in when its fixed. */
    new_damage = PhCoalesceTiles( PhMergeTiles (PhSortTiles( new_damage )));
    //new_damage = PhMergeTiles (PhSortTiles( new_damage ));
#endif

    if (new_damage == nsnull)
    { 
      /* Nothing to Draw */
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc Nothing to Draw damage is NULL\n"));
      return;	
    }

#if defined(DEBUG) && 1
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


    pWin->InitEvent(pev, NS_PAINT);
    pev.eventStructType = NS_PAINT_EVENT;
    pev.renderingContext = nsnull;
    pev.renderingContext = pWin->GetRenderingContext();
    if (pev.renderingContext == NULL)
	{
      //printf("nsWindow::RawDrawFunc error getting RenderingContext\n");
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
      //printf("nsWindow::RawDrawFunc damage rect = <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y);

      /* Do I need to Translate it?? If so by what? offset? offset+area?*/
      rect.ul.x -= area.pos.x;
	  rect.ul.y -= area.pos.y;
	  rect.lr.x -= area.pos.x;
	  rect.lr.y -= area.pos.y;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage rect after translate = <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y));

      /* If the damage tile is not within our bounds, do nothing */
      if(( rect.ul.x >= area.size.w ) || ( rect.ul.y >= area.size.h ) || ( rect.lr.x < 0 ) || ( rect.lr.y < 0 ))
      {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc damage tile is not within our bounds, do nothing\n"));
        //printf("nsWindow::RawDrawFunc damage tile is not within our bounds, do nothing\n");
      }

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc clipped damage <%d,%d,%d,%d>\n", rect.ul.x,rect.ul.y,rect.lr.x,rect.lr.y));

      nsDmg.x = rect.ul.x;
      nsDmg.y = rect.ul.y;
      nsDmg.width = rect.lr.x - rect.ul.x + 1;
      nsDmg.height = rect.lr.y - rect.ul.y + 1;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::RawDrawFunc nsDmg <%d,%d,%d,%d>\n", nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height));

      if ( (nsDmg.width <= 0 ) || (nsDmg.height <= 0 ) )
      {
        /* Move to the next Damage Tile */
        dmg = dmg->next;
        continue;
      }

#if 1
      /* Re-Setup Paint Event */
      pWin->InitEvent(pev, NS_PAINT);
      pev.eventStructType = NS_PAINT_EVENT;
      pev.point.x = nsDmg.x;
      pev.point.y = nsDmg.y;
      pev.rect = new nsRect(nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height);

      if (pev.renderingContext)
      {
        ClipRegion = pWin->GetRegion();
        ClipRegion->SetTo(nsDmg.x, nsDmg.y, nsDmg.width, nsDmg.height);
        pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(ClipRegion)),
                                          nsClipCombine_kReplace, aClipState);
		
        /* Not sure WHAT this sould be, probably nsDmg rect... */
         // pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(ClipRegion)),
         // pev.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *(pWin->mUpdateArea)),
          //                                    nsClipCombine_kReplace, aClipState);

         /* You can turn off most drawing if you take this out */
         result = pWin->DispatchWindowEvent(&pev);
      }
#endif
      /* Move to the next Damage Tile */
      dmg = dmg->next;
	}

    NS_RELEASE(pev.renderingContext);
  }

  PhFreeTiles(dmg);
    
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



/* Only called by nsWidget::Scroll */
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

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion mWidget=<%p> area=<%d,%d,%d,%d>\n", mWidget,area->pos.x, area->pos.x, area->size.w, area->size.h));

        GetParentClippedArea( rect );

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion after GetParentClippedArea rect=(%d,%d,%d,%d)\n", rect.x, rect.y, rect.width, rect.height));

        (*btiles)->rect.ul.x = rect.x;
        (*btiles)->rect.ul.y = rect.y;
        (*btiles)->rect.lr.x = rect.x + rect.width - 1;
        (*btiles)->rect.lr.y = rect.y + rect.height - 1;

        (*btiles)->next = nsnull;

        *ctiles = last = nsnull;

        for( w=PtWidgetBrotherInFront( mWidget ); w; w=PtWidgetBrotherInFront( w )) 
        { 
          long flags = PtWidgetFlags(w);

           PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
           PtGetResources( w, 1, &arg );

           PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetSiblingClippedRegion BrotherInFront w=<%p> area=<%d,%d,%d,%d> IsOpaque=<%d>  !=Disjoint=<%d> IsRealized=<%d>\n",
              w, area->pos.x, area->pos.x, area->size.w, area->size.h, (flags & Pt_OPAQUE), (w != PtFindDisjoint(w)), PtWidgetIsRealized( w ) ));

           /* Is the widget OPAQUE and Window is not a disjoint window */
           if ((flags & Pt_OPAQUE) && (w != PtFindDisjoint(w)))
     	  {
            PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  widget w=<%p> is opaque IsRealized=<%d>\n", w, PtWidgetIsRealized(w)));
            if ( PtWidgetIsRealized( w ))
            {
        
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
         }}
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
  PtWidget_t *aWidget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);
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
  PtWidget_t *aWidget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);

  PhPoint_t w_offset;
  PhArea_t w_area;
  PhDim_t w_dim;
  short abs_pos_x, abs_pos_y;

  clip_tiles = last = nsnull;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  this=<%p> offset=<%d,%d> mClipChildren=<%d> mClipSiblings=<%d>\n", this, offset.x, offset.y, mClipChildren, mClipSiblings));

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping    clipping children...\n"));

    PtWidgetOffset(w=PtWidgetChildFront( aWidget ), &w_offset);
    w_offset.x -= offset.x;
    w_offset.y -= offset.y;    
 
    for(; w; w=PtWidgetBrotherBehind( w )) 
    { 
      long flags = PtWidgetFlags(w);

#if 0
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( w, 1, &arg );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  clipping children w=<%p> area=<%d,%d,%d,%d> flags=<%ld>\n", w, area->pos.x, area->pos.y, area->size.w, area->size.h, flags));

      PtWidgetArea(w, &w_area);
      PtWidgetDim(w, &w_dim);

      PtGetAbsPosition(w, &abs_pos_x, &abs_pos_y);

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  clipping children w=<%p> offset=<%d,%d>  Abs Pos=<%d,%d> w_area=<%d,%d,%d,%d>\n", w, w_offset.x, w_offset.y, abs_pos_x, abs_pos_y, w_area.pos.x, w_area.pos.y, w_area.size.w, w_area.size.h));
#endif

       /* Is the widget OPAQUE and Window is not a disjoint window */
	  if ((flags & Pt_OPAQUE) && (w != PtFindDisjoint(w)))
	  {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  widget w=<%p> is opaque IsRealized=<%d>\n", w, PtWidgetIsRealized(w)));
        if ( PtWidgetIsRealized( w ))
        {
#if 1
      PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
      PtGetResources( w, 1, &arg );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  clipping children w=<%p> area=<%d,%d,%d,%d> flags=<%ld>\n", w, area->pos.x, area->pos.y, area->size.w, area->size.h, flags));

      PtWidgetArea(w, &w_area);
      PtWidgetDim(w, &w_dim);

      PtGetAbsPosition(w, &abs_pos_x, &abs_pos_y);

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::GetWindowClipping  clipping children w=<%p> offset=<%d,%d>  Abs Pos=<%d,%d> w_area=<%d,%d,%d,%d>\n", w, w_offset.x, w_offset.y, abs_pos_x, abs_pos_y, w_area.pos.x, w_area.pos.y, w_area.size.w, w_area.size.h));
#endif

          tile = PhGetTile();
          if( tile )
          {
            tile->rect.ul.x = area->pos.x + w_offset.x;
            tile->rect.ul.y = area->pos.y + w_offset.y;
            tile->rect.lr.x = tile->rect.ul.x + area->size.w - 1;
            tile->rect.lr.y = tile->rect.ul.y + area->size.h - 1;

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


#if defined(DEBUG) && 1
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


  return clip_tiles;
}

int nsWindow::ResizeHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  PhRect_t *extents = (PhRect_t *)cbinfo->cbdata; 
  nsWindow *someWindow = (nsWindow *) GetInstance(widget);
  nsRect rect;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ResizeHandler for someWindow=<%p>\n", someWindow));

  if( someWindow && extents)	// kedl
  {
    rect.x = extents->ul.x;
    rect.y = extents->ul.y;
    rect.width = extents->lr.x - rect.x + 1;
    rect.height = extents->lr.y - rect.y + 1;
    someWindow->mBounds.width  = rect.width;	// kedl, fix main window not resizing!!!!
    someWindow->mBounds.height = rect.height;

    /* This enables the resize holdoff */
    if (PtWidgetIsRealized(widget))
    {
      someWindow->ResizeHoldOff();
      someWindow->OnResize( rect );
    }
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


int nsWindow::ResizeWorkProc( void *data )
{
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

NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
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

  PRInt32 origX, origY;

  /* Keep a untouched version of the coordinates laying around for comparison */
  origX=aX;
  origY=aY;

#if defined(DEBUG) && 0
  if (mWindowType==eWindowType_popup)
  {
    /* For Pop-Up Windows print out Area Ancestry */
    int count = 1;
    PhArea_t area;
    PtWidget_t *top;
    PhPoint_t offset;
    
    PtWidgetArea(mWidget, &area);
    PtWidgetOffset(mWidget, &offset );
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d mWidget area=<%d,%d,%d,%d>\n", count, area.pos.x,area.pos.y,area.size.w,area.size.h));
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d mWidget offset=<%d,%d>", count, offset.x,offset.y));
    count++;
    top = PtWidgetParent(mWidget);
    while(top)
    {
      PtWidgetArea(top, &area);
      PtWidgetOffset(top, &offset );
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d parent area=<%d,%d,%d,%d>\n", count, area.pos.x,area.pos.y,area.size.w,area.size.h));
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d parent offset=<%d,%d>", count, offset.x,offset.y));
      count++;
      top = PtWidgetParent(top);    
    }
  }
#endif

#if defined(DEBUG) && 0
  if (mWindowType==eWindowType_popup)
  {
    /* For Pop-Up Windows print out Area Ancestry */
    int count = 1;
    PhArea_t area;
    PtWidget_t *top, *last=nsnull;
    PhPoint_t offset;
        
    PtWidgetArea(mWidget, &area);
    PtWidgetOffset(mWidget, &offset );
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d mWidget area=<%d,%d,%d,%d>\n", count, area.pos.x,area.pos.y,area.size.w,area.size.h));
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d mWidget offset=<%d,%d>", count, offset.x,offset.y));
    count++;
    top = PtFindDisjoint(mWidget);
    while((top != last) && top)
    {
      PtWidgetArea(top, &area);
      PtWidgetOffset(top, &offset );
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d disjoint area=<%d,%d,%d,%d>\n", count, area.pos.x,area.pos.y,area.size.w,area.size.h));
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move %d disjoint offset=<%d,%d>", count, offset.x,offset.y));
      if (PtWidgetIsClass(top, PtWindow))
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move disjoint window is a PtWindow\n"));
      if (PtWidgetIsClass(top, PtRegion))
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move disjoint window is a PtRegion\n"));
      count++;
      last = top;
      PtWidget_t *tmp = PtWidgetParent(top);
      if (tmp)
        top = PtFindDisjoint(tmp);    
      else
        top = tmp;
    }
  }
#endif


  if (mWindowType==eWindowType_popup)
  {
    short int FrameLeft, FrameTop, FrameBottom;
    PhArea_t area;
    PhPoint_t offset, total_offset;
    
      FrameLeft = FrameTop = FrameBottom = 0;
      /* Hard code these values for now */
      FrameLeft = 5;          /* 5 looks the best? */
      FrameTop = 21;        /* started with 21 */

      aX += FrameLeft;
      aY += FrameTop;

    PtWidget_t *parent, *disjoint = PtFindDisjoint(mWidget);
      disjoint = PtFindDisjoint(PtWidgetParent(disjoint));
      total_offset.x = 0;
      total_offset.y = 0;
      while(disjoint)
      {
         PtWidgetArea(disjoint, &area);
         total_offset.x += area.pos.x;
         total_offset.y += area.pos.y;
         if (PtWidgetIsClass(disjoint, PtWindow))
         {
            /* Stop at the first PtWindow */
            PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move disjoint window is a PtWindow exiting loop \n"));
            break;
         }
         parent = PtWidgetParent(disjoint);
         if (parent)
           disjoint = PtFindDisjoint(parent);
         else
         {
           disjoint = parent;
           break;
         }           
      }

     aX += total_offset.x;
     aY += total_offset.y;

    /* Add the Offset if the widget is offset from its parent.. */
      parent = PtWidgetParent( mWidget );
      PtWidgetOffset(parent, &offset);
      aX += offset.x;
      aY += offset.y;    
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move this=(%p) to (%ld,%ld) \n", this, aX, aY ));

 /* Offset to the current virtual console */
  if ( (mWindowType == eWindowType_dialog)
	   || (mWindowType == eWindowType_toplevel)
	 )
  {
  PhPoint_t offset = nsToolkit::GetConsoleOffset();
  
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move toplevel console offset=(%d,%d)\n", offset.x, offset.y));
     aX += offset.x;
     aY += offset.y;
  }

  /* Subtract off the console offset that got added earlier in this method */
  if (mWindowType == eWindowType_popup)
  {
    PhPoint_t offset = nsToolkit::GetConsoleOffset();
  
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move toplevel console offset=(%d,%d)\n", offset.x, offset.y));
     aX -= offset.x;
     aY -= offset.y;  
  
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::Move this=(%p) after console offset to (%ld,%ld) \n", this, aX, aY ));

  /* Call my base class */
  nsresult res = nsWidget::Move(aX, aY);

  /* If I am a top-level window my origin should always be 0,0 */

  if ( (mWindowType == eWindowType_dialog) ||
	   (mWindowType == eWindowType_toplevel) ||
       (mWindowType == eWindowType_popup) )
  {
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
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                     PRBool *aForWindow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::ModalEventFilter aEvent=<%p> - Not Implemented.\n", aEvent));

//printf ("kedl: modal event filter %d %d %p.......................................\n",aRealEvent,aForWindow,aEvent);
   *aForWindow = PR_TRUE;
    return NS_OK;
}

int nsWindow::MenuRegionCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
  nsWindow      *pWin = (nsWindow*) data;
  nsWindow      *pWin2 = (nsWindow*) GetInstance(widget);
  nsresult      result;
  PhEvent_t	    *event = cbinfo->event;

  /* eliminate 'unreferenced' warnings */
  widget = widget, data = data, cbinfo = cbinfo;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback this=<%p> widget=<%p> apinfo=<%p> mMenuRegion=<%p>\n", pWin, widget, data, pWin->mMenuRegion));
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback this=<%p> widget=<%p> apinfo=<%p> pWin2=<%p> EventName=<%s>\n", pWin, widget, data, pWin2, (const char *) nsCAutoString(PhotonEventToString(event), strlen(PhotonEventToString(event)) ) ));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback cbinfo->reason=<%d>\n", cbinfo->reason));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback cbinfo->subtype=<%d>\n", cbinfo->reason_subtype));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback cbinfo->cbdata=<%d>\n", cbinfo->cbdata));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::MenuRegionCallback event->type=<%d>\n", event->type));

#if 1
 if (event->type == Ph_EV_BUT_PRESS)
 {
    //printf("nsWindow::MenuRegionCallback event is Ph_EV_BUT_PRESS \n");
    //printf("nsWindow::MenuRegionCallback pWin->gRollupWidget=<%p> pWin->gRollupListener=<%p>\n", pWin->gRollupWidget, pWin->gRollupListener);

    if (pWin->gRollupWidget && pWin->gRollupListener)
    {
      /* Close the Window */
      pWin->gRollupListener->Rollup();
      IgnoreEvent = event->timestamp;
    }
  }
#endif

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
 PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetModal this=<%p>  mWidget=<%p>\n", this, mWidget));
  nsresult res = NS_ERROR_FAILURE;
 
  if (!mWidget)
	return NS_ERROR_FAILURE;

  PtWidget_t *toplevel = PtFindDisjoint(mWidget);
  if (!toplevel)
	return NS_ERROR_FAILURE;

/* JERRY Delete the mModalCount variable it is no longer needed! */
  if (aModal)
  {
       if (mParent)
         mParent->Enable(PR_FALSE);
	  PtModalStart();
	  res = NS_OK;
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetModal after call to PtModalStart\n"));
  }
  else
  {
       if (mParent)
         mParent->Enable(PR_TRUE);
	PtModalEnd();
    res = NS_OK;
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::SetModal after call to PtModalEnd\n"));
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


int nsWindow::PopupMenuRegionCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo)
{
  nsWindow      *pWin = (nsWindow*) data;
  nsWindow      *pWin2 = (nsWindow*) GetInstance(widget);
  nsresult      result;
  PhEvent_t	    *event = cbinfo->event;

  /* eliminate 'unreferenced' warnings */
  widget = widget, data = data, cbinfo = cbinfo;

 PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PopupMenuRegionCallback this=<%p> widget=<%p> data=<%p> mMenuRegion=<%p>\n", pWin, widget, data, pWin->mMenuRegion));

// printf("nsWindow::PopupMenuRegionCallback this=<%p> widget=<%p> data=<%p> mMenuRegion=<%p> EventName=<%s>\n", pWin, widget, data, pWin->mMenuRegion, (const char *) nsCAutoString(PhotonEventToString(event), strlen(PhotonEventToString(event) ));
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PMenuRegionCallback this=<%p> widget=<%p> data=<%p> pWin2=<%p> EventName=<%s>\n", pWin, widget, data, pWin2, (const char *) nsCAutoString(PhotonEventToString(event)) ));

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PMenuRegionCallback cbinfo->reason=<%d>\n", cbinfo->reason));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PMenuRegionCallback cbinfo->subtype=<%d>\n", cbinfo->reason_subtype));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PMenuRegionCallback cbinfo->cbdata=<%d>\n", cbinfo->cbdata));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWindow::PMenuRegionCallback event->type=<%d>\n", event->type));

  return RawEventHandler(widget, data, cbinfo);
}
