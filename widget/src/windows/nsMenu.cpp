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

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"

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
#include "nsMenuItem.h"
#include "nsCOMPtr.h"
#include "nsIMenuListener.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);

static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);


nsresult nsMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuIID)) {                                         
    *aInstancePtr = (void*)(nsIMenu*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIMenuListenerIID)) {                                      
    *aInstancePtr = (void*)(nsIMenuListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                     
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(nsMenu)
NS_IMPL_RELEASE(nsMenu)

//-------------------------------------------------------------------------
//
// nsMenu constructor
//
//-------------------------------------------------------------------------
nsMenu::nsMenu() : nsIMenu()
{
  NS_INIT_REFCNT();
  mMenu          = nsnull;
  mMenuBarParent = nsnull;
  mMenuParent    = nsnull;
  mListener      = nsnull;
  nsresult result = NS_NewISupportsArray(&mItems);
  //NS_ASSERT(result == NS_OK);
}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
  NS_IF_RELEASE(mMenuBarParent);
  NS_IF_RELEASE(mMenuParent);
  NS_IF_RELEASE(mListener);

  // Remove all references to the items
  mItems->Clear();
}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsISupports *aParent, const nsString &aLabel)

{

	if(aParent)

	{

		nsIMenuBar * menubar = nsnull;

		aParent->QueryInterface(kIMenuBarIID, (void**) &menubar);

		if(menubar)

		{

			mMenuBarParent = menubar;

			NS_ADDREF(mMenuBarParent);

			NS_RELEASE(menubar); // Balance the QI

		}

		else

		{

			nsIMenu * menu = nsnull;

			aParent->QueryInterface(kIMenuIID, (void**) &menu);

			if(menu)

			{

				mMenuParent = menu;

				NS_ADDREF(mMenuParent);

				NS_RELEASE(menu); // Balance the QI

			}

		}

	}



  mLabel = aLabel;

  mMenu = CreateMenu();

    

  return NS_OK;

}

