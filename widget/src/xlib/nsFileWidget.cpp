/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsFileWidget.h"


NS_IMPL_ADDREF(nsFileWidget)
NS_IMPL_RELEASE(nsFileWidget)

nsFileWidget::nsFileWidget() : nsIFileWidget()
{
  NS_INIT_REFCNT();
}

PRBool nsFileWidget::Show()
{
  return PR_TRUE;
}

nsresult nsFileWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
  nsresult result = NS_NOINTERFACE;

  static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kIFileWidgetIID)) {
    *aInstancePtr = (void*) ((nsIFileWidget*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  return result;
}




NS_METHOD nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[])
{
  return NS_OK;
}

NS_METHOD nsFileWidget::GetFile(nsFileSpec& aFile)
{
  return NS_OK;
}

nsFileDlgResults nsFileWidget::GetFile(nsIWidget        * aParent,
                                       const nsString         & promptString,
                                       nsFileSpec       & theFileSpec)
{
  return nsFileDlgResults_OK;
}

nsFileDlgResults nsFileWidget::GetFolder(nsIWidget        * aParent,
                                         const nsString         & promptString,
                                         nsFileSpec       & theFileSpec)
{
  return nsFileDlgResults_OK;
}

nsFileDlgResults nsFileWidget::PutFile(nsIWidget        * aParent,
                                       const nsString         & promptString,
                                       nsFileSpec       & theFileSpec)
{
  return nsFileDlgResults_OK;

}
NS_METHOD  nsFileWidget::SetDefaultString(const nsString& aString)
{
  return NS_OK;
}

NS_METHOD  nsFileWidget::SetDisplayDirectory(const nsFileSpec& aDirectory)
{
  return NS_OK;
}

NS_METHOD  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  return NS_OK;
}

NS_METHOD nsFileWidget::Create(nsIWidget *aParent,
                               const nsString& aTitle,
                               nsFileDlgMode aMode,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               void *aInitData)
{
  return NS_OK;
}

NS_METHOD nsFileWidget::GetSelectedType(PRInt16& theType)
{
  return NS_OK;
}

nsFileWidget::~nsFileWidget()
{
}
