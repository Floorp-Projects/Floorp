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

#ifndef nsRadioButton_h__
#define nsRadioButton_h__

#include "nsWindow.h"
#include "nsIRadioButton.h"

/**-------------------------------------------------------------------------------
 * nsRadioButton Class definition
 * @update dc 10/16/98 
 */
class nsRadioButton :  public nsWindow, public nsIRadioButton
{

public:
  nsRadioButton();
  virtual ~nsRadioButton();

	NS_DECL_ISUPPORTS

  NS_IMETHOD Create(nsIWidget *aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);
  NS_IMETHOD Create(nsNativeWidget aParent,
              const nsRect &aRect,
              EVENT_CALLBACK aHandleEventFunction,
              nsIDeviceContext *aContext = nsnull,
              nsIAppShell *aAppShell = nsnull,
              nsIToolkit *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull);


    // nsIRadioButton part
  NS_IMETHOD     SetLabel(const nsString& aText);
  NS_IMETHOD     GetLabel(nsString& aBuffer);
  NS_IMETHOD 		 SetState(const PRBool aState);
  NS_IMETHOD     GetState(PRBool& aState);
  NS_IMETHOD     Resize(PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);
  NS_IMETHOD     Resize(PRUint32 aX, PRUint32 aY,PRUint32 aWidth,PRUint32 aHeight, PRBool aRepaint);

  virtual PRBool OnPaint(nsPaintEvent & aEvent);
  virtual PRBool OnResize(nsSizeEvent &aEvent);
  virtual PRBool DispatchMouseEvent(nsMouseEvent &aEvent);
    

private:
	void DrawWidget(PRBool	aMouseInside);

  
  nsString			mLabel;
  PRBool				mMouseDownInButton;
  PRBool				mWidgetArmed;
  PRBool				mButtonSet;
};

#endif // nsRadioButton_h__
