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

#include "nsButton.h"
#include "nsIButton.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include "nsUnitConversion.h"

#include <quickdraw.h>


NS_IMPL_ADDREF(nsButton)
NS_IMPL_RELEASE(nsButton)

//-------------------------------------------------------------------------
//
// nsButton constructor
//
//-------------------------------------------------------------------------
nsButton::nsButton()
{
  strcpy(gInstanceClassName, "nsButton");
  mWidgetArmed = PR_FALSE;
}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
*/ 
nsresult nsButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIButton, NS_IBUTTON_IID);
    if (aIID.Equals(kIButton)) {
        *aInstancePtr = (void*) ((nsIButton*)this);
        AddRef();
        return NS_OK;
    }

    return nsWindow::QueryInterface(aIID,aInstancePtr);
}


NS_IMETHODIMP nsButton::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext */*aContext*/,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData */*aInitData*/) 
{

  mParent = aParent;
  aParent->AddChild(this);
	
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

NS_IMETHODIMP nsButton::Create(nsNativeWidget /*aParent*/,
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
//
// nsButton destructor
//
//-------------------------------------------------------------------------
nsButton::~nsButton()
{
}



//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsButton::OnPaint(nsPaintEvent &aEvent)
{
	
	  
	DrawWidget(FALSE,aEvent.renderingContext);	
  return PR_FALSE;
}

PRBool nsButton::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


PRBool 
nsButton::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 							result;
nsIRenderingContext	*theRC;
	
	theRC = this->GetRenderingContext();
	
	switch (aEvent.message)
		{
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			mMouseDownInButton = PR_TRUE;
			DrawWidget(PR_TRUE,theRC);
			result = nsWindow::DispatchMouseEvent(aEvent);
			result = nsEventStatus_eConsumeDoDefault;
			break;
		case NS_MOUSE_LEFT_BUTTON_UP:
			mMouseDownInButton = PR_FALSE;
			DrawWidget(PR_TRUE,theRC);
			if(mWidgetArmed==PR_TRUE)
				result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_EXIT:
			if(mMouseDownInButton)
				{
				DrawWidget(PR_FALSE,theRC);
				mWidgetArmed = PR_FALSE;
				}
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_ENTER:
			if(mMouseDownInButton)
				{
				DrawWidget(PR_TRUE,theRC);
				mWidgetArmed = PR_TRUE;
				}
			result = nsWindow::DispatchMouseEvent(aEvent);
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
nsButton::DrawWidget(PRBool	aMouseInside,nsIRenderingContext	*aTheContext)
{
PRInt16							width,x,y;
PRInt32							offx,offy;
nsRect							therect;
Rect								macrect,crect;
GrafPtr							theport;
RGBColor						blackcolor = {0,0,0};
RgnHandle						thergn;


	CalcOffset(offx,offy);
	GetPort(&theport);
	::SetPort(mWindowPtr);
	::SetOrigin(-offx,-offy);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	
	crect = macrect;
	thergn = ::NewRgn();
	::GetClip(thergn);
	::ClipRect(&crect);
	::PenNormal();
	::RGBForeColor(&blackcolor);
	
	::EraseRoundRect(&macrect,10,10);
	::PenSize(1,1);
	::FrameRoundRect(&macrect,10,10); 

  Str255 label;	
	StringToStr255(mLabel, label);
	
	
	width = ::StringWidth(label);
	x = (macrect.left+macrect.right)/2 - (width/2);
	
	::TextFont(0);
	::TextSize(12);
	::TextFace(bold);
	//::GetFontInfo(&fi);
	//height = fi.ascent;
	//height = 6;
	y = (macrect.top+macrect.bottom)/2 + 6;
	::MoveTo(x,y);
	::DrawString(label);
		
	if(mMouseDownInButton && aMouseInside)
		 ::InvertRoundRect(&macrect,10,10);
		
	::PenSize(1,1);
	::SetClip(thergn);
	::SetOrigin(0,0);
	::SetPort(theport);
}

/** nsIButton Implementation **/

/**
	* Set the label for this object to be equal to aText
	*
	* @param  Set the label to aText
	* @result NS_Ok if no errors
	*/
NS_METHOD nsButton::SetLabel(const nsString& aText)
{
	mLabel = aText;
	return NS_OK;
}

/**
	* Set a buffer to be equal to this objects label
	*
	* @param  Put the contents of the label into aBuffer
	* @result NS_Ok if no errors
	*/
NS_METHOD nsButton::GetLabel(nsString& aBuffer)
{
	aBuffer = mLabel;
  return NS_OK;
}


//-------------------------------------------------------------------------


