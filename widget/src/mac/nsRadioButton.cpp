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

#include "nsRadioButton.h"

NS_IMPL_ADDREF(nsRadioButton);
NS_IMPL_RELEASE(nsRadioButton);

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsMacControl(), nsIRadioButton() 
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "nsRadioButton");
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