/*

//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsIMenu *aParent, const nsString &aLabel)
{
  mMenuParent = aParent;
  NS_ADDREF(mMenuParent);


  mLabel = aLabel;

  mMenu = CreateMenu();

    

  return NS_OK;

}

*/

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{

  aParent = nsnull;
  if (nsnull != mMenuParent) {
    return mMenuParent->QueryInterface(kISupportsIID,(void**)&aParent);
  } else if (nsnull != mMenuBarParent) {
    return mMenuBarParent->QueryInterface(kISupportsIID,(void**)&aParent);
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetLabel(const nsString &aText)

{
   mLabel = aText;
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)

{

  if(aItem)

  {

    // Figure out what we're adding

		nsIMenuItem * menuitem = nsnull;

		aItem->QueryInterface(kIMenuItemIID, (void**) &menuitem);

		if(menuitem)

		{

			// case menuitem

			AddMenuItem(menuitem);

			NS_RELEASE(menuitem);

		}

		else

		{

			nsIMenu * menu = nsnull;

			aItem->QueryInterface(kIMenuIID, (void**) &menu);

			if(menu)

			{

				// case menu

				AddMenu(menu);

				NS_RELEASE(menu);

			}

		}

  }

  return NS_OK;
}

//-------------------------------------------------------------------------
// This does not return a ref counted object
// This is NOT an nsIMenu method
nsIMenuBar * nsMenu::GetMenuBar(nsIMenu * aMenu)
{
  if (!aMenu) {
    return nsnull;
  }

  nsMenu * menu = (nsMenu *)aMenu;

  if (menu->GetMenuBarParent()) {
    return menu->GetMenuBarParent();
  }

  if (menu->GetMenuParent()) {
    return GetMenuBar(menu->GetMenuParent());
  }

  return nsnull;
}

//-------------------------------------------------------------------------
// This does not return a ref counted object
// This is NOT an nsIMenu method
nsIWidget *  nsMenu::GetParentWidget()
{
  nsIWidget  * parent  = nsnull;
  nsIMenuBar * menuBar = GetMenuBar(this);
  if (menuBar) {
    menuBar->GetParent(parent);
  } 

  return parent;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  return InsertItemAt(mItems->Count(), (nsISupports *)aMenuItem);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  return InsertItemAt(mItems->Count(), (nsISupports *)aMenu);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  InsertSeparator(mItems->Count());

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  aCount = mItems->Count();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aCount, nsISupports *& aMenuItem)
{
  aMenuItem = (nsISupports *)mItems->ElementAt(aCount);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, nsISupports * aMenuItem)
{
  nsString name;
  BOOL status = FALSE;

  mItems->InsertElementAt(aMenuItem, (PRInt32)aCount);

  nsCOMPtr<nsIMenuItem> menuItem(do_QueryInterface(aMenuItem));
  if (menuItem) {
    menuItem->GetLabel(name);
    nsIWidget * win = GetParentWidget();
    PRInt32 id = ((nsWindow *)win)->GetNewCmdMenuId();
    ((nsMenuItem *)((nsIMenuItem *)menuItem))->SetCmdId(id);

    char * nameStr = name.ToNewCString();

    MENUITEMINFO menuInfo;
    menuInfo.cbSize     = sizeof(menuInfo);
    menuInfo.fMask      = MIIM_TYPE | MIIM_ID;
    menuInfo.fType      = MFT_STRING;
    menuInfo.dwTypeData = nameStr;
    menuInfo.wID        = (DWORD)id;
    menuInfo.cch        = strlen(nameStr);

    status = ::InsertMenuItem(mMenu, aCount, TRUE, &menuInfo);

    delete[] nameStr;
  } else {
    nsCOMPtr<nsIMenu> menu(do_QueryInterface(aMenuItem));
    if (menu) {
      nsString name;
      menu->GetLabel(name);
      //mItems->AppendElement((nsISupports *)(nsIMenu *)menu);

      char * nameStr = name.ToNewCString();

      HMENU nativeMenuHandle;
      void * voidData;
      menu->GetNativeData(&voidData);

      nativeMenuHandle = (HMENU)voidData;

      MENUITEMINFO menuInfo;

      menuInfo.cbSize     = sizeof(menuInfo);
      menuInfo.fMask      = MIIM_SUBMENU | MIIM_TYPE;
      menuInfo.hSubMenu   = nativeMenuHandle;
      menuInfo.fType      = MFT_STRING;
      menuInfo.dwTypeData = nameStr;

      BOOL status = ::InsertMenuItem(mMenu, aCount, TRUE, &menuInfo);

      delete[] nameStr;
    }
  }

  return (status ? NS_OK : NS_ERROR_FAILURE);
}


//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aCount)
{
  nsMenuItem * item = new nsMenuItem();
  item->Create(this);
  mItems->InsertElementAt((nsISupports *)(nsIMenuItem *)item, (PRInt32)aCount);

  MENUITEMINFO menuInfo;

  menuInfo.cbSize = sizeof(menuInfo);
  menuInfo.fMask  = MIIM_TYPE;
  menuInfo.fType  = MFT_SEPARATOR;

  BOOL status = ::InsertMenuItem(mMenu, aCount, TRUE, &menuInfo);
 
  return (status ? NS_OK : NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{

  //nsISupports * supports = (nsISupports *)mItems->ElementAt(aCount);
  mItems->RemoveElementAt(aCount);

  return (::RemoveMenu(mMenu, aCount, MF_BYPOSITION) ? NS_OK:NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  while (mItems->Count()) {
    mItems->RemoveElementAt(0);
    ::RemoveMenu(mMenu, 0, MF_BYPOSITION);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)

{

  *aData = (void *)mMenu;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;
  NS_ADDREF(mListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener) {
    NS_IF_RELEASE(mListener);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  char* menuLabel = mLabel.ToNewCString()
  printf("Menu Selected %s\n", menuLabel);
  delete[] menuLabel;
  if (nsnull != mListener) {
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  char* menuLabel = mLabel.ToNewCString()
  printf("Menu Deselected %s\n", menuLabel);
  delete[] menuLabel;
  if (nsnull != mListener) {
    mListener->MenuDeselected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

