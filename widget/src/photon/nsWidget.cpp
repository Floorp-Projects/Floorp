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
#include "nsWidget.h"
#include "nsWindow.h"
#include "nsIDeviceContext.h"
#include "nsIAppShell.h"
#include "nsGfxCIID.h"
#include "nsIComponentManager.h"
#include "nsIFontMetrics.h"
#include "nsIRollupListener.h"

#include <Pt.h>
#include "PtRawDrawContainer.h"

// BGR, not RGB - REVISIT
#define NSCOLOR_TO_PHCOLOR(g,n) \
  g.red=NS_GET_B(n); \
  g.green=NS_GET_G(n); \
  g.blue=NS_GET_R(n);

//static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

/* Define and Initialize global variables */
static nsIRollupListener *gRollupListener = nsnull;
static nsIWidget *gRollupWidget = nsnull;

/* Enable this to queue widget damage, this should be ON by default */
#define ENABLE_DAMAGE_QUEUE

/* Enable this causing extra redraw when the RELEASE is called */
#define ENABLE_DAMAGE_QUEUE_HOLDOFF

/* Initialize Static nsWidget class members */
DamageQueueEntry  *nsWidget::mDmgQueue = nsnull;
PtWorkProcId_t    *nsWidget::mWorkProcID = nsnull;
PRBool             nsWidget::mDmgQueueInited = PR_FALSE;
nsILookAndFeel    *nsWidget::sLookAndFeel = nsnull;
PRUint32           nsWidget::sWidgetCount = 0;


nsAutoString GuiEventToString(nsGUIEvent * aGuiEvent);

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
  mIsDestroying = PR_TRUE;
  if (nsnull != mWidget)
  {
    Destroy();
  }

  if (!sWidgetCount--)
  {
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
//  NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
  return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::ScreenToWidget - Not Implemented.\n" ));
//  NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
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

    // dispatching of the event may cause the reference count to drop
    // to 0 and result in this object being destroyed. To avoid that,
    // add a reference and then release it after dispatching the event
    mRefCnt += 100;
    DispatchStandardEvent(NS_DESTROY);
    mRefCnt -= 100;
  }
}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent(void)
{
  if (mParent) {
    NS_ADDREF(mParent);
  }
  return mParent;
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

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show this=<%p> IsRealized=<%d>\n", this, PtWidgetIsRealized( mWidget )));

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
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show Failed to Realize this=<%p> mWidget=<%p> mWidget->Parent=<%p> parent->IsRealized=<%d> \n", this, mWidget,parent, PtWidgetIsRealized(parent)));
      }

      EnableDamage( mWidget, PR_TRUE );

      PtSetArg(&arg, Pt_ARG_FLAGS, 0, Pt_DELAY_REALIZE);
      PtSetResources(mWidget, 1, &arg);

#if 1
	/* Always add it to the Widget Damage Queue when it gets realized */
    QueueWidgetDamage();
	
#else
    if (!mUpdateArea->IsEmpty())
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show mUpdateArea not empty adding to Damage Queue this=<%p>\n", this));
      QueueWidgetDamage();
    }
    else
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Show mUpdateArea is empty this=<%p>\n", this));
    }
#endif
  }
  else
  {
      PtUnrealizeWidget(mWidget);

      PtSetArg(&arg, Pt_ARG_FLAGS, Pt_DELAY_REALIZE, Pt_DELAY_REALIZE);
      PtSetResources(mWidget, 1, &arg);
  }

  mShown = bState;
  return NS_OK;
}

