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

#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
#include "nsToolkit.h"

#include "nsMacControl.h"
#include "nsColor.h"
#include "nsFontMetricsMac.h"


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsMacControl::nsMacControl() : nsWindow()
{
	mValue			= 0;
	mMin			= 0;
	mMax			= 1;
	mWidgetArmed	= PR_FALSE;
	mMouseInButton	= PR_FALSE;
	mControl		= nsnull;
	mControlType	= pushButProc;

	mLastLabel = "";
	mLastBounds.SetRect(0,0,0,0);
	mLastValue = 0;
	mLastHilite = 0;
}

/**-------------------------------------------------------------------------
 * See documentation in nsMacControl.h
 * @update -- 12/10/98 dwc
 */
NS_IMETHODIMP nsMacControl::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
	Inherited::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);
  
	// create native control. mBounds has been set up at this point
	nsresult		theResult = CreateOrReplaceMacControl(mControlType);

	mLastBounds = mBounds;

	return theResult;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsMacControl::~nsMacControl()
{
	if (mControl)
	{
		Show(PR_FALSE);
		::DisposeControl(mControl);
		mControl = nsnull;
	}
}

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacControl::OnPaint(nsPaintEvent &aEvent)
{
	if (mControl && mVisible)
	{
		// turn off drawing for setup to avoid ugliness
		Boolean		isVisible = IsControlVisible(mControl);
		::SetControlVisibility(mControl, false, false);

		// draw the control
		if (mLabel != mLastLabel)
		{
			mLastLabel = mLabel;
			Str255 aStr;
			StringToStr255(mLabel, aStr);
			::SetControlTitle(mControl, aStr);
		}

		if (mBounds != mLastBounds)
		{
			nsRect ctlRect;
			GetRectForMacControl(ctlRect);
			Rect macRect;
			nsRectToMacRect(ctlRect, macRect);

			if ((mBounds.x != mLastBounds.x) || (mBounds.y != mLastBounds.y))
				::MoveControl(mControl, macRect.left, macRect.top);
			if ((mBounds.width != mLastBounds.width) || (mBounds.height != mLastBounds.height))
				::SizeControl(mControl, ctlRect.width, ctlRect.height);

			mLastBounds = mBounds;
		}

		if (mValue != mLastValue)
		{
			mLastValue = mValue;
			if (nsToolkit::HasAppearanceManager())
				::SetControl32BitValue(mControl, mValue);
			else
				::SetControlValue(mControl, mValue);
		}

		PRInt16 hilite;
		if (mEnabled)
			hilite = (mWidgetArmed && mMouseInButton ? 1 : 0);
		else
			hilite = kControlInactivePart;
		if (hilite != mLastHilite)
		{
			mLastHilite = hilite;
			::HiliteControl(mControl, hilite);
		}

		::SetControlVisibility(mControl, isVisible, false);
		::DrawOneControl(mControl);
		::ValidRect(&(*mControl)->contrlRect);
	}
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool  nsMacControl::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	PRBool eatEvent = PR_FALSE;
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_DOUBLECLICK:
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			if (mEnabled)
			{
				mMouseInButton = PR_TRUE;
				mWidgetArmed = PR_TRUE;
				Invalidate(PR_TRUE);
			}
			break;

		case NS_MOUSE_LEFT_BUTTON_UP:
			// if the widget was not armed, eat the event
			if (!mWidgetArmed)
				eatEvent = PR_TRUE;
			// if the mouseUp happened on another widget, eat the event too
			// (the widget which got the mouseDown is always notified of the mouseUp)
			if (! mMouseInButton)
				eatEvent = PR_TRUE;
			mWidgetArmed = PR_FALSE;
			if (mMouseInButton)
				Invalidate(PR_TRUE);
			break;

		case NS_MOUSE_EXIT:
			mMouseInButton = PR_FALSE;
			if (mWidgetArmed)
				Invalidate(PR_TRUE);
			break;

		case NS_MOUSE_ENTER:
			mMouseInButton = PR_TRUE;
			if (mWidgetArmed)
				Invalidate(PR_TRUE);
			break;
	}
	if (eatEvent)
		return PR_TRUE;
	return (Inherited::DispatchMouseEvent(aEvent));
}

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacControl::Show(PRBool bState)
{
  Inherited::Show(bState);
  if (mControl)
  {
  	::SetControlVisibility(mControl, bState, false);		// don't redraw
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this control font
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacControl::SetFont(const nsFont &aFont)
{
	Inherited::SetFont(aFont);	

	SetupMacControlFont();
	
 	return NS_OK;
}

#pragma mark -

//-------------------------------------------------------------------------
//
// Get the rect which the Mac control uses. This may be different for
// different controls, so this method allows overriding
//
//-------------------------------------------------------------------------
void nsMacControl::GetRectForMacControl(nsRect &outRect)
{
		outRect = mBounds;
		outRect.x = outRect.y = 0;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

NS_METHOD nsMacControl::CreateOrReplaceMacControl(short inControlType)
{
	nsRect		controlRect;
	GetRectForMacControl(controlRect);
	Rect macRect;
	nsRectToMacRect(controlRect, macRect);

	if(nsnull != mWindowPtr)
	{
#ifdef DEBUG
		// we should have a root control at this point. If not, something's wrong.
		// it's made in nsMacWindow
		ControlHandle		rootControl = nil;
		OSErr		err = ::GetRootControl(mWindowPtr, &rootControl);
		NS_ASSERTION((err == noErr && rootControl != nil), "No root control exists for the window");
#endif

		if (mControl)
			::DisposeControl(mControl);

		StartDraw();
		mControl = ::NewControl(mWindowPtr, &macRect, "\p", mVisible, mValue, mMin, mMax, inControlType, nil);
  		EndDraw();
		
		// need to reset the font now
		// XXX to do: transfer the text in the old control over too
		if (mControl && mFontMetrics)
		{
			SetupMacControlFont();
		}
	}

	return (mControl) ? NS_OK : NS_ERROR_NULL_POINTER;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsMacControl::SetupMacControlFont()
{
	NS_PRECONDITION(mFontMetrics != nsnull, "No font metrics in SetupMacControlFont");
	NS_PRECONDITION(mContext != nsnull, "No context metrics in SetupMacControlFont");
	
	TextStyle		theStyle;
	nsFontMetricsMac::GetNativeTextStyle(*mFontMetrics, *mContext, theStyle);
	
#if DONT_USE_FONTS_SMALLER_THAN_9
	// impose a min size of 9pt on the control font
	if (theStyle.tsSize < 9)
		theStyle.tsSize = 9;
#endif
	
	ControlFontStyleRec fontStyleRec;
	fontStyleRec.flags = (kControlUseFontMask | kControlUseFaceMask | kControlUseSizeMask);
	fontStyleRec.font = theStyle.tsFont;
	fontStyleRec.size = theStyle.tsSize;
	fontStyleRec.style = theStyle.tsFace;
	::SetControlFontStyle(mControl, &fontStyleRec);
}

#pragma mark -

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

void nsMacControl::StringToStr255(const nsString& aText, Str255& aStr255)
{
	char buffer[256];
	
	aText.ToCString(buffer,255);
		
	PRInt32 len = strlen(buffer);
	memcpy(&aStr255[1],buffer,len);
	aStr255[0] = len;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

void nsMacControl::Str255ToString(const Str255& aStr255, nsString& aText)
{
	char 		buffer[256];
	PRInt32 len = aStr255[0];
  
	memcpy(buffer,&aStr255[1],len);
	buffer[len] = 0;

	aText = buffer;		
}
