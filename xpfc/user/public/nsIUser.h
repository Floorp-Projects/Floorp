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

#ifndef nsIUser_h___
#define nsIUser_h___

#include "nsISupports.h"
#include "nsString.h"

//5695c240-4cb8-11d2-924a-00805f8a7ab6
#define NS_IUSER_IID      \
 { 0x5695c240, 0x4cb8, 0x11d2, \
   {0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }


enum nsUserField {  
  nsUserField_username,            
  nsUserField_emailaddress, 
  nsUserField_displayname  
};


class nsIUser : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD GetUserName(nsString& aUserName) = 0;
  NS_IMETHOD SetUserName(nsString& aUserName) = 0;

  NS_IMETHOD GetUserField(nsUserField aUserField, nsString& aUserValue) = 0;
  NS_IMETHOD SetUserField(nsUserField aUserField, nsString& aUserValue) = 0;
};

#endif /* nsIUser_h___ */

