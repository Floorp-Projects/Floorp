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

#include "nsButton.h"

NS_IMPL_ADDREF(nsButton)
NS_IMPL_RELEASE(nsButton)

nsButton::nsButton() : nsWidget() , nsIButton()
{
  NS_INIT_REFCNT();
  mName.AssignWithConversion("nsButton");
}

nsButton::~nsButton()
{
}

nsresult nsButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  
  static NS_DEFINE_IID(kIButton, NS_IBUTTON_IID);
  if (aIID.Equals(kIButton)) {
    *aInstancePtr = (void*) ((nsIButton*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  return nsWidget::QueryInterface(aIID,aInstancePtr);
}

NS_METHOD nsButton::SetLabel (const nsString &aText)
{
  return NS_OK;
}

NS_METHOD nsButton::GetLabel (nsString &aBuffer)
{
  return NS_OK;
}

NS_METHOD nsButton::Paint(nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect)
{
  return NS_OK;
}
