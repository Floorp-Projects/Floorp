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

#include "nsLabel.h"

NS_IMPL_ADDREF(nsLabel)
NS_IMPL_RELEASE(nsLabel)

nsLabel::nsLabel() : nsWidget(), nsILabel()
{
  NS_INIT_REFCNT();
}

NS_METHOD nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
{
  return NS_OK;
}

nsLabel::~nsLabel()
{
}

nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kILabelIID)) {
    *aInstancePtr = (void*) ((nsILabel*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  
  return result;
}

NS_METHOD nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
  return NS_OK;
}

NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
  return NS_OK;
}

NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
  return NS_OK;
}

PRBool nsLabel::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsLabel::OnPaint()
{
  return PR_FALSE;
}

PRBool nsLabel::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

NS_METHOD nsLabel::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  return NS_OK;
}

NS_METHOD nsLabel::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_OK;
}

NS_METHOD nsLabel::GetBounds(nsRect &aRect)
{
  return NS_OK;
}
