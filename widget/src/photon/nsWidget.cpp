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
 *     Michael.Kedl@Nexwarecorp.com
 *	   Dale.Stansberry@Nexwarecorop.com
 */


#include "nsWidget.h"

#include "nsIAppShell.h"
#include "nsIComponentManager.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsToolkit.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include <Pt.h>
#include "PtRawDrawContainer.h"
#include "nsIRollupListener.h"
#include "nsIServiceManager.h"
#include "nsWindow.h"

#include "nsIPref.h"
#include "nsPhWidgetLog.h"

#include <errno.h>

static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

// BGR, not RGB - REVISIT
#define NSCOLOR_TO_PHCOLOR(g,n) \
  g.red=NS_GET_B(n); \
  g.green=NS_GET_G(n); \
  g.blue=NS_GET_R(n);

////////////////////////////////////////////////////////////////////
//
// Define and Initialize global variables
//
////////////////////////////////////////////////////////////////////
nsIRollupListener *nsWidget::gRollupListener = nsnull;
nsIWidget         *nsWidget::gRollupWidget = nsnull;
PRBool             nsWidget::gRollupConsumeRollupEvent = PR_FALSE;
PtWidget_t        *nsWidget::gRollupScreenRegion = NULL;

/* These are used to keep Popup Menus from drawing twice when they are put away */
/* because of extra EXPOSE events from Photon */
PhRid_t          nsWidget::gLastUnrealizedRegion = -1;
PhRid_t          nsWidget::gLastUnrealizedRegionsParent = -1;

//
// Keep track of the last widget being "dragged"
//
nsWidget           *nsWidget::sButtonMotionTarget = NULL;
int                 nsWidget::sButtonMotionRootX = -1;
int                 nsWidget::sButtonMotionRootY = -1;
int                 nsWidget::sButtonMotionWidgetX = -1;
int                 nsWidget::sButtonMotionWidgetY = -1;
DamageQueueEntry   *nsWidget::mDmgQueue = nsnull;
PtWorkProcId_t     *nsWidget::mWorkProcID = nsnull;
PRBool              nsWidget::mDmgQueueInited = PR_FALSE;
nsILookAndFeel     *nsWidget::sLookAndFeel = nsnull;
PRUint32            nsWidget::sWidgetCount = 0;

extern unsigned long		IgnoreEvent;

/* Enable this to queue widget damage, this should be ON by default */
#define ENABLE_DAMAGE_QUEUE

/* Enable this causing extra redraw when the RELEASE is called */
//#define ENABLE_DAMAGE_QUEUE_HOLDOFF


/* Enable experimental direct draw code, this bypasses PtDamageExtent */
/* and calls doPaint method which calls nsWindow::RawDrawFunc */
//#define ENABLE_DOPAINT

nsWidget::nsWidget()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::nsWidget this=(%p)\n", this));

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("%p C nsWidget CONSTRUCTOR LEAK_CHECK time=(%ld)\n", this, PR_IntervalNow()));

  // XXX Shouldn't this be done in nsBaseWidget?
  // NS_INIT_REFCNT();

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
  mPreferredWidth  = 0;
  mPreferredHeight = 0;
  mShown = PR_FALSE;
  mBounds.x = 0;
  mBounds.y = 0;
  mBounds.width = 0;
  mBounds.height = 0;
  mIsDestroying = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mIsDragDest = PR_FALSE;
  mIsToplevel = PR_FALSE;
  mListenForResizes = PR_FALSE;
  mHasFocus = PR_FALSE;

  if (NS_OK == nsComponentManager::CreateInstance(kRegionCID,
                                                  nsnull,
                                                  NS_GET_IID(nsIRegion),
                                                  (void**)&mUpdateArea))
  {
    mUpdateArea->Init();
    mUpdateArea->SetTo(0, 0, 0, 0);
  }

  sWidgetCount++;
}


nsWidget::~nsWidget()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::~nsWidget this=(%p) mWidget=<%p>\n", this, mWidget));

  NS_IF_RELEASE(mUpdateArea);

  // it's safe to always call Destroy() because it will only allow itself
  // to be called once
  Destroy();

  if (!sWidgetCount--)
  {
    NS_IF_RELEASE(sLookAndFeel);
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("%p D nsWidget DESTRUCTOR  LEAK_CHECK time=(%ld)\n", this, PR_IntervalNow()));

}

//-------------------------------------------------------------------------
//
// nsISupport stuff
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED(nsWidget, nsBaseWidget, nsIKBStateControl)

NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WidgetToScreen - Not Implemented.\n" ));
  
  if (mWidget)
  {
    /* This is NOT correct */
    aNewRect.x = aOldRect.x;
    aNewRect.y = aOldRect.y;
  }

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Destroy this=<%p> mRefCnt=<%d> mWidget=<%p> mIsDestroying=<%d>\n",this,mRefCnt, mWidget, mIsDestroying));

 // make sure we don't call this more than once.
  if (mIsDestroying)
    return NS_OK;

  // ok, set our state
  mIsDestroying = PR_TRUE;

  // call in and clean up any of our base widget resources
  // are released
  nsBaseWidget::Destroy();

  // destroy our native windows
  DestroyNative();

  // make sure to call the OnDestroy if it hasn't been called yet
  if (mOnDestroyCalled == PR_FALSE)
    OnDestroy();

  // make sure no callbacks happen
  mEventCallback = nsnull;

  return NS_OK;
}

// this is the function that will destroy the native windows for this widget.

/* virtual */
void nsWidget::DestroyNative(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DestroyNative this=<%p> mRefCnt=<%d> mWidget=<%p>\n", this, mRefCnt, mWidget ));

  if (mWidget)
  {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    RemoveDamagedWidget(mWidget);
    PtDestroyWidget( mWidget );

    mWidget = nsnull;
  }
}

// make sure that we clean up here

void nsWidget::OnDestroy()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnDestroy this=<%p> mRefCnt=<%d>\n", this, mRefCnt ));

  mOnDestroyCalled = PR_TRUE;
  // release references to children, device context, toolkit + app shell
  nsBaseWidget::OnDestroy();

#if 1
  nsCOMPtr<nsIWidget> kungFuDeathGrip = this;
  DispatchStandardEvent(NS_DESTROY);
#else
  NS_ADDREF_THIS();
  DispatchStandardEvent(NS_DESTROY);
  NS_RELEASE_THIS();
#endif
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnDestroy Exiting this=<%p> mRefCnt=<%d>\n", this, mRefCnt ));

}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParent - this=<%p> mParent=<%p> \n", this, mParent));

  nsIWidget* result = mParent;
  if (mParent) {
    NS_ADDREF(result);
  }

  return result;
}





//////////////////////////////////////////////////////////////////////
//
// nsIKBStateControl Mehthods
//
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsWidget::ResetInputState()
{
  nsresult res = NS_OK;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ResetInputState - Not Implemented this=<%p>\n", this));

  return res;
}

NS_IMETHODIMP nsWidget::PasswordFieldInit()
{
  // to be implemented
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::PasswordFieldInit - Not Implemented this=<%p>\n", this));

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Show(PRBool bState)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show this=<%p> State=<%d> mRefCnt=<%d> mWidget=<%p>\n", this, bState, mRefCnt, mWidget));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show - mWidget is NULL!\n" ));
    return NS_OK; // Will be null durring printing
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show this=<%p> IsRealized=<%d>\n", this, PtWidgetIsRealized(mWidget) ));

/* Note: Calling  PtWidgetIsRealized(mWidget) is not valid because usually
the parent window has not been realized yet when we get into this code. Also
calling PtRealizeWidget() returns an error when the parent has not been
realized. So you must just blindly bash on both the DELAY_REALIZE flags and
the PtRealizeWidget functions */

  PtArg_t   arg;

  if (bState)
  {
    int err = 0;

      EnableDamage( mWidget, PR_FALSE );
      err=PtRealizeWidget(mWidget);
      if (err == -1)
	  {
        PtWidget_t *parent = PtWidgetParent(mWidget);
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show Failed to Realize this=<%p> mWidget=<%p> mWidget->Parent=<%p> parent->IsRealized=<%d> \n", this, mWidget,parent, PtWidgetIsRealized(parent) ));
        //printf("nsWidget::Show Failed to Realize this=<%p> mWidget=<%p> mWidget->Parent=<%p> parent->IsRealized=<%d> \n", this, mWidget,parent, PtWidgetIsRealized(parent) );
      }

       if (mWidget->rid == -1)
       {
        PtRegionWidget_t *region = (PtRegionWidget_t *) mWidget;

         printf("nsWidget::errno = %s\n", strerror(errno));
         printf("nsWidget PtRealizeWidget <%p> rid=<%d> parent rid <%d>\n", mWidget, mWidget->rid, region->parent);
         NS_ASSERTION(0,"nsWidget::Show mWidget's rid == -1\n");
         //DebugBreak();
         //abort();
         mShown = PR_FALSE; 
         return NS_ERROR_FAILURE;
       }

      EnableDamage( mWidget, PR_TRUE );

      PtSetArg(&arg, Pt_ARG_FLAGS, 0, Pt_DELAY_REALIZE);
      PtSetResources(mWidget, 1, &arg);

	/* Always add it to the Widget Damage Queue when it gets realized */
    QueueWidgetDamage();
  }
  else
  {
      EnableDamage( mWidget, PR_FALSE );

      if (PtWidgetIsClass(mWidget, PtRegion))
      {
        PtWidget_t *w=nsnull, *w1=nsnull;

         gLastUnrealizedRegion = PtWidgetRid(mWidget);
         w = mWidget;
         while (1)
         {
           w1 = PtWidgetParent(w);
           if (w1==0)
           {
             gLastUnrealizedRegionsParent = PtWidgetRid(w);
             break;
           }
           w = w1;
         }

         PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show _EXPOSE gLastUnrealizedRegion=<%d> gLastUnrealizedRegionsParent=<%d> \n", gLastUnrealizedRegion, gLastUnrealizedRegionsParent));
      }

      PtUnrealizeWidget(mWidget);
      EnableDamage( mWidget, PR_TRUE );

      PtSetArg(&arg, Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
      PtSetResources(mWidget, 1, &arg);
  }

  mShown = bState;
  return NS_OK;
}

