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

#include "nsDialog.h"

NS_IMPL_ADDREF(nsDialog)
NS_IMPL_RELEASE(nsDialog)

nsDialog::nsDialog() : nsWidget(), nsIDialog()
{
  NS_INIT_REFCNT();
}

NS_METHOD nsDialog::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return NS_OK;
}

nsDialog::~nsDialog()
{
}

NS_METHOD nsDialog::SetLabel(const nsString& aText)
{
  return NS_OK;
}

NS_METHOD nsDialog::GetLabel(nsString& aBuffer)
{
  return NS_OK;
}

nsresult nsDialog::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);
  
  static NS_DEFINE_IID(kInsDialogIID, NS_IDIALOG_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsDialogIID)) {
    *aInstancePtr = (void*) ((nsIDialog*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  
  return result;
}

PRBool nsDialog::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsDialog::OnPaint()
{
  return PR_FALSE;
}

PRBool nsDialog::OnResize(nsRect &aWindowRect)
{
  return PR_FALSE;
}

NS_METHOD nsDialog::GetBounds(nsRect &aRect)
{
  return NS_OK;
}
