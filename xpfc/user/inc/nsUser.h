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

#ifndef nsUser_h___
#define nsUser_h___

#include "nsIUser.h"
#include "nsAgg.h"

class nsUser : public nsIUser

{
public:
  nsUser(nsISupports* outer);

  NS_DECL_AGGREGATED

  NS_IMETHOD Init();

  NS_IMETHOD GetUserName(nsString& aUserName);
  NS_IMETHOD SetUserName(nsString& aUserName);

  NS_IMETHOD GetUserField(nsUserField aUserField, nsString& aUserValue);
  NS_IMETHOD SetUserField(nsUserField aUserField, nsString& aUserValue);

protected:
  ~nsUser();

private:
  nsString mUserName;

};

#endif /* nsUser_h___ */
