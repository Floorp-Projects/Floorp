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
#include "nsIMenuListener.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
//NS_IMPL_ISUPPORTS(nsMenu, kMenuIID)

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
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;                        
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
  mMenu          = nsnull;
  mMenuParent    = nsnull;
  mMenuBarParent = nsnull;
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
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
void nsMenu::Create(Widget aParent, const nsString &aLabel)
{
  if (NULL == aParent) {
    return;
  }
  mLabel = aLabel;

  char * labelStr = mLabel.ToNewCString();

  char wName[512];

  sprintf(wName, "__pulldown_%s", labelStr);
  mMenu = XmCreatePulldownMenu(aParent, wName, NULL, 0);

  Widget casBtn;
  XmString str;

  str = XmStringCreateLocalized(labelStr);
  casBtn = XtVaCreateManagedWidget(labelStr,
                                   xmCascadeButtonGadgetClass, aParent,
                                   XmNsubMenuId, mMenu,
                                   XmNlabelString, str,
                                   NULL);
  XmStringFree(str);


  delete[] labelStr;
}

//-------------------------------------------------------------------------
Widget nsMenu::GetNativeParent()
{

  void * voidData; 
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(&voidData);
  } else if (nsnull != mMenuBarParent) {
    mMenuBarParent->GetNativeData(voidData);
  } else {
    return NULL;
  }
  return (Widget)voidData;

}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
//NS_METHOD nsMenu::Create(nsIMenuBar * aParent, const nsString &aLabel)
NS_METHOD nsMenu::Create(nsISupports * aParent, const nsString &aLabel)
{
//  FIXME: This needs to be fixed. This doesn't work at all.  --ZuperDee

//  mMenuBarParent = aParent;
//  NS_ADDREF(mMenuBarParent);
//  Create(GetNativeParent(), aLabel);
//  NS_ASSERTION(0, "nsIMenu has changed.  fix me!");
  //aParent->AddMenu(this);
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
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports* aItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  // XXX add aMenuItem to internal data structor list
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{

  // XXX add aMenu to internal data structor list
  return NS_OK;

}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{

  Widget widget = XtVaCreateManagedWidget("__sep", xmSeparatorGadgetClass,
                                          mMenu,
                                          NULL);

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
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  return NS_OK;
}

NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  aData = (void *)mMenu;
  return NS_OK;
}

NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  //FIXME: Need to implement.  --ZuperDee
  return NS_OK;
}

NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  //FIXME: Need to implement.  --ZuperDee
  return NS_OK;
}

NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
  //FIXME: Need to implement.  --ZuperDee
  return NS_OK;
}

NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{ 
  //FIXME: Need to implement.  --ZuperDee
  return NS_OK;
}

NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
  //FIXME: Need to implement.  --ZuperDee
  return NS_OK;
}

nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

// FIXME: Need to implement.  --ZuperDee
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

// FIXME: Need to implement.  --ZuperDee
nsEventStatus nsMenu::MenuConstruct(const struct nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, void * menubarNode, void * aWebShell)
{
  return nsEventStatus_eIgnore;
}

// FIXME: Need to implement.  --ZuperDee
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}