NS_IMETHODIMP nsWidget::CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CaptureRollupEvents() this = %p , doCapture = %i\n", this, aDoCapture));
  /* This got moved to nsWindow.cpp */
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetModal(PRBool aModal)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetModal - Not Implemented\n"));
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::IsVisible(PRBool &aState)
{
  if (mWidget)
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::IsVisible this=<%p> IsRealized=<%d> mShown=<%d>\n", this, PtWidgetIsRealized(mWidget), mShown));

  /* Try a simpler algorthm */
  aState = mShown;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Constrain a potential move to see if it fits onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Move(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move (%p) from (%ld,%ld) to (%ld,%ld)\n", this, mBounds.x, mBounds.y, aX, aY ));

  if (( mBounds.x == aX ) && ( mBounds.y == aY ))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move  already there.\n" ));
    return NS_OK;
  }

  mBounds.x = aX;
  mBounds.y = aY;

  if (mWidget)
  {
    PtArg_t arg;
    PhPoint_t *oldpos;
    PhPoint_t pos;
    
     pos.x = aX;
     pos.y = aY;

    EnableDamage( mWidget, PR_FALSE );

    PtSetArg( &arg, Pt_ARG_POS, &oldpos, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if(( oldpos->x != pos.x ) || ( oldpos->y != pos.y ))
      {
        int err;
        PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
        err=PtSetResources( mWidget, 1, &arg );
        if (err==-1)
        {
           PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::Move ERROR in PtSetRes (%p) to (%ld,%ld)\n", this, aX, aY ));
	    }
      }
      else
      {
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move (%p)  already at (%d,%d)\n", this, pos.x, pos.y ));
      }      
    }

    EnableDamage( mWidget, PR_TRUE );
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move - mWidget is NULL!\n" ));
  }
  
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
#if 1
       PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize broder size =<%d>\n", border));
        PhDim_t dim = {aWidth - 2*(*border), aHeight - 2*(*border)};
#else
        PhDim_t dim = {0,0};
        
        dim.w = aWidth;
        dim.h = aHeight; 
#endif
        EnableDamage( mWidget, PR_FALSE );

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize (%p) actually resizing to <%d, %d>\n", this, dim.w, dim.h ));

        PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
        PtSetResources( mWidget, 1, &arg );

        EnableDamage( mWidget, PR_TRUE );
      }

#if 0
 /* GTK Does not bother doing this... */
      if (mShown)
        Invalidate( aRepaint );
#endif

    }
    else
	{
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize FAILED- mWidget is NULL!\n" ));
	}
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
  delete foo;
  }

  return result;
}

//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::OnMove to (%d,%d)\n", aX, aY));

  nsGUIEvent event;

  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;
  PRBool result = DispatchWindowEvent(&event);

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
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Enable this=<%p> being set to %d\n", this, bState));

    PtArg_t arg;
    if( bState )
      PtSetArg( &arg, Pt_ARG_FLAGS, 0, Pt_BLOCKED );
    else
      PtSetArg( &arg, Pt_ARG_FLAGS, Pt_BLOCKED, Pt_BLOCKED );

    PtSetResources( mWidget, 1, &arg );
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Enable - mWidget is NULL!\n" ));
  }
  
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFocus(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFocus - mWidget=<%p>!\n", mWidget));

  // call this so that any cleanup will happen that needs to...
  LooseFocus();

  if (mWidget)
  {
    if (!PtIsFocused(mWidget))
      PtContainerGiveFocus( mWidget, NULL );
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetFocus - mWidget is NULL!\n" ));
  }
  
  return NS_OK;
}


void nsWidget::LooseFocus(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::LooseFocus - this=<%p> mWidget=<%p> mHasFocus=<%d>\n", this, mWidget, mHasFocus));

  // doesn't do anything.  needed for nsWindow housekeeping, really.
  if (mHasFocus == PR_FALSE) {
    return;
  }
  
  mHasFocus = PR_FALSE;
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

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetCursor(nsCursor aCursor)
{
  unsigned short curs = 0;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetCursor to <%d> was <%d>\n", aCursor, mCursor));

  // Only change cursor if it's changing
  if (aCursor != mCursor) {
    switch( aCursor )
    {
	case eCursor_sizeNW:
		curs = Ph_CURSOR_DRAG_TL;
		break;
	case eCursor_sizeSE:
		curs = Ph_CURSOR_DRAG_BR;
		break;
	case eCursor_sizeNE:
		curs = Ph_CURSOR_DRAG_TL;
		break;
	case eCursor_sizeSW:
		curs = Ph_CURSOR_DRAG_BL;
		break;

	case eCursor_crosshair:
		curs = Ph_CURSOR_CROSSHAIR;
		break;

	case eCursor_copy:
	case eCursor_alias:
	case eCursor_context_menu:
		break;

	case eCursor_cell:
		break;

	case eCursor_spinning:
		break;

	case eCursor_count_up:
	case eCursor_count_down:
	case eCursor_count_up_down:
		break;

  case eCursor_move:
    curs = Ph_CURSOR_MOVE;
    break;
      
  case eCursor_help:
    curs = Ph_CURSOR_QUESTION_POINT;
    break;
      
  case eCursor_grab:
  case eCursor_grabbing:
    curs = Ph_CURSOR_FINGER;
    break;
      
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

  if( mWidget && curs)
  {
    PtArg_t arg;

    PtSetArg( &arg, Pt_ARG_CURSOR_TYPE, curs, 0 );
    PtSetResources( mWidget, 1, &arg );
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetCursor - mWidget is NULL!\n" ));
  }
  
  mCursor = aCursor;

  }
  return NS_OK;
}


NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 this=<%p> IsSynch=<%d> mBounds=(%d,%d,%d,%d)\n", this, aIsSynchronous,
    mBounds.x, mBounds.y, mBounds.width, mBounds.height));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 - mWidget is NULL!\n" ));
    return NS_OK; // mWidget will be null during printing. 
  }
  
  if (!PtWidgetIsRealized(mWidget))
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 - mWidget is not realized\n"));
      return NS_OK;
  }

  nsRect   rect = mBounds;
  PtWidget_t *aWidget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);
  long widgetFlags = PtWidgetFlags(aWidget);

    /* Damage has to be relative Widget coords */
    mUpdateArea->SetTo( rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height );

    if (aIsSynchronous)
      UpdateWidgetDamage();
    else
      QueueWidgetDamage();

  return NS_OK;
}

// This is a experimental routine to see if I can bypass calls to PtDamageExtent
// and call RawDrawFunc directly...
NS_METHOD nsWidget::doPaint()
{
  nsresult res = NS_OK;
  PhTile_t * nativeRegion = nsnull;
  PtWidget_t *widget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);  

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::doPaint this=<%p> mWidget=<%p> widget=<%p> PtWidgetIsRealized(widget)=<%d>\n", this, mWidget, widget,PtWidgetIsRealized(widget) ));

  if ((widget) && (PtWidgetIsRealized(widget)))
  {
    if (mUpdateArea && (!mUpdateArea->IsEmpty()))
    {
      PhTile_t * nativeRegion = nsnull;

        mUpdateArea->GetNativeRegion((void *&) nativeRegion );

        PtWidget_t *widget = (PtWidget_t *)GetNativeData(NS_NATIVE_WIDGET);

        nsWindow::RawDrawFunc(widget, PhCopyTiles(nativeRegion));
    }
	else
	{
      /* mUpdateArea is Empty so re-draw the whole widget */	
	  PhTile_t *nativeRegion = PhGetTile();
      /* This rect probably needs to be differnt depending on what this */
	  /* widget really is.... widget vs PtWindow vs PtRawDrawContainer */
	  nativeRegion->rect.ul.x = mBounds.x;
	  nativeRegion->rect.ul.y = mBounds.y;
	  nativeRegion->rect.lr.x = mBounds.width - 1;
	  nativeRegion->rect.lr.y = mBounds.height - 1;
	  nativeRegion->next = NULL;

        nsWindow::RawDrawFunc(widget, nativeRegion);
	}
  }
  else
  {
    //printf("nsWidget::doPaint ERROR widget=<%p> PtWidgetIsRealized(widget)=<%d>\n", widget, PtWidgetIsRealized(widget));
  }

  return res;
}


