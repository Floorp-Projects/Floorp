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
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsString.h"
#include "nsGtkEventHandler.h"
#include "nsCOMPtr.h"


#include "nsWidgetsCID.h"
static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kMenuCID,          NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID,      NS_MENUITEM_CID);

//-------------------------------------------------------------------------
nsresult nsContextMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  if (aIID.Equals(nsIContextMenu::GetIID())) {
    *aInstancePtr = (void*)(nsIContextMenu*) this;
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
  mMenu          = nsnull;
  mParent        = nsnull;
  mListener      = nsnull;
  mConstructed = PR_FALSE;
  
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
NS_METHOD nsContextMenu::Create(nsISupports *aParent)
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
NS_METHOD nsContextMenu::GetNativeData(void ** aData)
{
  *aData = (void *)mMenu;
  return NS_OK;
}

/* used for gtk_menu_popup */
void MenuPosFunc(GtkMenu *menu, gint *x, gint *y, gpointer data)
{
  nsContextMenu *cm = (nsContextMenu*)data;
  *x = cm->GetX();
  *y = cm->GetY();
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::Show(PRBool aShow)
{
  if (aShow == PR_TRUE)
  {
    GtkWidget *parent = GTK_WIDGET(mParent->GetNativeData(NS_NATIVE_WIDGET));
    gtk_menu_popup(GTK_MENU(mMenu),
                   parent, nsnull,
                   MenuPosFunc,
                   this, 1, GDK_CURRENT_TIME);
  }
  else
  {
    printf("we suck\n");
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetLocation(PRInt32 aX, PRInt32 aY,
                                     const nsString& anAlignment,
                                     const nsString& anAnchorAlignment)
{

  mAlignment       = anAlignment;
  mAnchorAlignment = anAnchorAlignment;

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
nsEventStatus nsContextMenu::MenuConstruct(const nsMenuEvent &aMenuEvent,
                                           nsIWidget         *aParentWindow, 
                                           void              *menuNode,
                                           void              *aWebShell)
{
  //printf("nsMenu::MenuConstruct called \n");
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
      } else if (menuitemNodeType.Equals("menuseparator")) {
        //        AddSeparator();
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
  //printf("nsMenu::MenuDestruct called \n");
  // We cannot call RemoveAll() yet because menu item selection may need it
  //RemoveAll();
  /*  
  PRUint32 cnt;
  mItems->Count(&cnt);
  while (cnt) {
    mItems->RemoveElementAt(0);
    ::RemoveMenu(mMenu, 0, MF_BYPOSITION);
    mItems->Count(&cnt);
  }
  */
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
  menuitemElement->GetAttribute(nsAutoString("value"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, nsIMenuItem::GetIID(), (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);   
	
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);
          
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
      return;
    }
    
    nsAutoString cmdAtom("onaction");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);

    pnsMenuItem->SetCommand(cmdName);
    // DO NOT use passed in wehshell because of messed up windows dynamic loading
    // code. 
    pnsMenuItem->SetWebShell(mWebShell);
    pnsMenuItem->SetDOMElement(domElement);
    
    if(disabled == NS_STRING_TRUE )
      pnsMenuItem->SetEnabled(PR_TRUE);
    
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
  menuElement->GetAttribute(nsAutoString("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, nsIMenu::GetIID(), (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI

    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 

    // Make nsMenu a child of parent nsMenu. The parent takes ownership
    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports);
    NS_RELEASE(supports);

    pnsMenu->SetWebShell(mWebShell);
    pnsMenu->SetDOMNode(menuNode);
    pnsMenu->SetDOMElement(menuElement);

    // We're done with the menu
    NS_RELEASE(pnsMenu);
  }
}

//----------------------------------------
void nsContextMenu::LoadMenuItem(nsIContextMenu *pParentMenu,
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
  menuitemElement->GetAttribute(nsAutoString("value"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, nsIMenuItem::GetIID(), (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);   

    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    //    pParentMenu->AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);

    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
      return;
    }

    nsAutoString cmdAtom("onaction");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);

    pnsMenuItem->SetCommand(cmdName);
    // DO NOT use passed in wehshell because of messed up windows dynamic loading
    // code. 
    pnsMenuItem->SetWebShell(mWebShell);
    pnsMenuItem->SetDOMElement(domElement);

    if(disabled == NS_STRING_TRUE )
      pnsMenuItem->SetEnabled(PR_TRUE);

    NS_RELEASE(pnsMenuItem);
  }
  return;
}

//----------------------------------------
void nsContextMenu::LoadSubMenu(nsIContextMenu  *pParentMenu,
                                nsIDOMElement   *menuElement,
                                nsIDOMNode      *menuNode)
{
  nsString menuName;
  menuElement->GetAttribute(nsAutoString("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, nsIMenu::GetIID(), (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI

    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 

    // Make nsMenu a child of parent nsMenu. The parent takes ownership
    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
    //    pParentMenu->AddItem(supports);
    NS_RELEASE(supports);

    pnsMenu->SetWebShell(mWebShell);
    pnsMenu->SetDOMNode(menuNode);
    pnsMenu->SetDOMElement(menuElement);

    // We're done with the menu
    NS_RELEASE(pnsMenu);
  }
}



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
