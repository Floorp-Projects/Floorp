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

#include "nsUser.h"
#include "nsxpfcCIID.h"
#include "nspr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCUserCID,     NS_USER_CID);
static NS_DEFINE_IID(kCIUserIID,    NS_IUSER_IID);

nsUser :: nsUser(nsISupports* outer)
{
  NS_INIT_AGGREGATED(outer);
}

nsUser :: ~nsUser()
{
}

NS_IMPL_AGGREGATED(nsUser);

NS_METHOD nsUser::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kCIUserIID) || aIID.Equals(kCUserCID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports *)this);//&fAggregated);
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


nsresult nsUser :: Init()
{
  return NS_OK;
}

nsresult nsUser :: GetUserName(nsString& aUserName)
{
  GetUserField(nsUserField_username, aUserName);
  return NS_OK;
}

nsresult nsUser :: SetUserName(nsString& aUserName)
{
  SetUserField(nsUserField_username, aUserName);
  return NS_OK;
}

nsresult nsUser :: GetUserField(nsUserField aUserField, nsString& aUserValue)
{
  switch(aUserField)
  {
    case nsUserField_username:
      aUserValue = mUserName ;
      break;

    case nsUserField_emailaddress:
      break;

    case nsUserField_displayname:
      break;

    default:
      break;
  }

  return NS_OK;
}

nsresult nsUser :: SetUserField(nsUserField aUserField, nsString& aUserValue)
{
  switch(aUserField)
  {
    case nsUserField_username:
      mUserName = aUserValue ;
      break;

    case nsUserField_emailaddress:
      break;

    case nsUserField_displayname:
      break;

    default:
      break;
  }

  return NS_OK;
}

