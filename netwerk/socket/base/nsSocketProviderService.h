/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef _NSSOCKETPROVIDERSERVICE_H_
#define _NSSOCKETPROVIDERSERVICE_H_

#include "nsISocketProviderService.h"

class nsSocketProviderService : public nsISocketProviderService
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSISOCKETPROVIDERSERVICE

  // nsISocketProviderService methods:
  nsSocketProviderService();
  virtual ~nsSocketProviderService();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  nsresult Init();

protected:
};

#endif /* _NSSOCKETPROVIDERSERVICE_H */
