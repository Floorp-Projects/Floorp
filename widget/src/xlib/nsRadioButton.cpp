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

NS_IMPL_ADDREF(nsRadioButton)
NS_IMPL_RELEASE(nsRadioButton)

nsRadioButton::nsRadioButton() : nsWidget(), nsIRadioButton()
{
  NS_INIT_REFCNT();
}

nsRadioButton::~nsRadioButton()
{
}

nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kIRadioButtonIID)) {
    *aInstancePtr = (void*) ((nsIRadioButton*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  return result;
}

NS_METHOD nsRadioButton::SetState(const PRBool aState) 
{
  return NS_OK;
}

NS_METHOD nsRadioButton::GetState(PRBool& aState)
{
  return NS_OK;
}

NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
  return NS_OK;
}

NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
  return NS_OK;
}

PRBool nsRadioButton::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsRadioButton::OnPaint()
{
    return PR_FALSE;
}

PRBool nsRadioButton::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

NS_METHOD nsRadioButton::Paint(nsIRenderingContext& aRenderingContext,
			       const nsRect&        aDirtyRect)
{
  return NS_OK;
}
