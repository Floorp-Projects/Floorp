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

#include "nsContextMenu.h"
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsString.h"
#include "nsGtkEventHandler.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

static NS_DEFINE_CID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID,         NS_MENUITEM_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//-------------------------------------------------------------------------
nsresult nsContextMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

//-------------------------------------------------------------------------
NS_IMPL_ADDREF(nsContextMenu)
NS_IMPL_RELEASE(nsContextMenu)

//-------------------------------------------------------------------------
//
// nsContextMenu constructor
//
//-------------------------------------------------------------------------
nsContextMenu::nsContextMenu()
{
  NS_INIT_REFCNT();
  mNumMenuItems  = 0;
  mMenu          = nsnull;
  mParent        = nsnull;
  mListener      = nsnull;
  mConstructCalled = PR_FALSE;
  
  mDOMNode       = nsnull;
  mWebShell      = nsnull;
  mDOMElement    = nsnull;

  mAlignment      = "topleft";
  mAnchorAlignment = "none";
}

//-------------------------------------------------------------------------
//
// nsContextMenu destructor
//
//-------------------------------------------------------------------------
nsContextMenu::~nsContextMenu()
{

}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::Create(nsISupports *aParent,
                                const nsString& anAlignment,
                                const nsString& anAnchorAlignment)
{
  if(aParent)
  {
    nsIWidget *parent = nsnull;
    aParent->QueryInterface(nsIWidget::GetIID(), (void**) &parent);
    if(parent)
    {
      mParent = parent;
      NS_RELEASE(parent);
    }
  }

  mAlignment       = anAlignment;
  mAnchorAlignment = anAnchorAlignment;

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
NS_METHOD nsContextMenu::GetParent(nsISupports*& aParent)
{
  aParent = nsnull;
  if (nsnull != mParent) {
    return mParent->QueryInterface(kISupportsIID,
                                   (void**)&aParent);
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::AddItem(nsISupports * aItem)
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
NS_METHOD nsContextMenu::AddMenuItem(nsIMenuItem * aMenuItem)
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

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::AddMenu(nsIMenu * aMenu)
{
  nsString Label;
  GtkWidget *newmenu=nsnull;
  char *labelStr;
  void *voidData=NULL;
  
  aMenu->GetLabel(Label);
  labelStr = Label.ToNewCString();

  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID,
                                                   nsnull,
                                                   nsIMenuItem::GetIID(),
                                                   (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    nsISupports * supports = nsnull;
    QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenuItem->Create(supports, labelStr, PR_FALSE); //PR_TRUE); 
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

  delete[] labelStr;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::AddSeparator() 
{
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(
    kMenuItemCID, nsnull, nsIMenuItem::GetIID(), (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    nsString tmp = "separator";
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
NS_METHOD nsContextMenu::GetItemCount(PRUint32 &aCount)
{
  // this should be right.. does it need to be +1 ?
  aCount = g_list_length(GTK_MENU_SHELL(mMenu)->children);
  //g_print("nsMenu::GetItemCount = %i\n", aCount);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::GetItemAt(const PRUint32 aCount, nsISupports *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::InsertItemAt(const PRUint32 aCount, nsISupports * aMenuItem)
{
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::InsertSeparator(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::RemoveItem(const PRUint32 aCount)
{
#if 0
  // this may work here better than Removeall(), but i'm not sure how to test this one
  nsISupports *item = mMenuItemVoidArray[aPos];
  delete item;
  mMenuItemVoidArray.RemoveElementAt(aPos);
#endif
  /*
  gtk_menu_shell_remove (GTK_MENU_SHELL (mMenu), item);

  delete[] labelStr;

  voidData = NULL;

  aMenu->GetNativeData(&voidData);
  newmenu = GTK_WIDGET(voidData);

  gtk_menu_item_remove_submenu (GTK_MENU_ITEM (item));
  */
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::RemoveAll()
{
  //g_print("nsMenu::RemoveAll()\n");
#undef DEBUG_pavlov
#ifdef DEBUG_pavlov
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
      if (NS_OK == item->QueryInterface(nsIMenuItem::GetIID(), (void**)&menuitem))
      {
        // we do this twice because we have to do it once for QueryInterface,
        // then we want to get rid of it.
        // g_print("remove nsMenuItem\n");
        NS_RELEASE(menuitem);
        NS_RELEASE(item);
        menuitem = nsnull;
      } else if (NS_OK == item->QueryInterface(nsIMenu::GetIID(), (void**)&menu)) {
#ifdef NOISY_MENUS
        g_print("remove nsMenu\n");
#endif
        NS_RELEASE(menu);
        NS_RELEASE(item);
        menu = nsnull;
      }
      // mMenuItemVoidArray.RemoveElementAt(i-1);
    }
  }
  mMenuItemVoidArray.Clear();

  return NS_OK;
#else
  for (int i = mMenuItemVoidArray.Count(); i > 0; i--) {
    if(nsnull != mMenuItemVoidArray[i-1]) {
      nsIMenuItem * menuitem = nsnull;
      ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(nsIMenuItem::GetIID(),
                                                              (void**)&menuitem);
      if(menuitem) {
        void *gtkmenuitem = nsnull;
        menuitem->GetNativeData(gtkmenuitem);
        if (gtkmenuitem) {
          gtk_widget_ref(GTK_WIDGET(gtkmenuitem));
          //gtk_widget_destroy(GTK_WIDGET(gtkmenuitem));
          g_print("%p, %p\n",
                  GTK_WIDGET(GTK_CONTAINER(GTK_MENU_SHELL(GTK_MENU(mMenu)))),
                  GTK_WIDGET(GTK_WIDGET(gtkmenuitem)->parent));
          gtk_container_remove(GTK_CONTAINER(GTK_MENU_SHELL(GTK_MENU(mMenu))),
                               GTK_WIDGET(gtkmenuitem));
        }
 
      } else {
 
        nsIMenu * menu= nsnull;
        ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(nsIMenu::GetIID(),
                                                                (void**)&menu);
        if(menu)
          {
            void * gtkmenu = nsnull;
            menu->GetNativeData(&gtkmenu);
 
            if(gtkmenu){
              g_print("nsMenu::RemoveAll() trying to remove nsMenu");
 
              //gtk_menu_item_remove_submenu (GTK_MENU_ITEM (item));
            }
          }
 
      }
    }
  }
//g_print("end RemoveAll\n");
  return NS_OK;
#endif
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::GetNativeData(void ** aData)
{
  *aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;
  NS_ADDREF(mListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener) {
    NS_IF_RELEASE(mListener);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsContextMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  if (nsnull != mListener) {
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

void nsContextMenu::MenuPosFunc(GtkMenu *menu,
                                  gint *x,
                                  gint *y,
                                  gpointer data)
{
  nsContextMenu *cm = (nsContextMenu*)data;
  *x = cm->GetX();
  *y = cm->GetY();
}

nsEventStatus nsContextMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  GtkWidget *parent = GTK_WIDGET(mParent->GetNativeData(NS_NATIVE_WIDGET));
  gtk_menu_popup (GTK_MENU(mMenu),
                  parent, NULL,
                  nsContextMenu::MenuPosFunc,
                  this, 1, GDK_CURRENT_TIME);

  if (nsnull != mListener) {
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsIMenuItem * nsContextMenu::FindMenuItem(nsIContextMenu * aMenu, PRUint32 aId)
{
  return nsnull;
}

//-------------------------------------------------------------------------
nsEventStatus nsContextMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  if (nsnull != mListener) {
    mListener->MenuDeselected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsContextMenu::MenuConstruct(const nsMenuEvent &aMenuEvent,
                                           nsIWidget         *aParentWindow, 
                                           void              *menuNode,
                                           void              *aWebShell)
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
          LoadMenuItem(this,
                       menuitemElement,
                       menuitemNode,
                       menuIndex,
                       (nsIWebShell*)aWebShell);
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
nsEventStatus nsContextMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
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

//----------------------------------------
void nsContextMenu::LoadMenuItem(nsIMenu        *pParentMenu,
                                 nsIDOMElement  *menuitemElement,
                                 nsIDOMNode     *menuitemNode,
                                 unsigned short  menuitemIndex,
                                 nsIWebShell    *aWebShell)
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
            
    if(disabled == NS_STRING_TRUE) {
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
void nsContextMenu::LoadSubMenu(nsIMenu         *pParentMenu,
                                nsIDOMElement   *menuElement,
                                nsIDOMNode      *menuNode)
{
  nsString menuName;
  menuElement->GetAttribute(nsAutoString("name"), menuName);
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
    */
  }     
}

//----------------------------------------
void nsContextMenu::LoadMenuItem(nsIContextMenu *pParentMenu,
                                 nsIDOMElement  *menuitemElement,
                                 nsIDOMNode     *menuitemNode,
                                 unsigned short  menuitemIndex,
                                 nsIWebShell    *aWebShell)
{

}

//----------------------------------------
void nsContextMenu::LoadSubMenu(nsIContextMenu  *pParentMenu,
                                nsIDOMElement   *menuElement,
                                nsIDOMNode      *menuNode)
{

}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetLocation(PRInt32 aX, PRInt32 aY)
{
  mX = aX;
  mY = aY;
  return NS_OK;
}


// local methods
gint nsContextMenu::GetX(void)
{
  return mX;
}

gint nsContextMenu::GetY(void)
{
  return mY;
}
// end silly local methods

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetDOMNode(nsIDOMNode *aMenuNode)
{
  mDOMNode = aMenuNode;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetDOMElement(nsIDOMElement *aMenuElement)
{
  mDOMElement = aMenuElement;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetWebShell(nsIWebShell *aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}