NS_IMETHODIMP nsWidget::CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CaptureRollupEvents() this = %p , doCapture = %i\n", this, aDoCapture));
  
  if (aDoCapture) {
    //    gtk_grab_add(mWidget);
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
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

NS_IMETHODIMP nsWidget::SetModal(PRBool aModal)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::SetModal - Not Implemented\n"));

  if (!mWidget)
	return NS_ERROR_FAILURE;

  PtWidget_t *toplevel = PtFindDisjoint(mWidget);
  if (!toplevel)
	return NS_ERROR_FAILURE;
	
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Move (%p) from (%ld,%ld) to (%ld,%ld)\n", this, mBounds.x, mBounds.y, aX, aY ));

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
        int err;
        PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
        err=PtSetResources( mWidget, 1, &arg );
        if (err==-1)
        {
           printf("nsWidget::Move ERROR in PtSetRes (%p) to (%ld,%ld)\n", this, aX, aY );
	    }
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
        PhDim_t dim = {aWidth - 2*(*border), aHeight - 2*(*border)};

        EnableDamage( mWidget, PR_FALSE );

        PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
        PtSetResources( mWidget, 1, &arg );

        EnableDamage( mWidget, PR_TRUE );
      }

      Invalidate( aRepaint );
    }
    else
	{
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Resize - mWidget is NULL!\n" ));
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
//kedl      PtSetArg( &arg, Pt_ARG_FLAGS, 0, Pt_BLOCKED );

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
  PtWidget_t *aWidget = GetNativeData(NS_NATIVE_WIDGET);
  long widgetFlags = PtWidgetFlags(aWidget);

//  if (Pt_DISJOINT && widgetFlags)
  {
     PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 - This is a disjoint widget so ignore parent clipping\n"));

     //if (PtWidgetIsRealized(mWidget))
     {
         /* Damage has to be relative Widget coords */
         mUpdateArea->SetTo( rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height );

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
/* HACK! */
#if 0
  else if ( GetParentClippedArea(rect) == PR_TRUE)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 Clipped rect=(%i,%i,%i,%i)\n", rect.x, rect.y, rect.width, rect.height  ));

	  /* Damage has to be relative Widget coords */
      mUpdateArea->SetTo( rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height );

      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 Clipped mUpdateArea=(%i,%i,%i,%i)\n", rect.x - mBounds.x, rect.y - mBounds.y, rect.width, rect.height ));

      if (PtWidgetIsRealized(mWidget))
      {
        if (aIsSynchronous)
        {
          UpdateWidgetDamage();
        }
        else
        {
          QueueWidgetDamage();
        }
      }
      else
	  {
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 Not Relealized skipping Damage Queue\n"));
	  }
  }
  else
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Invalidate 1 Skipping because GetParentClippedArea(rect returned empty rect\n"));
  }
#endif

  return NS_OK;
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
  if (PtWidgetIsClass(mWidget, PtWindow))
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

  return NS_OK;
}

/* Returns only the rect that is inside of all my parents */
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

      if ((parent == disjoint) || (PtWidgetIsClass(parent, PtWindow)))
      {
        rect2.x = rect2.y = 0;
	  }
	  else
	  {
        PtWidgetOffset( parent, &offset );
        rect2.x = area->pos.x + offset.x;
        rect2.y = area->pos.y + offset.y;
      }

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea parent at: %d,%d,%d,%d\n", rect2.x, rect2.y, rect2.width, rect2.height ));

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

//PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::GetParentClippedArea new widget coords: %d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height ));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::Update \n" ));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget this=<%p> mRefCnt=<%d> aRect=<%d,%d,%d,%d> aContext=<%p>\n", this, mRefCnt, aRect.x, aRect.y, aRect.width, aRect.height, aContext));
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget this=<%p> aParent=<%p> aNativeParent=<%p>\n", this, aParent, aNativeParent));

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

 nsIWidget *baseParent = aInitData &&
    (aInitData->mWindowType == eWindowType_dialog ||
     aInitData->mWindowType == eWindowType_toplevel ) ?
    nsnull : aParent;

  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  mParent = aParent;
  NS_IF_ADDREF(mParent);


  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::CreateWidget after BaseCreate  mRefCnt=<%d> mBounds=<%d,%d,%d,%d> mContext=<%p>\n", 
    mRefCnt, mBounds.x, mBounds.y, mBounds.width, mBounds.height, mContext));


  if( aNativeParent )
  {
    parentWidget = (PtWidget_t*)aNativeParent;
    /* Kirk added this back in... */
    //mParent = GetInstance( (PtWidget_t*)aNativeParent );
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
 
  //printf ("kedl: before de\n"); 
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::DispatchEvent this=<%p> widget=<%p> eventType=<%d> message=<%d> <%s>\n", 
     this, aEvent->widget, aEvent->eventStructType, aEvent->message,
	 (const char *) nsCAutoString(GuiEventToString(aEvent))));

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
  { NS_VK_MULTIPLY,   Pk_KP_Multiply, PR_FALSE },
  { NS_VK_ADD,        Pk_KP_Add, PR_FALSE },
  { NS_VK_SEPARATOR,  Pk_KP_Separator, PR_FALSE },
  { NS_VK_SUBTRACT,   Pk_KP_Subtract, PR_FALSE },
  { NS_VK_DECIMAL,    Pk_KP_Decimal, PR_FALSE },
  { NS_VK_DIVIDE,     Pk_KP_Divide, PR_FALSE },
  { NS_VK_RETURN,     Pk_KP_Enter, PR_FALSE },
  { NS_VK_COMMA,      Pk_comma, PR_TRUE },
  { NS_VK_PERIOD,     Pk_period, PR_TRUE },
  { NS_VK_SLASH,      Pk_slash, PR_TRUE },
  { NS_VK_OPEN_BRACKET,  Pk_bracketleft, PR_TRUE },
  { NS_VK_CLOSE_BRACKET, Pk_bracketright, PR_TRUE },
  { NS_VK_QUOTE,         Pk_quotedbl, PR_TRUE },
  { NS_VK_NUMPAD0,       Pk_KP_0, PR_TRUE },
  { NS_VK_NUMPAD1,       Pk_KP_1, PR_TRUE },
  { NS_VK_NUMPAD2,       Pk_KP_2, PR_TRUE },
  { NS_VK_NUMPAD3,       Pk_KP_3, PR_TRUE },
  { NS_VK_NUMPAD4,       Pk_KP_4, PR_TRUE },
  { NS_VK_NUMPAD5,       Pk_KP_5, PR_TRUE },
  { NS_VK_NUMPAD6,       Pk_KP_6, PR_TRUE },
  { NS_VK_NUMPAD7,       Pk_KP_7, PR_TRUE },
  { NS_VK_NUMPAD8,       Pk_KP_8, PR_TRUE },
  { NS_VK_NUMPAD9,       Pk_KP_9, PR_TRUE }
  };

  const int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);
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

  if (keysym >= Pk_KP_0 && keysym <= Pk_KP_9)
     return keysym - Pk_KP_0 + NS_VK_NUMPAD0;
						  
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
	
	keysym = nsConvertKey(aPhKeyEvent->key_cap, &IsChar);

    printf("nsWidget::InitKeyEvent EventType=<%d> key_cap=<%lu> converted=<%lu> IsChar=<%d>\n",
	     aEventType, aPhKeyEvent->key_cap, keysym, IsChar);

    anEvent.isShift =   ( aPhKeyEvent->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = ( aPhKeyEvent->key_mods & Pk_KM_Ctrl )  ? PR_TRUE : PR_FALSE;
    anEvent.isAlt =     ( aPhKeyEvent->key_mods & Pk_KM_Alt )   ? PR_TRUE : PR_FALSE;
    anEvent.isMeta =    PR_FALSE;

    if ((aEventType == NS_KEY_PRESS) && (IsChar == PR_TRUE))
	{
      anEvent.charCode = aPhKeyEvent->key_sym;
      anEvent.keyCode =  0;  /* I think the spec says this should be 0 */
printf("nsWidget::InitKeyEvent charCode=<%d>\n", anEvent.charCode);

      if (anEvent.isControl)
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
	


//  printf("nsWidget::InitKeyEvent Modifiers Valid=<%d,%d,%d> Shift=<%d> Control=<%d> Alt=<%d> Meta=<%d>\n", 
//    (aPhKeyEvent->key_flags & Pk_KF_Scan_Valid), (aPhKeyEvent->key_flags & Pk_KF_Sym_Valid), (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid), anEvent.isShift, anEvent.isControl, anEvent.isAlt, anEvent.isMeta);
  }
}


