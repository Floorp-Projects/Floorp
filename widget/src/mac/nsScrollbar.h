/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

protected:
	
	virtual void			GetRectForMacControl(nsRect &outRect);

private:

	static 
	#if TARGET_CARBON
	pascal
	#endif
	        void					ScrollActionProc(ControlHandle, ControlPartCode);
	void							DoScrollAction(ControlPartCode);

// DATA
private:

	PRUint32					mFullImageSize;
	PRUint32					mVisibleImageSize;
	PRUint32 					mLineIncrement;
	PRBool						mMouseDownInScroll;
	ControlPartCode				mClickedPartCode;
	static ControlActionUPP		sControlActionProc; // we only need one of these
};



class nsHorizScrollbar : public nsScrollbar
{
public:
		nsHorizScrollbar() : nsScrollbar(PR_FALSE) {}
};

class nsVertScrollbar : public nsScrollbar
{
public:
		nsVertScrollbar() : nsScrollbar(PR_TRUE) {}								
};


#endif // nsScrollbar_
