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

#ifndef _nsAppTest_h__
#define _nsAppTest_h__

#include <stdio.h>
#include "nsIApplicationShell.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsShellInstance.h"

#define NS_IAPPTEST_IID      \
 { 0x5040bd10, 0xdec5, 0x11d1, \
   {0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

/*
 * AppTest Class Declaration
 */

class nsAppTest : public nsIApplicationShell {
public:
  nsAppTest();
  ~nsAppTest();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD Run();

};

/*
 * AppTestFactory Class Declaration
 */

class nsAppTestFactory : public nsIFactory {

public:
  nsAppTestFactory();
  ~nsAppTestFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports * aOuter,
                            const nsIID &aIID,
                            void ** aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

};


#endif