PRBool  nsWidget::DispatchKeyEvent(PhKeyEvent_t *aPhKeyEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchEvent Got a Key Event aPhEkyEvent->key_mods:<%x> aPhEkyEvent->key_flags:<%x> aPhEkyEvent->key_sym=<%x> aPhEkyEvent->key_caps=<%x>\n",aPhKeyEvent->key_mods, aPhKeyEvent->key_flags, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap));

  NS_ASSERTION(aPhKeyEvent, "nsWidget::DispatchKeyEvent a NULL PhKeyEvent was passed in");

  nsKeyEvent keyEvent;
  PRBool result = PR_FALSE;
   

  if ( (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid) == 0)
  {
     printf("nsWidget::DispatchKeyEvent throwing away invalid key: Modifiers Valid=<%d,%d,%d> this=<%p>\n",
	     (aPhKeyEvent->key_flags & Pk_KF_Scan_Valid), (aPhKeyEvent->key_flags & Pk_KF_Sym_Valid), (aPhKeyEvent->key_flags & Pk_KF_Cap_Valid), this );

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
       || ( aPhKeyEvent->key_cap == Pk_Control_R )
     )
  {
    //PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::DispatchKeyEvent Ignoring SHIFT, CONTROL and ALT keypress\n"));
    return PR_TRUE;
  }

  nsWindow *w = (nsWindow *) this;
  
//printf("nsWidget::DispatchKeyEvent KeyEvent Info: this=<%p> key_flags=<%lu> key_mods=<%lu>  key_sym=<%lu> key_cap=<%lu> key_scan=<%d> Focused=<%d>\n",
//	this, aPhKeyEvent->key_flags, aPhKeyEvent->key_mods, aPhKeyEvent->key_sym, aPhKeyEvent->key_cap, aPhKeyEvent->key_scan, PtIsFocused(mWidget));
	

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
  //printf ("kedl: nswidget raweventhandler\n");

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

    PhEvent_t* event = aCbInfo->event;
    switch ( event->type )
    {
      case Ph_EV_KEY:
       {
	    PhKeyEvent_t* keyev = (PhKeyEvent_t*) PhGetData( event );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::HandleEvent keyev=<%p>\n", keyev));
		result = DispatchKeyEvent(keyev);
       }
        break;

      case Ph_EV_PTR_MOTION_BUTTON:
      case Ph_EV_PTR_MOTION_NOBUTTON:
       {
	    PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
	    nsMouseEvent   theMouseEvent;

        if( ptrev )
        {
          ScreenToWidget( ptrev->pos );
 	      InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MOVE );
          //result = DispatchMouseEvent( ptrev->pos, NS_MOUSE_MOVE );
          result = DispatchMouseEvent(theMouseEvent);
        }
       }
	    break;

      case Ph_EV_BUT_PRESS:
       {
	    PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
        nsMouseEvent   theMouseEvent;
		

        if (gRollupWidget && gRollupListener)
        {
          PtWidget_t *rollupWidget =  gRollupWidget->GetNativeData(NS_NATIVE_WIDGET);
          PtWidget_t *thisWidget = GetNativeData(NS_NATIVE_WIDGET);
          if (rollupWidget != thisWidget && PtFindDisjoint(thisWidget) != rollupWidget)
          {
            gRollupListener->Rollup();
            return;
          }
        }
  

        if( ptrev )
        {
          printf( "nsWidget::HandleEvent Window mouse click: (%ld,%ld)\n", ptrev->pos.x, ptrev->pos.y );

          ScreenToWidget( ptrev->pos );
          if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
          {
			InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_LEFT_BUTTON_DOWN );
          }
          else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
          {
			InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_RIGHT_BUTTON_DOWN );
          }
          else // middle button
          {
			InitMouseEvent(ptrev, this, theMouseEvent, NS_MOUSE_MIDDLE_BUTTON_DOWN );
          }

		  result = DispatchMouseEvent(theMouseEvent);
        }
       }
	    break;

      case Ph_EV_BUT_RELEASE:
       {
	    PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );
        nsMouseEvent      theMouseEvent;

        if (event->subtype==Ph_EV_RELEASE_REAL)
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
   	    printf("nsWidget::HandleEvent Ph_EV_WM  this=<%p> subtype=<%d> vent_f=<%d>\n",
		  this, event->subtype, wmev->event_f);
