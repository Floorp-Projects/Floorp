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
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsIMenuBar *aParent, const nsString &aLabel)
{
  mMenuBarParent = aParent;
  NS_ADDREF(mMenuBarParent);
    
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsIMenu *aParent, const nsString &aLabel)
{
  mMenuParent = aParent;
  NS_ADDREF(mMenuParent);

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
NS_METHOD nsMenu::SetLabel(nsString &aText)
{
   mLabel = aText;
   
  mMacMenuHandle = nsnull;

  mMacMenuHandle = ::NewMenu(mMacMenuIDCount, c2pstr(mLabel.ToNewCString()));
  mMacMenuID = mMacMenuIDCount;
  mMacMenuIDCount++;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(const nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  NS_IF_ADDREF(aMenuItem);
  mMenuItemVoidArray.AppendElement(aMenuItem);
  
  nsString label;
  aMenuItem->GetLabel(label);
  mNumMenuItems++;
  ::InsertMenuItem(mMacMenuHandle, c2pstr(label.ToNewCString()), mNumMenuItems );
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
  mNumMenuItems++;
  ::InsertMenuItem(mMacMenuHandle, c2pstr(label.ToNewCString()), mNumMenuItems);
  
  void * menuHandle;
  aMenu->GetNativeData(menuHandle);
  ::InsertMenu((MenuHandle)menuHandle, hierMenu);
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
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aPos)
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
NS_METHOD nsMenu::GetNativeData(void *& aData)
{
  aData = (void*) mMacMenuHandle;
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
  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

