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

#include "nsTextAreaWidget.h"

NS_IMPL_ADDREF(nsTextAreaWidget)
NS_IMPL_RELEASE(nsTextAreaWidget)

nsTextAreaWidget::nsTextAreaWidget()
{
  NS_INIT_REFCNT();
}

nsTextAreaWidget::~nsTextAreaWidget()
{
}

nsresult nsTextAreaWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  
  if (aIID.Equals(kITextAreaWidgetIID)) {
    nsITextAreaWidget* textArea = this;
    *aInstancePtr = (void*) (textArea);
    NS_ADDREF_THIS();
    return NS_OK;
  } 
  else if (aIID.Equals(kIWidgetIID)) {
    nsIWidget* widget = this;
    *aInstancePtr = (void*) (widget);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  
  return nsWidget::QueryInterface(aIID, aInstancePtr);
}

PRBool nsTextAreaWidget::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsTextAreaWidget::OnPaint()
{
    return PR_FALSE;
}

PRBool nsTextAreaWidget::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

NS_METHOD nsTextAreaWidget::GetBounds(nsRect &aRect)
{
    return NS_OK;
}

NS_METHOD nsTextAreaWidget::Paint(nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect)
{
  return NS_OK;
}