NS_METHOD nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 2 this=<%p> dmg rect=(%ld,%ld,%ld,%ld) IsSync=<%d>\n", this, aRect.x, aRect.y, aRect.width, aRect.height, aIsSynchronous));

  if (!mWidget)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 2 - mWidget is NULL!\n" ));
    return NS_OK; // mWidget will be null during printing. 
  }
  
  if (!PtWidgetIsRealized(mWidget))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 2 - mWidget is not realized\n"));
    return NS_OK;
//    return NS_ERROR_FAILURE;
  }
  
  nsRect   rect = aRect;

    /* convert to parent coords */
    rect.x += mBounds.x;
    rect.y += mBounds.y;

    if ( GetParentClippedArea(rect) == PR_TRUE )
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 2  before Clipped rect=(%d,%d,%d,%d) mBounds=(%d,%d)\n", rect.x, rect.y, rect.width, rect.height, mBounds.x, mBounds.y));

      /* convert back widget coords */
      rect.x -= mBounds.x;
      rect.y -= mBounds.y;

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 2  Clipped rect=(%i,%i,%i,%i)\n", rect.x, rect.y, rect.width, rect.height  ));

      mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);

   if (PtWidgetIsRealized(mWidget))
/* GTK only allows Queue'd damage here ... */
//    if( aIsSynchronous)
//    {
//      UpdateWidgetDamage();
//    }
//    else
    {
      QueueWidgetDamage();
    }

    }

  return NS_OK;
}

NS_IMETHODIMP nsWidget::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::InvalidateRegion  isSync=<%d>\n",aIsSynchronous));
  //printf("nsWidget::InvalidateRegion  isSync=<%d> mWidget=<%p> mUpdateArea=<%p> IsRealized=<%d> \n",aIsSynchronous, mWidget, mUpdateArea, PtWidgetIsRealized(mWidget) ); 
#if 1
    mUpdateArea->Union(*aRegion);
    if (aIsSynchronous)
    {
      UpdateWidgetDamage();
    }
    else
    {
      QueueWidgetDamage();
    }
#else
  if (!PtWidgetIsRealized(mWidget))
  {
	return NS_OK;  
  }
  
  nsRegionRectSet *regionRectSet = nsnull;
  PhRect_t        extent;
  PhArea_t        area;
  nsRect          temp_rect;
  PRUint32        len;
  PRUint32        i;

  mUpdateArea->Union(*aRegion);
  if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
  {
    return NS_ERROR_FAILURE;
  }

  PtWidgetArea( mWidget, &area ); // parent coords
  //printf("nsWidget::InvalidateRegion mWidget=<%p> area=<%d,%d,%d,%d>\n", mWidget, area.pos.x, area.pos.y, area.size.w, area.size.h);
  if ((PtWidgetIsClass(mWidget, PtWindow)) || (PtWidgetIsClass(mWidget, PtRegion)))
  {
    printf("nsWidget::InvalidateRegion mWidget=<%p> is a PtWindow\n");
	area.pos.x = area.pos.y = 0;  
  }
  
  len = regionRectSet->mRectsLen;
  for (i=0;i<len;++i)
  {
    nsRegionRect *r = &(regionRectSet->mRects[i]);

    //printf("nsWidget::InvalidateRegion r=<%d,%d,%d,%d>\n",r->x, r->y, r->width, r->height );

    temp_rect.SetRect(r->x, r->y, r->width, r->height);
	  
    extent.ul.x = temp_rect.x + area.pos.x; // convert widget coords to parent
    extent.ul.y = temp_rect.y + area.pos.y;
    extent.lr.x = extent.ul.x + temp_rect.width - 1;
    extent.lr.y = extent.ul.y + temp_rect.height - 1;
		
    //printf("nsWidget::InvalidateRegion damaging widget=<%p> %d rect=<%d,%d,%d,%d>\n", mWidget, i,extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y);
#if 0
      PtDamageExtent( mWidget, &extent );
#else
       if (aIsSynchronous)
        {
          UpdateWidgetDamage();
        }
        else
        {
          QueueWidgetDamage();
        }
#endif
  }

  // drop the const.. whats the right thing to do here?
  ((nsIRegion*)aRegion)->FreeRects(regionRectSet);

  ((nsIRegion*)aRegion)->SetTo(0,0,0,0);
#endif

  return NS_OK;
}

/* Returns only the rect that is inside of all my parents, problem on test 9 */
PRBool nsWidget::GetParentClippedArea( nsRect &rect )
{
// Traverse parent heirarchy and clip the passed-in rect bounds

  PtArg_t    arg;
  PhArea_t   *area;
  PtWidget_t *parent;
  PhPoint_t  offset;
  nsRect     rect2;
  PtWidget_t *disjoint = PtFindDisjoint( mWidget );

PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea Clipping widget (%p) rect: %d,%d,%d,%d disjointParent=<%p>\n", this, rect.x, rect.y, rect.width, rect.height, disjoint ));

  // convert passed-in rect to absolute window coords first...
  PtWidgetOffset( mWidget, &offset );
  rect.x += offset.x;
  rect.y += offset.y;


PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea screen coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height ));

  parent = PtWidgetParent( mWidget );
  while( parent )
  {
    PtSetArg( &arg, Pt_ARG_AREA, &area, 0 );
    if( PtGetResources( parent, 1, &arg ) == 0 )
    {
      rect2.width = area->size.w;
      rect2.height = area->size.h;

      if ((parent == disjoint) || (PtWidgetIsClass(parent, PtWindow)) 
	      || (PtWidgetIsClass(parent, PtRegion)))
      {
        rect2.x = rect2.y = 0;
	  }
	  else
	  {
        PtWidgetOffset( parent, &offset );
        rect2.x = area->pos.x + offset.x;
        rect2.y = area->pos.y + offset.y;
      }

PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea parent at: %d,%d,%d,%d\n", rect2.x, rect2.y, rect2.width, rect2.height ));

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

PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea new widget coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height ));

      }
    }

    parent = PtWidgetParent( parent );
  }


  // convert rect back to widget coords...
  PtWidgetOffset( mWidget, &offset );
  rect.x -= offset.x;
  rect.y -= offset.y;

PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea final widget coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height ));

  if( rect.width && rect.height )
    return PR_TRUE;
  else
    return PR_FALSE;
}



NS_METHOD nsWidget::Update(void)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Update this=<%p> mWidget=<%p>\n", this, mWidget));

  /* if the widget has been invalidated or damaged then re-draw it */
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
    {    
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetNativeData(mWidget) - mWidget is NULL!\n" ));
    }
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

NS_METHOD nsWidget::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ScrollWidgets - Not Implemented   this=<%p> mWidget=<%p> aDx=<%d aDy=<%d>\n",
    this, mWidget,  aDx, aDy));

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

NS_IMETHODIMP nsWidget::SetTitle(const nsString &aTitle)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetTitle - this=<%p> aTitle=<%s>\n", this, nsAutoCString(aTitle) ));

  //gtk_widget_set_name(mWidget, "foo");
  return NS_OK;
}

NS_METHOD nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetMenuBar - Not Implemented.\n" ));
  return NS_ERROR_FAILURE;
}


