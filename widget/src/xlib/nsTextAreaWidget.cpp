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

NS_METHOD nsTextAreaWidget::Paint(nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect)
{
  return NS_OK;
}
