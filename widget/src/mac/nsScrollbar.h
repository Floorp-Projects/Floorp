/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.	You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.	All Rights
 * Reserved.
 */

#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsMacControl.h"
#include "nsIScrollbar.h"
#include <Controls.h>


/**
 * Mac Scrollbar. 
 */

class nsScrollbar : public nsMacControl, public nsIScrollbar
{
private:
	typedef nsMacControl Inherited;

public:
								nsScrollbar(PRBool aIsVertical);
	virtual						~nsScrollbar();

	// nsISupports
	NS_IMETHOD_(nsrefcnt)		AddRef();
	NS_IMETHOD_(nsrefcnt)		Release();
	NS_IMETHOD					QueryInterface(const nsIID& aIID, void** aInstancePtr);

	NS_IMETHODIMP				Create(nsIWidget *aParent,
									const nsRect &aRect,
									EVENT_CALLBACK aHandleEventFunction,
									nsIDeviceContext *aContext = nsnull,
									nsIAppShell *aAppShell = nsnull,
									nsIToolkit *aToolkit = nsnull,
									nsWidgetInitData *aInitData = nsnull);

	// nsWindow Interface
	virtual PRBool				DispatchMouseEvent(nsMouseEvent &aEvent);

	// nsIScrollbar part
	NS_IMETHOD					SetMaxRange(PRUint32 aEndRange);
	NS_IMETHOD					GetMaxRange(PRUint32& aMaxRange);
	NS_IMETHOD					SetPosition(PRUint32 aPos);
	NS_IMETHOD					GetPosition(PRUint32& aPos);
	NS_IMETHOD					SetThumbSize(PRUint32 aSize);
	NS_IMETHOD					GetThumbSize(PRUint32& aSize);
	NS_IMETHOD					SetLineIncrement(PRUint32 aSize);
	NS_IMETHOD					GetLineIncrement(PRUint32& aSize);
	
	
	/**-------------------------------------------------------------------------------
	 *	Set all the scrollbar parameters
	 *	@update	dc 09/16/98
	 *	@param aMaxRange -- max range of the scrollbar in relative units
	 *	@param aThumbSize -- thumb size, in relative units
	 *	@param aPosition -- the thumb position in relative units
	 *	@param aLineIncrement -- the increment levelof the scrollbar
	 *	@return NS_OK if the position is valid
	 */
	NS_IMETHOD		 			SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
								PRUint32 aPosition, PRUint32 aLineIncrement);
	ControlHandle				GetControl() { return mControl; }

private:

	void						DrawWidget();
	static void					ScrollActionProc(ControlHandle, ControlPartCode);
	void						DoScrollAction(ControlPartCode);

// DATA
private:

	PRUint32					mMaxRange;
	PRUint32					mThumbSize;
	PRUint32 					mLineIncrement;
	PRBool						mMouseDownInScroll;
	ControlPartCode				mClickedPartCode;
	static ControlActionUPP		sControlActionProc; // we only need one of these
};

#endif // nsScrollbar_
