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
#include "nsPopUpMenu.h"
#include "nsIPopUpMenu.h"
#include "nsIMenu.h"

#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>

#include "nsIAppShell.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kPopUpMenuIID, NS_IPOPUPMENU_IID);
NS_IMPL_ISUPPORTS(nsPopUpMenu, kPopUpMenuIID)


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

  mMenu   = CreatePopupMenu();
    
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
  PRUint32 command;
  nsString name;

  aMenuItem->GetCommand(command);
  aMenuItem->GetLabel(name);
  char * nameStr = name.ToNewCString();

  MENUITEMINFO menuInfo;
  menuInfo.cbSize     = sizeof(menuInfo);
  menuInfo.fMask      = MIIM_TYPE | MIIM_ID;
  menuInfo.fType      = MFT_STRING;
  menuInfo.dwTypeData = nameStr;
  menuInfo.wID        = (DWORD)command;
  menuInfo.cch        = strlen(nameStr);

  BOOL status = InsertMenuItem(mMenu, mNumMenuItems++, TRUE, &menuInfo);

  delete[] nameStr;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddMenu(nsIMenu * aMenu)
{
  nsString name;
  aMenu->GetLabel(name);
  char * nameStr = name.ToNewCString();

  HMENU nativeMenuHandle;
  void * voidData;
  aMenu->GetNativeData(&voidData);

  nativeMenuHandle = (HMENU)voidData;

  MENUITEMINFO menuInfo;

  menuInfo.cbSize     = sizeof(menuInfo);
  menuInfo.fMask      = MIIM_SUBMENU | MIIM_TYPE;
  menuInfo.hSubMenu   = nativeMenuHandle;
  menuInfo.fType      = MFT_STRING;
  menuInfo.dwTypeData = nameStr;

  BOOL status = InsertMenuItem(mMenu, mNumMenuItems++, TRUE, &menuInfo);

  delete[] nameStr;

  return NS_OK;

}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddSeparator() 
{
  MENUITEMINFO menuInfo;

  menuInfo.cbSize = sizeof(menuInfo);
  menuInfo.fMask  = MIIM_TYPE;
  menuInfo.fType  = MFT_SEPARATOR;

  BOOL status = InsertMenuItem(mMenu, mNumMenuItems++, TRUE, &menuInfo);

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
  if (nsnull != mParent) {
    HWND pWnd = (HWND)mParent->GetNativeData(NS_NATIVE_WIDGET);
    if (nsnull != pWnd) {
      nsRect rect;
      POINT point;
      mParent->GetBounds(rect);
      point.x = 0;
      point.y = 0;
      ClientToScreen (pWnd, &point) ;
      point.x += aX;
      point.y += aY;
      BOOL status = TrackPopupMenu(mMenu, TPM_LEFTALIGN | TPM_TOPALIGN, point.x, point.y, 0, pWnd, NULL);
    }
  }


  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetNativeData(void *& aData)
{
  //void * data = (void *)mMenu;
  //aData = data;
  aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetParent(nsIWidget *& aParent)
{
  aParent = mParent;
  return NS_OK;
}


