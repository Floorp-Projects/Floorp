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
#include "nsMacWindow.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"
#include "nsIDeviceContext.h"

NS_IMPL_ADDREF(nsScrollbar);
NS_IMPL_RELEASE(nsScrollbar);

/**-------------------------------------------------------------------------------
 * nsScrollbar Constructor
 *  @update  dc 10/31/98
 * @param aIsVertical -- Tells if the scrollbar had a vertical or horizontal orientation
 */
nsScrollbar::nsScrollbar(PRBool aIsVertical)
{
  NS_INIT_REFCNT();
	strcpy(gInstanceClassName, "nsScrollbar");
	mIsVertical = aIsVertical;
	mLineIncrement = 0;
	mWidgetArmed = PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * The create method for a scrollbar, using a nsIWidget as the parent
 * @update  dc 08/31/98
 * @param  aParent -- the widget which will be this widgets parent in the tree
 * @param  aRect -- The bounds in parental coordinates of this widget
 * @param  aHandleEventFunction -- Procedures to be executed for this widget
 * @param  aContext -- device context to be used by this widget
 * @param  aAppShell -- 
 * @param  aToolkit -- toolkit to be used by this widget
 * @param  aInitData -- Initialization data used by frames
 * @return -- NS_OK if everything was created correctly
 */ 
NS_IMETHODIMP nsScrollbar::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
	nsWindow::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);
  

	//¥TODO: create the native control here


	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Destuctor for the nsScrollbar
 * @update  dc 10/31/98
 */ 
nsScrollbar::~nsScrollbar()
{
}

/**-------------------------------------------------------------------------------
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @update  dc 08/31/98
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

/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this scrollbar
 * @update  dc 08/31/98
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool 
nsScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 	result;

	switch (aEvent.message)
		{
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			mMouseDownInScroll = PR_TRUE;
			StartDraw();
			DrawWidget();
			EndDraw();
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_LEFT_BUTTON_UP:
			mMouseDownInScroll = PR_FALSE;
			StartDraw();
			DrawWidget();
			EndDraw();
			if(mWidgetArmed==PR_TRUE)
				result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_EXIT:
			if( mMouseDownInScroll )
				{
				StartDraw();
				DrawWidget();
				EndDraw();
				mWidgetArmed = PR_FALSE;
				}
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_ENTER:
			if( mMouseDownInScroll )
				{
				StartDraw();
				DrawWidget();
				EndDraw();
				//mWidgetArmed = PR_TRUE;
				mWidgetArmed = PR_FALSE;
				}
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_MOVE:
			if(mWidgetArmed)
				{
				
				//this->SetPosition();
				StartDraw();
				this->DrawWidget();
				EndDraw();
				}
			break;
		}
		
	return result;
}

/**-------------------------------------------------------------------------------
 *  Draw in the scrollbar and thumb
 *  @update  dc 10/16/98
 *  @param   aMouseInside -- A boolean indicating if the mouse is inside the control
 *  @return  nothing is returned
 */
void
nsScrollbar::DrawWidget()
{
	if (mVisible)
	{
		nsRect	theRect;
		Rect		macRect;

		GetBounds(theRect);
		theRect.x = theRect.y = 0;
		nsRectToMacRect(theRect, macRect);
		::FrameRect(&macRect);

		DrawThumb(PR_FALSE);
	}
}

/**-------------------------------------------------------------------------------
 *  Draw or clear the thumb area of the scrollbar
 *  @update  dc 10/31/98
 *  @param   aClear -- A boolean indicating if it will be erased instead of painted
 *  @return  nothing is returned
 */
void
nsScrollbar::DrawThumb(PRBool	aClear)
{
	nsRect							frameRect,thumbRect;
	Rect								macFrameRect,macThumbRect;
	RGBColor						blackcolor = {0,0,0};
	RGBColor						redcolor = {255<<8,0,0};

	GetBounds(frameRect);
	frameRect.x = frameRect.y = 0;
	nsRectToMacRect(frameRect, macFrameRect);
	
	// draw or clear the thumb
	if (mIsVertical)
	{	
		thumbRect.width = frameRect.width;
		thumbRect.height = (frameRect.height * mThumbSize) / mMaxRange;
		thumbRect.x = frameRect.x;
		thumbRect.y = frameRect.y + (frameRect.height * mPosition) / mMaxRange;
	}
	else
	{
		thumbRect.width = (frameRect.width * mThumbSize) / mMaxRange;
		thumbRect.height = frameRect.height;
		thumbRect.y = frameRect.y + (frameRect.width * mPosition) / mMaxRange;
		thumbRect.y = frameRect.y;
	}

	nsRectToMacRect(thumbRect, macThumbRect);
	::InsetRect(&macThumbRect, 1, 1);
	if (aClear == PR_TRUE)
		::EraseRect(&macThumbRect);
	else
	{
		::RGBForeColor(&redcolor);
		::PaintRect(&macThumbRect);
	}
}



/**-------------------------------------------------------------------------------
 *	set the maximum range of a scroll bar
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- the maximum to set this to
 *  @return -- If a good size was returned
 */
NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    mMaxRange = aEndRange;
		return (NS_OK);
}

/**-------------------------------------------------------------------------------
 *	get the maximum range of a scroll bar
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current maximum this slider can be
 *  @return -- If a good size was returned
 */
NS_METHOD nsScrollbar::GetMaxRange(PRUint32& aMaxRange)
{
	aMaxRange = mMaxRange;
	return (NS_OK);
}

