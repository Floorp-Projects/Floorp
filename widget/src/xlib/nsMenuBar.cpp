/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMenuBar.h"

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

nsEventStatus nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
	return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                       nsIWidget         * aParentWindow, 
                                       void              * menubarNode,
                                       void              * aWebShell)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsMenuBar::nsMenuBar() : nsIMenuBar(), nsIMenuListener()
{
  NS_INIT_REFCNT();
}

nsMenuBar::~nsMenuBar()
{
}

NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::SetParent(nsIWidget *aParent)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aPos)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::RemoveAll()
{
  return NS_OK;
}

NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::SetNativeData(void * aData)
{
  return NS_OK;
}

NS_METHOD nsMenuBar::Paint()
{
  return NS_OK;
}

NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aPos, nsIMenu *& aMenu)
{
  return NS_OK;
}
