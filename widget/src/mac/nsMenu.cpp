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

#if defined(XP_MAC)
#include <TextUtils.h>
#include <ToolUtils.h>
#endif

//#include <Xm/CascadeBG.h>
//#include <Xm/SeparatoG.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
//NS_IMPL_ISUPPORTS(nsMenu, kMenuIID)

PRInt16 nsMenu::mMacMenuIDCount = 256;

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
    *aInstancePtr = (void*)(nsISupports*) this;                        
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

#ifdef MOTIF
//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
void nsMenu::Create(Widget aParent, const nsString &aLabel)
{
  /*
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
  */
}


//-------------------------------------------------------------------------
Widget nsMenu::GetNativeParent()
{

  void * voidData; 
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(voidData);
  } else if (nsnull != mMenuBarParent) {
    mMenuBarParent->GetNativeData(voidData);
  } else {
    return NULL;
  }
  return (Widget)voidData;

}

#endif

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsIMenuBar *aParent, const nsString &aLabel)
{
  mMenuBarParent = aParent;
  NS_ADDREF(mMenuBarParent);

  //Create(GetNativeParent(), aLabel);


  //aParent->AddMenu(this);
    
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsIMenu *aParent, const nsString &aLabel)
{
  mMenuParent = aParent;
  NS_ADDREF(mMenuParent);

  //Create(GetNativeParent(), aLabel);
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
NS_METHOD nsMenu::SetLabel(nsString &aText)
{
   mLabel = aText;
   
  mMacMenuHandle = nsnull;
  mMacMenuHandle = ::NewMenu(mMacMenuIDCount, c2pstr(mLabel.ToNewCString()) );
  mMacMenuID = mMacMenuIDCount;
  mMacMenuIDCount++;
  /*
  Str255 test;
  strcpy((char*)&test, "test");
  c2pstr((char*)test);
  mMacMenuHandle = ::NewMenu(500, test);
  */
  
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
  // XXX add aMenuItem to internal data structor list
  mMenuItemVoidArray.AppendElement(aMenuItem);
  
  nsString label;
  aMenuItem->GetLabel(label);
  ::InsertMenuItem(mMacMenuHandle, c2pstr(label.ToNewCString()), mNumMenuItems );
  mNumMenuItems++;
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

  //Widget widget = XtVaCreateManagedWidget("__sep", xmSeparatorGadgetClass, mMenu, NULL);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, nsIMenuItem *& aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, const nsString & aMenuItemName)
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

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void *& aData)
{
  aData = (void *)mMacMenuHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsIMenuListener interface
//
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // Determine if this is the correct menu to handle the event
  PRInt16 menuID = HiWord(((nsMenuEvent)aMenuEvent).mCommand);
  if(mMacMenuID == menuID)
  {
    for (int i = mMenuItemVoidArray.Count(); i >= 0; i--)
    {
      nsEventStatus eventStatus = nsEventStatus_eIgnore;
	  nsIMenuListener * menuListener = nsnull;
	  ((nsIMenuItem*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuListenerIID, &menuListener);
	  eventStatus = menuListener->MenuSelected(aMenuEvent);
	  //NS_RELEASE(menuListener);
	  if(nsEventStatus_eIgnore != eventStatus)
	    return eventStatus;
	}
  }
  else
    return nsEventStatus_eIgnore;
}