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

#ifndef nsCheckButton_h__
#define nsCheckButton_h__

#include "nsMacControl.h"
#include "nsICheckButton.h"


class nsCheckButton : public nsMacControl, public nsICheckButton
{

public:
   	nsCheckButton();
	virtual ~nsCheckButton();

	// nsISupports
	NS_IMETHOD_(nsrefcnt) AddRef();
	NS_IMETHOD_(nsrefcnt) Release();
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);


	// nsICheckButton part
	NS_IMETHOD		SetLabel(const nsString& aText);
	NS_IMETHOD		GetLabel(nsString& aBuffer);
	NS_IMETHOD		SetState(const PRBool aState);
	NS_IMETHOD		GetState(PRBool& aState);
};


#endif // nsCheckButton_h__
