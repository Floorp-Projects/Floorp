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

#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"

NS_IMPL_ADDREF(nsScrollbar)
NS_IMPL_RELEASE(nsScrollbar)

//=================================================================
/*	Constructor for the scrollbar
 *  @update  gpk 08/27/98
 *  @param   aX -- x offset in widget local coordinates
 *  @param   aY -- y offset in widget local coordinates
 *  @return  PR_TRUE if the pt is contained in the widget
 */
nsScrollbar::nsScrollbar(PRBool aIsVertical)
{
    strcpy(gInstanceClassName, "nsScrollbar");
    mIsVertical  = aIsVertical;
    mLineIncrement = 0;

}

//=================================================================
/*	Creates a scrollbar with aParent as the widgets parent
 *  @update  dc 09/16/98
 *  @param  aParent -- The parent of this widget
 *  @param  aHandleEventFunction -- Eventhandler
 *  @param  aContext -- Device context for widget
 *  @param  aToolkit -- Tookkit for all the widgets
 *	@param	aInitData -- 
 *  @return nothing
 */
NS_IMETHODIMP nsScrollbar::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  mParent = aParent;
  aParent->AddChild(this);
	
	WindowPtr window = nsnull;

  if (aParent) 
  	{
    window = (WindowPtr) aParent->GetNativeData(NS_NATIVE_WIDGET);
  	} 
 	else 
 		if (aAppShell) 
 			{
   		window = (WindowPtr) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  		}

  mIsMainWindow = PR_FALSE;
  mWindowMadeHere = PR_TRUE;
	mWindowRecord = (WindowRecord*)window;
	mWindowPtr = (WindowPtr)window;
  
  NS_ASSERTION(window!=nsnull,"The WindowPtr for the widget cannot be null")
	if (window)
		{
	  InitToolkit(aToolkit, aParent);
	  // InitDeviceContext(aContext, parentWidget);
		
		// Set the bounds to the local rect
		SetBounds(aRect);
		
		// Convert to macintosh coordinates		
		Rect r;
		nsRectToMacRect(aRect,r);
				
		mWindowRegion = NewRgn();
		SetRectRgn(mWindowRegion,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);		 

	  // save the event callback function
	  mEventCallback = aHandleEventFunction;
	  
	  mMouseDownInButton = PR_FALSE;
	  mWidgetArmed = PR_FALSE;

	  //InitCallbacks("nsButton");
	  InitDeviceContext(mContext, (nsNativeWidget)mWindowPtr);
		}
		return NS_OK;
}

//=================================================================
/*	Creates a scrollbar with aParent as the widgets parent, this should never be called
 *  @update  dc 09/16/98
 *  @param  aParent -- The parent of this widget
 *  @param  aHandleEventFunction -- Eventhandler
 *  @param  aContext -- Device context for widget
 *  @param  aToolkit -- Tookkit for all the widgets
 *	@param	aInitData -- 
 *  @return nothing
 */
NS_IMETHODIMP nsScrollbar::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
nsWindow		*theNsWindow=nsnull;
nsRefData		*theRefData;

	if(0!=aParent)
		{
		theRefData = (nsRefData*)(((WindowPeek)aParent)->refCon);
		theNsWindow = (nsWindow*)theRefData->GetTopWidget();
		}
		
	if(nsnull!=theNsWindow)
		Create(theNsWindow, aRect,aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);

	//NS_ERROR("This Widget must not use this Create method");
	return NS_OK;

}

//-------------------------------------------------------------------------
//
// nsScrollbar destructor
//
//-------------------------------------------------------------------------
nsScrollbar::~nsScrollbar()
{
}


/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
*/ 
nsresult nsScrollbar::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIScrollbarIID, NS_ISCROLLBAR_IID);
    if (aIID.Equals(kIScrollbarIID)) {
        *aInstancePtr = (void*) ((nsIScrollbar*)this);
        AddRef();
        return NS_OK;
    }

    return nsWindow::QueryInterface(aIID,aInstancePtr);
}

