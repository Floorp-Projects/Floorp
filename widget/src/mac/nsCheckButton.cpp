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

#include "nsCheckButton.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

NS_IMPL_ADDREF(nsCheckButton)
NS_IMPL_RELEASE(nsCheckButton)

#define DBG 0
/**-------------------------------------------------------------------------------
 * nsCheckButton Constructor
 * @update  dc 08/31/98
 */
nsCheckButton::nsCheckButton() : nsWindow(), nsICheckButton()
{
  strcpy(gInstanceClassName, "nsCheckButton");
  mButtonSet = PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * the create method for a nsCheckButton, using a nsIWidget as the parent
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
NS_IMETHODIMP nsCheckButton::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext */*aContext*/,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData */*aInitData*/) 
{
  mParent = aParent;
  aParent->AddChild(this);

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);
	
	WindowPtr window = nsnull;

  if (aParent) {
    window = (WindowPtr) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else if (aAppShell) {
    window = (WindowPtr) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  mIsMainWindow = PR_FALSE;
  mWindowMadeHere = PR_TRUE;
	mWindowRecord = (WindowRecord*)window;
	mWindowPtr = (WindowPtr)window;
  
  NS_ASSERTION(window!=nsnull,"The WindowPtr for the widget cannot be null")
	if (window){
	  InitToolkit(aToolkit, aParent);
	  // InitDeviceContext(aContext, parentWidget);

	  if (DBG) fprintf(stderr, "Parent 0x%x\n", window);

		// Set the bounds to the local rect
		SetBounds(aRect);

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

/**-------------------------------------------------------------------------------
 * the create method for a checkbutton, using a nsNativeWidget as the parent
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
NS_IMETHODIMP nsCheckButton::Create(nsNativeWidget /*aParent*/,
                      const nsRect &/*aRect*/,
                      EVENT_CALLBACK /*aHandleEventFunction*/,
                      nsIDeviceContext */*aContext*/,
                      nsIAppShell */*aAppShell*/,
                      nsIToolkit */*aToolkit*/,
                      nsWidgetInitData */*aInitData*/)
{
	NS_ERROR("This Widget must not use this Create method");
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Destuctor for the nsCheckButton
 * @update  dc 08/31/98
 */ 
nsCheckButton::~nsCheckButton()
{
}

/**-------------------------------------------------------------------------------
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @update  gk 09/15/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */ 
nsresult nsCheckButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
    if (aIID.Equals(kICheckButtonIID)) {
        *aInstancePtr = (void*) ((nsICheckButton*)this);
        AddRef();
        return NS_OK;
    }
    return nsWindow::QueryInterface(aIID,aInstancePtr);
}


/**-------------------------------------------------------------------------------
 * The onPaint handleer for a button -- this may change, inherited from windows
 * @update  dc 08/31/98
 * @param aEvent -- The paint event to respond to
 * @return -- PR_TRUE if painted, false otherwise
 */ 
PRBool nsCheckButton::OnPaint(nsPaintEvent &aEvent)
{
	  
	DrawWidget(FALSE);	
  return PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * Resizes the button, currently handles by nsWindow
 * @update  dc 08/31/98
 * @Param aEvent -- The event for this resize
 * @return -- True if the event was handled, PR_FALSE is always return for now
 */ 
PRBool nsCheckButton::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this checkbutton
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool 
nsCheckButton::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 	result;
	
	switch (aEvent.message){
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			mMouseDownInButton = PR_TRUE;
			DrawWidget(PR_TRUE);
			result = nsWindow::DispatchMouseEvent(aEvent);
			result = nsEventStatus_eConsumeDoDefault;
			break;
		case NS_MOUSE_LEFT_BUTTON_UP:
			mMouseDownInButton = PR_FALSE;
			if(mWidgetArmed==PR_TRUE)
				{
				result = nsWindow::DispatchMouseEvent(aEvent);
				}
			DrawWidget(PR_TRUE);
			break;
		case NS_MOUSE_EXIT:
			DrawWidget(PR_FALSE);
			mWidgetArmed = PR_FALSE;
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_ENTER:
			DrawWidget(PR_TRUE);
			mWidgetArmed = PR_TRUE;
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
	}
	
	return result;
}


/**-------------------------------------------------------------------------------
 *  Draw in the different modes depending on the state of the mouse and checkbutton
 *  @update  dc 08/31/98
 *  @param   aMouseInside -- A boolean indicating if the mouse is inside the control
 *  @return  nothing is returned
 */
void
nsCheckButton::DrawWidget(PRBool	aMouseInside)
{
PRInt16		width,x,y,buttonSize=14;
PRInt32		offX,offY;
nsRect		theRect;
Rect			macRect,rb;
GrafPtr		thePort;
RGBColor	blackColor = {0,0,0};
RgnHandle	theRgn;
Str255		tempString;

	CalcOffset(offX,offY);
	GetPort(&thePort);
	::SetPort(mWindowPtr);
	::SetOrigin(-offX,-offY);
	GetBounds(theRect);
	nsRectToMacRect(theRect,macRect);
	theRgn = ::NewRgn();
	::GetClip(theRgn);
	::ClipRect(&macRect);
	::PenNormal();
	::RGBForeColor(&blackColor);
	
	
	::PenSize(1,1);
	::SetRect(&rb,macRect.left,(macRect.bottom-1)-buttonSize,macRect.left+buttonSize,macRect.bottom-1);
	::EraseRect(&rb);
	::FrameRect(&rb); 
	
	
	StringToStr255(mLabel,tempString);
	width = ::StringWidth(tempString);
	x = macRect.left+buttonSize+5;
	
	::TextFont(0);
	::TextSize(12);
	::TextFace(bold);
	y = macRect.bottom-1;
	::MoveTo(x,y);
	::DrawString(tempString);
		
	if(  (mButtonSet && !mMouseDownInButton) ||  
	     (mMouseDownInButton && aMouseInside && !mButtonSet) ||
	      (mMouseDownInButton && !aMouseInside && mButtonSet) ){
		::MoveTo(rb.left,rb.top);
		::LineTo(rb.right-1,rb.bottom-1);
		
		::MoveTo(rb.right-1,rb.top);
		::LineTo(rb.left-1,rb.bottom-1);
	}
		
	::PenSize(1,1);
	::SetClip(theRgn);
	::DisposeRgn(theRgn);
	::SetOrigin(0,0);
	::SetPort(thePort);
}

/**-------------------------------------------------------------------------------
 * Set the state for this checkButton
 * @update  dc 08/31/98
 * @param  aState -- boolean TRUE if checked, FALSE otherwise
 * @result NS_Ok 
 */
NS_METHOD nsCheckButton::SetState(PRBool aState) 
{
int state = aState;
  
  mButtonSet = aState;
  DrawWidget(PR_FALSE);
  
  return NS_OK;
  
  //if (mIsArmed) {
    //mNewValue    = aState;
    //mValueWasSet = PR_TRUE;
  //}
}

/**-------------------------------------------------------------------------------
 * Get the check state.
 * @update  dc 08/31/98
 * @param aState PR_TRUE if checked. PR_FALSE if unchecked.
 * @result set to NS_OK if method successful
 */
NS_METHOD nsCheckButton::GetState(PRBool& aState)
{
	aState = mButtonSet;
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set the label for this object to be equal to aText
 * @update  dc 08/31/98
 * @param  Set the label to aText
 * @result NS_Ok if no errors
 */
NS_METHOD nsCheckButton::SetLabel(const nsString& aText)
{
	mLabel = aText;
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set a buffer to be equal to this objects label
 * @update  dc 08/31/98
 * @param  Put the contents of the label into aBuffer
 * @result NS_Ok if no errors
 */
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
{
	aBuffer = mLabel;
  return NS_OK;
}
