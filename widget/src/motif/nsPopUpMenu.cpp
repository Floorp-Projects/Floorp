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
#include "nsPopUpMenu.h"
#include "nsIPopUpMenu.h"
#include "nsIMenu.h"
#include "nsIWidget.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include <Xm/RowColumn.h>
#include <Xm/SeparatoG.h>

NS_IMPL_ISUPPORTS1(nsPopUpMenu, nsIPopUpMenu)

//-------------------------------------------------------------------------
//
// nsPopUpMenu constructor
//
//-------------------------------------------------------------------------
nsPopUpMenu::nsPopUpMenu() : nsIPopUpMenu()
{
  NS_INIT_REFCNT();
  mNumMenuItems = 0;
  mParent       = nsnull;
  mMenu         = nsnull;
}

//-------------------------------------------------------------------------
//
// nsPopUpMenu destructor
//
//-------------------------------------------------------------------------
nsPopUpMenu::~nsPopUpMenu()
{
  NS_IF_RELEASE(mParent);
}



//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::Create(nsIWidget *aParent)
{

  mParent = aParent;
  NS_ADDREF(mParent);

  Widget parentWidget = (Widget) mParent->GetNativeData(NS_NATIVE_WINDOW);
  mMenu = XmCreatePopupMenu(parentWidget, "_popup", NULL, 0);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddItem(const nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddItem(nsIMenuItem * aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddMenu(nsIMenu * aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddSeparator() 
{
  XtVaCreateManagedWidget("__sep", xmSeparatorGadgetClass, mMenu, NULL);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetItemCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::InsertItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::InsertItemAt(const PRUint32 aCount, const nsString & aMenuItemName)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::InsertSeparator(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::RemoveItem(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::RemoveAll()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::ShowMenu(PRInt32 aX, PRInt32 aY)
{

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetNativeData(void *& aData)
{
  aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetParent(nsIWidget *& aParent)
{
  aParent = mParent;
  return NS_OK;
}


