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

#include "nsCheckButton.h"
#if TARGET_CARBON
#include <ControlDefinitions.h>
#endif

NS_IMPL_ADDREF(nsCheckButton);
NS_IMPL_RELEASE(nsCheckButton);

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton() : nsMacControl(), nsICheckButton()
{
  NS_INIT_REFCNT();
  gInstanceClassName = "nsCheckButton";
  SetControlType(checkBoxProc);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsCheckButton::~nsCheckButton()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsresult nsCheckButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
    if (aIID.Equals(kICheckButtonIID)) {
        *aInstancePtr = (void*) ((nsICheckButton*)this);
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
NS_METHOD nsCheckButton::SetState(PRBool aState) 
{
	mValue = (aState ? 1 : 0);
	Invalidate(PR_TRUE);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetState(PRBool& aState)
{
	aState = (mValue != 0);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetLabel(const nsString& aText)
{
	mLabel = aText;
	Invalidate(PR_TRUE);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
{
	aBuffer = mLabel;
	return NS_OK;
}
