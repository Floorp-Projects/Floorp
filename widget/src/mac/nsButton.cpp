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

#include <Controls.h>


#define DBG 0
//-------------------------------------------------------------------------
//
// nsButton constructor
//
//-------------------------------------------------------------------------
nsButton::nsButton(nsISupports *aOuter) : nsWindow(aOuter)
{
  strcpy(gInstanceClassName, "nsButton");
}


/*
 * Convert an nsPoint into mac local coordinated.
 * The tree hierarchy is navigated upwards, changing
 * the x,y offset by the parent's coordinates
 *
 */
void nsButton::LocalToWindowCoordinate(nsPoint& aPoint)
{
	nsIWidget* 	parent = GetParent();
  nsRect 			bounds;
  
	while (parent)
	{
		parent->GetBounds(bounds);
		aPoint.x += bounds.x;
		aPoint.y += bounds.y;	
		parent = parent->GetParent();
	}
}

/* 
 * Convert an nsRect's local coordinates to global coordinates
 */
void nsButton::LocalToWindowCoordinate(nsRect& aRect)
{
	nsIWidget* 	parent = GetParent();
  nsRect 			bounds;
  
	while (parent)
	{
		parent->GetBounds(bounds);
		aRect.x += bounds.x;
		aRect.y += bounds.y;	
		parent = parent->GetParent();
	}
}


void nsButton::Create(nsIWidget *aParent,
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
	if (window)
	{
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
				
		mControl = NewControl ( window, &r, title, visible, 
												    initialValue, minValue, maxValue, 
												    ctrlType, (long)this);

		mWindowRegion = NewRgn();
		SetRectRgn(mWindowRegion,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);		 


	  if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mControl, this);

	  // save the event callback function
	  mEventCallback = aHandleEventFunction;
	  
	  mMouseDownInButton = PR_FALSE;
	  mWidgetArmed = PR_FALSE;

	  //InitCallbacks("nsButton");
	  InitDeviceContext(mContext, (nsNativeWidget)mWindowPtr);
	}
}

void nsButton::Create(nsNativeWidget /*aParent*/,
                      const nsRect &/*aRect*/,
                      EVENT_CALLBACK /*aHandleEventFunction*/,
                      nsIDeviceContext */*aContext*/,
                      nsIAppShell */*aAppShell*/,
                      nsIToolkit */*aToolkit*/,
                      nsWidgetInitData */*aInitData*/)
{
	NS_ERROR("This Widget must not use this Create method");
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
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsButton::QueryObject(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kIButtonIID,    NS_IBUTTON_IID);

  if (aIID.Equals(kIButtonIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryObject(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
// Convert a nsString to a PascalStr255
//
//-------------------------------------------------------------------------
void nsButton::StringToStr255(const nsString& aText, Str255& aStr255)
{
  char buffer[256];
	
	aText.ToCString(buffer,255);
		
	PRInt32 len = strlen(buffer);
	memcpy(&aStr255[1],buffer,len);
	aStr255[0] = len;
	
		
}



//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsButton::SetLabel(const nsString& aText)
{
	NS_ASSERTION(mControl != nsnull,"Control must not be null");
	if (mControl != nsnull)
	{
		Str255 s;
		StringToStr255(aText,s);
		SetControlTitle(mControl,s);
	}
}



//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
void nsButton::GetLabel(nsString& aBuffer)
{

}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsButton::OnPaint(nsPaintEvent &aEvent)
{
	  
	DrawWidget(FALSE);	
  return PR_FALSE;
}

PRBool nsButton::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


#define GET_OUTER() ((nsButton*) ((char*)this - nsButton::GetOuterOffset()))

void nsButton::AggButton::GetLabel(nsString& aBuffer)
{
  GET_OUTER()->GetLabel(aBuffer);
}

void nsButton::AggButton::SetLabel(const nsString& aText)
{
  GET_OUTER()->SetLabel(aText);
}

/*
 *  @update  gpk 08/27/98
 *  @param   aX -- x offset in widget local coordinates
 *  @param   aY -- y offset in widget local coordinates
 *  @return  PR_TRUE if the pt is contained in the widget
 */
PRBool
nsButton::PtInWindow(PRInt32 aX,PRInt32 aY)
{
	PRBool	result = PR_FALSE;
	nsPoint	hitPt(aX,aY);
	nsRect	bounds;
	
	GetBounds(bounds);
	if(bounds.Contains(hitPt))
		result = PR_TRUE;
	return(result);
}

PRBool 
nsButton::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 	result;
	
	switch (aEvent.message)
		{
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			mMouseDownInButton = PR_TRUE;
			DrawWidget(PR_TRUE);
			result = nsWindow::DispatchMouseEvent(aEvent);
			break;
		case NS_MOUSE_LEFT_BUTTON_UP:
			mMouseDownInButton = PR_FALSE;
			DrawWidget(PR_TRUE);
			if(mWidgetArmed==PR_TRUE)
				result = nsWindow::DispatchMouseEvent(aEvent);
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


//-------------------------------------------------------------------------
/*  Track this control and draw in the different modes depending on the state of the mouse and buttons
 *  @update  dc 08/31/98
 *  @param   aMouseInside -- A boolean indicating if the mouse is inside the control
 *  @return  nothing is returned
 */
void
nsButton::DrawWidget(PRBool	aMouseInside)
{
nsRect							therect;
Rect								macrect;
GrafPtr							theport;
RGBColor						blackcolor = {0,0,0};
RgnHandle						thergn;


	GetPort(&theport);
	::SetPort(mWindowPtr);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	thergn = ::NewRgn();
	::GetClip(thergn);
	::ClipRect(&macrect);
	::PenNormal();
	::RGBForeColor(&blackcolor);
	
	::EraseRoundRect(&macrect,10,10);
	::PenSize(3,3);
	::FrameRoundRect(&macrect,10,10); 
	
	if(mMouseDownInButton && aMouseInside)
		 ::InvertRoundRect(&macrect,10,10);
		
	::PenSize(1,1);
	::SetClip(thergn);
	::SetPort(theport);
}

//-------------------------------------------------------------------------


BASE_IWIDGET_IMPL(nsButton, AggButton);


