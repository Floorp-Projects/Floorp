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

#include "nsMenuBar.h"
#include "nsIMenu.h"
#include "nsIWidget.h"
#include "nsISupports.h"

#include "nsString.h"
#include "nsStringUtil.h"

#if defined(XP_MAC)
#include <Menus.h>
#include <TextUtils.h>
#endif

static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult nsMenuBar::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuBarIID)) {                                         
    *aInstancePtr = (void*) ((nsIMenuBar*) this);                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) ((nsISupports*)(nsIMenuBar*) this);                     
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

NS_IMPL_ADDREF(nsMenuBar)
NS_IMPL_RELEASE(nsMenuBar)

//-------------------------------------------------------------------------
//
// nsMenuListener interface
//
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // Dispatch menu event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  for (int i = mMenuVoidArray.Count(); i > 0; --i)
  {
    nsIMenuListener * menuListener = nsnull;
    ((nsISupports*)mMenuVoidArray[i-1])->QueryInterface(kIMenuListenerIID, &menuListener);
    if(menuListener){
      eventStatus = menuListener->MenuSelected(aMenuEvent);
      NS_RELEASE(menuListener);
      if(nsEventStatus_eIgnore != eventStatus)
        return eventStatus;
    }
  }
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
//
// nsMenuBar constructor
//
//-------------------------------------------------------------------------
nsMenuBar::nsMenuBar() : nsIMenuBar(), nsIMenuListener()
{
  NS_INIT_REFCNT();
  mNumMenus = 0;
  mParent   = nsnull;
  mIsMenuBarAdded = PR_FALSE;
  
  mOriginalMacMBarHandle = nsnull;
  mMacMBarHandle = nsnull;
  mMacMBarHandle = ::GetMenuBar(); // Get a copy of the menu list
  ::ClearMenuBar(); // Clear the copy
}

//-------------------------------------------------------------------------
//
// nsMenuBar destructor
//
//-------------------------------------------------------------------------
nsMenuBar::~nsMenuBar()
{
  //NS_IF_RELEASE(mParent);

  while(mNumMenus)
  {
    --mNumMenus;
    nsISupports* menu = (nsISupports*)mMenuVoidArray[mNumMenus];
    NS_IF_RELEASE( menu );
  }
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
  SetParent(aParent);

  return NS_OK;
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
  // XXX add to internal data structure
  nsISupports * supports = nsnull;
  aMenu->QueryInterface(kISupportsIID, (void**)&supports);
  if(supports){
    mMenuVoidArray.AppendElement( supports );
  }

  MenuHandle menuHandle = nsnull;
  aMenu->GetNativeData(&menuHandle);
  
  mNumMenus++;
  ::InsertMenu(menuHandle, 0);
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
  aData = (void *) mMacMBarHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetNativeData(void* aData)
{
  mMacMBarHandle = (Handle) aData;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
  ::DrawMenuBar();
  return NS_OK;
}
