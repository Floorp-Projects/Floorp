/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMacControl_h__
#define nsMacControl_h__

#include "nsChildWindow.h"
#include <Controls.h>

class nsMacControl : public nsChildWindow
{
private:
	typedef nsChildWindow Inherited;

public:
						nsMacControl();
	virtual				~nsMacControl();

	NS_IMETHOD 			Create(nsIWidget *aParent,
				              const nsRect &aRect,
				              EVENT_CALLBACK aHandleEventFunction,
				              nsIDeviceContext *aContext = nsnull,
				              nsIAppShell *aAppShell = nsnull,
				              nsIToolkit *aToolkit = nsnull,
				              nsWidgetInitData *aInitData = nsnull);

	virtual void		SetControlType(short type)	{mControlType = type;}
	short				GetControlType()			{return mControlType;}

	// event handling
	virtual PRBool		OnPaint(nsPaintEvent & aEvent);
	virtual PRBool		DispatchMouseEvent(nsMouseEvent &aEvent);
    
	// nsIWidget interface
	NS_IMETHOD			Enable(PRBool bState);
	NS_IMETHOD			Show(PRBool aState);
	NS_IMETHODIMP		SetFont(const nsFont &aFont);

	// Mac string utilities
	// (they really should be elsewhere but, well, only the Mac controls use them)
	static void 		StringToStr255(const nsString& aText, Str255& aStr255);
	static void 		Str255ToString(const Str255& aStr255, nsString& aText);

protected:
	
	NS_METHOD			CreateOrReplaceMacControl(short inControlType);
	virtual void		GetRectForMacControl(nsRect &outRect);
	void				SetupMacControlFont();
	void				ControlChanged(PRInt32 aNewValue);
	void				NSStringSetControlTitle(ControlHandle theControl, nsString title);
	void				SetupMacControlFontForScript(short theScript);
	
	nsString			mLabel;
	PRBool				mWidgetArmed;
	PRBool				mMouseInButton;

	PRInt32				mValue;
	PRInt32				mMin;
	PRInt32				mMax;
	ControlHandle		mControl;
	short				mControlType;

	nsString			mLastLabel;
	nsRect				mLastBounds;
	PRInt32				mLastValue;
	PRInt16				mLastHilite;
};

#endif // nsMacControl_h__