NS_METHOD nsWidget::ShowMenuBar( PRBool aShow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ShowMenuBar aShow=<%d>\n",aShow));
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget this=<%p> mRefCnt=<%d> aRect=<%d,%d,%d,%d> aContext=<%p>\n", this, mRefCnt, aRect.x, aRect.y, aRect.width, aRect.height, aContext));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget this=<%p> aParent=<%p> aNativeParent=<%p> mParent=<%p>\n", this, aParent, aNativeParent, mParent));

  if (aParent)
  {
     PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget (%p) nsIWidget parent\n", this));
  }
  else if (aNativeParent)
  {
     PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget (%p) native parent\n",this));
  }
  else if(aAppShell)
  {
     PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::CreateWidget (%p) nsAppShell parent\n",this));
  }
  			
  PtWidget_t *parentWidget = nsnull;

  nsIWidget *baseParent = aInitData && (aInitData->mWindowType == eWindowType_dialog ||
    	aInitData->mWindowType == eWindowType_toplevel ) ?  nsnull : aParent;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget before BaseCreate this=<%p> mParent=<%p> baseParent=<%p>\n", this, mParent, baseParent));

  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);

  mParent = aParent;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget after BaseCreate  mRefCnt=<%d> mBounds=<%d,%d,%d,%d> mContext=<%p> mParent=<%p> baseParent=<%p> aParent=<%p>\n", 
    mRefCnt, mBounds.x, mBounds.y, mBounds.width, mBounds.height, mContext, mParent, baseParent, aParent));

  if( aNativeParent )
  {
    parentWidget = (PtWidget_t*)aNativeParent;
    // we've got a native parent so listen for resizes
    mListenForResizes = PR_TRUE;
  }
  else if( aParent )
  {
    parentWidget = (PtWidget_t*) (aParent->GetNativeData(NS_NATIVE_WIDGET));
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget - No parent specified!\n" ));
  }

  mBounds = aRect;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget before CreateNative this=<%p> mParent=<%p> baseParent=<%p>\n", this, mParent, baseParent));

  CreateNative (parentWidget);

  Resize(aRect.width, aRect.height, PR_FALSE);

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget - bounds=(%i,%i,%i,%i) mRefCnt=<%d>\n", mBounds.x, mBounds.y, mBounds.width, mBounds.height, mRefCnt));

  if( mWidget )
  {
    SetInstance(mWidget, this);
    PtAddCallback( mWidget, Pt_CB_GOT_FOCUS, GotFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_LOST_FOCUS, LostFocusCallback, this );
    PtAddCallback( mWidget, Pt_CB_IS_DESTROYED, DestroyedCallback, this );
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
  //printf("nsWidget::DispatchWindowEvent  status=<%d> convtered=<%d>\n", status, ConvertStatus(status) );

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
  return result;
}


//-------------------------------------------------------------------------
//
// Invokes callback and ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *aEvent,
                                      nsEventStatus &aStatus)
{
#if 0
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent this=<%p> widget=<%p> eventType=<%d> message=<%d> <%s>\n", 
     this, aEvent->widget, aEvent->eventStructType, aEvent->message,
	 (const char *) nsCAutoString(GuiEventToString(aEvent)) ));
#endif

/* Stolen from GTK */

  NS_ADDREF(aEvent->widget);
  if (nsnull != mMenuListener)
  {
    if (NS_MENU_EVENT == aEvent->eventStructType)
    {
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *aEvent));
    }
  }

  aStatus = nsEventStatus_eIgnore;
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(aEvent);
  }

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*aEvent);
  }

   NS_RELEASE(aEvent->widget);

  return NS_OK;
}

//==============================================================
void nsWidget::InitMouseEvent(PhPointerEvent_t *aPhButtonEvent,
                              nsWidget * aWidget,
                              nsMouseEvent &anEvent,
                              PRUint32   aEventType)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::InitMouseEvent \n"));

  //printf("nsWidget::InitMouseEvent click_count=%d\n", aPhButtonEvent->click_count);

  anEvent.message = aEventType;
  anEvent.widget  = aWidget;
  anEvent.eventStructType = NS_MOUSE_EVENT;

  if (aPhButtonEvent != nsnull)
  {
    anEvent.time =      PR_IntervalNow();
    anEvent.isShift =   ( aPhButtonEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhButtonEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhButtonEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
	anEvent.isMeta =	PR_FALSE;
    anEvent.point.x =   aPhButtonEvent->pos.x; 
    anEvent.point.y =   aPhButtonEvent->pos.y;
    anEvent.clickCount = aPhButtonEvent->click_count;
  }
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent)
{
  //printf("nsWidget::DispatchMouseEvent \n");

  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  // call the event callback
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&aEvent);

    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_MOVE: {
//         result = ConvertStatus(mMouseListener->MouseMoved(aEvent));
//         nsRect rect;
//         GetBounds(rect);
//         if (rect.Contains(event.point.x, event.point.y)) {
//           if (mCurrentWindow == NULL || mCurrentWindow != this) {
//             printf("Mouse enter");
//             mCurrentWindow = this;
//           }
//         } else {
//           printf("Mouse exit");
//         }

      } break;

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(aEvent));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(aEvent));
        result = ConvertStatus(mMouseListener->MouseClicked(aEvent));
        break;

    case NS_DRAGDROP_DROP:
      printf("nsWidget::DispatchMouseEvent, NS_DRAGDROP_DROP\n");
      break;

    default:
      break;

    } // switch
  }
  return result;
}

//-------------------------------------------------------------------------
// Old icky code I am trying to replace!
//-------------------------------------------------------------------------
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
  event.isMeta = PR_FALSE;
  event.clickCount = 0;       /* hack  makes the mouse not work */
  
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
PRUint32 nsWidget::nsConvertKey(unsigned long keysym, PRBool *aIsChar )
{

  struct nsKeyConverter {
    PRUint32       vkCode; // Platform independent key code
    unsigned long  keysym; // Photon key_sym key code
    PRBool         isChar;
  };

 struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     Pk_Cancel, PR_FALSE },
  { NS_VK_BACK,       Pk_BackSpace, PR_FALSE },
  { NS_VK_TAB,        Pk_Tab, PR_FALSE },
  { NS_VK_CLEAR,      Pk_Clear, PR_FALSE },
  { NS_VK_RETURN,     Pk_Return, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_L, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_R, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_L, PR_FALSE },
  { NS_VK_SHIFT,      Pk_Shift_R, PR_FALSE },
  { NS_VK_CONTROL,    Pk_Control_L, PR_FALSE },
  { NS_VK_CONTROL,    Pk_Control_R, PR_FALSE },
  { NS_VK_ALT,        Pk_Alt_L, PR_FALSE },
  { NS_VK_ALT,        Pk_Alt_R, PR_FALSE },
  { NS_VK_PAUSE,      Pk_Pause, PR_FALSE },
  { NS_VK_CAPS_LOCK,  Pk_Caps_Lock, PR_FALSE },
  { NS_VK_ESCAPE,     Pk_Escape, PR_FALSE },
  { NS_VK_SPACE,      Pk_space, PR_TRUE },
  { NS_VK_PAGE_UP,    Pk_Pg_Up, PR_FALSE },
  { NS_VK_PAGE_DOWN,  Pk_Pg_Down, PR_FALSE },
  { NS_VK_END,        Pk_End, PR_FALSE },
  { NS_VK_HOME,       Pk_Home, PR_FALSE },
  { NS_VK_LEFT,       Pk_Left, PR_FALSE },
  { NS_VK_UP,         Pk_Up, PR_FALSE },
  { NS_VK_RIGHT,      Pk_Right, PR_FALSE },
  { NS_VK_DOWN,       Pk_Down, PR_FALSE },
  { NS_VK_PRINTSCREEN, Pk_Print, PR_FALSE },
  { NS_VK_INSERT,     Pk_Insert, PR_FALSE },
  { NS_VK_DELETE,     Pk_Delete, PR_FALSE },
  { NS_VK_COMMA,      Pk_comma, PR_TRUE },
  { NS_VK_PERIOD,     Pk_period, PR_TRUE },
  { NS_VK_SLASH,      Pk_slash, PR_TRUE },
  { NS_VK_OPEN_BRACKET,  Pk_bracketleft, PR_TRUE },
  { NS_VK_CLOSE_BRACKET, Pk_bracketright, PR_TRUE },
  { NS_VK_QUOTE,         Pk_quotedbl, PR_TRUE },
  { NS_VK_MULTIPLY,      Pk_KP_Multiply, PR_TRUE },
  { NS_VK_ADD,           Pk_KP_Add, PR_TRUE },
  { NS_VK_COMMA,         Pk_KP_Separator, PR_FALSE },
  { NS_VK_SUBTRACT,      Pk_KP_Subtract, PR_TRUE },
  { NS_VK_PERIOD,        Pk_KP_Decimal, PR_TRUE },
  { NS_VK_DIVIDE,        Pk_KP_Divide, PR_TRUE },
  { NS_VK_RETURN,        Pk_KP_Enter, PR_FALSE },
  { NS_VK_INSERT,        Pk_KP_0, PR_FALSE },
  { NS_VK_END,           Pk_KP_1, PR_FALSE },
  { NS_VK_DOWN,          Pk_KP_2, PR_FALSE },
  { NS_VK_PAGE_DOWN,     Pk_KP_3, PR_FALSE },
  { NS_VK_LEFT,          Pk_KP_4, PR_FALSE },
  { NS_VK_NUMPAD5,       Pk_KP_5, PR_FALSE },
  { NS_VK_RIGHT,         Pk_KP_6, PR_FALSE },
  { NS_VK_HOME,          Pk_KP_7, PR_FALSE },
  { NS_VK_UP,            Pk_KP_8, PR_FALSE },
  { NS_VK_PAGE_UP,       Pk_KP_9, PR_FALSE }
  };

  const int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  //printf("nsWidget::nsConvertKey - Looking for <%x> length=<%d>\n", keysym, length);

  if (aIsChar)
  {
	/* Default this to TRUE */
    *aIsChar = PR_TRUE;
  }
	
  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  if (keysym >= Pk_a && keysym <= Pk_z)
     return keysym - Pk_a + NS_VK_A;

  if (keysym >= Pk_A && keysym <= Pk_Z)
     return keysym - Pk_A + NS_VK_A;

  if (keysym >= Pk_0 && keysym <= Pk_9)
     return keysym - Pk_0 + NS_VK_0;
		  
  if (keysym >= Pk_F1 && keysym <= Pk_F24)
  {
     *aIsChar = PR_FALSE;
     return keysym - Pk_F1 + NS_VK_F1;
  }

  for (int i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
    {
      //printf("nsWidget::nsConvertKey - Converted <%x> to <%x>\n", keysym, nsKeycodes[i].vkCode);
      if (aIsChar)
        *aIsChar = (nsKeycodes[i].isChar);
      return (nsKeycodes[i].vkCode);
    }
  }

  //NS_WARNING("nsWidget::nsConvertKey - Did not Find Key! - Not Implemented\n");

  return((int) 0);
}

