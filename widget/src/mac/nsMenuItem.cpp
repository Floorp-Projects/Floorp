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
#include "nsIWidget.h"
#include "nsIMenuListener.h"

#include "nsStringUtil.h"

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
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenuItem*)this;                     
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

NS_IMPL_ADDREF(nsMenuItem)
NS_IMPL_RELEASE(nsMenuItem)

//-------------------------------------------------------------------------
//
// nsMenuItem constructor
//
//-------------------------------------------------------------------------
nsMenuItem::nsMenuItem() : nsIMenuItem()
{
  NS_INIT_REFCNT();
  //mMenu        = nsnull;
  mMenuParent  = nsnull;
  mPopUpParent = nsnull;
  mTarget      = nsnull;
  mXULCommandListener = nsnull;
  mIsSeparator = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
  NS_IF_RELEASE(mTarget);
  NS_IF_RELEASE(mXULCommandListener);
}

#ifdef NOTNOW
//-------------------------------------------------------------------------
void nsMenuItem::Create(nsIWidget      *aMBParent, 
                        Widget          aParent, 
                        const nsString &aLabel, 
                        PRUint32        aCommand)
{
  /*
  mTarget  = aMBParent;
  mCommand = aCommand;
  mLabel   = aLabel;

  if (NULL == aParent || nsnull == aMBParent) {
    return;
  }

  mTarget = aMBParent;
  char * nameStr = mLabel.ToNewCString();

  Widget parentMenuHandle = GetNativeParent();

  mMenu = XtVaCreateManagedWidget(nameStr, xmCascadeButtonGadgetClass,
                                          parentMenuHandle,
                                          NULL);

  XtAddCallback(mMenu, XmNactivateCallback, nsXtWidget_Menu_Callback, 
                (nsIMenuItem *)this);

  delete[] nameStr;
  */

}
//-------------------------------------------------------------------------
Widget nsMenuItem::GetNativeParent()
{

  void * voidData;
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(voidData);
  } else if (nsnull != mPopUpParent) {
    mPopUpParent->GetNativeData(voidData);
  } else {
    return NULL;
  }
  return (Widget)voidData;

}

#endif

//-------------------------------------------------------------------------
nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
  nsIWidget    * widget  = nsnull; // MenuBar's Parent
  nsIMenu      * menu    = nsnull;
  nsIMenuBar   * menuBar = nsnull;
  nsIPopUpMenu * popup   = nsnull;
  nsISupports  * parent  = aParent;

/*
  // Bump the ref count on the parent, since it gets released unconditionally..
  NS_ADDREF(parent);
  while (1) {
    if (NS_OK == parent->QueryInterface(kIMenuIID,(void**)&menu)) {
      NS_RELEASE(parent);
      if (NS_OK != menu->GetParent(parent)) {
        NS_RELEASE(menu);
        return nsnull;
      }
      NS_RELEASE(menu);

    } else if (NS_OK == parent->QueryInterface(kIPopUpMenuIID,(void**)&popup)) {
      if (NS_OK != popup->GetParent(widget)) {
        widget =  nsnull;
      } 
      NS_RELEASE(parent);
      NS_RELEASE(popup);
      return widget;

    } else if (NS_OK == parent->QueryInterface(kIMenuBarIID,(void**)&menuBar)) {
      if (NS_OK != menuBar->GetParent(widget)) {
        widget =  nsnull;
      } 
      NS_RELEASE(parent);
      NS_RELEASE(menuBar);
      return widget;
    } else {
      NS_RELEASE(parent);
      return nsnull;
    }
  }
  */
  return nsnull;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu        *aParent, 
                             const nsString &aLabel, 
                             PRBool         aIsSeparator)
                            
{
  mIsSeparator = aIsSeparator;
  SetLabel(NS_CONST_CAST(nsString&, aLabel));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu   *aParent, 
                             const nsString &aLabel,  
                             PRUint32        aCommand)
{
  mPopUpParent = aParent;
  if ( mPopUpParent ) {

    nsIWidget * widget = nsnull;

    mPopUpParent->AddItem(this);
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu * aParent)
{
  mIsSeparator = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent)
{
  mIsSeparator = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetLabel(nsString &aText)
{
  mLabel = aText;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetCommand(PRUint32 & aCommand)
{
  aCommand = mCommand;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetTarget(nsIWidget *& aTarget)
{
  aTarget = mTarget;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  //aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
    NS_IF_RELEASE(mXULCommandListener);
    NS_IF_ADDREF(aMenuListener);
	mXULCommandListener = aMenuListener;
	return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
	if(mXULCommandListener)
		return mXULCommandListener->MenuSelected(aMenuEvent);
		
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell)
{
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Sets the JavaScript Command to be invoked when a "gui" event occurs on a source widget
* @param aStrCmd the JS command to be cached for later execution
* @return NS_OK 
*/
NS_METHOD nsMenuItem::SetCommand(const nsString & aStrCmd)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Executes the "cached" JavaScript Command 
* @return NS_OK if the command was executed properly, otherwise an error code
*/
NS_METHOD nsMenuItem::DoCommand()
{
	return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetDOMElement(nsIDOMElement * aDOMElement)
{
	return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetDOMElement(nsIDOMElement ** aDOMElement)
{
	return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetWebShell(nsIWebShell * aWebShell)
{
	return NS_OK;
}
    