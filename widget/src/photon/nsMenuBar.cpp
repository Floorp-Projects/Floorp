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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::~nsMenuBar Destructor Called - Not Implmenented\n"));

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
    PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenuBar::Create Failed to create the PtMenuBar\n"));
    return NS_ERROR_FAILURE;	  
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenuBar::Create with nsIWidget parent=%p, this=%p Photon menuBar=<%p>\n", aParent, this, mMenuBar));

    SetParent(aParent);
//    PtRealizeWidget(mMenuBar);
    return NS_OK;
  }
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::GetParent mParent=<%p>\n", mParent));

  aParent = mParent;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetParent(nsIWidget *aParent)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::SetParent aParent=<%p>\n", aParent));

  mParent = aParent;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{
  NS_ASSERTION(aMenu, "NULL Pointer in nsMenuBar::AddMenu");
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::AddMenu aMenu=<%p>\n", aMenu));

  /* Add the nsMenu to our list */
  mItems->AppendElement(aMenu);
  NS_ADDREF(aMenu);

#ifdef DEBUG		  
  nsString Label;
  aMenu->GetLabel(Label);
  char *labelStr = Label.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::AddMenu Label is <%s>\n", labelStr));
  delete[] labelStr;
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  aCount = mItems->Count();;
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::GetMenuCount aCount=<%d>\n", aCount));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::GetMenuAt %d\n", aPos));
  aMenu = (nsIMenu *) mItems->ElementAt(aPos);
  NS_ADDREF(aMenu);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::InsertMenuAt aPos=<%d> - Not Implemented\n", aPos));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::RemoveMenu aPos=<%d>\n", aPos));

  nsIMenu * menu = (nsIMenu *) mItems->ElementAt(aPos);
  NS_RELEASE(menu);
  mItems->RemoveElementAt(aPos);
  
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::RemoveAll\n"));

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::GetNativeData this=<%p> mMenuBar=<%p>\n", this, mMenuBar));
  aData = (void *)mMenuBar;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetNativeData(void * aData)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::SetNativeData to <%p> - Not Implemented\n", aData));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::Paint\n"));
  mParent->Invalidate(PR_TRUE);
  return NS_OK;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::MenuItemSelected - Not Implemented\n"));
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // I should determine which menu was selected and call MenuConstruct
  // on it.. 
  // Not really sure what to do here, will have to wait until someone
  // calls it!
    
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::MenuSelected  aMenuEvent->nativeMsg=<%p> - Not Implemented\n", aMenuEvent.nativeMsg));

  /* I have no idea what this really is... */
//  PtWidget_t *PhMenu = (PtWidget_t *) aMenuEvent.nativeMsg;

  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::MenuDeSelected - Not Implemented\n"));
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menubarNode,
    void              * aWebShell)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::MenuConstruct\n"));

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

nsEventStatus nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuBar::MenuDeconstruct - Not Implemented\n"));
  return nsEventStatus_eIgnore;
}
