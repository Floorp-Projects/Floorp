/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsIFactory_h
#define __nsIFactory_h

#include "prtypes.h"
#include "nsISupports.h"

/*
 * nsIFactory interface
 */

// {00000001-0000-0000-c000-000000000046}
#define NS_IFACTORY_IID \
{ 0x00000001, 0x0000, 0x0000, \
  { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } }

class nsIFactory: public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IFACTORY_IID; return iid; }

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult) = 0;

  NS_IMETHOD LockFactory(PRBool aLock) = 0;
};

#endif
