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

static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);
//static NS_DEFINE_IID(kIMenuListenerIID, NS_IMENULISTENER_IID);

#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuBarCID, NS_MENUBAR_CID);
static NS_DEFINE_CID(kMenuCID, NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);

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
nsEventStatus nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
	// Dispatch the menu event
	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  printf("nsMenuBar::MenuSelected \n");
  // Find which menu was selected and call MenuConstruct on it
  HMENU aNativeMenu = (HMENU) aMenuEvent.nativeMsg;

  if(aNativeMenu == mMenu) {
    UINT      aSubMenuItemNum = (UINT)LOWORD(aMenuEvent.mCommand);
    nsIMenu * menu;
    GetMenuAt(aSubMenuItemNum, menu);

    nsIMenuListener * listener = nsnull;
    menu->QueryInterface(kIMenuListenerIID, (void**) listener);
    if(listener) {
      listener->MenuSelected(aMenuEvent);
      NS_RELEASE(listener);
	}
  } else {
    // The menu is being deselected
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menubarNode,
	void              * aWebShell)
{
    mWebShell = (nsIWebShell*) aWebShell;
	mDOMNode  = (nsIDOMNode*)menubarNode;

    nsIMenuBar * pnsMenuBar = nsnull;
    nsresult rv = nsComponentManager::CreateInstance(kMenuBarCID, nsnull, kIMenuBarIID, (void**)&pnsMenuBar);
    if (NS_OK == rv) {
      if (nsnull != pnsMenuBar) {
        pnsMenuBar->Create(aParentWindow);
      
        // set pnsMenuBar as a nsMenuListener on aParentWindow
        nsCOMPtr<nsIMenuListener> menuListener;
        pnsMenuBar->QueryInterface(kIMenuListenerIID, getter_AddRefs(menuListener));
        aParentWindow->AddMenuListener(menuListener);

        nsCOMPtr<nsIDOMNode> menuNode;
        ((nsIDOMNode*)menubarNode)->GetFirstChild(getter_AddRefs(menuNode));
        while (menuNode) {
          nsCOMPtr<nsIDOMElement> menuElement(do_QueryInterface(menuNode));
          if (menuElement) {
            nsString menuNodeType;
            nsString menuName;
            menuElement->GetNodeName(menuNodeType);
            if (menuNodeType.Equals("menu")) {
              menuElement->GetAttribute(nsAutoString("name"), menuName);
              // Don't create the menu yet, just add in the top level names
              
                // Create nsMenu
                nsIMenu * pnsMenu = nsnull;
                rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
                if (NS_OK == rv) {
                  // Call Create
                  nsISupports * supports = nsnull;
                  pnsMenuBar->QueryInterface(kISupportsIID, (void**) &supports);
                  pnsMenu->Create(supports, menuName);
                  NS_RELEASE(supports);

				  // Set JavaScript execution parameters
				  pnsMenu->SetDOMNode(menuNode);
				  pnsMenu->SetDOMElement(menuElement);
				  pnsMenu->SetWebShell((nsIWebShell*)aWebShell);

                  // Set nsMenu Name
                  pnsMenu->SetLabel(menuName); 
                  // Make nsMenu a child of nsMenuBar
                  pnsMenuBar->AddMenu(pnsMenu); 

                  // Release the menu now that the menubar owns it
                  //NS_RELEASE(pnsMenu);
                }
             } 

          }
          nsCOMPtr<nsIDOMNode> oldmenuNode(menuNode);  
          oldmenuNode->GetNextSibling(getter_AddRefs(menuNode));
        } // end while (nsnull != menuNode)
          
        // Give the aParentWindow this nsMenuBar to hold onto.
        aParentWindow->SetMenuBar(pnsMenuBar);
      
        // HACK: force a paint for now
        pnsMenuBar->Paint();
        
        // HACK for M4, should be removed by M5
        #ifdef XP_MAC
        Handle tempMenuBar = ::GetMenuBar(); // Get a copy of the menu list
		pnsMenuBar->SetNativeData((void*)tempMenuBar);
		#endif
    } // end if ( nsnull != pnsMenuBar )
  }
  
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
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
  mMenu     = nsnull;
  mParent   = nsnull;
  mIsMenuBarAdded = PR_FALSE;
  mItems    = new nsVoidArray();
  mWebShell = nsnull;
  mDOMNode  = nsnull;
  mDOMElement = nsnull;
  mConstructed = false;
}

//-------------------------------------------------------------------------
//
// nsMenuBar destructor
//
//-------------------------------------------------------------------------
nsMenuBar::~nsMenuBar()
{
  NS_IF_RELEASE(mParent);

  // Remove all references to the menus
  mItems->Clear();
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
  mMenu = CreateMenu();
  SetParent(aParent);
  return NS_OK;

}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
  aParent = mParent;
  NS_ADDREF(mParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetParent(nsIWidget *aParent)
{

  NS_IF_RELEASE(mParent);
  mParent = aParent;
  NS_IF_ADDREF(mParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{
  InsertMenuAt(mItems->Count(), aMenu);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  aCount = mItems->Count();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  aMenu = (nsIMenu *)mItems->ElementAt(aPos);
  NS_ADDREF(aMenu);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  nsString name;
  aMenu->GetLabel(name);
  char * nameStr = GetACPString(name);

  mItems->InsertElementAt(aMenu, (PRInt32)aPos);
  NS_ADDREF(aMenu);

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

  BOOL status = ::InsertMenuItem(mMenu, aPos, TRUE, &menuInfo);

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
NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aPos)
{
  nsIMenu * menu = (nsIMenu *)mItems->ElementAt(aPos);
  NS_RELEASE(menu);
  mItems->RemoveElementAt(aPos);
  return (::RemoveMenu(mMenu, aPos, MF_BYPOSITION) ? NS_OK:NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
  while (mItems->Count()) {
    nsISupports * supports = (nsISupports *)mItems->ElementAt(0);
    NS_RELEASE(supports);
    mItems->RemoveElementAt(0);

    ::RemoveMenu(mMenu, 0, MF_BYPOSITION);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
  aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetNativeData(void * aData)
{
  // Temporary hack for MacOS. Will go away when nsMenuBar handles it's own
  // construction
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
  mParent->Invalidate(PR_TRUE);
  return NS_OK;
}
