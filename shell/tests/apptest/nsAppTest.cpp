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
#include "nsAppTest.h"
#include "AppTestImpl.h"

// All Applications must specify this *special* application CID
// to their own unique IID.
nsIID kIXPCOMApplicationShellCID = NS_IAPPTEST_IID ; 

// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{
  nsresult res = NSRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsAppTestFactory(),
                                               PR_FALSE) ;

  return res;
}

/*
 * nsAppTest Definition
 */

nsAppTest::nsAppTest()
{
  NS_INIT_REFCNT();
}

nsAppTest::~nsAppTest()
{
}

NS_IMPL_ADDREF(nsAppTest)
NS_IMPL_RELEASE(nsAppTest)
NS_IMPL_QUERY_INTERFACE(nsAppTest,NS_IAPPTEST_IID);

nsresult nsAppTest::Init()
{
  InitAppTest(this);
  return NS_OK;
}

nsresult nsAppTest::Run()
{
  RunAppTest(this);
  return NS_OK;
}

/*
 * nsAppTestFactory Definition
 */

nsAppTestFactory::nsAppTestFactory()
{    
  NS_INIT_REFCNT();
}

nsAppTestFactory::~nsAppTestFactory()
{    
}

NS_IMPL_ADDREF(nsAppTestFactory)
NS_IMPL_RELEASE(nsAppTestFactory)
NS_IMPL_QUERY_INTERFACE(nsAppTestFactory,NS_IFACTORY_IID);

nsresult nsAppTestFactory::CreateInstance(nsISupports * aOuter,
                                                   const nsIID &aIID,
                                                   void ** aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL ;  

  nsISupports * inst = new nsAppTest() ;

  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {
    delete inst ;
  }

  return res;

}

nsresult nsAppTestFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}
