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

#include "nsMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIWidget.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include "nsIComponentManager.h"
#include "nsCOMPtr.h"

// CIDS
#include "nsWidgetsCID.h"
static NS_DEFINE_IID(kMenuBarCID,          NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID,             NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID,         NS_MENUITEM_CID);

// IIDS
static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult nsMenuBar::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr)
  {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuBarIID))
  {                                         
    *aInstancePtr = (void*) ((nsIMenuBar*) this);                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtr = (void*) ((nsISupports*)(nsIMenuBar*) this);                     
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }

  if (aIID.Equals(kIMenuListenerIID))
  {
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
// nsMenuBar constructor
//
//-------------------------------------------------------------------------
nsMenuBar::nsMenuBar() : nsIMenuBar(), nsIMenuListener()
{
  NS_INIT_REFCNT();
  mMenuBar  = nsnull;
  mParent   = nsnull;
  mIsMenuBarAdded = PR_FALSE;
  mConstructed   = PR_FALSE;
  mWebShell = nsnull;
  mDOMNode = nsnull;
  mDOMElement = nsnull;
  
  mItems = new nsVoidArray();
}

//-------------------------------------------------------------------------
//
// nsMenuBar destructor
//
//-------------------------------------------------------------------------
nsMenuBar::~nsMenuBar()
{
  // Remove all references to menus on this menubar
  mItems->Clear();

 if (mMenuBar)
   PtDestroyWidget(mMenuBar);
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
  void        *voidData;
  PtWidget_t  *parent;
  void        *me        = (void *) this;
  PtArg_t     arg[1];

  voidData = aParent->GetNativeData(NS_NATIVE_WINDOW);
  parent = (PtWidget_t *) voidData;
  
  PtSetArg(&arg[0], Pt_ARG_USER_DATA, &me, sizeof(void *) );
  mMenuBar = PtCreateWidget( PtMenuBar, parent, 1, arg);
  if (!mMenuBar)
  {
    return NS_ERROR_FAILURE;	  
  }
  else
  {

    SetParent(aParent);
//    PtRealizeWidget(mMenuBar);
    return NS_OK;
  }
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
  aParent = mParent;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetParent(nsIWidget *aParent)
{
  mParent = aParent;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{
  NS_ASSERTION(aMenu, "NULL Pointer in nsMenuBar::AddMenu");
  
  /* Add the nsMenu to our list */
  mItems->AppendElement(aMenu);
  NS_ADDREF(aMenu);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  aCount = mItems->Count();;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  aMenu = (nsIMenu *) mItems->ElementAt(aPos);
  NS_ADDREF(aMenu);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  mItems->InsertElementAt(aMenu, aPos);
  NS_ADDREF(aMenu);

  if (!mIsMenuBarAdded)
  {
    if (mParent)
	{
	  mParent->SetMenuBar(this);
    }
    mIsMenuBarAdded = PR_TRUE;
  }
  	    
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aPos)
{
  nsIMenu * menu = (nsIMenu *) mItems->ElementAt(aPos);
  NS_RELEASE(menu);
  mItems->RemoveElementAt(aPos);
  
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
 while (mItems->Count()) {
    nsISupports * supports = (nsISupports *)mItems->ElementAt(0);
    NS_RELEASE(supports);
    mItems->RemoveElementAt(0);
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
  aData = (void *)mMenuBar;
  return NS_OK;
}

//-------------------------------------------------------------------------
// Not Implemented
NS_METHOD nsMenuBar::SetNativeData(void * aData)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
  mParent->Invalidate(PR_TRUE);
  return NS_OK;
}

//-------------------------------------------------------------------------
// Not Implemented
nsEventStatus nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
// Not Implemented
nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // I should determine which menu was selected and call MenuConstruct
  // on it.. 
  // Not really sure what to do here, will have to wait until someone
  // calls it!
    
  /* I have no idea what this really is... */
//  PtWidget_t *PhMenu = (PtWidget_t *) aMenuEvent.nativeMsg;

  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
// Not Implemented
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
              menuElement->GetAttribute(nsAutoString("value"), menuName);
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

                  pnsMenu->SetLabel(menuName); 
                  pnsMenu->SetDOMNode(menuNode);
                  pnsMenu->SetDOMElement(menuElement);
                  pnsMenu->SetWebShell(mWebShell);

                  // Make nsMenu a child of nsMenuBar
  		          // nsMenuBar takes ownership of the nsMenu
                  pnsMenuBar->AddMenu(pnsMenu); 
		  
                  // Release the menu now that the menubar owns it
                  NS_RELEASE(pnsMenu);
                }
             } 

          }
          nsCOMPtr<nsIDOMNode> oldmenuNode(menuNode);  
          oldmenuNode->GetNextSibling(getter_AddRefs(menuNode));
        } // end while (nsnull != menuNode)
          
        // Give the aParentWindow this nsMenuBar to hold onto.
	    // The parent window should take ownership at this point
        aParentWindow->SetMenuBar(pnsMenuBar);
      
        // HACK: force a paint for now
        pnsMenuBar->Paint();
        
		NS_RELEASE(pnsMenuBar);
    } // end if ( nsnull != pnsMenuBar )
  }
  
  return nsEventStatus_eIgnore;
}

// Not Implemented
nsEventStatus nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}
