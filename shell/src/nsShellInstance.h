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
#include <stdio.h>
#include "nsIApplicationShell.h"
#include "nsIFactory.h"
#include "nscore.h"
#include "nsweb.h"
#include "nsCRT.h"
#include "nsIShellInstance.h"

// platform independent native handle to application instance
typedef void * nsNativeApplicationInstance ;

class nsShellInstance : public nsIShellInstance {
public:
  nsShellInstance();
  ~nsShellInstance();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD Run();

  NS_METHOD RegisterFactories();

  NS_METHOD_(void *) GetNativeInstance(); 
  NS_METHOD_(void)   SetNativeInstance(void * aNativeInstance);

  NS_METHOD_(nsIApplicationShell *) GetApplicationShell(); 
  NS_METHOD_(void)   SetApplicationShell(nsIApplicationShell * aApplicationShell);


private:
  nsNativeApplicationInstance mNativeInstance ;
  nsIApplicationShell * mApplicationShell ;
};


