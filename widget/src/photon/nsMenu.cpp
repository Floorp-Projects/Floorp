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

#include <Pt.h>
#include "nsPhWidgetLog.h"

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"

#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsIAppShell.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsGfxCIID.h"
#include "nsMenuItem.h"
#include "nsCOMPtr.h"
#include "nsIMenuListener.h"
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
                                                                                        
  if (aIID.Equals(kIMenuIID))
  {                                         
    *aInstancePtr = (void*)(nsIMenu*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      

  if (aIID.Equals(kISupportsIID))
  {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }

  if (aIID.Equals(kIMenuListenerIID))
  {
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
  mMenuButton    = nsnull;
  mMenuParent    = nsnull;
  mMenuBarParent = nsnull;
  mListener      = nsnull;
  mConstruct     = PR_FALSE;
  mIsSubMenu     = PR_FALSE;
  mParentWindow  = nsnull;  
  mDOMNode       = nsnull;
  mDOMElement    = nsnull;
  mWebShell      = nsnull;

  nsresult result = NS_NewISupportsArray(&mItems);
}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
  char *str=mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::~nsMenu Destructor called for <%s>\n", str));
  delete [] str;

  NS_IF_RELEASE(mListener);

  // Remove all references to the items
  mItems->Clear();

  /* Destroy my Photon Objects */
  if (mMenuButton)
    PtDestroyWidget(mMenuButton);
  if (mMenu)
    PtDestroyWidget(mMenu);
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsISupports * aParent, const nsString &aLabel)
{
 char *str=aLabel.ToNewCString();
 PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::Create with nsISupports aLabel=<%s> this=<%p>\n", str, this));
 delete [] str;
 
  PtArg_t       arg[5];
  void         *voidData  = nsnull;
  PtWidget_t   *myParent  = nsnull;
  void         *me        = (void *) this;
 
  /* Store the label in a local variable */
  mLabel = aLabel;

  /* Create a Char * string from a nsString */
  char *labelStr = mLabel.ToNewCString();

 if(aParent)
 {
   nsIMenuBar * menubar = nsnull;
   aParent->QueryInterface(kIMenuBarIID, (void**) &menubar);
   if(menubar)
   {
     mMenuBarParent = menubar;

     PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::Create with nsISupports menu on a menubar\n"));

     /* Get Photon Pointer that I will attach this menu to */
     mMenuBarParent->GetNativeData(voidData);
     myParent = (PtWidget_t *) voidData;

     PtSetArg(&arg[0], Pt_ARG_BUTTON_TYPE, Pt_MENU_DOWN, 0);
     PtSetArg(&arg[1], Pt_ARG_TEXT_STRING, labelStr, 0);
     PtSetArg(&arg[2], Pt_ARG_USER_DATA, &me, sizeof(void *));
     mMenuButton = PtCreateWidget(PtMenuButton, myParent, 3, arg);
     if (!mMenuButton)
     {
       PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenu::Create failed to create menubuton for menu\n"));  
       return NS_ERROR_FAILURE;
     }

     /* Set the Call back on this top level menu button */
     PtAddCallback(mMenuButton, Pt_CB_ARM, TopLevelMenuItemArmCb, this);

     /* Now Create the Photon Menu that is attached to the mMenuButton */
     PtSetArg(&arg[0], Pt_ARG_MENU_FLAGS, Pt_MENU_AUTO, 0xFFFFFFFF);
     PtSetArg(&arg[1], Pt_ARG_USER_DATA, &me, sizeof(void *) );
     mMenu = PtCreateWidget (PtMenu, mMenuButton, 2, arg);
     if (!mMenu)
     {
       PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenu::Create failed to create menu for menubutton\n"));  
       return NS_ERROR_FAILURE;
     }

     NS_RELEASE(menubar);
   }
   else
   {
     nsIMenu * menu = nsnull;
     aParent->QueryInterface(kIMenuIID, (void**) &menu);
     if(menu)
     {
       mMenuParent = menu;

       PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::Create with nsISupports menu on a menu\n"));

       /* Get Photon Pointer that I will attach this menu to */
       mMenuParent->GetNativeData(&voidData);
       myParent = (PtWidget_t *) voidData;
	 
       PtSetArg(&arg[0], Pt_ARG_BUTTON_TYPE, Pt_MENU_RIGHT, 0);
       PtSetArg(&arg[1], Pt_ARG_TEXT_STRING, labelStr, 0);
       PtSetArg(&arg[2], Pt_ARG_USER_DATA, &me, sizeof(void *));
       mMenuButton = PtCreateWidget(PtMenuButton, myParent, 3, arg);
       if (!mMenuButton)
       {
         PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenu::Create failed to create menubuton for menu\n"));  
         return NS_ERROR_FAILURE;
       }

       /* Set the Call back on this top level menu button */
       PtAddCallback(mMenuButton, Pt_CB_MENU, SubMenuMenuItemMenuCb, this);
       PtAddCallback(mMenuButton, Pt_CB_ARM, SubMenuMenuItemArmCb, this);

       mIsSubMenu = PR_TRUE;
	   NS_RELEASE(menu);
	 }
   }
 }

  if (mMenu)
  {
    /* Set the Call back on each menu */
//    PtAddCallback(mMenu, Pt_CB_REALIZED, MenuRealizedCb, this);
    PtAddCallback(mMenu, Pt_CB_UNREALIZED, MenuUnRealizedCb, this);
  }
  
  PtRealizeWidget(mMenuButton);
  delete[] labelStr;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::Create with nsIMenuBar PtMenuButton=<%p> PtMenu=<%p>\n", mMenuButton, mMenu));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{
  aParent = nsnull;
  if (nsnull != mMenuParent)
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetParent is menu=<%p>\n", aParent));
      return mMenuParent->QueryInterface(kISupportsIID,(void**)&aParent);
  }
  else if (nsnull != mMenuBarParent)
  {
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetParent is menubar=<%p>\n", aParent));
      return mMenuBarParent->QueryInterface(kISupportsIID,(void**)&aParent);
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetLabel(nsString &aText)
{
#if 1
  char *str = mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetLabel mLabel=<%s>\n", str));
  delete [] str;
#endif
  
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetLabel(const nsString &aText)
{
  mLabel = aText;
    
#if 1
  char * labelStr = mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SetLabel to this=<%p> <%s>\n", this, labelStr));
  delete [] labelStr;
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetAccessKey - Not Implemented\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SetAccessKey - Not Implemented\n"));
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
NS_METHOD nsMenu::AddItem(nsISupports* aItem)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::AddItem with nsISupports - this=<%p> aIten=<%p>.\n", this, aItem));

 if(aItem)
 {
   nsIMenuItem * menuitem = nsnull;
   aItem->QueryInterface(kIMenuItemIID, (void**) &menuitem);
   if(menuitem)
   {
     AddMenuItem(menuitem); // nsMenu now owns this
     NS_RELEASE(menuitem);
   }
   else
   {
     nsIMenu * menu = nsnull;
	 aItem->QueryInterface(kIMenuIID, (void**) &menu);
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
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::AddItem nsIMenuItem this=<%p> aMenuItem=<%p>\n", this, aMenuItem));

  return mItems->AppendElement((nsISupports *) aMenuItem);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{  
#if 1
  nsString Label;
  
  aMenu->GetLabel(Label);
  char *labelStr = Label.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::AddMenu this=<%p> aMenu=<%p> label=<%s>\n", this, aMenu, labelStr));
  delete[] labelStr;
#endif

  return mItems->AppendElement((nsISupports *) aMenu);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::AddSeparater this=<%p>\n", this));

  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv)
  {
    nsISupports * supports = nsnull;
    QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenuItem->Create(supports, "", PR_TRUE);
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
  mItems->Count(&aCount);
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetItemCount: %d\n", aCount));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetItemAt aPos=<%d>\n", aPos));
  aMenuItem = (nsISupports *)mItems->ElementAt(aPos);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::InsertItemAt - Not Implemented\n"));
 
 mItems->InsertElementAt(aMenuItem, (PRInt32) aPos);
 
 return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aPos)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::InsertSeparator - Not Implemented\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::RemoveItem at %d\n", aPos));
  mItems->RemoveElementAt(aPos);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::RemoveAll - Not Implemented\n"));
    
  while (PR_TRUE)
  {
    PRUint32 cnt;
	mItems->Count(&cnt);
	if (cnt == 0)
	  break;
	mItems->RemoveElementAt(0);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void **aData)
{
  *aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetNativeData(void * aData)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SetNativeData - Not Implemented\n"));
  return NS_OK;
}

PtWidget_t *nsMenu::GetNativeParent()
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetNativeParent\n"));

  void * voidData; 
  if (nsnull != mMenuParent)
  {
     mMenuParent->GetNativeData(&voidData);
  }
  else if (nsnull != mMenuBarParent)
  {
     mMenuBarParent->GetNativeData(voidData);
  }
  else
  {
     voidData =  nsnull;
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::GetNativeParent parent=<%p>\n", voidData));

  return (PtWidget_t *) voidData;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::AddMenuListener\n"));

  NS_IF_RELEASE(mListener);
  mListener = aMenuListener;
  NS_ADDREF(mListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::RemoveMenuListener\n"));

  if (aMenuListener == mListener)
  {
    NS_IF_RELEASE(mListener);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  char *labelStr = mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuItemSelected mLabel=<%s>\n", labelStr));
  delete [] labelStr;
  
  if (nsnull != mListener)
  {
    NS_ASSERTION(false, "nsMenu::MenuItemSelected - get debugger");
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  char *labelStr = mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuSelected  mLabel=<%s> mConstruct=<%d>\n", labelStr, mConstruct));
  delete [] labelStr;

  if(mConstruct)
  {
    MenuDestruct(aMenuEvent);
	mConstruct = false;
  }

  if(!mConstruct)
  {
    MenuConstruct(aMenuEvent, mParentWindow, mDOMNode, mWebShell);
    mConstruct = true;
  }
 
  if (nsnull != mListener)
  {
    NS_ASSERTION(false, "nsMenu::MenuSelected - get debugger");
    mListener->MenuSelected(aMenuEvent);
  }

  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  char *str=mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuDeSelected  for <%s> - Not Implemented\n", str));
  delete [] str;

  if (nsnull != mListener)
  {
    NS_ASSERTION(false, "nsMenu::MenuDeselected - get debugger");
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
  char *str=mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuConstruct for <%s>\n", str));
  delete [] str;

  if(menuNode)
  {
    SetDOMNode((nsIDOMNode*) menuNode);
  }
  
  if(!aWebShell)
  {
    aWebShell = mWebShell;
  }

  // Open the node.
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
        LoadMenuItem(this, menuitemElement, menuitemNode, menuIndex, (nsIWebShell*)aWebShell);
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
  char *str=mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuDestruct called for <%s> mRefCnt=<%d>\n", str, this->mRefCnt));
  delete [] str;

  // Close the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->RemoveAttribute("open");

  // Now remove the kids
  while (PR_TRUE)
  {
    PRUint32 cnt;
	mItems->Count(&cnt);
    if (cnt == 0)
      break;

    mItems->RemoveElementAt(0);
  }

  /* Only destroy submenus */
  if ((mIsSubMenu) && (mMenu))
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuDestruct called for SubMenu\n"));
    PtDestroyWidget(mMenu);
	mMenu = nsnull;  
	NS_RELEASE_THIS();	/* Release the ref. I added in SubMenuMenuItemArm */
  }
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Set DOMNode
*
*/
NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SetDOMNode\n"));
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SetDOMElement\n"));
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SetWebShell\n"));
  mWebShell = aWebShell;
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
  menuitemElement->GetAttribute(nsAutoString("value"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);

#if 1
  char *str = menuitemName.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::LoadMenuItem  -1- Label=<%s> mRefCnt=<%d> this=<%p>\n",str, mRefCnt, this));
  delete [] str; 
#endif
        
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv)
  {
    pnsMenuItem->Create(pParentMenu, menuitemName, PR_FALSE);
                     
    nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);
            
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement)
	{
		return;
    }
    
    nsAutoString cmdAtom("oncommand");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);
    pnsMenuItem->SetCommand(cmdName);
    pnsMenuItem->SetWebShell(mWebShell);
    pnsMenuItem->SetDOMElement(domElement);

#if 1
	if(disabled == NS_STRING_TRUE )
	{
		/* REVISIT: need to disable the item?? */
		pnsMenuItem->SetEnabled(PR_FALSE);
	}
#endif

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
  menuElement->GetAttribute(nsAutoString("value"), menuName);

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::LoadSubMenu <%s>\n", menuName.ToNewCString()));

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
  if (NS_OK == rv)
  {
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
    pnsMenu->SetDOMElement(menuElement);

	// We're done with the menu
    NS_RELEASE(pnsMenu);
  }     
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
int nsMenu::TopLevelMenuItemArmCb (PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo)
{
  nsMenu *aMenu = (nsMenu *) aNSMenu;

  if ((aMenu) && aMenu->mMenu)
  {
    char *labelStr = aMenu->mLabel.ToNewCString();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::TopLevelMenuItemArmCb - mLabel=<%s>\n", labelStr));
    delete [] labelStr;
  }
      
  if ( (aMenu) && (aMenu->mMenu))
  {
    nsIMenuListener *menuListener = nsnull;
    nsIMenu *menu = (nsIMenu *)aNSMenu;

    if (menu != nsnull)
	{
	  nsMenuEvent mevent;
	  mevent.message = NS_MENU_SELECTED;
	  mevent.eventStructType = NS_MENU_EVENT;
	  mevent.point.x = 0;
	  mevent.point.y = 0;
	  mevent.widget = nsnull;
	  mevent.time = PR_IntervalNow();
	  menu->QueryInterface(kIMenuListenerIID, (void**)&menuListener);
	  if (menuListener)
	  {
	    menuListener->MenuConstruct(mevent,nsnull,   // parent window
                    				       nsnull,   // menuNode
										   nsnull);  // webshell
	    NS_IF_RELEASE(menuListener);
	  }
	}
  }
  
  if ((aMenu) && aMenu->mMenu)
  {
    PtPositionMenu ( aMenu->mMenu, NULL);
	PtRealizeWidget( aMenu->mMenu);  
  }
  return( Pt_CONTINUE );
}

//-------------------------------------------------------------------------
int nsMenu::SubMenuMenuItemArmCb (PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo)
{
  PtArg_t       arg[5];
  nsMenu *aMenu = (nsMenu *) aNSMenu;

  char *labelStr = aMenu->mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SubMenuMenuItemArmCb - mLabel=<%s> mRefCnt=<%d> aMenu->mMenu=<%p> Reason=<%d>\n",
  	labelStr, aMenu->mRefCnt, aMenu->mMenu, cbinfo->reason));
  delete [] labelStr;

  if (aMenu->mMenu == nsnull)
  {

	NS_ADDREF(aMenu); 	/* Make sure the aMenu hangs around till its destructed */
	
    /* Now Create the Photon Menu that is attached to the mMenuButton */
    PtSetArg(&arg[0], Pt_ARG_MENU_FLAGS, Pt_MENU_CHILD | Pt_MENU_AUTO, 0xFFFFFFFF);
    PtSetArg(&arg[1], Pt_ARG_USER_DATA, &aNSMenu, sizeof(void *) );
    aMenu->mMenu = PtCreateWidget (PtMenu, aMenu->mMenuButton, 2, arg);

    /* Set the Call back on each menu */
//  PtAddCallback(aMenu->mMenu, Pt_CB_REALIZED, MenuRealizedCb, aMenu);
    PtAddCallback(aMenu->mMenu, Pt_CB_UNREALIZED, MenuUnRealizedCb, aMenu);

  	   
  if ((aMenu) && (aMenu->mMenu))
  {
    nsIMenuListener *menuListener = nsnull;
    nsIMenu *menu = (nsIMenu *)aNSMenu;

    if (menu != nsnull)
	{
	  nsMenuEvent mevent;
	  mevent.message = NS_MENU_SELECTED;
	  mevent.eventStructType = NS_MENU_EVENT;
	  mevent.point.x = 0;
	  mevent.point.y = 0;
	  mevent.widget = nsnull;
	  mevent.time = PR_IntervalNow();
	  menu->QueryInterface(kIMenuListenerIID, (void**)&menuListener);
	  if (menuListener)
	  {
	    menuListener->MenuConstruct(mevent,nsnull,   // parent window
                    				       nsnull,   // menuNode
										   nsnull);  // webshell
	    NS_IF_RELEASE(menuListener);
	  }
      else
      {
	    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SubMenuMenuItemArmCb - Error no menuListener defined!\n"));
      }
	}
  }

  }
  
  if ((aMenu) && (aMenu->mMenu))
  {
	int err;
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SubMenuMenuItemArmCb - Showing Menu\n"));

    PtPositionMenu ( aMenu->mMenu, NULL);
	err=PtRealizeWidget( aMenu->mMenu);  
    if (err != 0)
      PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SubMenuMenuItemArmCb - Error Showing Menu after PtRealizeWidget\n"));
  }
 
  return( Pt_CONTINUE );
}

//-------------------------------------------------------------------------
int nsMenu::MenuRealizedCb (PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo)
{
  nsMenu *aMenu = (nsMenu *) aNSMenu;


  if ((aMenu) && aMenu->mMenu)
  {
    char *labelStr = aMenu->mLabel.ToNewCString();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuRealizedCb - mLabel=<%s>\n", labelStr));
    delete [] labelStr;
  }

  return( Pt_CONTINUE );
}

//-------------------------------------------------------------------------
int nsMenu::MenuUnRealizedCb (PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo)
{
  nsMenu *aMenu = (nsMenu *) aNSMenu;

  if ((aMenu) && aMenu->mMenu)
  {
    char *labelStr = aMenu->mLabel.ToNewCString();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::MenuUnRealizedCb - mLabel=<%s>\n", labelStr));
    delete [] labelStr;
  }
  
  /* This starts the destruction sequence the menus and submenus */
  if ((aMenu) && aMenu->mMenu)
  {
    nsIMenuListener *menuListener = nsnull;
    nsIMenu *menu = (nsIMenu *) aNSMenu;
	if (menu != nsnull)
	{
	  nsMenuEvent mevent;
	  mevent.message = NS_MENU_SELECTED;
	  mevent.eventStructType = NS_MENU_EVENT;
	  mevent.point.x = 0;
	  mevent.point.y = 0;
	  mevent.widget = nsnull;
	  mevent.time = PR_IntervalNow();
	  menu->QueryInterface(kIMenuListenerIID, (void**)&menuListener);
	  if (menuListener)
	  {
	     menuListener->MenuDestruct(mevent);
	     NS_IF_RELEASE(menuListener);
	  }
	}  
  }

  return( Pt_CONTINUE );
}

//-------------------------------------------------------------------------
int nsMenu::SubMenuMenuItemMenuCb (PtWidget_t *widget, void *aNSMenu, PtCallbackInfo_t *cbinfo)
{
  nsMenu *aMenu = (nsMenu *) aNSMenu;
  if ((aMenu) && aMenu->mMenu)
  {
    char *labelStr = aMenu->mLabel.ToNewCString();
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenu::SubMenuMenuItemMenuCb - mLabel=<%s>\n", labelStr));
    delete [] labelStr;
  }

  return( Pt_CONTINUE );
}
