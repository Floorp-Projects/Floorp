/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

// Check button control

#include "nsCheckButton.h"

// XP-com
NS_IMPL_ADDREF(nsCheckButton)
NS_IMPL_RELEASE(nsCheckButton)

nsresult nsCheckButton::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
  nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

  if( result == NS_NOINTERFACE && aIID.Equals( nsICheckButton::GetIID()))
  {
     *aInstancePtr = (void*) ((nsICheckButton*)this);
     NS_ADDREF_THIS();
     result = NS_OK;
  }

  return result;
}

NS_IMPL_LABEL(nsCheckButton)

// checked-ness
nsresult nsCheckButton::GetState( PRBool &aState)
{
   MRESULT rc = mOS2Toolkit->SendMsg( mWnd, BM_QUERYCHECK);
   aState = SHORT1FROMMR(rc);
   return NS_OK;
}

nsresult nsCheckButton::SetState( const PRBool aState)
{
   mOS2Toolkit->SendMsg( mWnd, BM_SETCHECK, MPFROMLONG(aState));
   return NS_OK;
}

// Creation hooks
PCSZ nsCheckButton::WindowClass()
{
   return WC_BUTTON;
}

ULONG nsCheckButton::WindowStyle()
{ 
   return BASE_CONTROL_STYLE | BS_CHECKBOX;
}
