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
#include "nsIDeviceContext.h"

NS_IMPL_ADDREF(nsScrollbar);
NS_IMPL_RELEASE(nsScrollbar);

/**-------------------------------------------------------------------------------
 * nsScrollbar Constructor
 *  @update  dc 10/31/98
 * @param aIsVertical -- Tells if the scrollbar had a vertical or horizontal orientation
 */
nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsMacControl(), nsIScrollbar()
{
  NS_INIT_REFCNT();
	strcpy(gInstanceClassName, "nsScrollbar");
  SetControlType(scrollBarProc);

	mIsVertical				= aIsVertical;
	mLineIncrement		= 0;
	mClickedPartCode	= 0;
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
	Inherited::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);

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
PRBool  nsScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			PRInt32	sBarMessage = 0;
			StartDraw();
			{
				Point thePoint;
				thePoint.h = aEvent.point.x;
				thePoint.v = aEvent.point.y;

				mClickedPartCode = ::TestControl(mControl, thePoint);
				if (mClickedPartCode > 0)
					::HiliteControl(mControl, mClickedPartCode);

				switch (mClickedPartCode)
				{
					case kControlUpButtonPart:
						sBarMessage = NS_SCROLLBAR_LINE_PREV;
						mValue -= mLineIncrement;
						break;

					case kControlDownButtonPart:
						sBarMessage = NS_SCROLLBAR_LINE_NEXT;
						mValue += mLineIncrement;
						break;

					case kControlPageUpPart:
						sBarMessage = NS_SCROLLBAR_PAGE_PREV;
						mValue -= mThumbSize;
						break;

					case kControlPageDownPart:
						sBarMessage = NS_SCROLLBAR_PAGE_NEXT;
						mValue += mThumbSize;
						break;

					case kControlIndicatorPart:
						sBarMessage = NS_SCROLLBAR_POS;
						::TrackControl(mControl, thePoint, nil);	//¥TODO: should implement live-scrolling
						mValue = ::GetControlValue(mControl);
						break;
				}
				SetPosition(mValue);
			}
			EndDraw();

			nsScrollbarEvent sBarEvent;
			sBarEvent.eventStructType = NS_GUI_EVENT;
			sBarEvent.widget = this;
			sBarEvent.message = sBarMessage;
			sBarEvent.position = mValue;
			DispatchWindowEvent(sBarEvent);
			break;


		case NS_MOUSE_LEFT_BUTTON_UP:
			mClickedPartCode = 0;
			break;

		case NS_MOUSE_EXIT:
			if (mWidgetArmed)
			{
				StartDraw();
				::HiliteControl(mControl, 0);
				EndDraw();
			}
			break;

		case NS_MOUSE_ENTER:
			if (mWidgetArmed)
			{
				StartDraw();
				::HiliteControl(mControl, mClickedPartCode);
				EndDraw();
			}
			break;
	}
	return (Inherited::DispatchMouseEvent(aEvent));

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
	Invalidate(PR_TRUE);
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
	if (aPos < 0)
		aPos = 0;
	mValue = ((int)aPos) > mMaxRange ? (mMaxRange - 1) : ((int)aPos);
	return (NS_OK);
}


/**-------------------------------------------------------------------------------
 *	Get the current position of the slider
 *  @update  dc 09/16/98
 *  @param  aMaxRange -- The current slider position.
 *  @return -- NS_OK if the position is valid
 */
NS_METHOD nsScrollbar::GetPosition(PRUint32& aPos)
{
  aPos = mValue;
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
  	mThumbSize = aSize;

  	if (mThumbSize <= 0)
  		mThumbSize = 1;

  	if (mThumbSize > mMaxRange)
  		mThumbSize = mMaxRange;

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
	mMaxRange				= (((int)aMaxRange) > 0 ? aMaxRange : 10);
	mLineIncrement	= (((int)aLineIncrement) > 0 ? aLineIncrement : 1);

	SetPosition(aPosition);
	SetThumbSize(aThumbSize);
	
	StartDraw();
	::SetControlMaximum(mControl, mMaxRange);
	EndDraw();

	return(NS_OK);
}



