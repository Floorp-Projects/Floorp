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

#include <gtk/gtk.h>

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"

#include "nsString.h"
#include "nsStringUtil.h"
#include "nsIMenuListener.h"

#include "nsGtkEventHandler.h"

#include "nsCOMPtr.h"

#include "nsIComponentManager.h"

#include "nsWidgetsCID.h"
static NS_DEFINE_IID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID,         NS_MENUITEM_CID);

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
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  if (aIID.Equals(kIMenuListenerIID)) {                                      
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
  mNumMenuItems  = 0;
  mMenu          = nsnull;
  mMenuParent    = nsnull;
  mMenuBarParent = nsnull;
  mListener      = nsnull;
  mConstructCalled = false;
  
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
}

//-------------------------------------------------------------------------
//
// Create the proper widget
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
      NS_RELEASE(menubar);
    }
    else
    {
      nsIMenu * menu = nsnull;
      aParent->QueryInterface(kIMenuIID, (void**) &menu);
      if(menu)
      {
        mMenuParent = menu;
        NS_ADDREF(mMenuParent);
        NS_RELEASE(menu);
      }
    }
  }

  mLabel = aLabel;
  mMenu = gtk_menu_new();

  gtk_signal_connect (GTK_OBJECT (mMenu), "map",
                      GTK_SIGNAL_FUNC(menu_map_handler),
		      this);
  gtk_signal_connect (GTK_OBJECT (mMenu), "unmap",
                      GTK_SIGNAL_FUNC(menu_unmap_handler),
		      this); 	    		      
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
  /* we Do GetLabel() when we are adding the menu...
   *  but we might want to redo this.
   */
  mLabel = aText;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)
{
  if(aItem)
  {
    nsIMenuItem * menuitem = nsnull;
    aItem->QueryInterface(kIMenuItemIID, (void**) &menuitem);
    if(menuitem)
    {
      AddMenuItem(menuitem);
      NS_RELEASE(menuitem);
    }
    else
    {
      nsIMenu * menu = nsnull;
      aItem->QueryInterface(kIMenuIID, (void**) &menu);
      if(menu)
      {
        AddMenu(menu);
        NS_RELEASE(menu);
      }
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  GtkWidget *widget;
  void      *voidData;
  
  aMenuItem->GetNativeData(voidData);
  widget = GTK_WIDGET(voidData);

  gtk_menu_shell_append (GTK_MENU_SHELL (mMenu), widget);

  // XXX add aMenuItem to internal data structor list
  NS_IF_ADDREF(aMenuItem);
  mMenuItemVoidArray.AppendElement(aMenuItem);
  mNumMenuItems++;
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  nsString Label;
  GtkWidget *item=nsnull, *newmenu=nsnull;
  char *labelStr;
  void *voidData=NULL;
  
  aMenu->GetLabel(Label);
  labelStr = Label.ToNewCString();

  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(
    kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(this, labelStr, 0);                 
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);
          
    //NS_RELEASE(pnsMenuItem);
  } 
  //item = gtk_menu_item_new_with_label (labelStr);
  //gtk_widget_show(item);
  //gtk_menu_shell_append (GTK_MENU_SHELL (mMenu), item);

  delete[] labelStr;

  void * menuitem = nsnull;
  pnsMenuItem->GetNativeData(menuitem);
  
  voidData = NULL;
  aMenu->GetNativeData(&voidData);
  newmenu = GTK_WIDGET(voidData);

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), newmenu);

  // XXX add aMenuItem to internal data structor list
  //NS_IF_ADDREF(aMenu);
  //mMenuItemVoidArray.AppendElement(aMenu);
  //mNumMenuItems++;
  
  NS_RELEASE(pnsMenuItem);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  //GtkWidget *widget;
  //widget = gtk_menu_item_new ();
  //gtk_widget_show(widget);
  //gtk_menu_shell_append (GTK_MENU_SHELL (mMenu), widget);
  
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(
    kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    // This is gross and I'll make it go away ASAP -cps
    nsString tmp = "separator";
    pnsMenuItem->Create(this, tmp, PR_TRUE);                 
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);
          
    //NS_RELEASE(pnsMenuItem);
  } 
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aPos)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{


  //gtk_menu_shell_remove (GTK_MENU_SHELL (mMenu), item);

  //delete[] labelStr;

  //voidData = NULL;

  //aMenu->GetNativeData(&voidData);
  //newmenu = GTK_WIDGET(voidData);

  //gtk_menu_item_remove_submenu (GTK_MENU_ITEM (item));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  for (int i = mMenuItemVoidArray.Count(); i > 0; i--) {
    if(nsnull != mMenuItemVoidArray[i-1]) {
      nsIMenuItem * menuitem = nsnull;
      ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuItemIID, (void**)&menuitem);
      if(menuitem) {
        void * gtkmenuitem = nsnull;
        menuitem->GetNativeData(gtkmenuitem);
	if(gtkmenuitem){
          gtk_container_remove (GTK_CONTAINER (mMenu), GTK_WIDGET(gtkmenuitem) );
	}
      }else{
        nsIMenu * menu= nsnull;
        ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuIID, (void**)&menu);
        if(menu){
	  void * gtkmenu = nsnull;
          menu->GetNativeData(&gtkmenu);
	  if(gtkmenu){
	    g_print("gtkmenu removed");
            
	    //gtk_menu_item_remove_submenu (GTK_MENU_ITEM (item));
	  }
        }
      }
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  *aData = (void *)mMenu;
  return NS_OK;
}