//=================================================================
/*	Handle a mousedown event
 *  @update  dc 09/16/98
 *  @param  nsMouseEvent -- the mouse event to handle
 *  @return -- True if the event was handled
 */
PRBool 
nsScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 	result;

	switch (aEvent.message)
		{
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			mMouseDownInButton = PR_TRUE;
			DrawWidget();
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_LEFT_BUTTON_UP:
			mMouseDownInButton = PR_FALSE;
			DrawWidget();
			if(mWidgetArmed==PR_TRUE)
				result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_EXIT:
			DrawWidget();
			mWidgetArmed = PR_FALSE;
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_ENTER:
			DrawWidget();
			mWidgetArmed = PR_TRUE;
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_MOVE:
			if(mWidgetArmed)
				{
				
				//this->SetPosition();
				this->DrawWidget();
				}
			break;
		}
		
	return result;
}

//-------------------------------------------------------------------------
/*  Draw in the different modes depending on the state of the mouse and buttons
 *  @update  dc 08/31/98
 *  @param   aMouseInside -- A boolean indicating if the mouse is inside the control
 *  @return  nothing is returned
 */
void
nsScrollbar::DrawWidget()
{
PRInt32							offx,offy;
nsRect							therect,tr;
Rect								macrect;
GrafPtr							theport;
RGBColor						blackcolor = {0,0,0};
RgnHandle						thergn;


	CalcOffset(offx,offy);
	GetPort(&theport);
	::SetPort(mWindowPtr);
	::SetOrigin(-offx,-offy);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	thergn = ::NewRgn();
	::GetClip(thergn);
	::ClipRect(&macrect);
	::PenNormal();
	::RGBForeColor(&blackcolor);
	
	// Frame the general scrollbar
	::FrameRect(&macrect);


	// draw the thumb
	if(mIsVertical)
		{
		tr.width = therect.width;
		tr.height = mThumbSize;
		tr.x = therect.x;
		tr.y = therect.y+mPosition;
		}
	else
		{
		tr.width = mThumbSize;
		tr.height = therect.height;
		tr.x = therect.x+mPosition;
		tr.y = therect.y;
		}
		
	nsRectToMacRect(tr,macrect);
	::PaintRect(&macrect);
		
	::PenSize(1,1);
	::SetClip(thergn);
	::SetOrigin(0,0);
	::SetPort(theport);
}

//=================================================================
/*	set the maximum range of a scroll bar
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- the maximum to set this to
 *  @return -- If a good size was returned
 */
NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    mMaxRange = aEndRange;
		return (NS_OK);
}

//=================================================================
/*	get the maximum range of a scroll bar
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current maximum this slider can be
 *  @return -- If a good size was returned
 */
NS_METHOD nsScrollbar::GetMaxRange(PRUint32& aMaxRange)
{
	aMaxRange = mMaxRange;
	return (NS_OK);
}

