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

#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIPopUpMenu.h"

static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuItemIID)) {                                         
    *aInstancePtr = (void*)(nsIMenuItem*)this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIMenuListenerIID)) {                                      
    *aInstancePtr = (void*)(nsIMenuListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                     
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenuItem*)this;                     
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(nsMenuItem)
NS_IMPL_RELEASE(nsMenuItem)

nsMenuItem::nsMenuItem() : nsIMenuItem()
{
  NS_INIT_REFCNT();
}

nsMenuItem::~nsMenuItem()
{
}

nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
  return nsnull;
}

NS_METHOD nsMenuItem::Create(nsIMenu * aParent, const nsString &aLabel, PRUint32 aCommand)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent, const nsString &aLabel, PRUint32 aCommand)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::Create(nsIMenu * aParent)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetLabel(nsString &aText)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::SetLabel(nsString &aText)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetCommand(PRUint32 & aCommand)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetTarget(nsIWidget *& aTarget)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

void nsMenuItem::SetCmdId(PRInt32 aId)
{
}

PRInt32 nsMenuItem::GetCmdId()
{
  return 0;
}

NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
  return NS_OK;
}

nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

