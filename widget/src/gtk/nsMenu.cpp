/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <gtk/gtk.h>

#include "nsMenu.h"
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsGtkEventHandler.h"
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

  if (aIID.Equals(NS_GET_IID(nsIMenu))) {
    *aInstancePtr = (void*)(nsIMenu*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIMenuListener))) {
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
  mConstructCalled = PR_FALSE;
  
  mDOMNode       = nsnull;
  mWebShell      = nsnull;
  mDOMElement    = nsnull;
  mAccessKey     = "_";
}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
  //g_print("nsMenu::~nsMenu() called\n");
  NS_IF_RELEASE(mListener);
  // Free our menu items
  RemoveAll();
  gtk_widget_destroy(mMenu);
  mMenu = nsnull;
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
    aParent->QueryInterface(NS_GET_IID(nsIMenuBar), (void**) &menubar);
    if(menubar)
    {
      mMenuBarParent = menubar;
      NS_RELEASE(menubar);
    }
    else
    {
      nsIMenu * menu = nsnull;
      aParent->QueryInterface(NS_GET_IID(nsIMenu), (void**) &menu);
      if(menu)
      {
        mMenuParent = menu;
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
    return mMenuParent->QueryInterface(kISupportsIID,
                                       (void**)&aParent);
  } else if (nsnull != mMenuBarParent) {
    return mMenuBarParent->QueryInterface(kISupportsIID,
                                          (void**)&aParent);
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
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  aText = mAccessKey;
  char *foo = ToNewCString(mAccessKey);

#ifdef DEBUG_pavlov
  g_print("GetAccessKey returns \"%s\"\n", foo);
#endif

  nsCRT::free(foo);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
  mAccessKey = aText;
  char *foo = ToNewCString(mAccessKey);

#ifdef DEBUG_pavlov
  g_print("SetAccessKey setting to \"%s\"\n", foo);
#endif

  nsCRT::free(foo);
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
/**
* Get enabled state
*
*/
NS_METHOD nsMenu::GetEnabled(PRBool* aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Query if this is the help menu
*
*/
NS_METHOD nsMenu::IsHelpMenu(PRBool* aIsHelpMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)
{
  if(aItem)
  {
    nsIMenuItem * menuitem = nsnull;
    aItem->QueryInterface(NS_GET_IID(nsIMenuItem),
                          (void**)&menuitem);
    if(menuitem)
    {
      AddMenuItem(menuitem); // nsMenu now owns this
      NS_RELEASE(menuitem);
    }
    else
    {
      nsIMenu * menu = nsnull;
      aItem->QueryInterface(NS_GET_IID(nsIMenu),
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

// local method used by nsMenu::AddItem
//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  GtkWidget *widget;
  void      *voidData;
  
  aMenuItem->GetNativeData(voidData);
  widget = GTK_WIDGET(voidData);

  gtk_menu_shell_append (GTK_MENU_SHELL (mMenu), widget);

  // XXX add aMenuItem to internal data structor list
  // Need to be adding an nsISupports *, not nsIMenuItem *
  nsISupports * supports = nsnull;
  aMenuItem->QueryInterface(kISupportsIID,
                            (void**)&supports);
  {
    mMenuItemVoidArray.AppendElement(supports);
    mNumMenuItems++;
  }
  
  return NS_OK;
}

// local method used by nsMenu::AddItem
//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  nsString Label;
  GtkWidget *newmenu=nsnull;
  void *voidData=NULL;
  
  aMenu->GetLabel(Label);

  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID,
                                                   nsnull,
                                                   NS_GET_IID(nsIMenuItem),
                                                   (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    nsISupports * supports = nsnull;
    QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenuItem->Create(supports, Label, PR_FALSE); //PR_TRUE); 
    NS_RELEASE(supports);               
    
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);
          
    void * menuitem = nsnull;
    pnsMenuItem->GetNativeData(menuitem);
  
    voidData = NULL;
    aMenu->GetNativeData(&voidData);
    newmenu = GTK_WIDGET(voidData);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), newmenu);
  
    NS_RELEASE(pnsMenuItem);
  } 

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(
    kMenuItemCID, nsnull, NS_GET_IID(nsIMenuItem), (void**)&pnsMenuItem);
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
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  // this should be right.. does it need to be +1 ?
  aCount = g_list_length(GTK_MENU_SHELL(mMenu)->children);
  //g_print("nsMenu::GetItemCount = %i\n", aCount);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aPos,
                            nsISupports *&aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aPos,
                               nsISupports *aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{
#if 0
  // this may work here better than Removeall(), but i'm not sure how to test this one
  nsISupports *item = mMenuItemVoidArray[aPos];
  delete item;
  mMenuItemVoidArray.RemoveElementAt(aPos);
#endif
  /*
  gtk_menu_shell_remove (GTK_MENU_SHELL (mMenu), item);

  nsCRT::free(labelStr);

  voidData = NULL;

  aMenu->GetNativeData(&voidData);
  newmenu = GTK_WIDGET(voidData);

  gtk_menu_item_remove_submenu (GTK_MENU_ITEM (item));
  */
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  //g_print("nsMenu::RemoveAll()\n");
#if 0
  // this doesn't work quite right, but this is about all that should really be needed
  int i=0;
  nsIMenu *menu = nsnull;
  nsIMenuItem *menuitem = nsnull;
  nsISupports *item = nsnull;

  for (i=mMenuItemVoidArray.Count(); i>0; i--)
  {
    item = (nsISupports*)mMenuItemVoidArray[i-1];

    if(nsnull != item)
    {
      if (NS_OK == item->QueryInterface(NS_GET_IID(nsIMenuItem), (void**)&menuitem)) {
        // we do this twice because we have to do it once for QueryInterface,
        // then we want to get rid of it.
        // g_print("remove nsMenuItem\n");
        NS_RELEASE(menuitem);
        NS_RELEASE(item);
        menuitem = nsnull;
      } else if (NS_OK == item->QueryInterface(NS_GET_IID(nsIMenu), (void**)&menu)) {
        // g_print("remove nsMenu\n");
        NS_RELEASE(menu);
        NS_RELEASE(item);
        menu = nsnull;
      }
      // mMenuItemVoidArray.RemoveElementAt(i-1);
    }
  }
  mMenuItemVoidArray.Clear();
#else
  for (int i = mMenuItemVoidArray.Count(); i > 0; i--) {
    if(nsnull != mMenuItemVoidArray[i-1]) {
      nsIMenuItem * menuitem = nsnull;
      ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(NS_GET_IID(nsIMenuItem),
                                                              (void**)&menuitem);
      if(menuitem) {
        void *gtkmenuitem = nsnull;
        menuitem->GetNativeData(gtkmenuitem);
        if (gtkmenuitem) {
          //          gtk_widget_ref(GTK_WIDGET(gtkmenuitem));
          //gtk_widget_destroy(GTK_WIDGET(gtkmenuitem));
           gtk_container_remove (GTK_CONTAINER (mMenu), GTK_WIDGET(gtkmenuitem));
        }
 
      } else {
 
        nsIMenu * menu= nsnull;
        ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(NS_GET_IID(nsIMenu),
                                                                (void**)&menu);
        if(menu)
          {
            void * gtkmenu = nsnull;
            menu->GetNativeData(&gtkmenu);
 
            if(gtkmenu){
              //g_print("gtkmenu removed");
 
              //gtk_menu_item_remove_submenu (GTK_MENU_ITEM (item));
            }
          }
 
      }
    }
  }
#endif
//g_print("end RemoveAll\n");
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
  mConstructCalled = PR_FALSE;
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
* Get DOMNode
*
*/
NS_METHOD nsMenu::GetDOMNode(nsIDOMNode ** aMenuNode)
{
  if(aMenuNode) {
    *aMenuNode = mDOMNode;
    NS_IF_ADDREF(mDOMNode);
  }
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
  menuitemElement->GetAttribute(nsAutoString("label"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
      
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID,
                                                   nsnull, 
                                                   NS_GET_IID(nsIMenuItem), 
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
  menuElement->GetAttribute(nsAutoString("label"), menuName);
  //printf("Creating Menu [%s] \n", NS_LossyConvertUCS2toASCII(menuName).get());

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID,
                                                   nsnull,
                                                   NS_GET_IID(nsIMenu),
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
        printf("Type [%s] %d\n", NS_LossyConvertUCS2toASCII(menuitemNodeType).get(), menuitemNodeType.Equals("menuseparator"));
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