//==============================================================
void nsWidget::InitKeyEvent(PhKeyEvent_t *aPhKeyEvent,
                            nsWidget * aWidget,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::InitKeyEvent\n"));

  if (aPhKeyEvent != nsnull)
  {
  
    anEvent.message = aEventType;
    anEvent.widget  = aWidget;
    anEvent.eventStructType = NS_KEY_EVENT;
    anEvent.nativeMsg = (void *)aPhKeyEvent;
    anEvent.time =      PR_IntervalNow();
    anEvent.point.x = 0; 
    anEvent.point.y = 0;


    PRBool IsChar;
    unsigned long keysym;
	
    if (Pk_KF_Sym_Valid & aPhKeyEvent->key_flags)
        keysym = nsConvertKey(aPhKeyEvent->key_sym, &IsChar);
    else
		/* Need this to support key release events on numeric key pad */
    	keysym = nsConvertKey(aPhKeyEvent->key_cap, &IsChar);


    //printf("nsWidget::InitKeyEvent EventType=<%d> key_cap=<%lu> converted=<%lu> IsChar=<%d>\n", aEventType, aPhKeyEvent->key_cap, keysym, IsChar);

    anEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
    anEvent.isMeta =    PR_FALSE;

    if ((aEventType == NS_KEY_PRESS) && (IsChar == PR_TRUE))
	{
      anEvent.charCode = aPhKeyEvent->key_sym;
      anEvent.keyCode =  0;  /* I think the spec says this should be 0 */
      //printf("nsWidget::InitKeyEvent charCode=<%d>\n", anEvent.charCode);

      if ((anEvent.isControl) || (anEvent.isAlt))
      {
        anEvent.charCode = aPhKeyEvent->key_cap;
	  }
	  else
	  {
	    anEvent.isShift = anEvent.isControl = anEvent.isAlt = anEvent.isMeta = PR_FALSE;
	  }
    }
	else
    {
      anEvent.charCode = 0; 
      anEvent.keyCode  =  (keysym  & 0x00FF);
    }

    //printf("nsWidget::InitKeyEvent Modifiers Valid=<%d,%d,%d> Shift=<%d> Control=<%d> Alt=<%d> Meta=<%d>\n", (aPhKeyEvent->key_flags & Pk_KF_Scan_Valid), (aPhKeyEvent->key_flags & Pk_KF_Sym_Valid), (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid), anEvent.isShift, anEvent.isControl, anEvent.isAlt, anEvent.isMeta);
  }
}


PRBool  nsWidget::DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchKeyEvent Got a Key Event aPhEkyEvent->key_mods:<%x> aPhEkyEvent->key_flags:<%x> aPhEkyEvent->key_sym=<%x> aPhEkyEvent->key_caps=<%x>\n",aPhKeyEvent->key_mods, aPhKeyEvent->key_flags, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap));
  NS_ASSERTION(aPhKeyEvent, "nsWidget::DispatchKeyEvent a NULL PhKeyEvent was passed in");

  nsKeyEvent keyEvent;
  PRBool result = PR_FALSE;

  if ( (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid) == 0)
  {
     //printf("nsWidget::DispatchKeyEvent throwing away invalid key: Modifiers Valid=<%d,%d,%d> this=<%p>\n",
	 //    (aPhKeyEvent->key_flags & Pk_KF_Scan_Valid), (aPhKeyEvent->key_flags & Pk_KF_Sym_Valid), (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid), this );

     return PR_TRUE;
  }


  if ( PtIsFocused(mWidget) != 2)
  {
     //printf("nsWidget::DispatchKeyEvent Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(mWidget));
     return PR_FALSE;
  }
  
  if ( ( aPhKeyEvent->key_cap == Pk_Shift_L )
       || ( aPhKeyEvent->key_cap == Pk_Shift_R )
       || ( aPhKeyEvent->key_cap == Pk_Control_L )
       || ( aPhKeyEvent->key_cap ==  Pk_Control_R )
       || ( aPhKeyEvent->key_cap ==  Pk_Num_Lock )
       || ( aPhKeyEvent->key_cap ==  Pk_Scroll_Lock )
     )
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchKeyEvent Ignoring SHIFT or CONTROL keypress\n"));
    return PR_TRUE;
  }

  nsWindow *w = (nsWindow *) this;

#if 0
printf("nsWidget::DispatchKeyEvent KeyEvent Info: this=<%p> key_flags=<%lu> key_mods=<%lu>  key_sym=<%lu> key_cap=<%lu> key_scan=<%d> Focused=<%d>\n",
	this, aPhKeyEvent->key_flags, aPhKeyEvent->key_mods, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap, aPhKeyEvent->key_scan, PtIsFocused(mWidget));
#endif	

  w->AddRef();
  
  if (aPhKeyEvent->key_flags & Pk_KF_Key_Down)
  {
    //printf("nsWidget::DispatchKeyEvent Before Key Down \n");
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_DOWN);
    result = w->OnKey(keyEvent); 
    //printf("nsWidget::DispatchKeyEvent after Key_Down event result=<%d>\n", result);

    //printf("nsWidget::DispatchKeyEvent Before Key Press\n");
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_PRESS);
    result = w->OnKey(keyEvent); 
  }
  else if (aPhKeyEvent->key_flags & Pk_KF_Key_Repeat)
  {
    //printf("nsWidget::DispatchKeyEvent Before Key Press\n");
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_PRESS);
    result = w->OnKey(keyEvent);   
  }
  else if (PkIsKeyDown(aPhKeyEvent->key_flags) == 0)
  {
    //printf("nsWidget::DispatchKeyEvent Before Key Up\n");
    InitKeyEvent(aPhKeyEvent, this, keyEvent, NS_KEY_UP);
    result = w->OnKey(keyEvent); 
  }

  w->Release();

  //printf("nsWidget::DispatchKeyEvent after events result=<%d>\n", result);
 
  return result;
}



//-------------------------------------------------------------------------
//
// the nsWidget raw event callback for all nsWidgets in this toolkit
//
//-------------------------------------------------------------------------
int nsWidget::RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  //PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::RawEventHandler raweventhandler widget=<%p> this=<%p> IsFocused=<%d>\n", widget, data,  PtIsFocused(widget) ));
  //printf("nsWidget::RawEventHandler raweventhandler widget=<%p> this=<%p> IsFocused=<%d>\n", widget, data,  PtIsFocused(widget));

  // Get the window which caused the event and ask it to process the message
  nsWidget *someWidget = (nsWidget*) data;

  // kedl, shouldn't handle events if the window is being destroyed
  if ( (someWidget) &&
        (someWidget->mIsDestroying == PR_FALSE) &&
        (someWidget->HandleEvent( cbinfo ))
      )
  {
        return( Pt_END ); // Event was consumed
  }

  return( Pt_CONTINUE );
}



PRBool nsWidget::HandleEvent( PtCallbackInfo_t* aCbInfo )
{
  PRBool  result = PR_TRUE; // call the default nsWindow proc
  int     err;
  PhEvent_t* event = aCbInfo->event;


//printf("nsWidget::HandleEvent entering this=<%p> mWidget=<%p> Event Consumed=<%d>  Event=<%s>\n",
//	this, mWidget, (event->processing_flags & Ph_CONSUMED), (const char *) nsCAutoString(PhotonEventToString(event)) );

#if 0 
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::HandleEvent entering this=<%p> mWidget=<%p> Event Consumed=<%d>  Event=<%s>\n",
	this, mWidget, (event->processing_flags & Ph_CONSUMED),  (const char *) nsCAutoString( (const PRUnichar *) PhotonEventToString(event)) ));