//=================================================================
/*	set the current position of the slider
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current value to set the slider position to.
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
	if(aPos>=0)
		{
		mPosition = aPos;
		this->DrawWidget();
  	return (NS_OK);
  	}
  else
  	return(NS_ERROR_FAILURE);
}


//=================================================================
/*	get the current position of the slider
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current slider position.
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetPosition(PRUint32& aPos)
{
  aPos = mPosition;
  return (NS_OK);
}


//=================================================================
/*	Set the hieght of a vertical, or width of a horizontal scroll bar thumb control
 *  @update  dc 09/16/98
 *  @param  aSize -- the size to set the thumb control to
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
    if (aSize > 0) 
    	{
    	mThumbSize = aSize;
    	this->DrawWidget();
    	return(NS_OK);
    	}
    else
			return(NS_ERROR_FAILURE);
}


//=================================================================
/*	get the height of a vertical, or width of a horizontal scroll bar thumb control
 *  @update  dc 09/16/98
 *  @param  aSize -- the size to set the thumb control to
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
 
	aSize = mThumbSize;
	return(NS_OK);
}


//=================================================================
/*	Set the increment of the scroll bar
 *  @update  dc 09/16/98
 *  @param  aLineIncrement -- the control increment
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
  if (aLineIncrement > 0) 
  	{
    mLineIncrement = aLineIncrement;
  	}
	return(NS_OK);
}


//=================================================================
/*	Get the increment of the scroll bar
 *  @update  dc 09/16/98
 *  @param aLineIncrement -- the control increment
 *  @return NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aLineIncrement)
{
		aLineIncrement = mLineIncrement;
    return(NS_OK);
}


//-------------------------------------------------------------------------
//
// Set all scrolling parameters
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{

	mThumbSize = (((int)aThumbSize) > 0?aThumbSize:1);
	mMaxRange  = (((int)aMaxRange) > 0?aMaxRange:10);
	mLineIncrement = (((int)aLineIncrement) > 0?aLineIncrement:1);

	mPosition    = ((int)aPosition) > mMaxRange ? mMaxRange-1 : ((int)aPosition);

	 return(NS_OK);
}


//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnPaint(nsPaintEvent & aEvent)
{
	DrawWidget();
  return PR_FALSE;
}


PRBool nsScrollbar::OnResize(nsSizeEvent &aEvent)
{

  return nsWindow::OnResize(aEvent);
  //return PR_FALSE;
}

//-------------------------------------------------------------------------
int nsScrollbar::AdjustScrollBarPosition(int aPosition) 
{
  int maxRange=0,cap,sliderSize=0;

	cap = maxRange - sliderSize;
  return aPosition > cap ? cap : aPosition;
}

//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos)
{
PRBool 	result = PR_TRUE;
int 		newPosition=0;
int			range;

	switch (aEvent.message) 
		{
	  // scroll one line right or down
	  case NS_SCROLLBAR_LINE_NEXT: 
	  	{
	    newPosition += mLineIncrement;
	    PRUint32 max = GetMaxRange(range) - GetThumbSize(range);
	    if (newPosition > (int)max) 
	        newPosition = (int)max;

	    // if an event callback is registered, give it the chance
	    // to change the increment
	    if (mEventCallback) 
	    	{
	      aEvent.position = newPosition;
	      result = ConvertStatus((*mEventCallback)(&aEvent));
	      newPosition = aEvent.position;
	    	}
	    break;
	  	}

      // scroll one line left or up
      case NS_SCROLLBAR_LINE_PREV: 
      {
      newPosition -= mLineIncrement;
      if (newPosition < 0) 
          newPosition = 0;

      // if an event callback is registered, give it the chance
      // to change the decrement
      if (mEventCallback) 
      	{
        aEvent.position = newPosition;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        newPosition = aEvent.position;
      	}
      break;
      }

      // Scrolls one page right or down
      case NS_SCROLLBAR_PAGE_NEXT: 
      {
      PRUint32 max = GetMaxRange(range) - GetThumbSize(range);
      if (newPosition > (int)max) 
          newPosition = (int)max;

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) 
      	{
        aEvent.position = newPosition;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        newPosition = aEvent.position;
      	}
      break;
      }

      // Scrolls one page left or up.
    case NS_SCROLLBAR_PAGE_PREV: 
      {
      //XtVaGetValues(mWidget, XmNvalue, &newPosition, nsnull);
      if (newPosition < 0) 
          newPosition = 0;

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) 
      	{
        aEvent.position = newPosition;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        newPosition = aEvent.position;
      	}

      break;
      }

      // Scrolls to the absolute position. The current position is specified by 
      // the cPos parameter.
      case NS_SCROLLBAR_POS: 
      	{
        newPosition = cPos;

        // if an event callback is registered, give it the chance
        // to change the increment
        if (mEventCallback) 
        	{
          aEvent.position = newPosition;
          result = ConvertStatus((*mEventCallback)(&aEvent));
          newPosition = aEvent.position;
        	}
        break;
      	}
    }
    return result;
}


