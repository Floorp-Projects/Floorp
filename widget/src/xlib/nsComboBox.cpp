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

#include "nsComboBox.h"

NS_IMPL_ADDREF(nsComboBox)
NS_IMPL_RELEASE(nsComboBox)

nsComboBox::nsComboBox() : nsWidget(), nsIListWidget(), nsIComboBox()
{
  NS_INIT_REFCNT();
}

NS_METHOD nsComboBox::AddItemAt(nsString &aItem, PRInt32 aPosition)
{
  return NS_OK;
}

PRInt32  nsComboBox::FindItem(nsString &aItem, PRInt32 aStartPos)
{
  return 0;
}

PRInt32  nsComboBox::GetItemCount()
{
  return 0;
}

PRBool  nsComboBox::RemoveItemAt(PRInt32 aPosition)
{
  return PR_TRUE;
}

PRBool nsComboBox::GetItemAt(nsString& anItem, PRInt32 aPosition)
{
  return PR_FALSE;
}

NS_METHOD nsComboBox::GetSelectedItem(nsString& aItem)
{
  return NS_OK;
}

PRInt32 nsComboBox::GetSelectedIndex()
{
  return 0;
}

NS_METHOD nsComboBox::SelectItem(PRInt32 aPosition)
{
  return NS_OK;
}

NS_METHOD nsComboBox::Deselect()
{
  return NS_OK;
}

nsComboBox::~nsComboBox()
{
}

nsresult nsComboBox::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kInsComboBoxIID, NS_ICOMBOBOX_IID);
  static NS_DEFINE_IID(kInsListWidgetIID, NS_ILISTWIDGET_IID);

  if (aIID.Equals(kInsComboBoxIID)) {
    *aInstancePtr = (void*) ((nsIComboBox*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (aIID.Equals(kInsListWidgetIID)) {
    *aInstancePtr = (void*) ((nsIListWidget*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return nsWidget::QueryInterface(aIID,aInstancePtr);
}

PRBool nsComboBox::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsComboBox::OnPaint()
{
    return PR_FALSE;
}

PRBool nsComboBox::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}

NS_METHOD nsComboBox::PreCreateWidget(nsWidgetInitData *aInitData)
{
  return NS_OK;
}

PRInt32 nsComboBox::GetHeight(PRInt32 aProposedHeight)
{
  return 0;
}

NS_METHOD nsComboBox::GetBounds(nsRect &aRect)
{
  return NS_OK;
}

NS_METHOD nsComboBox::Paint(nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect)
{
  return NS_OK;
}