GtkWidget *nsMenu::GetNativeParent()
{
  void * voidData; 
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(&voidData);
  } else if (nsnull != mMenuBarParent) {
    mMenuBarParent->GetNativeData(voidData);
  } else {
    return nsnull;
  }
  return GTK_WIDGET(voidData);
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
nsEventStatus nsMenu::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
    void              * aWebShell)
{
  g_print("nsMenu::MenuConstruct called \n");
  if(menuNode){
    SetDOMNode((nsIDOMNode*)menuNode);
  }
  
  if(!aWebShell){
    aWebShell = mWebShell;
  }

   printf("nsMenu::MenuConstruct called \n");
   // Begin menuitem inner loop
    nsCOMPtr<nsIDOMNode> menuitemNode;
    ((nsIDOMNode*)mDOMNode)->GetFirstChild(getter_AddRefs(menuitemNode));
 
    unsigned short menuIndex = 0;

    while (menuitemNode) {
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        nsString menuitemName;
        menuitemElement->GetNodeName(menuitemNodeType);
        if (menuitemNodeType.Equals("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(this, menuitemElement, menuitemNode, menuIndex, (nsIWebShell*)aWebShell);
        } else if (menuitemNodeType.Equals("separator")) {
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
  g_print("nsMenu::MenuDestruct called \n");
  mConstructCalled = false;
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
  NS_IF_RELEASE(mDOMNode);
  mDOMNode = aMenuNode;
  NS_IF_ADDREF(mDOMNode);
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Set DOMElement
*
*/
NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{
  NS_IF_RELEASE(mDOMElement);
  mDOMElement = aMenuElement;
  NS_IF_ADDREF(mDOMElement);
  return NS_OK;
}
    
//-------------------------------------------------------------------------
/**
* Set WebShell
*
*/
NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
  NS_IF_RELEASE(mWebShell);
  mWebShell = aWebShell;
  NS_IF_ADDREF(mWebShell);
  return NS_OK;
}

//----------------------------------------
void nsMenu::LoadMenuItem(
  nsIMenu *       pParentMenu,
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
  menuitemElement->GetAttribute(nsAutoString("name"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
      
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
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
    
    nsAutoString cmdAtom("onclick");
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
void nsMenu::LoadSubMenu(
  nsIMenu *       pParentMenu,
  nsIDOMElement * menuElement,
  nsIDOMNode *    menuNode)
{
  nsString menuName;
  menuElement->GetAttribute(nsAutoString("name"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI

    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 
    // Make nsMenu a child of parent nsMenu
    //pParentMenu->AddMenu(pnsMenu);
    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
	pParentMenu->AddItem(supports);
	NS_RELEASE(supports);

	NS_ASSERTION(mWebShell, "get debugger");
	pnsMenu->SetWebShell(mWebShell);
	pnsMenu->SetDOMNode(menuNode);

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
        printf("Type [%s] %d\n", menuitemNodeType.ToNewCString(), menuitemNodeType.Equals("separator"));
#endif

        if (menuitemNodeType.Equals("menuitem")) {
          // Load a menuitem
          LoadMenuItem(pnsMenu, menuitemElement, menuitemNode, menuIndex, mWebShell);
        } else if (menuitemNodeType.Equals("separator")) {
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
  }     
}