#endif
    
    /* Photon 2 added a Consumed flag which indicates a  previous receiver of the */
    /* event has processed it */
	if (event->processing_flags & Ph_CONSUMED)
	{
		return PR_TRUE;
	}

    if (IgnoreEvent == event->timestamp)
    {
      //printf("nsWidget::HandleEvent  Ignoring Event!\n");
      return PR_TRUE;
    }
    switch ( event->type )
    {
      case Ph_EV_KEY:
       {
	    PhKeyEvent_t* keyev = (PhKeyEvent_t*) PhGetData( event );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::HandleEvent keyev=<%p>\n", keyev));
		result = DispatchKeyEvent(keyev);
       }
        break;

      case Ph_EV_DRAG:
	    {
          PhDragEvent_t* ptrev = (PhDragEvent_t*) PhGetData( event );
          nsMouseEvent   theMouseEvent;

		  //printf("nsWidget::HandleEvent Ph_EV_DRAG subtype=<%d> flags=<%d>\n", event->subtype, ptrev->flags );
          switch(event->subtype)
		  {
		  case Ph_EV_DRAG_BOUNDARY: 
  		    //printf("nsWidget::HandleEvent Ph_EV_DRAG_BOUNDARY\n");
			break;
		  case Ph_EV_DRAG_COMPLETE: 
            {  
 		      nsMouseEvent      theMouseEvent;
              PhPointerEvent_t* ptrev2 = (PhPointerEvent_t*) PhGetData( event );

              //printf("nsWidget::HandleEvent Ph_EV_DRAG_COMPLETE\n");
              ScreenToWidget( ptrev2->pos );
              InitMouseEvent(ptrev2, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_UP );
              result = DispatchMouseEvent(theMouseEvent);
            }
			break;
		  case Ph_EV_DRAG_INIT: 
  		    //printf("nsWidget::HandleEvent Ph_EV_DRAG_INIT\n");
			break;
		  case Ph_EV_DRAG_KEY_EVENT: 
  		    //printf("nsWidget::HandleEvent Ph_EV_DRAG_KEY_EVENT\n");
			break;
		  case Ph_EV_DRAG_MOTION_EVENT: 
  		    {
            PhPointerEvent_t* ptrev2 = (PhPointerEvent_t*) PhGetData( event );
            ScreenToWidget( ptrev2->pos );
			//printf("nsWidget::HandleEvent Ph_EV_DRAG_MOTION_EVENT pos=(%d,%d)\n", ptrev2->pos.x,ptrev2->pos.y );
  	        InitMouseEvent(ptrev2, this, theMouseEvent, NS_MOUSE_MOVE );
            result = DispatchMouseEvent(theMouseEvent);
			}
			break;
		  case Ph_EV_DRAG_MOVE: 
  		    //printf("nsWidget::HandleEvent Ph_EV_DRAG_BOUNDARY\n");
			break;
		  case Ph_EV_DRAG_START: 
  		    //printf("nsWidget::HandleEvent Ph_EV_DRAG_START\n");
			break;			
		  }
		}
        break;

     case Ph_EV_PTR_MOTION_BUTTON:
      {
        PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    nsMouseEvent   theMouseEvent;

		//printf("nsWidget::HandleEvent Ph_EV_PTR_MOTION_BUTTON\n");

#if 0
        if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
        {
          /* Initiate a PhInitDrag() */
            /* I am not sure what rect and boundary should be but this works */
			PhRect_t rect = {0,0,0,0};
			PhRect_t boundary = {0,0,640,480};
			err=PhInitDrag( PtWidgetRid(mWidget), ( Ph_DRAG_KEY_MOTION | Ph_DRAG_TRACK ),&rect, &boundary, aCbInfo->event->input_group , NULL, NULL, NULL);
            if (err==-1)
			{
			  NS_WARNING("nsWidget::HandleEvent PhInitDrag Failed!\n");
			  result = NS_ERROR_FAILURE;
			}
        }
#endif

        if( ptrev )
        {
          ScreenToWidget( ptrev->pos );
  	 	  //printf("nsWidget::HandleEvent Ph_EV_PTR_MOTION_BUTTON pos=(%d,%d)\n", ptrev->pos.x,ptrev->pos.y );

 	      InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
          result = DispatchMouseEvent(theMouseEvent);
        }
      }
      break;

      case Ph_EV_PTR_MOTION_NOBUTTON:
       {
	    PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    nsMouseEvent   theMouseEvent;

		//printf("nsWidget::HandleEvent Ph_EV_PTR_MOTION_NOBUTTON\n");

        if( ptrev )
        {
          ScreenToWidget( ptrev->pos );
  	 	  //printf("nsWidget::HandleEvent Ph_EV_PTR_MOTION_NOBUTTON pos=(%d,%d)\n", ptrev->pos.x,ptrev->pos.y );

 	      InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
          result = DispatchMouseEvent(theMouseEvent);
        }
       }
	    break;

      case Ph_EV_BUT_PRESS:
       {
	    PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
        nsMouseEvent   theMouseEvent;

		//printf("nsWidget::HandleEvent Ph_EV_BUT_PRESS this=<%p>\n", this);

        if (ptrev)
        {
          ScreenToWidget( ptrev->pos );

          if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
			InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_DOWN );
          else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
			InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_RIGHT_BUTTON_DOWN );
          else // middle button
			InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MIDDLE_BUTTON_DOWN );

		  result = DispatchMouseEvent(theMouseEvent);
        }
       }
	    break;		

      case Ph_EV_BUT_RELEASE:
       {
	    PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
        nsMouseEvent      theMouseEvent;

		//printf("nsWidget::HandleEvent Ph_EV_BUT_RELEASE this=<%p> event->subtype=<%d>\n", this,event->subtype);

        if (event->subtype==Ph_EV_RELEASE_REAL || event->subtype==Ph_EV_RELEASE_PHANTOM)
        {
          if (ptrev)
          {
            ScreenToWidget( ptrev->pos );
            if ( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
              InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_UP );
            else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
              InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_RIGHT_BUTTON_UP );
            else // middle button
              InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MIDDLE_BUTTON_UP );

            result = DispatchMouseEvent(theMouseEvent);
          }
        }
        else if (event->subtype==Ph_EV_RELEASE_OUTBOUND)
        {
          if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
          {
            /* Initiate a PhInitDrag() */
            /* I am not sure what rect and boundary should be but this works */
			PhRect_t rect = {0,0,0,0};
			PhRect_t boundary = {0,0,640,480};
			err=PhInitDrag( PtWidgetRid(mWidget), ( Ph_DRAG_KEY_MOTION | Ph_DRAG_TRACK ),&rect, &boundary, aCbInfo->event->input_group , NULL, NULL, NULL, NULL, NULL);
            if (err==-1)
			{
			  NS_WARNING("nsWidget::HandleEvent PhInitDrag Failed!\n");
			  result = NS_ERROR_FAILURE;
			}
          }
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
            //result = DispatchStandardEvent( NS_MOUSE_EXIT );
            break;
          default:
            break;
        }
        break;

#if 0
      case Ph_EV_WM:
       {
	    PhWindowEvent_t* wmev = (PhWindowEvent_t*) PhGetData( event );
   	    printf("nsWidget::HandleEvent Ph_EV_WM  this=<%p> subtype=<%d> vent_f=<%d>\n",
          this, event->subtype, wmev->event_f);

        switch( wmev->event_f )
        {
          case Ph_WM_FOCUS:
            if ( wmev->event_state == Ph_WM_EVSTATE_FOCUS )
              result = DispatchStandardEvent(NS_GOTFOCUS);
            else
              result = DispatchStandardEvent(NS_LOSTFOCUS);
            break;
        }

       }
	    break;
#endif

      case Ph_EV_EXPOSE:
      {
        int reg;
        int parreg;

         reg = event->emitter.rid;
         parreg = PtWidgetRid(mWidget);
         PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::HandleEvent Ph_EV_EXPOSE reg=<%d> gLastUnrealizedRegion=<%d> parreg=<%d> gLastUnrealizedRegionsParent=<%d>\n",reg, gLastUnrealizedRegion, parreg, gLastUnrealizedRegionsParent));

//         if (parreg == gLastUnrealizedRegionsParent)
         if (reg == gLastUnrealizedRegion && parreg == gLastUnrealizedRegionsParent)
         {
           PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::HandleEvent Ph_EV_EXPOSE returning TRUE\n"));
           result = PR_TRUE;
         }
         else
         {
           PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::HandleEvent Ph_EV_EXPOSE returning FALSE\n"));
           result = PR_FALSE;
         }
       }
       break;
    }

  return result;
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

