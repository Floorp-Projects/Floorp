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

#include <iostream.h>
#include "pratom.h"
#include "TestFactory.h"
#include "nsCom.h"
#include "nsISupports.h"
#include "nsRepository.h"

NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_CID(kTestLoadedFactoryCID, NS_TESTLOADEDFACTORY_CID);
NS_DEFINE_IID(kTestClassIID, NS_ITESTCLASS_IID);

/**
 * ITestClass implementation
 */

class TestDynamicClassImpl: public ITestClass {
  NS_DECL_ISUPPORTS

  TestDynamicClassImpl() {
    NS_INIT_REFCNT();
  }
  void Test();
};

NS_IMPL_ISUPPORTS(TestDynamicClassImpl, kTestClassIID);

void TestDynamicClassImpl::Test() {
  cout << "hello, dynamic world!\n";
}

/**
 * TestFactory implementation
 */

static PRInt32 g_FactoryCount = 0;
static PRInt32 g_LockCount = 0;

class TestDynamicFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
  
  TestDynamicFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_FactoryCount);
  }

  ~TestDynamicFactory() {
    PR_AtomicDecrement(&g_FactoryCount);
  }

  NS_IMETHOD CreateInstance(nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock) {
    if (aLock) {
      PR_AtomicIncrement(&g_LockCount);
    } else {
      PR_AtomicDecrement(&g_LockCount);
    }
    return NS_OK;
  };
};

NS_IMPL_ISUPPORTS(TestDynamicFactory, kFactoryIID);

nsresult TestDynamicFactory::CreateInstance(nsISupports *aDelegate,
                                            const nsIID &aIID,
                                            void **aResult)
{
  if (aDelegate != NULL) {
    return NS_ERROR_NO_AGGREGATION;
  }

  TestDynamicClassImpl *t = new TestDynamicClassImpl();
  
  if (t == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult res = t->QueryInterface(aIID, aResult);
  
  if (NS_FAILED(res)) {
    *aResult = NULL;
    delete t;
  }

  return res;
}

extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aCID,
                                           nsIFactory **aFactory) {
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aCID.Equals(kTestLoadedFactoryCID)) {
    TestDynamicFactory *factory = new TestDynamicFactory();
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }
    return res;
  }
  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool NSCanUnload() {
  return PRBool(g_FactoryCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(const char *path)
{
  return NSRepository::RegisterFactory(kTestLoadedFactoryCID, path, 
                                       PR_TRUE, PR_TRUE);
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(const char *path)
{
  return NSRepository::UnregisterFactory(kTestLoadedFactoryCID, path);
}
