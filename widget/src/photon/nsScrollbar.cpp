/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

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
 */

#include <Pt.h>
#include "nsPhWidgetLog.h"

#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"


NS_IMPL_ADDREF_INHERITED(nsScrollbar, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsScrollbar, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsScrollbar, nsIScrollbar, nsIWidget)

//-------------------------------------------------------------------------
//
// nsScrollbar constructor
//
//-------------------------------------------------------------------------
nsScrollbar::nsScrollbar (PRBool aIsVertical):nsWidget (), nsIScrollbar ()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::nsScrollbar this=<%p> IsVertical=<%d>\n", this, aIsVertical));

  NS_INIT_REFCNT ();

  mOrientation = (aIsVertical) ? Pt_VERTICAL : Pt_HORIZONTAL;
}

//-------------------------------------------------------------------------
//
// nsScrollbar destructor
//
//-------------------------------------------------------------------------
nsScrollbar::~nsScrollbar ()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::~nsScrollbar this=<%p>\n", this));
}

//-------------------------------------------------------------------------
//
// Create the native scrollbar widget
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::CreateNative (PtWidget_t * parentWindow)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::CreateNative this=<%p>\n", this));

  nsresult  res = NS_ERROR_FAILURE;
  PhPoint_t pos;
  PhDim_t   dim;
  PtArg_t   arg[5];

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width;
  dim.h = mBounds.height;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::CreateNative at (%d,%d) w,h=(%d,%d)\n",
    mBounds.x, mBounds.y, mBounds.width, mBounds.height));
    
  PtSetArg( &arg[0], Pt_ARG_ORIENTATION, mOrientation, 0 );
  PtSetArg( &arg[1], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[2], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[3], Pt_ARG_FLAGS, 0, Pt_GETS_FOCUS);
  mWidget = PtCreateWidget( PtScrollbar, parentWindow, 4, arg );
  if( mWidget )
  {
    res = NS_OK;

    /* Add an Activate Callback */
    PtAddCallback(mWidget, Pt_CB_SCROLL_MOVE, handle_scroll_move_event, this);
  }

  return res;
}

//-------------------------------------------------------------------------
//
// Define the range settings
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetMaxRange (PRUint32 aEndRange)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::SetMaxRange to %d\n", aEndRange));

  if( mWidget )
  {
    PtArg_t arg;
    PtSetArg( &arg, Pt_ARG_MAXIMUM, aEndRange, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
      res = NS_OK;
  }

  return res;
}


//-------------------------------------------------------------------------
//
// Return the range settings
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetMaxRange (PRUint32 & aMaxRange)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::GetMaxRange\n"));

  if( mWidget )
  {
    PtArg_t  arg;
    int     *max;

    PtSetArg( &arg, Pt_ARG_MAXIMUM, &max, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      aMaxRange = *max;
      res = NS_OK;
    }
  }

  return res;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetPosition (PRUint32 aPos)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::SetPosition to <%d>\n", aPos));

  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PtArg_t  arg;

#ifdef PHOTON1_ONLY
    PtSetArg( &arg, Pt_ARG_SCROLL_POSITION, aPos, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
    {
      res = NS_OK;
    }
#endif

  }

  return res;
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetPosition (PRUint32 & aPos)
{
  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PtArg_t  arg;
    int     *pos;
#ifdef PHOTON1_ONLY
    PtSetArg( &arg, Pt_ARG_SCROLL_POSITION, &pos, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      aPos = *pos;
      res = NS_OK;
    }
#endif
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::GetPosition position=<%d>\n", aPos));

  return res;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize (PRUint32 aSize)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::SetThumbSize aSize=<%d>\n", aSize));
  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PtArg_t  arg;

    PtSetArg( &arg, Pt_ARG_SLIDER_SIZE, aSize, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
    {
      res = NS_OK;
    }
  }

  return res;
}


//-------------------------------------------------------------------------
//
// Get the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetThumbSize (PRUint32 & aThumbSize)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::GetThumbSize\n"));

  if( mWidget )
  {
    PtArg_t  arg;
    int     *size;

    PtSetArg( &arg, Pt_ARG_SLIDER_SIZE, &size, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      aThumbSize = *size;
      res = NS_OK;
    }
  }

  return res;
}


