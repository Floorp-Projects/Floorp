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

#include "nsRadioButton.h"
#if TARGET_CARBON
#include <ControlDefinitions.h>
#endif

NS_IMPL_ADDREF(nsRadioButton);
NS_IMPL_RELEASE(nsRadioButton);

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsMacControl(), nsIRadioButton() 
{
  NS_INIT_REFCNT();
  gInstanceClassName = "nsRadioButton";
  SetControlType(radioButProc);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsRadioButton::~nsRadioButton()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
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

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetState(PRBool aState) 
{
	mValue = (aState ? 1 : 0);
	Invalidate(PR_TRUE);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetState(PRBool& aState)
{
	aState = (mValue != 0);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
	mLabel = aText;
	Invalidate(PR_TRUE);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
	aBuffer = mLabel;
	return NS_OK;
}
