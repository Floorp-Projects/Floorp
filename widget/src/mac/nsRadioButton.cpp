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

#include "nsRadioButton.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

NS_IMPL_ADDREF(nsRadioButton)
NS_IMPL_RELEASE(nsRadioButton)

#define DBG 0
/**-------------------------------------------------------------------------------
 * nsRadioButton Constructor
 */
nsRadioButton::nsRadioButton() : nsWindow(), nsIRadioButton() 
{
  strcpy(gInstanceClassName, "nsRadioButton");
  mButtonSet = PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * The create method for a nsRadioButton, using a nsIWidget as the parent
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
NS_IMETHODIMP nsRadioButton::Create(nsIWidget *aParent,
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

		// NOTE: CREATE MACINTOSH CONTROL HERE
		Str255  title = "";
		Boolean visible = PR_TRUE;
		PRInt16 initialValue = 0;
		PRInt16 minValue = 0;
		PRInt16 maxValue = 1;
		PRInt16 ctrlType = pushButProc;
		
		// Set the bounds to the local rect
		SetBounds(aRect);
		
		// Convert to macintosh coordinates		
		Rect r;
		nsRectToMacRect(aRect,r);
				
		//mControl = NewControl ( window, &r, title, visible, 
												    //initialValue, minValue, maxValue, 
												    //ctrlType, (long)this);

		mWindowRegion = NewRgn();
		SetRectRgn(mWindowRegion,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);		 


	  //if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mControl, this);

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
 * The create method for a Radiobutton, using a nsNativeWidget as the parent
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
NS_IMETHODIMP nsRadioButton::Create(nsNativeWidget /*aParent*/,
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

//-------------------------------------------------------------------------
/**
 * Destuctor for the nsCheckButton
 */ 
nsRadioButton::~nsRadioButton()
{
}

/**-------------------------------------------------------------------------------
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @update  dc 08/31/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */ 
nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
    if (aIID.Equals(kIRadioButtonIID)) {
        *aInstancePtr = (void*) ((nsIRadioButton*)this);
        AddRef();
        return NS_OK;
    }

    return nsWindow::QueryInterface(aIID,aInstancePtr);
}

/**-------------------------------------------------------------------------------
 * The onPaint handleer for a nsRadioButton -- this may change, inherited from windows
 * @update  dc 08/31/98
 * @param aEvent -- The paint event to respond to
 * @return -- PR_TRUE if painted, false otherwise
 */ 
PRBool nsRadioButton::OnPaint(nsPaintEvent &aEvent)
{
	  
	DrawWidget(FALSE);	
  return PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * Resizes the nsRadioButton, currently handles by nsWindow
 * @Param aEvent -- The event for this resize
 * @return -- True if the event was handled, PR_FALSE is always return for now
 */ 
PRBool nsRadioButton::OnResize(nsSizeEvent &aEvent)
{
	return PR_FALSE;
}


/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this button
 * @update  dc 08/31/98
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool 
nsRadioButton::DispatchMouseEvent(nsMouseEvent &aEvent)
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
			if(mWidgetArmed==PR_TRUE){
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
 * Draw in the different modes depending on the state of the mouse and nsRadioButton state
 * @update  dc 08/31/98
 * @param   aMouseInside -- A boolean indicating if the mouse is inside the control
 * @return  nothing is returned
 */
void
nsRadioButton::DrawWidget(PRBool	aMouseInside)
{
PRInt16							width,x,y,buttonsize=14;
PRInt32							offx,offy;
nsRect							therect;
Rect								macrect,rb;
GrafPtr							theport;
RGBColor						blackcolor = {0,0,0};
RgnHandle						thergn;
Str255		tempstring;

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
	
	
	::PenSize(1,1);
	::SetRect(&rb,macrect.left,(macrect.bottom-1)-buttonsize,macrect.left+buttonsize,macrect.bottom-1);
	::EraseOval(&rb);
	::FrameOval(&rb); 
	
	
	StringToStr255(mLabel,tempstring);
	width = ::StringWidth(tempstring);
	x = macrect.left+buttonsize+5;
	
	::TextFont(0);
	::TextSize(12);
	::TextFace(bold);
	y = macrect.bottom-1;
	::MoveTo(x,y);
	::DrawString(tempstring);
		
	if(  (mButtonSet && !mMouseDownInButton) ||  
	     (mMouseDownInButton && aMouseInside && !mButtonSet) ||
	      (mMouseDownInButton && !aMouseInside && mButtonSet) ){
		::InsetRect(&rb,2,2);
		::PaintOval(&rb);
	}
		
	::PenSize(1,1);
	::SetClip(thergn);
	::SetOrigin(0,0);
	::SetPort(theport);
}

/**-------------------------------------------------------------------------------
 * Set the state for this RadioButton
 * @update  dc 08/31/98
 * @param  aState -- boolean TRUE if hilited, FALSE otherwise
 * @result NS_Ok 
 */
NS_METHOD nsRadioButton::SetState(PRBool aState) 
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
 * Get the RadioButtons state.
 * @update  dc 08/31/98
 * @param aState PR_TRUE if checked. PR_FALSE if unchecked.
 * @result set to NS_OK if method successful
 */
NS_METHOD nsRadioButton::GetState(PRBool& aState)
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
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
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
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
	aBuffer = mLabel;
  return NS_OK;
}
