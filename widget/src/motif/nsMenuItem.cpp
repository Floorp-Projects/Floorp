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

#include <Xm/CascadeBG.h>

#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"

#include "nsXtEventHandler.h"

#include "nsIPopUpMenu.h"

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"   
#include "nsIWebShell.h"
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"

#include "nsStringUtil.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;

  if (aIID.Equals(nsIMenuItem::GetIID())) {
    *aInstancePtr = (void*)(nsIMenuItem*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIMenuItem*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIMenuListener::GetIID())) {
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
  mMenuItem    = nsnull;
  mMenuParent  = nsnull;
  mPopUpParent = nsnull;
  mTarget      = nsnull;
  mXULCommandListener = nsnull;
  mIsSeparator = PR_FALSE;
  mWebShell    = nsnull;
  mDOMElement  = nsnull;
  mIsSubMenu   = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
  NS_IF_RELEASE(mMenuParent);
  NS_IF_RELEASE(mPopUpParent);
  NS_IF_RELEASE(mTarget);
}

//-------------------------------------------------------------------------
Widget nsMenuItem::GetNativeParent()
{
  void * voidData;
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(&voidData);
  } else if (nsnull != mPopUpParent) {
    mPopUpParent->GetNativeData(voidData);
  } else {
    return NULL;
  }
  return (Widget)voidData;
}


//-------------------------------------------------------------------------
nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
  nsIWidget    * widget  = nsnull; // MenuBar's Parent
  nsIMenu      * menu    = nsnull;
  nsIMenuBar   * menuBar = nsnull;
  nsIPopUpMenu * popup   = nsnull;
  nsISupports  * parent  = aParent;

  // Bump the ref count on the parent, since it gets released unconditionally..
  NS_ADDREF(parent);
  while (1) {
    if (NS_OK == parent->QueryInterface(nsIMenu::GetIID(),(void**)&menu)) {
      NS_RELEASE(parent);
      if (NS_OK != menu->GetParent(parent)) {
        NS_RELEASE(menu);
        return nsnull;
      }
      NS_RELEASE(menu);

    } else if (NS_OK == parent->QueryInterface(nsIPopUpMenu::GetIID(),(void**)&popup)) {
      if (NS_OK != popup->GetParent(widget)) {
        widget =  nsnull;
      } 
      NS_RELEASE(parent);
      NS_RELEASE(popup);
      return widget;

    } else if (NS_OK == parent->QueryInterface(nsIMenuBar::GetIID(),(void**)&menuBar)) {
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
  return nsnull;
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

NS_METHOD nsMenuItem::SetShortcutChar(const nsString &aText)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetShortcutChar(nsString &aText)
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
//  aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  if(!mIsSeparator) {
    DoCommand();
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  printf("nsMenuItem::MenuSelected called\n");
  if(mXULCommandListener)
    return mXULCommandListener->MenuSelected(aMenuEvent);

  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  printf("nsMenuItem::MenuDeselected called\n");
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuConstruct(const nsMenuEvent &aMenuEvent,
                                        nsIWidget *aParentWindow,
                                        void *menuNode,
                                        void *aWebShell)
{
  printf("nsMenuItem::MenuConstruct called\n");
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  printf("nsMenuItem::MenuDestruct called\n");
  return nsEventStatus_eIgnore;
}

//----------------------------------------------------
NS_METHOD nsMenuItem::Create(nsISupports *aParent,
                             const nsString &aLabel,
                             PRBool aIsSeparator)
{
  printf("nsMenuItem::Create called\n");
  if (nsnull == aParent) {
    return NS_ERROR_FAILURE;
  }

  if(aParent) {
    nsIMenu * menu;
    aParent->QueryInterface(nsIMenu::GetIID(), (void**) &menu);
    mMenuParent = menu;
    NS_RELEASE(menu);
  }

  nsIWidget   * widget  = nsnull; // MenuBar's Parent
  nsISupports * sups;
  if (NS_OK == aParent->QueryInterface(kISupportsIID,(void**)&sups)) {
    widget = GetMenuBarParent(sups);
    // GetMenuBarParent will call release for us
    // NS_RELEASE(sups);
    mTarget = widget;
  }

  mIsSeparator = aIsSeparator;
  mLabel = aLabel;

  // create the native menu item

  if(mIsSeparator) {
    mMenuItem = nsnull;
  } else {
    char * nameStr = mLabel.ToNewCString();
    Widget parentMenuHandle = GetNativeParent();
    mMenuItem = XtVaCreateManagedWidget(nameStr, xmCascadeButtonGadgetClass,
                                                 parentMenuHandle,
                                                 NULL);
    XtAddCallback(mMenuItem, XmNactivateCallback, nsXtWidget_Menu_Callback,
                  (nsIMenuItem *)this);
    delete[] nameStr;
  }
  return NS_OK;
}

NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
  //XXX:Implement this.
  return nsEventStatus_eIgnore;
}

NS_METHOD nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
  //XXX:Implement this.
  return nsEventStatus_eIgnore;
}

NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
  NS_IF_RELEASE(mXULCommandListener);
  NS_IF_ADDREF(aMenuListener);
  mXULCommandListener = aMenuListener;
  return NS_OK;
}

NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}

NS_METHOD nsMenuItem::SetCommand(const nsString &aStrCmd)
{
  return nsEventStatus_eIgnore;
}

NS_METHOD nsMenuItem::DoCommand()
{
  //XXX:Implement this.
  return nsEventStatus_eIgnore;
}

NS_METHOD nsMenuItem::SetDOMNode(nsIDOMNode * aDOMNode)
{
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetDOMNode(nsIDOMNode ** aDOMNode)
{
  return NS_OK;
}


NS_METHOD nsMenuItem::SetDOMElement(nsIDOMElement * aDOMElement)
{
  mDOMElement = aDOMElement;
  return NS_OK;
}

NS_METHOD nsMenuItem::GetDOMElement(nsIDOMElement ** aDOMElement)
{
  return NS_OK;
}

NS_METHOD nsMenuItem::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}

NS_METHOD nsMenuItem::SetModifiers(PRUint8 aModifiers)
{
  mModifiers = aModifiers;
  return NS_OK;
}

NS_METHOD nsMenuItem::GetModifiers(PRUint8 * aModifiers)
{
  *aModifiers = mModifiers;
  return NS_OK;
}