#ifdef ENABLE_DAMAGE_QUEUE_HOLDOFF
    int Global_Widget_Hold_Count;
      Global_Widget_Hold_Count =  PtHold();
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::InitDamageQueue PtHold Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, this));
#endif

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::QueueWidgetDamage this=<%p> mDmgQueueInited=<%d>\n", this, mDmgQueueInited));

  if( !mDmgQueueInited )
    InitDamageQueue();

  if( mWidget && mDmgQueueInited )
  {
    /* This  keeps everything from drawing */
    //if (!PtWidgetIsRealized(mWidget))
    //  return;
	  
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged this=<%p> mWidget=<%p>\n", this, mWidget));

    if (mWidget == NULL)
    {
      NS_ASSERTION(0, "nsWidget::UpdateWidgetDamaged mWidget is NULL");
      return;
	}

    if( !PtWidgetIsRealized( mWidget ))	
    {
      //NS_ASSERTION(0, "nsWidget::UpdateWidgetDamaged skipping update because widget is not Realized");
      return;
    }
	
    RemoveDamagedWidget( mWidget );

    if (mUpdateArea->IsEmpty())
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged skipping update because mUpdateArea IsEmpty() this=<%p>\n", this));
      return;
    }

  PhRect_t         extent;
  PhArea_t         area;
  nsRegionRectSet *regionRectSet = nsnull;
  PRUint32         len;
  PRUint32         i;
  nsRect           temp_rect;

    PtWidgetArea( mWidget, &area );

    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged mWidget=<%p> area=<%d,%d,%d,%d>\n", mWidget, area.pos.x, area.pos.y, area.size.w, area.size.h));

#if 1
    if ((PtWidgetIsClass(mWidget, PtWindow)) || (PtWidgetIsClass(mWidget, PtRegion)))
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged mWidget=<%p> is a PtWindow, set x,y=0\n", mWidget));
	  area.pos.x = area.pos.y = 0;  
    }
#endif

#if defined(ENABLE_DOPAINT)
  // HACK, call RawDrawFunc directly instead of Damaging the widget
  printf("nsWidget::UpdateWidgetDamaged calling doPaint\n");
  doPaint();
#else
    if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
    {
	  NS_ASSERTION(0,"nsWidget::UpdateWidgetDamaged Error mUpdateArea->GetRects returned NULL");
      return;
    }


#if 0
    int Global_Widget_Hold_Count;
      Global_Widget_Hold_Count =  PtContainerHold(mWidget);
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged PtHold Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, this));
#endif

    len = regionRectSet->mRectsLen;

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged %d rects to damage\n", len));

    for (i=0;i<len;++i)
    {
      nsRegionRect *r = &(regionRectSet->mRects[i]);
      temp_rect.SetRect(r->x, r->y, r->width, r->height);

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged %d temp_rect=(%d,%d,%d,%d)\n", i, r->x, r->y, r->width, r->height));
	  
      if( GetParentClippedArea(temp_rect))
      {
        extent.ul.x = temp_rect.x + area.pos.x;
        extent.ul.y = temp_rect.y + area.pos.y;
        extent.lr.x = extent.ul.x + temp_rect.width - 1;
        extent.lr.y = extent.ul.y + temp_rect.height - 1;

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged this=<%p> mWidget=<%p> PtDamageExtent=(%d,%d,%d,%d)\n", this, mWidget, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y));

        PtDamageExtent( mWidget, &extent );
      }
      else
	  {
        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged SKIPPING due to GetParentClippedArea this=<%p> extent=(%d,%d,%d,%d)\n", this, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y));
	  }
    }
  
    // drop the const.. whats the right thing to do here?
    mUpdateArea->FreeRects(regionRectSet);
#endif

    //PtFlush();  //HOLD_HACK

#if 0
    Global_Widget_Hold_Count =  PtContainerRelease(mWidget);
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged PtHold/PtRelease Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, this));
#endif

    mUpdateArea->SetTo(0,0,0,0);
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

	  /* If removing the item empties the queue */
      if( nsnull == mDmgQueue )
      {
        mDmgQueueInited = PR_FALSE;

#ifdef ENABLE_DAMAGE_QUEUE_HOLDOFF
        /* The matching PtHold is in nsWidget::InitDamageQueue */
        int Global_Widget_Hold_Count;
          Global_Widget_Hold_Count =  PtRelease();
          PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::RemoveDamagedWidget PtHold/PtRelease Global_Widget_Hold_Count=<%d> this=<%p>\n", Global_Widget_Hold_Count, this));
#endif

        if( mWorkProcID )
          PtAppRemoveWorkProc( nsnull, mWorkProcID );

        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::RemoveDamagedWidget finished removing last item\n"));
      }
    }
}


int nsWidget::WorkProc( void *data )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::WorkProc begin\n"));

  DamageQueueEntry **dq = (DamageQueueEntry **) data;
  	  
  if( dq && (*dq))
  {
    DamageQueueEntry *dqe = *dq;
    DamageQueueEntry *last_dqe;
    PhRect_t extent;
    PhArea_t area;
    PtWidget_t *w;

/* This uses the new mUpdateRect */

    while( dqe )
    {
      if( PtWidgetIsRealized( dqe->widget ))
      {
#if defined( ENABLE_DOPAINT)
        printf("nsWidget::WorkProc calling doPaint\n");
        dqe->inst->doPaint();
#else		
        nsRegionRectSet *regionRectSet = nsnull;
        PRUint32         len;
        PRUint32         i;
        nsRect           temp_rect;

        PtWidgetArea( dqe->widget, &area ); // parent coords
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> area=<%d,%d,%d,%d>\n", dqe->widget, area.pos.x, area.pos.y, area.size.w, area.size.h));
//printf("nsWidget::WorkProc PtWindow origin at (%d,%d) IsEmpty=<%d>\n", area.pos.x, area.pos.y, dqe->inst->mUpdateArea->IsEmpty());

// this was enabled... what was it doing?
#if 0
        /* Is forcing the damage to 0,0 really a good idea here?? */
        if ((PtWidgetIsClass(dqe->widget, PtWindow)) || (PtWidgetIsClass(dqe->widget, PtRegion)))
		{
		  printf("nsWidget::WorkProc Forced PtWindow origin to 0,0\n");
		  area.pos.x = area.pos.y = 0;
		}
#endif

		if (dqe->inst->mUpdateArea->IsEmpty())
		{
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> mUpdateArea empty\n"));

          extent.ul.x = 0; //area.pos.x; // convert widget coords to parent
          extent.ul.y = 0; //area.pos.y;
          extent.lr.x = extent.ul.x + area.size.w - 1;
          extent.lr.y = extent.ul.y + area.size.h - 1;

          PtWidget_t *aPtWidget;
		  nsWidget   *aWidget = GetInstance( (PtWidget_t *) dqe->widget );
          aPtWidget = (PtWidget_t *)aWidget->GetNativeData(NS_NATIVE_WIDGET);		  
          PtDamageExtent( aPtWidget, &extent);
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> %d PtDamageExtent=<%d,%d,%d,%d> next=<%p>\n",  aPtWidget, i, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y, dqe->next));
		}
		else
		{
          dqe->inst->mUpdateArea->GetRects(&regionRectSet);

          len = regionRectSet->mRectsLen;
          for (i=0;i<len;++i)
          {
            nsRegionRect *r = &(regionRectSet->mRects[i]);

            // convert widget coords to parent
            temp_rect.SetRect(r->x, r->y, r->width, r->height);
            extent.ul.x = temp_rect.x; // + area.pos.x;
            extent.ul.y = temp_rect.y; // + area.pos.y;
            extent.lr.x = extent.ul.x + temp_rect.width - 1;
            extent.lr.y = extent.ul.y + temp_rect.height - 1;
		
            PtWidget_t *aPtWidget;
		    nsWidget   *aWidget = GetInstance( (PtWidget_t *) dqe->widget );
            aPtWidget = (PtWidget_t *)aWidget->GetNativeData(NS_NATIVE_WIDGET);		  
            PtDamageExtent( aPtWidget, &extent);

            PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> %d PtDamageExtent=<%d,%d,%d,%d> next=<%p>\n", aPtWidget, i, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y, dqe->next));
          }
  
          dqe->inst->mUpdateArea->FreeRects(regionRectSet);
          dqe->inst->mUpdateArea->SetTo(0,0,0,0);
		}
#endif  //end of doPaint Hack
      }

      last_dqe = dqe;
      dqe = dqe->next;
      delete last_dqe;
    }

    *dq = nsnull;
    mDmgQueueInited = PR_FALSE;
    int Global_Widget_Hold_Count;

#ifdef ENABLE_DAMAGE_QUEUE_HOLDOFF
	/* The matching PtHold is in nsWidget::InitDamageQueue */
      Global_Widget_Hold_Count =  PtRelease();
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::WorkProc end, PtHold/PtRelease Global_Widget_Hold_Count=<%d>\n", Global_Widget_Hold_Count));
#endif

#if 0
    Global_Widget_Hold_Count = PtFlush();  /* this may not be necessary  since after PtRelease */
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::WorkProc  PtFlush Global_Widget_Hold_Count=<%d>\n", Global_Widget_Hold_Count));
#endif
  }

  return Pt_END;
}

int nsWidget::GotFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWidget *pWidget = (nsWidget *) data;

