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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsMenu.h"
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStringUtil.h"

#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"

#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/RowColumn.h>

static NS_DEFINE_CID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID,         NS_MENUITEM_CID);
static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);

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
  NS_IF_RELEASE(mListener);
  // Free our menu items
  RemoveAll();
  XtDestroyWidget(mMenu);
  mMenu = nsnull;
}

//-------------------------------------------------------------------------
Widget nsMenu::GetNativeParent()
{
  void * voidData; 
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(&voidData);
  } else if (nsnull != mMenuBarParent) {
    mMenuBarParent->GetNativeData(voidData);
  } else {
    return NULL;
  }
  return (Widget)voidData;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsISupports * aParent, const nsString &aLabel)
{
  printf("nsMenu::Create called\n");
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

  char * labelStr = ToNewCString(mLabel);

  char wName[512];

  sprintf(wName, "__pulldown_%s", labelStr);
  NS_ADDREF(mMenuBarParent);
  mMenu = XmCreatePulldownMenu(GetNativeParent(), wName, NULL, 0);

  Widget casBtn;
  XmString str;

  str = XmStringCreateLocalized(labelStr);
  casBtn = XtVaCreateManagedWidget(labelStr,
                                   xmCascadeButtonGadgetClass, GetNativeParent(),
                                   XmNsubMenuId, mMenu,
                                   XmNlabelString, str,
                                   NULL);
  XmStringFree(str);

  delete[] labelStr;

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

NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  return NS_OK;
}

NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)
{
  printf("nsMenu::AddItem called\n");
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
  // XXX add aMenuItem to internal data structor list
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  // XXX add aMenu to internal data structor list
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  printf("nsMenu::AddSeparator() called\n");
  XtVaCreateManagedWidget("__sep", xmSeparatorGadgetClass, mMenu, NULL);
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
NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  return NS_OK;
}

NS_METHOD nsMenu::SetNativeData(void * aData)
{
  return NS_OK;
}

NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  aData = (void **)mMenu;
  return NS_OK;
}

NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;
  NS_ADDREF(mListener);
  return NS_OK;
}

NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener) {
    NS_IF_RELEASE(mListener);
  }
  return NS_OK;
}

NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
  mDOMNode = aMenuNode;
  return NS_OK;
}

NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{ 
  mDOMElement = aMenuElement;
  return NS_OK;
}

NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}

void nsMenu::LoadMenuItem(nsIMenu *       pParentMenu,
                          nsIDOMElement * menuitemElement,
                          nsIDOMNode *    menuitemNode,
                          unsigned short  menuitemIndex,
                          nsIWebShell *   aWebShell)
{
  //XXX:Implement this.
  return;
}

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

nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  if (nsnull != mListener) {
    mListener->MenuDeselected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                    nsIWidget         * aParentWindow,
                                    void              * menuNode,
                                    void              * aWebShell)
{
  printf("nsMenu::MenuConstruct called\n");
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

nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  printf("nsMenu::MenuDestruct called\n");
  // Close the node.   
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->RemoveAttribute("open");
  
  mConstructCalled = PR_FALSE;
  RemoveAll();
  return nsEventStatus_eIgnore;
}
