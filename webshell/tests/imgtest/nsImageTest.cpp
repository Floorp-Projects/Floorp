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
#include "nsImageTest.h"
#include "ImageTestImpl.h"

// All Applications must specify this *special* application CID
// to their own unique IID.
nsIID kIXPCOMApplicationShellCID = NS_IIMAGETEST_IID ; 

// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{
  nsresult res = NSRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsImageTestFactory(),
                                               PR_FALSE) ;

  return res;
}

/*
 * nsImageTest Definition
 */

nsImageTest::nsImageTest()
{
  NS_INIT_REFCNT();
}

nsImageTest::~nsImageTest()
{
}

NS_IMPL_ADDREF(nsImageTest)
NS_IMPL_RELEASE(nsImageTest)
NS_IMPL_QUERY_INTERFACE(nsImageTest,NS_IIMAGETEST_IID);

nsresult nsImageTest::Init()
{
  InitImageTest(this);
  return NS_OK;
}

nsresult nsImageTest::Run()
{
  RunImageTest(this);
  return NS_OK;
}

/*
 * nsImageTestFactory Definition
 */

nsImageTestFactory::nsImageTestFactory()
{    
  NS_INIT_REFCNT();
}

nsImageTestFactory::~nsImageTestFactory()
{    
}

NS_IMPL_ADDREF(nsImageTestFactory)
NS_IMPL_RELEASE(nsImageTestFactory)
NS_IMPL_QUERY_INTERFACE(nsImageTestFactory,NS_IFACTORY_IID);

nsresult nsImageTestFactory::CreateInstance(nsISupports * aOuter,
                                                   const nsIID &aIID,
                                                   void ** aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL ;  

  nsISupports * inst = new nsImageTest() ;

  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {
    delete inst ;
  }

  return res;

}

nsresult nsImageTestFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}