/**-------------------------------------------------------------------------------
 *	Set the current position of the slider
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current value to set the slider position to.
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
	if (aPos >= 0)
	{
		StartDraw();
			DrawThumb(PR_TRUE);
			mPosition = aPos;
			DrawThumb(PR_FALSE);
		EndDraw();
  	return (NS_OK);
	}
  else
  	return(NS_ERROR_FAILURE);
}


/**-------------------------------------------------------------------------------
 *	Get the current position of the slider
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current slider position.
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetPosition(PRUint32& aPos)
{
  aPos = mPosition;
  return (NS_OK);
}

/**-------------------------------------------------------------------------------
 *	Set the hieght of a vertical, or width of a horizontal scroll bar thumb control
 *  @update  dc 09/16/98
 *  @param  aSize -- the size to set the thumb control to
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
    if (aSize <= 0) 
  		aSize = 1;
  	mThumbSize = aSize;
  	return(NS_OK);
}

/**-------------------------------------------------------------------------------
 *	get the height of a vertical, or width of a horizontal scroll bar thumb control
 *  @update  dc 09/16/98
 *  @param  aSize -- the size to set the thumb control to
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
	aSize = mThumbSize;
	return(NS_OK);
}

/**-------------------------------------------------------------------------------
 *	Set the increment of the scroll bar
 *  @update  dc 09/16/98
 *  @param  aLineIncrement -- the control increment
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
  if (aLineIncrement > 0) 
    mLineIncrement = aLineIncrement;

	return(NS_OK);
}


/**-------------------------------------------------------------------------------
 *	Get the increment of the scroll bar
 *  @update  dc 09/16/98
 *  @param aLineIncrement -- the control increment
 *  @return NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aLineIncrement)
{
		aLineIncrement = mLineIncrement;
    return(NS_OK);
}


/**-------------------------------------------------------------------------------
 *	Set all the scrollbar parameters
 *  @update  dc 09/16/98
 *  @param aMaxRange -- max range of the scrollbar in relative units
 *  @param aThumbSize -- thumb size, in relative units
 *  @param aPosition -- the thumb position in relative units
 *  @param aLineIncrement -- the increment levelof the scrollbar
 *  @return NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{

	this->SetThumbSize(aThumbSize);
	
	mMaxRange  = (((int)aMaxRange) > 0?aMaxRange:10);
	mLineIncrement = (((int)aLineIncrement) > 0?aLineIncrement:1);

	mPosition    = ((int)aPosition) > mMaxRange ? mMaxRange-1 : ((int)aPosition);

	 return(NS_OK);
}


/**-------------------------------------------------------------------------------
 * The onPaint handleer for a button -- this may change, inherited from windows
 * @param aEvent -- The paint event to respond to
 * @return -- PR_TRUE if painted, false otherwise
 */ 
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnPaint(nsPaintEvent & aEvent)
{
	DrawWidget();
  return PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * Set the scrollbar position
 * @update  dc 10/31/98
 * @Param aPosition -- position in relative units
 * @return -- return the position
 */ 
int nsScrollbar::AdjustScrollBarPosition(int aPosition) 
{
int maxRange=0,cap,sliderSize=0;

	cap = maxRange - sliderSize;
  return aPosition > cap ? cap : aPosition;
}

/**-------------------------------------------------------------------------------
 * Deal with scrollbar messages (actually implemented only in nsScrollbar)
 * @update  dc 08/31/98
 * @Param aEvent -- the event to handle
 * @Param cPos -- the current position
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool nsScrollbar::OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos)
{
PRBool 				result = PR_TRUE;
PRUint32 			newPosition=0;
PRUint32			range;

	switch (aEvent.message) {
	  // scroll one line right or down
	  case NS_SCROLLBAR_LINE_NEXT: {
	    newPosition += mLineIncrement;
	    PRUint32 max = GetMaxRange(range) - GetThumbSize(range);
	    if (newPosition > (int)max) 
	        newPosition = (int)max;

	    // if an event callback is registered, give it the chance
	    // to change the increment
	    if (mEventCallback) {
	      aEvent.position = newPosition;
	      result = ConvertStatus((*mEventCallback)(&aEvent));
	      newPosition = aEvent.position;
	    }
	    break;
	  }

    // scroll one line left or up
    case NS_SCROLLBAR_LINE_PREV: {
      newPosition -= mLineIncrement;
      if (newPosition < 0) 
          newPosition = 0;

      // if an event callback is registered, give it the chance
      // to change the decrement
      if (mEventCallback) {
        aEvent.position = newPosition;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        newPosition = aEvent.position;
      }
      break;
    }

    // Scrolls one page right or down
    case NS_SCROLLBAR_PAGE_NEXT: {
      PRUint32 max = GetMaxRange(range) - GetThumbSize(range);
      if (newPosition > (int)max) 
          newPosition = (int)max;

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) {
        aEvent.position = newPosition;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        newPosition = aEvent.position;
      }
      break;
    }

      // Scrolls one page left or up.
    case NS_SCROLLBAR_PAGE_PREV: {
      //XtVaGetValues(mWidget, XmNvalue, &newPosition, nsnull);
      if (newPosition < 0) 
          newPosition = 0;

      // if an event callback is registered, give it the chance
      // to change the increment
      if (mEventCallback) {
        aEvent.position = newPosition;
        result = ConvertStatus((*mEventCallback)(&aEvent));
        newPosition = aEvent.position;
      }

    break;
    }

      // Scrolls to the absolute position. The current position is specified by 
      // the cPos parameter.
    case NS_SCROLLBAR_POS: {
        newPosition = cPos;

        // if an event callback is registered, give it the chance
        // to change the increment
        if (mEventCallback) {
          aEvent.position = newPosition;
          result = ConvertStatus((*mEventCallback)(&aEvent));
          newPosition = aEvent.position;
        }
     break;
     }
  }
  return result;
}


