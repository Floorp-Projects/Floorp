/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

NS_METHOD  nsFileWidget::GetFile(nsString& aFile)
{
  return NS_OK;
}

NS_METHOD nsFileWidget::GetFile(nsFileSpec& aFile)
{
  return NS_OK;
}

nsFileDlgResults nsFileWidget::GetFile(nsIWidget        * aParent,
                                       nsString         & promptString,
                                       nsFileSpec       & theFileSpec)
{
  return nsFileDlgResults_OK;
}

nsFileDlgResults nsFileWidget::GetFolder(nsIWidget        * aParent,
                                         nsString         & promptString,
                                         nsFileSpec       & theFileSpec)
{
  return nsFileDlgResults_OK;
}

nsFileDlgResults nsFileWidget::PutFile(nsIWidget        * aParent,
                                       nsString         & promptString,
                                       nsFileSpec       & theFileSpec)
{
  return nsFileDlgResults_OK;

}
NS_METHOD  nsFileWidget::SetDefaultString(nsString& aString)
{
  return NS_OK;
}

NS_METHOD  nsFileWidget::SetDisplayDirectory(nsString& aDirectory)
{
  return NS_OK;
}

NS_METHOD  nsFileWidget::GetDisplayDirectory(nsString& aDirectory)
{
  return NS_OK;
}

NS_METHOD nsFileWidget::Create(nsIWidget *aParent,
                           nsString& aTitle,
                           nsFileDlgMode aMode,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           void *aInitData)
{
  return NS_OK;
}

nsFileWidget::~nsFileWidget()
{
}
