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

#include "nscore.h"
#include "nsMenuItem.h"
#include "nsIXMLParserObject.h"
#include "nsXPFCActionCommand.h"
#include "nsRepository.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kIXMLParserObjectIID, NS_IXML_PARSER_OBJECT_IID);

nsMenuItem::nsMenuItem()
{
  NS_INIT_REFCNT();
  mName = "Item";
  mLabel = "Label";
  mParent = nsnull;
  mSeparator = PR_FALSE;
  mAlignmentStyle = eAlignmentStyle_none;
  mID = 0;
  mReceiver = nsnull;
  mCommand = "Command";
  mEnabled = PR_TRUE;
}

nsMenuItem::~nsMenuItem()
{
}

NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

NS_IMPL_ADDREF(nsMenuItem)
NS_IMPL_RELEASE(nsMenuItem)

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kIMenuItemIID);                         

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIXMLParserObjectIID)) {                                      
    *aInstancePtr = (nsIXMLParserObject*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return NS_NOINTERFACE;

}

nsresult nsMenuItem::Init()
{
  return NS_OK;
}

nsresult nsMenuItem :: SetParameter(nsString& aKey, nsString& aValue)
{
  if (aKey == "name") {

    SetName(aValue);

  } else if (aKey == "align") {

    if (aValue == "left")
      mAlignmentStyle = eAlignmentStyle_left;
    else if (aValue == "right")
      mAlignmentStyle = eAlignmentStyle_right;
    
  } else if (aKey == "enable") {

    if (aValue == "False")
      mEnabled = PR_FALSE;
    else if (aValue == "True")
      mEnabled = PR_TRUE;
    
  } else if (aKey == "type") {

    if (aValue == "separator")
      mSeparator = PR_TRUE;

  } else if (aKey == "label") {
    
    SetLabel(aValue);

  } else if (aKey.EqualsIgnoreCase("command")) {

    if (aValue.Find("LoadUrl?") != -1)
    {
      nsString command;

      aValue.Mid(command,8,aValue.Length()-8);

      SetCommand(command);    

    } else {

      SetCommand(aValue);    

    }
    
  }


  return NS_OK;
}

PRBool nsMenuItem::IsSeparator()
{
  return (mSeparator);
}

nsAlignmentStyle nsMenuItem::GetAlignmentStyle()
{
  return (mAlignmentStyle);
}

nsresult nsMenuItem::SetAlignmentStyle(nsAlignmentStyle aAlignmentStyle)
{
  mAlignmentStyle = aAlignmentStyle;
  return NS_OK;
}

PRBool nsMenuItem::GetEnabled()
{
  return (mEnabled);
}

nsresult nsMenuItem::SetEnabled(PRBool aEnable)
{
  mEnabled = aEnable;
  return NS_OK;
}

nsresult nsMenuItem :: SetName(nsString& aName)
{
  mName = aName;
  return NS_OK;
}

nsString& nsMenuItem :: GetName()
{
  return (mName);
}

nsresult nsMenuItem :: SetCommand(nsString& aCommand)
{
  mCommand = aCommand;
  return NS_OK;
}

nsString& nsMenuItem :: GetCommand()
{
  return (mCommand);
}

nsresult nsMenuItem :: SetLabel(nsString& aLabel)
{
  mLabel = aLabel;
  return NS_OK;
}

nsString& nsMenuItem :: GetLabel()
{
  return (mLabel);
}

nsIMenuContainer * nsMenuItem::GetParent()
{
  return (mParent);
}

nsresult nsMenuItem::SetParent(nsIMenuContainer * aParent)
{
  mParent = aParent;
  return NS_OK;
}

nsresult nsMenuItem :: SetMenuID(PRUint32 aID)
{
  mID = aID;
  return NS_OK;
}

PRUint32 nsMenuItem :: GetMenuID()
{
  return (mID);
}

nsresult nsMenuItem::SetReceiver(nsIXPFCCommandReceiver * aReceiver)
{
  mReceiver = aReceiver;
  return NS_OK;
}

nsresult nsMenuItem :: SendCommand()
{
  if (mReceiver == nsnull)
    return NS_OK;

  nsXPFCActionCommand * command;

  static NS_DEFINE_IID(kCXPFCActionCommandCID, NS_XPFC_ACTION_COMMAND_CID);
  static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

  nsresult res = nsRepository::CreateInstance(kCXPFCActionCommandCID, 
                                              nsnull, 
                                              kXPFCCommandIID, 
                                              (void **)&command);

  if (NS_OK != res)
    return res ;

  command->Init();

  command->mAction = mCommand;

  mReceiver->Action(command);

  NS_RELEASE(command);

  return (NS_OK);
}

