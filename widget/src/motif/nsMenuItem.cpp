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

#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenu.h"

#include "nsStringUtil.h"

#include "nsIPopUpMenu.h"
#include <Xm/CascadeBG.h>

static NS_DEFINE_IID(kMenuItemIID, NS_IMENUITEM_IID);
NS_IMPL_ISUPPORTS(nsMenuItem, kMenuItemIID)


//-------------------------------------------------------------------------
//
// nsMenuItem constructor
//
//-------------------------------------------------------------------------
nsMenuItem::nsMenuItem() : nsIMenuItem()
{
  NS_INIT_REFCNT();
  mMenu        = nsnull;
  mMenuParent  = nsnull;
  mPopUpParent = nsnull;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
  NS_IF_RELEASE(mMenuParent);
  NS_IF_RELEASE(mPopUpParent);
}

//-------------------------------------------------------------------------
void nsMenuItem::Create(Widget aParent, const nsString &aLabel, PRUint32 aCommand)
{
  mCommand = aCommand;
  mLabel   = aLabel;

  if (NULL == aParent) {
    return;
  }

  char * nameStr = mLabel.ToNewCString();

  Widget parentMenuHandle = GetNativeParent();

  mMenu = XtVaCreateManagedWidget(nameStr, xmCascadeButtonGadgetClass,
                                          parentMenuHandle,
                                          NULL);

  /*MenuCallbackStruct * cbs = new MenuCallbackStruct();
  cbs->mCallback = aCallback;
  cbs->mId       = aID;
  XtAddCallback(widget, XmNactivateCallback, nsXtWidget_Menu_Callback, cbs);*/

  delete[] nameStr;

}

//-------------------------------------------------------------------------
Widget nsMenuItem::GetNativeParent()
{

  void * voidData;
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(voidData);
  } else if (nsnull != mPopUpParent) {
    mPopUpParent->GetNativeData(voidData);
  } else {
    return NULL;
  }
  return (Widget)voidData;

}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu * aParent, const nsString &aLabel, PRUint32 aCommand)
{
  mMenuParent = aParent;
  NS_ADDREF(mMenuParent);

  Create(GetNativeParent(), aLabel, aCommand);
  aParent->AddItem(this);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent, const nsString &aLabel, PRUint32 aCommand)
{
  mPopUpParent = aParent;
  NS_ADDREF(mPopUpParent);

  Create(GetNativeParent(), aLabel, aCommand);
  aParent->AddItem(this);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetCommand(PRUint32 & aCommand)
{
  aCommand = mCommand;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  aData = (void *)mMenu;
  return NS_OK;
}