/*
        switch( wmev->event_f )
        {
          case Ph_WM_FOCUS:
            if ( wmev->event_state == Ph_WM_EVSTATE_FOCUS )
              result = DispatchStandardEvent(NS_GOTFOCUS);
            else
              result = DispatchStandardEvent(NS_LOSTFOCUS);
            break;
        }
*/
       }
	    break;

      case Ph_EV_EXPOSE:
   	    printf("nsWidget::HandleEvent Ph_EV_EXPOSE this=<%p> subtype=<%d> flags=<%d> num_rects=<%d>\n", 
		  this, event->subtype, event->flags, event->num_rects );
        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::HandleEvent Ph_EV_EXPOSE this=<%p> subtype=<%d> flags=<%d> num_rects=<%d>\n", 
		  this, event->subtype, event->flags, event->num_rects ));
		PhRect_t *rects = PhGetRects(event);
		unsigned short rect_count = event->num_rects;
		while(rect_count--)
		{
			printf("\t rect %d (%d,%d) ->  (%d,%d)\n", rect_count,
			  rects->ul.x, rects->ul.y, rects->lr.x, rects->lr.y);
			rects++;
		}
        result = PR_TRUE;
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

PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged 1 \n"));
    if (mUpdateArea->IsEmpty())
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged skipping update because mUpdateArea IsEmpty() this=<%p>\n", this));
      return;
    }

PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged 2 \n"));

  PhRect_t         extent;
  PhArea_t         area;
  nsRegionRectSet *regionRectSet = nsnull;
  PRUint32         len;
  PRUint32         i;
  nsRect           temp_rect;

    PtWidgetArea( mWidget, &area );
PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged 3 \n"));

    if (PtWidgetIsClass(mWidget, PtWindow))
    {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged mWidget=<%p> is a PtWindow, set x,y=0\n", mWidget));
	  area.pos.x = area.pos.y = 0;  
    }

PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged 4 \n"));

    if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
    {
	  NS_ASSERTION(0,"nsWidget::UpdateWidgetDamaged Error mUpdateArea->GetRects returned NULL");
      return;
    }

PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged 5 \n"));


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


      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged temp_rect=(%d,%d,%d,%d)\n", r->x, r->y, r->width, r->height));

	  
      if( GetParentClippedArea(temp_rect))
      {
        extent.ul.x = temp_rect.x + area.pos.x;
        extent.ul.y = temp_rect.y + area.pos.y;
        extent.lr.x = extent.ul.x + temp_rect.width - 1;
        extent.lr.y = extent.ul.y + temp_rect.height - 1;

        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::UpdateWidgetDamaged this=<%p> mWidget=<%p> extent=(%d,%d,%d,%d)\n", this, mWidget, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y));

        PtDamageExtent( mWidget, &extent );
      }
      else
	  {
        PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::UpdateWidgetDamaged SKIPPING due to GetParentClippedArea this=<%p> extent=(%d,%d,%d,%d)\n", this, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y));
	  }
    }
  
    // drop the const.. whats the right thing to do here?
    mUpdateArea->FreeRects(regionRectSet);

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
        nsRegionRectSet *regionRectSet = nsnull;
        PRUint32         len;
        PRUint32         i;
        nsRect           temp_rect;

        PtWidgetArea( dqe->widget, &area ); // parent coords
