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
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"

#include "nsString.h"
#include "nsStringUtil.h"
#include "nsIMenuListener.h"

#if defined(XP_MAC)
#include <TextUtils.h>
#include <ToolUtils.h>
#endif

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

const PRInt16 kMacMenuID = 1;
PRInt16 nsMenu::mMacMenuIDCount = kMacMenuID;

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
    *aInstancePtr = (void*)this;                        
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
  mMenuParent    = nsnull;
  mMenuBarParent = nsnull;
  
  mMacMenuID = 0;
  mMacMenuHandle = nsnull;
  mListener      = nsnull;
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

  while(mNumMenuItems)
  {
    --mNumMenuItems;
    
    // Figure out what we're releasing
    nsIMenuItem * menuitem = nsnull;
    ((nsISupports*)mMenuItemVoidArray[mNumMenuItems])->QueryInterface(kIMenuItemIID, (void**) &menuitem);  
    if(menuitem)
    {
      // case menuitem
      NS_RELEASE(menuitem); // Release our hold
      NS_RELEASE(menuitem); // Balance QI
    }
    else
    {
	  nsIMenu * menu = nsnull;
	  ((nsISupports*)mMenuItemVoidArray[mNumMenuItems])->QueryInterface(kIMenuIID, (void**) &menu);
	  if(menu)
	  {
	    // case menu
	    NS_RELEASE(menu); // Release our hold 
	    NS_RELEASE(menu); // Balance QI
	  }
	}
  }
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
    aParent->QueryInterface(kIMenuBarIID, (void**) &menubar);
    if(menubar)
    {
      mMenuBarParent = menubar;
      NS_ADDREF(mMenuBarParent);
      NS_RELEASE(menubar); // Balance the QI
    }
    else
    {
      nsIMenu * menu = nsnull;
      aParent->QueryInterface(kIMenuIID, (void**) &menu);
      {
      	mMenuParent = menu;
      	NS_ADDREF(mMenuParent);
      	NS_RELEASE(menu); // Balance the QI
      }
    }
  }
  
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

  char* menuLabel = mLabel.ToNewCString();
  mMacMenuHandle = ::NewMenu(mMacMenuIDCount, c2pstr(menuLabel));
  delete[] menuLabel;

  mMacMenuID = mMacMenuIDCount;
  mMacMenuIDCount++;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports* aItem)
{
  if(aItem)
  {
    // Figure out what we're adding
    nsIMenuItem * menuitem = nsnull;
    aItem->QueryInterface(kIMenuItemIID, (void**) &menuitem);  
    if(menuitem)
    {
      // case menuitem
      AddMenuItem(menuitem);
      NS_RELEASE(menuitem);
    }
    else
    {
	  nsIMenu * menu = nsnull;
	  aItem->QueryInterface(kIMenuIID, (void**) &menu);
	  if(menu)
	  {
	    // case menu
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
  NS_IF_ADDREF(aMenuItem);
  mMenuItemVoidArray.AppendElement(aMenuItem);
  
  nsString label;
  aMenuItem->GetLabel(label);
  char* menuLabel = label.ToNewCString();
  mNumMenuItems++;
  ::InsertMenuItem(mMacMenuHandle, c2pstr(menuLabel), mNumMenuItems);
  delete[] menuLabel;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  // Add a submenu
  NS_IF_ADDREF(aMenu);
  mMenuItemVoidArray.AppendElement(aMenu);
  
  // We have to add it as a menu item and then associate it with the item
  nsString label;
  aMenu->GetLabel(label);
  char* menuLabel = label.ToNewCString();
  mNumMenuItems++;
  ::InsertMenuItem(mMacMenuHandle, c2pstr(menuLabel), mNumMenuItems);
  delete[] menuLabel;
  
  MenuHandle menuHandle;
  aMenu->GetNativeData((void**)&menuHandle);
  ::InsertMenu(menuHandle, hierMenu);
  PRInt16 temp = mMacMenuIDCount;
  ::SetMenuItemHierarchicalID((MenuHandle) mMacMenuHandle, mNumMenuItems, --temp);
 
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  // HACK - We're not really appending an nsMenuItem but it 
  // needs to be here to make sure that event dispatching isn't off by one.
  mMenuItemVoidArray.AppendElement(nsnull);
  ::InsertMenuItem(mMacMenuHandle, "\p(-", mNumMenuItems );
  mNumMenuItems++;
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
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  *aData = mMacMenuHandle;
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
//
// nsIMenuListener interface
//
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
      
  // Determine if this is the correct menu to handle the event
  PRInt16 menuID = HiWord(((nsMenuEvent)aMenuEvent).mCommand);
  if(mMacMenuID == menuID)
  {
    // Call MenuSelected on the correct nsMenuItem
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    nsIMenuListener * menuListener = nsnull;
    ((nsIMenuItem*)mMenuItemVoidArray[menuItemID-1])->QueryInterface(kIMenuListenerIID, &menuListener);
	if(menuListener) {
	  eventStatus = menuListener->MenuSelected(aMenuEvent);
	  NS_IF_RELEASE(menuListener);
	}
  } 
  else
  {
    // Make sure none of our submenus are the ones that should be handling this
      for (int i = mMenuItemVoidArray.Count(); i > 0; i--)
	  {
	    if(nsnull != mMenuItemVoidArray[i-1])
	    {
		    nsIMenu * submenu = nsnull;
		    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuIID, &submenu);
		    if(submenu)
		    {
			    nsIMenuListener * menuListener = nsnull;
			    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuListenerIID, &menuListener);
			    if(menuListener){
			      eventStatus = menuListener->MenuSelected(aMenuEvent);
			      NS_IF_RELEASE(menuListener);
			      if(nsEventStatus_eIgnore != eventStatus)
			        return eventStatus;
			    }
		    }
		}
	  }
  
  }
  return eventStatus;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
      
  // Determine if this is the correct menu to handle the event
  PRInt16 menuID = HiWord(((nsMenuEvent)aMenuEvent).mCommand);
  if(mMacMenuID == menuID)
  {
    // Call MenuSelected on the correct nsMenuItem
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    nsIMenuListener * menuListener = nsnull;
    ((nsIMenuItem*)mMenuItemVoidArray[menuItemID-1])->QueryInterface(kIMenuListenerIID, &menuListener);
	if(menuListener) {
	  eventStatus = menuListener->MenuSelected(aMenuEvent);
	  NS_IF_RELEASE(menuListener);
	}
  } 
  else
  {
    // Make sure none of our submenus are the ones that should be handling this
      for (int i = mMenuItemVoidArray.Count(); i > 0; i--)
	  {
	    if(nsnull != mMenuItemVoidArray[i-1])
	    {
		    nsIMenu * submenu = nsnull;
		    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuIID, &submenu);
		    if(submenu)
		    {
			    nsIMenuListener * menuListener = nsnull;
			    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuListenerIID, &menuListener);
			    if(menuListener){
			      eventStatus = menuListener->MenuSelected(aMenuEvent);
			      NS_IF_RELEASE(menuListener);
			      if(nsEventStatus_eIgnore != eventStatus)
			        return eventStatus;
			    }
		    }
		}
	  }
  
  }
  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Set DOMNode
*
*/
NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Set DOMElement
*
*/
NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{
	return NS_OK;
}
    
//-------------------------------------------------------------------------
/**
* Set WebShell
*
*/
NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
	return NS_OK;
}
