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

#include "nsMenuBar.h"
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

static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
//NS_IMPL_ISUPPORTS(nsMenuBar, kMenuBarIID)

nsresult nsMenuBar::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuBarIID)) {                                         
    *aInstancePtr = (void*) ((nsIMenuBar*) this);                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) ((nsISupports*)(nsIMenuBar*) this);                     
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  if (aIID.Equals(kIMenuListenerIID)) {                                      
    *aInstancePtr = (void*) ((nsIMenuListener*)this);                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                     
  return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(nsMenuBar)
NS_IMPL_RELEASE(nsMenuBar)
//-------------------------------------------------------------------------
//
// nsMenuListener interface
//
//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
//
// nsMenuBar constructor
//
//-------------------------------------------------------------------------
nsMenuBar::nsMenuBar() : nsIMenuBar(), nsIMenuListener()
{
  NS_INIT_REFCNT();
  mNumMenus = 0;
  mMenu     = nsnull;
  mParent   = nsnull;
  mIsMenuBarAdded = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsMenuBar destructor
//
//-------------------------------------------------------------------------
nsMenuBar::~nsMenuBar()
{
  NS_IF_RELEASE(mParent);
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
  mMenu = CreateMenu();
  mParent = aParent;
  NS_ADDREF(mParent);
  return NS_OK;

}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
  aParent = mParent;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{

  nsString name;
  aMenu->GetLabel(name);
  char * nameStr = name.ToNewCString();

  HMENU nativeMenuHandle;
  void * voidData;
  aMenu->GetNativeData(voidData);
  nativeMenuHandle = (HMENU)voidData;

  MENUITEMINFO menuInfo;

  menuInfo.cbSize     = sizeof(menuInfo);
  menuInfo.fMask      = MIIM_SUBMENU | MIIM_TYPE;
  menuInfo.hSubMenu   = nativeMenuHandle;
  menuInfo.fType      = MFT_STRING;
  menuInfo.dwTypeData = nameStr;

  BOOL status = InsertMenuItem(mMenu, mNumMenus++, TRUE, &menuInfo);

  delete[] nameStr;

  if (!mIsMenuBarAdded) {
  	if(mParent){
      mParent->SetMenuBar(this);
    }
    mIsMenuBarAdded = PR_TRUE;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
  aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
  return NS_OK;
}
