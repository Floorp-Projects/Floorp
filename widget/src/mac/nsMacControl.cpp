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

#include "nsMacControl.h"
#include "nsColor.h"


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsMacControl::nsMacControl() : nsWindow()
{
  mButtonSet			= PR_FALSE;
  mWidgetArmed		= PR_FALSE;
  mMouseInButton	= PR_FALSE;
  mControl				= nsnull;
  mControlType		= pushButProc;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacControl::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
	nsWindow::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);

  
	// create native control
	nsRect ctlRect = aRect;
	ctlRect.x = ctlRect.y = 0;
	Rect macRect;
	nsRectToMacRect(ctlRect, macRect);
	mControl = ::NewControl(mWindowPtr, &macRect, "\p", true, 0, 0, 1, mControlType, nil);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsMacControl::~nsMacControl()
{
	if (mControl)
	{
		::DisposeControl(mControl);
		mControl = nsnull;
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacControl::OnPaint(nsPaintEvent &aEvent)
{
	if (mControl)
	{
		// set the control text attributes
		// (the rendering context has already set these attributes for
		// the window: we just have to transfer them over to the control)
		ControlFontStyleRec fontStyleRec;
		fontStyleRec.flags = (kControlUseFontMask | kControlUseFaceMask | kControlUseSizeMask);
		fontStyleRec.font = mWindowPtr->txFont;
		fontStyleRec.size = mWindowPtr->txSize;
		fontStyleRec.style = mWindowPtr->txFace;
		::SetControlFontStyle(mControl, &fontStyleRec);

/*
		// set the background color
		//¥TODO: this should be done by the rendering context
		#define COLOR8TOCOLOR16(color8)	 (color8 == 0xFF ? 0xFFFF : (color8 << 8))
		nscolor backColor = GetBackgroundColor();
		RGBColor macColor;
		macColor.red   = COLOR8TOCOLOR16(NS_GET_R(backColor));
		macColor.green = COLOR8TOCOLOR16(NS_GET_G(backColor));
		macColor.blue  = COLOR8TOCOLOR16( NS_GET_B(backColor));
		::RGBBackColor(&macColor);
*/
		// draw the control
		Str255 aStr;
		StringToStr255(mLabel, aStr);
		::SetControlTitle(mControl, aStr);

		::SetControlValue(mControl, (mButtonSet ? 1 : 0));

		if (mEnabled)
			::HiliteControl(mControl, (mWidgetArmed && mMouseInButton ? 1 : 0));
		else
			::HiliteControl(mControl, kControlInactivePart);

		::ValidRect(&(*mControl)->contrlRect);
	}
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacControl::OnResize(nsSizeEvent &aEvent)
{
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
			if (aEvent.widget != this)
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
	return (nsWindow::DispatchMouseEvent(aEvent));
}