/*
  if (widget->parent)
    printf("nsWidget::GotFocusCallback widget->parent=<%p> PtIsFocused(widget)=<%d>\n", widget->parent, PtIsFocused(widget));
  else
    printf("nsWidget::GotFocusCallback widget->parent=<%p>\n", widget->parent);
*/  
  
  if ((!widget->parent) || (PtIsFocused(widget) != 2))
  {
     //printf("nsWidget::GotFocusCallback widget->parent=<%p>\n", widget->parent);
     return Pt_CONTINUE;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::GotFocusCallback pWidget=<%p>\n", pWidget));
printf("GOTFOCUS %X\n ", widget);
  pWidget->DispatchStandardEvent(NS_GOTFOCUS);

  return Pt_CONTINUE;
}

int nsWidget::LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWidget *pWidget = (nsWidget *) data;

  if ((widget->parent) && (PtIsFocused(widget) != 2))
  {
     PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::LostFocusCallback Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(widget) ));
     printf("nsWidget::LostFocusCallback Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(widget));
     return Pt_CONTINUE;
  }
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::LostFocusCallback pWidget=<%p>\n", pWidget));

//  pWidget->DispatchStandardEvent(NS_LOSTFOCUS);

  return Pt_CONTINUE;
}

int nsWidget::DestroyedCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWidget *pWidget = (nsWidget *) data;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DestroyedCallback pWidget=<%p> mWidget=<%p> mIsDestroying=<%d>\n", pWidget, pWidget->mWidget, pWidget->mIsDestroying));
  if (!pWidget->mIsDestroying)
  {
    pWidget->RemoveDamagedWidget(pWidget->mWidget);
    pWidget->OnDestroy();
  }
   
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

//#if defined(DEBUG)
/**************************************************************/
/* This was stolen from widget/src/xpwidgets/nsBaseWidget.cpp */
nsAutoString nsWidget::GuiEventToString(nsGUIEvent * aGuiEvent)
{
  NS_ASSERTION(nsnull != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName; eventName.AssignWithConversion("UNKNOWN");

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName = (const PRUnichar *)  _name ; break

  switch(aGuiEvent->message)
  {
    _ASSIGN_eventName(NS_BLUR_CONTENT,"NS_BLUR_CONTENT");
    _ASSIGN_eventName(NS_CONTROL_CHANGE,"NS_CONTROL_CHANGE");
    _ASSIGN_eventName(NS_CREATE,"NS_CREATE");
    _ASSIGN_eventName(NS_DESTROY,"NS_DESTROY");
    _ASSIGN_eventName(NS_DRAGDROP_GESTURE,"NS_DND_GESTURE");
    _ASSIGN_eventName(NS_DRAGDROP_DROP,"NS_DND_DROP");
    _ASSIGN_eventName(NS_DRAGDROP_ENTER,"NS_DND_ENTER");
    _ASSIGN_eventName(NS_DRAGDROP_EXIT,"NS_DND_EXIT");
    _ASSIGN_eventName(NS_DRAGDROP_OVER,"NS_DND_OVER");
    _ASSIGN_eventName(NS_FOCUS_CONTENT,"NS_FOCUS_CONTENT");
    _ASSIGN_eventName(NS_FORM_SELECTED,"NS_FORM_SELECTED");
    _ASSIGN_eventName(NS_FORM_CHANGE,"NS_FORM_CHANGE");
    _ASSIGN_eventName(NS_FORM_INPUT,"NS_FORM_INPUT");
    _ASSIGN_eventName(NS_FORM_RESET,"NS_FORM_RESET");
    _ASSIGN_eventName(NS_FORM_SUBMIT,"NS_FORM_SUBMIT");
    _ASSIGN_eventName(NS_GOTFOCUS,"NS_GOTFOCUS");
    _ASSIGN_eventName(NS_IMAGE_ABORT,"NS_IMAGE_ABORT");
    _ASSIGN_eventName(NS_IMAGE_ERROR,"NS_IMAGE_ERROR");
    _ASSIGN_eventName(NS_IMAGE_LOAD,"NS_IMAGE_LOAD");
    _ASSIGN_eventName(NS_KEY_DOWN,"NS_KEY_DOWN");
    _ASSIGN_eventName(NS_KEY_PRESS,"NS_KEY_PRESS");
    _ASSIGN_eventName(NS_KEY_UP,"NS_KEY_UP");
    _ASSIGN_eventName(NS_LOSTFOCUS,"NS_LOSTFOCUS");
    _ASSIGN_eventName(NS_MENU_SELECTED,"NS_MENU_SELECTED");
    _ASSIGN_eventName(NS_MOUSE_ENTER,"NS_MOUSE_ENTER");
    _ASSIGN_eventName(NS_MOUSE_EXIT,"NS_MOUSE_EXIT");
    _ASSIGN_eventName(NS_MOUSE_LEFT_BUTTON_DOWN,"NS_MOUSE_LEFT_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_LEFT_BUTTON_UP,"NS_MOUSE_LEFT_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_LEFT_CLICK,"NS_MOUSE_LEFT_CLICK");
    _ASSIGN_eventName(NS_MOUSE_LEFT_DOUBLECLICK,"NS_MOUSE_LEFT_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_BUTTON_DOWN,"NS_MOUSE_MIDDLE_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_BUTTON_UP,"NS_MOUSE_MIDDLE_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_CLICK,"NS_MOUSE_MIDDLE_CLICK");
    _ASSIGN_eventName(NS_MOUSE_MIDDLE_DOUBLECLICK,"NS_MOUSE_MIDDLE_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MOVE,"NS_MOUSE_MOVE");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_BUTTON_DOWN,"NS_MOUSE_RIGHT_BTN_DOWN");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_BUTTON_UP,"NS_MOUSE_RIGHT_BTN_UP");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_CLICK,"NS_MOUSE_RIGHT_CLICK");
    _ASSIGN_eventName(NS_MOUSE_RIGHT_DOUBLECLICK,"NS_MOUSE_RIGHT_DBLCLICK");
    _ASSIGN_eventName(NS_MOVE,"NS_MOVE");
    _ASSIGN_eventName(NS_PAGE_LOAD,"NS_PAGE_LOAD");
    _ASSIGN_eventName(NS_PAGE_UNLOAD,"NS_PAGE_UNLOAD");
    _ASSIGN_eventName(NS_PAINT,"NS_PAINT");
    _ASSIGN_eventName(NS_MENU_CREATE,"NS_MENU_CREATE");
    _ASSIGN_eventName(NS_MENU_DESTROY,"NS_MENU_DESTROY");
    _ASSIGN_eventName(NS_MENU_ACTION, "NS_MENU_ACTION");
    _ASSIGN_eventName(NS_XUL_BROADCAST, "NS_XUL_BROADCAST");
    _ASSIGN_eventName(NS_XUL_COMMAND_UPDATE, "NS_XUL_COMMAND_UPDATE");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_NEXT,"NS_SB_LINE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_PREV,"NS_SB_LINE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_NEXT,"NS_SB_PAGE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_PREV,"NS_SB_PAGE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_POS,"NS_SB_POS");
    _ASSIGN_eventName(NS_SIZE,"NS_SIZE");

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %d",aGuiEvent->message);
      
      eventName = (const PRUnichar *) buf;
    }
    break;
  }
  
  return nsAutoString(eventName);
}



nsAutoString nsWidget::PhotonEventToString(PhEvent_t * aPhEvent)
{
  NS_ASSERTION(nsnull != aPhEvent,"cmon, null photon gui event.");

  nsAutoString eventName; eventName.AssignWithConversion("UNKNOWN");

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName = (const PRUnichar *) _name ; break
 
   switch ( aPhEvent->type )
    {
	  _ASSIGN_eventName(Ph_EV_KEY, "Ph_EV_KEY");
	  _ASSIGN_eventName(Ph_EV_DRAG, "Ph_EV_DRAG");
	  _ASSIGN_eventName(Ph_EV_PTR_MOTION_BUTTON, "Ph_EV_PTR_MOTION_BUTTON");
	  _ASSIGN_eventName(Ph_EV_PTR_MOTION_NOBUTTON, "Ph_EV_PTR_MOTION_NOBUTTON");
	  _ASSIGN_eventName(Ph_EV_BUT_PRESS, "Ph_EV_BUT_PRESS");
	  _ASSIGN_eventName(Ph_EV_BUT_RELEASE, "Ph_EV_BUT_RELEASE");
	  _ASSIGN_eventName(Ph_EV_BOUNDARY, "Ph_EV_BOUNDARY");
	  _ASSIGN_eventName(Ph_EV_WM, "Ph_EV_WM");
	  _ASSIGN_eventName(Ph_EV_EXPOSE, "Ph_EV_EXPOSE");	  

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %d",aPhEvent->type);
      
      eventName = (const PRUnichar *) buf;
    }
    break;
  }
  
  return nsAutoString(eventName);
}
//#endif