//        if (PtWidgetIsClass(dqe->widget, PtWindow))
//        {
//	      area.pos.x = area.pos.y = 0;  
//        }
  
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> area=<%d,%d,%d,%d>\n", dqe->widget, area.pos.x, area.pos.y, area.size.w, area.size.h));

        if (PtWidgetIsClass(dqe->widget, PtWindow))
		{
		  printf("nsWidget::WorkProc Forced PtWindow origin to 0,0\n");
		  area.pos.x = area.pos.y = 0;
		}

		if (dqe->inst->mUpdateArea->IsEmpty())
		{
PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> mUpdateArea empty\n"));

          extent.ul.x = area.pos.x; // convert widget coords to parent
          extent.ul.y = area.pos.y;
          extent.lr.x = extent.ul.x + area.size.w - 1;
          extent.lr.y = extent.ul.y + area.size.h - 1;

#if 1
          PtWidget_t *aPtWidget;
		  nsWidget   *aWidget = GetInstance( (PtWidget_t *) dqe->widget );
          aPtWidget = aWidget->GetNativeData(NS_NATIVE_WIDGET);		  
          PtDamageExtent( aPtWidget, &extent);
   PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> %d rect=<%d,%d,%d,%d> next=<%p>\n",  aPtWidget, i, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y, dqe->next));

#else
          PtDamageExtent( dqe->widget, &extent);
   PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> %d rect=<%d,%d,%d,%d> next=<%p>\n", dqe->widget, i, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y, dqe->next));

#endif


		}
		else
		{
        dqe->inst->mUpdateArea->GetRects(&regionRectSet);

        len = regionRectSet->mRectsLen;
        for (i=0;i<len;++i)
        {
          nsRegionRect *r = &(regionRectSet->mRects[i]);
          temp_rect.SetRect(r->x, r->y, r->width, r->height);
          extent.ul.x = temp_rect.x + area.pos.x; // convert widget coords to parent
          extent.ul.y = temp_rect.y + area.pos.y;
          extent.lr.x = extent.ul.x + temp_rect.width - 1;
          extent.lr.y = extent.ul.y + temp_rect.height - 1;
		
#if 1
          PtWidget_t *aPtWidget;
		  nsWidget   *aWidget = GetInstance( (PtWidget_t *) dqe->widget );
          aPtWidget = aWidget->GetNativeData(NS_NATIVE_WIDGET);		  
          PtDamageExtent( aPtWidget, &extent);
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> %d rect=<%d,%d,%d,%d> next=<%p>\n", aPtWidget, i, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y, dqe->next));
#else
          PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsWidget::WorkProc damaging widget=<%p> %d rect=<%d,%d,%d,%d> next=<%p>\n", dqe->widget, i, extent.ul.x, extent.ul.y, extent.lr.x, extent.lr.y, dqe->next));
          PtDamageExtent( dqe->widget, &extent);
#endif
        }
  
        dqe->inst->mUpdateArea->FreeRects(regionRectSet);
        dqe->inst->mUpdateArea->SetTo(0,0,0,0);
		}
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


  if ( PtIsFocused(widget) != 2)
  {
     printf("nsWidget::GetFocusCallback Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(widget));
     return Pt_CONTINUE;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::GetFocusCallback pWidget=<%p>\n", pWidget));

  pWidget->DispatchStandardEvent(NS_GOTFOCUS);

  return Pt_CONTINUE;
}


int nsWidget::LostFocusCallback( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  nsWidget *pWidget = (nsWidget *) data;

  if ( PtIsFocused(widget) != 2)
  {
     PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsWidget::GetFocusCallback Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(widget)));
     printf("nsWidget::GetFocusCallback Not on focus leaf! PtIsFocused(mWidget)=<%d>\n", PtIsFocused(widget));
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
    pWidget->OnDestroy();
    pWidget->RemoveDamagedWidget(pWidget->mWidget);
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

#if DEBUG
/**************************************************************/
/* This was stolen from widget/src/xpwidgets/nsBaseWidget.cpp */
nsAutoString GuiEventToString(nsGUIEvent * aGuiEvent)
{
  NS_ASSERTION(nsnull != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName = "UNKNOWN";

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName = _name ; break

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
      
      eventName = buf;
    }
    break;
  }
  
  return nsAutoString(eventName);
}
#endif