//-------------------------------------------------------------------------
//
// Set the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetLineIncrement (PRUint32 aLineIncrement)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::SetLineIncrement to %d \n", aLineIncrement));

  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PtArg_t  arg;

    PtSetArg( &arg, Pt_ARG_INCREMENT, aLineIncrement, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
    {
      res = NS_OK;
    }
  }

  return res;
}


//-------------------------------------------------------------------------
//
// Get the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetLineIncrement (PRUint32 & aLineInc)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::GetLineIncrement\n"));

  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PtArg_t  arg;
    int     *incr;

    PtSetArg( &arg, Pt_ARG_INCREMENT, &incr, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      aLineInc = *incr;
      res = NS_OK;
    }
  }

  return res;
}


//-------------------------------------------------------------------------
//
// Set all scrolling parameters
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetParameters (PRUint32 aMaxRange, PRUint32 aThumbSize,
	       PRUint32 aPosition, PRUint32 aLineIncrement)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::SetParameters this=<%p> MaxRange=<%d> ThumbSize=<%d> Position=<%d> LineIncrement=<%d>\n",
    this, aMaxRange, aThumbSize, aPosition, aLineIncrement));

  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PtArg_t arg[5];

    PtSetArg( &arg[0], Pt_ARG_MAXIMUM , aMaxRange, 0 );
    PtSetArg( &arg[1], Pt_ARG_SLIDER_SIZE , aThumbSize, 0 );
#ifdef PHOTON1_ONLY
    PtSetArg( &arg[2], Pt_ARG_SCROLL_POSITION , aPosition, 0 );
#endif
    PtSetArg( &arg[3], Pt_ARG_INCREMENT, aLineIncrement, 0 );

    if( PtSetResources( mWidget, 4, arg ) == 0 )
    {
      res = NS_OK;
    }
  }

  return res;
}

//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
// --- This funciton is not necessary ----
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnScroll (nsScrollbarEvent & aEvent, PRUint32 cPos)
{
  PRBool result = PR_TRUE;
//  float  newPosition;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::OnScroll cPos=<%d> aEvent.message=<%d>\n", cPos, aEvent.message));

  if (mEventCallback)
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsScrollbar::OnScroll Inside mEventCallback portion\n", cPos));

    aEvent.position = cPos;
    result = ConvertStatus((*mEventCallback)(&aEvent));
//	newPosition = aEvent.position;
  }  
  else
    PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsScrollbar::OnScroll Error no mEventCallback defined\n"));
  
  return result;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
int nsScrollbar::handle_scroll_move_event (PtWidget_t *aWidget, void *aData, PtCallbackInfo_t *aCbinfo )
{
  nsScrollbar             *me = (nsScrollbar *) aData;
  nsScrollbarEvent        scroll_event;
  PRUint32                thePos = 0;
  PtScrollbarCallback_t   *theScrollbarCallback = (PtScrollbarCallback_t *) aCbinfo->cbdata;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsScrollbar::handle_activate_event me=<%p> new position=<%d>\n",me, theScrollbarCallback->position));

  scroll_event.message = NS_SCROLLBAR_POS;
  scroll_event.widget = (nsWidget *) me;
  scroll_event.eventStructType = NS_SCROLLBAR_EVENT;
  thePos = theScrollbarCallback->position;

  switch (theScrollbarCallback->action)
  {
   case Pt_SCROLL_DECREMENT:
     scroll_event.message = NS_SCROLLBAR_LINE_PREV;
     break;
   case Pt_SCROLL_INCREMENT:
     scroll_event.message = NS_SCROLLBAR_LINE_NEXT;
     break;	  
   case Pt_SCROLL_PAGE_INCREMENT:
     scroll_event.message = NS_SCROLLBAR_PAGE_NEXT;
     break;
   case Pt_SCROLL_PAGE_DECREMENT:
     scroll_event.message = NS_SCROLLBAR_PAGE_PREV;
     break;
   case NS_SCROLLBAR_POS:
     scroll_event.message = NS_SCROLLBAR_POS;
     break;	 	    
   default:
     PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsScrollbar::handle_activate_event  Invalid Scroll Type!\n"));
   	 break;
  }
  
  me->OnScroll(scroll_event, thePos);

  return (Pt_CONTINUE);
}
