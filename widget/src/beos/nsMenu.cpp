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
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_CID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID,         NS_MENUITEM_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult nsMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  if (aIID.Equals(nsIMenu::GetIID())) {
    *aInstancePtr = (void*)(nsIMenu*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIMenuListener::GetIID())) {
    *aInstancePtr = (void*)(nsIMenuListener*)this;
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
  NS_NewISupportsArray(&mItems);

  mDOMNode       = nsnull;
  mWebShell      = nsnull;
  mDOMElement    = nsnull;
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
    aParent->QueryInterface(nsIMenuBar::GetIID(), (void**) &menubar);
    if(menubar)
    {
      mMenuBarParent = menubar;
      NS_RELEASE(menubar);
    }
    else
    {
      nsIMenu * menu = nsnull;
      aParent->QueryInterface(nsIMenu::GetIID(), (void**) &menu);
      if(menu)
      {
        mMenuParent = menu;
        NS_RELEASE(menu);
      }
    }
  }

  mLabel = aLabel;
  char *str = mLabel.ToNewCString();
  mMenu = new BMenu(str);
  delete [] str;

  return NS_OK;
}

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
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Set enabled state
*
*/
NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)
{
  if(aItem)
  {
    nsIMenuItem * menuitem = nsnull;
    aItem->QueryInterface(nsIMenuItem::GetIID(),
                          (void**)&menuitem);
    if(menuitem)
    {
      AddMenuItem(menuitem); // nsMenu now owns this
      NS_RELEASE(menuitem);
    }
    else
    {
      nsIMenu * menu = nsnull;
      aItem->QueryInterface(nsIMenu::GetIID(),
                            (void**)&menu);
      if(menu)
      {
        AddMenu(menu); // nsMenu now owns this
        NS_RELEASE(menu);
      }
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(
    kMenuItemCID, nsnull, nsIMenuItem::GetIID(), (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    nsString tmp = "menuseparator";
    nsISupports * supports = nsnull;
    QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenuItem->Create(supports, tmp, PR_TRUE);  
    NS_RELEASE(supports);
    
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports); 
          
    NS_RELEASE(pnsMenuItem);
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
  PRUint32 cnt;
  nsresult rv = mItems->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  return InsertItemAt(cnt, (nsISupports *)aMenuItem);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  PRUint32 cnt;
  nsresult rv = mItems->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  return InsertItemAt(cnt, (nsISupports *)aMenu);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return mItems->Count(&aCount);
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
  bool status = false;

  mItems->InsertElementAt(aMenuItem, (PRInt32)aCount);

  nsCOMPtr<nsIMenuItem> menuItem(do_QueryInterface(aMenuItem));
  if (menuItem) {
    BMenuItem *it;
    void *data;
    menuItem->GetNativeData(data);
    it = (BMenuItem *)data;
    mMenu->AddItem(it, aCount);
    status = true;
  } else {
    nsCOMPtr<nsIMenu> menu(do_QueryInterface(aMenuItem));
    if (menu) {
      BMenu *m;
      void *data;
      menu->GetNativeData(&data);
      m = (BMenu *)data;
      mMenu->AddItem(m, aCount);
      status = true;
    }
  }

  return (status ? NS_OK : NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{
printf("nsMenu::RemoveItem - FIXME: not implemented\n");
#if 0
  //nsISupports * supports = (nsISupports *)mItems->ElementAt(aCount);
  mItems->RemoveElementAt(aCount);

  return (::RemoveMenu(mMenu, aCount, MF_BYPOSITION) ? NS_OK:NS_ERROR_FAILURE);
#endif
	return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
printf("nsMenu::RemoveAll - FIXME: not implemented\n");
#if 0
  while (mItems->Count()) {
    mItems->RemoveElementAt(0);
    ::RemoveMenu(mMenu, 0, MF_BYPOSITION);
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  *aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetNativeData(void * aData)
{
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
nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  if (nsnull != mListener) {
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  if (nsnull != mListener) {
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  if (nsnull != mListener) {
    mListener->MenuDeselected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                    nsIWidget         * aParentWindow, 
                                    void              * menuNode,
                                    void              * aWebShell)
{
  //g_print("nsMenu::MenuConstruct called \n");
  if(menuNode){
    SetDOMNode((nsIDOMNode*)menuNode);
  }
  
  if(!aWebShell){
    aWebShell = mWebShell;
  }

  // First open the menu.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->SetAttribute("open", "true");


   /// Now get the kids. Retrieve our menupopup child.
  nsCOMPtr<nsIDOMNode> menuPopupNode;
  mDOMNode->GetFirstChild(getter_AddRefs(menuPopupNode));
  while (menuPopupNode) {
    nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
    if (menuPopupElement) {
      nsString menuPopupNodeType;
      menuPopupElement->GetNodeName(menuPopupNodeType);
      if (menuPopupNodeType.Equals("menupopup"))
        break;
    }
    nsCOMPtr<nsIDOMNode> oldMenuPopupNode(menuPopupNode);
    oldMenuPopupNode->GetNextSibling(getter_AddRefs(menuPopupNode));
  }

  if (!menuPopupNode)
    return nsEventStatus_eIgnore;

  nsCOMPtr<nsIDOMNode> menuitemNode;
  menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

  unsigned short menuIndex = 0;

    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        nsString menuitemName;
        menuitemElement->GetNodeName(menuitemNodeType);
        if (menuitemNodeType.Equals("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(this,
                       menuitemElement,
                       menuitemNode,
                       menuIndex,
                       (nsIWebShell*)aWebShell);
        } else if (menuitemNodeType.Equals("menuseparator")) {
          AddSeparator();
        } else if (menuitemNodeType.Equals("menu")) {
          // Load a submenu
          LoadSubMenu(this, menuitemElement, menuitemNode);
        }
      }
      
      ++menuIndex;
      
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  // Close the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->RemoveAttribute("open");

  //g_print("nsMenu::MenuDestruct called \n");
  RemoveAll();
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Set DOMNode
*
*/
NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
  mDOMNode = aMenuNode;
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Set DOMElement
*
*/
NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{
  mDOMElement = aMenuElement;
  return NS_OK;
}
    
//-------------------------------------------------------------------------
/**
* Set WebShell
*
*/
NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}

//----------------------------------------
void nsMenu::LoadMenuItem(nsIMenu *       pParentMenu,
                          nsIDOMElement * menuitemElement,
                          nsIDOMNode *    menuitemNode,
                          unsigned short  menuitemIndex,
                          nsIWebShell *   aWebShell)
{
  static const char* NS_STRING_TRUE = "true";
  nsString disabled;
  nsString menuitemName;
  nsString menuitemCmd;

  menuitemElement->GetAttribute(nsAutoString("disabled"), disabled);
  menuitemElement->GetAttribute(nsAutoString("value"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
      
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID,
                                                   nsnull, 
                                                   nsIMenuItem::GetIID(), 
                                                   (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, PR_FALSE);
                     
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);
            
    if(disabled == NS_STRING_TRUE ) {
      pnsMenuItem->SetEnabled(PR_FALSE);
    }
  
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
      //return NS_ERROR_FAILURE;
		return;
    }
    
    nsAutoString cmdAtom("oncommand");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);

    pnsMenuItem->SetCommand(cmdName);
   // DO NOT use passed in webshell because of messed up windows dynamic loading
   // code. 
    pnsMenuItem->SetWebShell(mWebShell);
    pnsMenuItem->SetDOMElement(domElement);

    NS_RELEASE(pnsMenuItem);
    
  } 
  return;
}

//----------------------------------------
void nsMenu::LoadSubMenu(nsIMenu *       pParentMenu,
                         nsIDOMElement * menuElement,
                         nsIDOMNode *    menuNode)
{
  nsString menuName;
  menuElement->GetAttribute(nsAutoString("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID,
                                                   nsnull,
                                                   nsIMenu::GetIID(),
                                                   (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI

    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 

    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports); // parent takes ownership
    NS_RELEASE(supports);

    pnsMenu->SetWebShell(mWebShell);
    pnsMenu->SetDOMNode(menuNode);

    /*
    // Begin menuitem inner loop
    unsigned short menuIndex = 0;

    nsCOMPtr<nsIDOMNode> menuitemNode;
    menuNode->GetFirstChild(getter_AddRefs(menuitemNode));
    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        menuitemElement->GetNodeName(menuitemNodeType);

#ifdef DEBUG_saari
        printf("Type [%s] %d\n", menuitemNodeType.ToNewCString(), menuitemNodeType.Equals("menuseparator"));
#endif

        if (menuitemNodeType.Equals("menuitem")) {
          // Load a menuitem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode, menuIndex, mWebShell);
        } else if (menuitemNodeType.Equals("menuseparator")) {
          pnsMenu->AddSeparator();
        } else if (menuitemNodeType.Equals("menu")) {
          // Add a submenu
          LoadSubMenu(pnsMenu, menuitemElement, menuitemNode);
        }
      }
	  ++menuIndex;
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
    */
  }     
}
