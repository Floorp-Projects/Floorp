/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
}

//-------------------------------------------------------------------------
//
// Create the native scrollbar widget
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::CreateNative (PtWidget_t * parentWindow)
{
  nsresult  res = NS_ERROR_FAILURE;
  PhPoint_t pos;
  PhDim_t   dim;
  PtArg_t   arg[5];

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width;
  dim.h = mBounds.height;

  PtSetArg( &arg[0], Pt_ARG_ORIENTATION, mOrientation, 0 );
  PtSetArg( &arg[1], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[2], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[3], Pt_ARG_FLAGS, 0, Pt_GETS_FOCUS);
	PtSetArg( &arg[4], Pt_ARG_BASIC_FLAGS, Pt_ALL_INLINES, -1 );
  mWidget = PtCreateWidget( PtScrollbar, parentWindow, 5, arg );
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
	
	if( mWidget ) {
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
	
	if( mWidget ) {
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
	nsresult res = NS_ERROR_FAILURE;
	
	if( mWidget ) {
		PtArg_t arg;
		
		PtSetArg( &arg, Pt_ARG_GAUGE_VALUE, aPos, 0);
		if (PtSetResources(mWidget, 1, &arg) == 0)
		   res = NS_OK;
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
	  PtArg_t arg;
	  int *pos;
	  PtSetArg(&arg, Pt_ARG_GAUGE_VALUE, &pos, 0);
	  if (PtGetResources(mWidget, 1, &arg) == 0) {
		  aPos = (PRUint32)*pos;
		  res = NS_OK;
	  }
  }
  return res;
}

//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize (PRUint32 aSize)
{
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
	nsresult res = NS_ERROR_FAILURE;
	
	if( mWidget ) {
		PtArg_t arg[4];
		
		PtSetArg( &arg[0], Pt_ARG_MAXIMUM , aMaxRange, 0 );
		PtSetArg( &arg[1], Pt_ARG_SLIDER_SIZE , aThumbSize, 0 );
		PtSetArg( &arg[2], Pt_ARG_INCREMENT, aLineIncrement, 0 );
		PtSetArg( &arg[3], Pt_ARG_GAUGE_VALUE, aPosition, 0);
		if( PtSetResources( mWidget, 4, arg ) == 0 )
		   res = NS_OK;
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
	
	if (mEventCallback) {
		aEvent.position = cPos;
		result = ConvertStatus((*mEventCallback)(&aEvent));
	}  
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
	
	scroll_event.message = NS_SCROLLBAR_POS;
	scroll_event.widget = (nsWidget *) me;
	scroll_event.eventStructType = NS_SCROLLBAR_EVENT;
	thePos = theScrollbarCallback->position;
	
	switch (theScrollbarCallback->action) {
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
		break;
	}
	me->OnScroll(scroll_event, thePos);
	return (Pt_CONTINUE);
}
