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

#include "nsTooltipWidget.h"

NS_IMPL_ADDREF(nsTooltipWidget)
NS_IMPL_RELEASE(nsTooltipWidget)


nsTooltipWidget::nsTooltipWidget()
{
  NS_INIT_REFCNT();
}

nsTooltipWidget::~nsTooltipWidget()
{
}

nsresult nsTooltipWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kInsTooltipWidgetIID, NS_ITOOLTIPWIDGET_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsTooltipWidgetIID)) {
      *aInstancePtr = (void*) ((nsITooltipWidget*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
  }

  return result;
}

PRBool nsTooltipWidget::OnPaint()
{
  return PR_FALSE;
}

PRBool nsTooltipWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

PRBool nsTooltipWidget::AutoErase()
{
  return(PR_TRUE);
}

NS_METHOD nsTooltipWidget::GetBounds(nsRect &aRect)
{
  return NS_OK;
}
