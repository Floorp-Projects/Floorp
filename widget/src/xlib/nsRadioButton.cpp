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

NS_METHOD nsRadioButton::Paint(nsIRenderingContext& aRenderingContext,
			       const nsRect&        aDirtyRect)
{
  return NS_OK;
}
