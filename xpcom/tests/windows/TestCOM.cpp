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
#include "nsISupports.h"
#include "nsIFactory.h"

// {5846BA30-B856-11d1-A98A-00805F8A7AC4}
#define NS_ITEST_COM_IID \
{ 0x5846ba30, 0xb856, 0x11d1, \
  { 0xa9, 0x8a, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

class nsITestCom: public nsISupports
{
public:
  NS_IMETHOD Test() = 0;
};

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kITestComIID, NS_ITEST_COM_IID);

/*
 * nsTestCom
 */

class nsTestCom: public nsITestCom {
  NS_DECL_ISUPPORTS

public:
  nsTestCom() {
    NS_INIT_REFCNT();
  }
  virtual ~nsTestCom() {
    cout << "nsTestCom instance successfully deleted\n";
  }

  NS_IMETHOD Test() {
    cout << "Accessed nsITestCom::Test() from COM\n";
    return NS_OK;
  }
};

NS_IMPL_QUERY_INTERFACE(nsTestCom, kITestComIID);

nsrefcnt nsTestCom::AddRef() 
{
  nsrefcnt res = ++mRefCnt;
  cout << "nsTestCom: Adding ref = " << res << "\n";
  return res;
}

nsrefcnt nsTestCom::Release() 
{
  nsrefcnt res = --mRefCnt;
  cout << "nsTestCom: Releasing = " << res << "\n";
  if (res == 0) {
    delete this;
  }
  return res;
}

class nsTestComFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
public:
  nsTestComFactory() {
    NS_INIT_REFCNT();
  }
  
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD_(void) LockFactory(PRBool aLock) {
    cout << "nsTestComFactory: ";
    cout << (aLock == PR_TRUE ? "Locking server" : "Unlocking server");
    cout << "\n";
  }
};

NS_IMPL_ISUPPORTS(nsTestComFactory, kIFactoryIID);

nsresult nsTestComFactory::CreateInstance(nsISupports *aOuter,
					  const nsIID &aIID,
					  void **aResult)
{
  if (aOuter != NULL) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsTestCom *t = new nsTestCom();
  
  if (t == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsresult res = t->QueryInterface(aIID, aResult);

  if (NS_FAILED(res)) {
    *aResult = NULL;
    delete t;
  }

  cout << "nsTestComFactory: successfully created nsTestCom instance\n";

  return res;
}

/*
 * main
 */

int main(int argc, char *argv[])
{
  nsTestComFactory *inst = new nsTestComFactory();
  IClassFactory *iFactory;
  inst->QueryInterface(kIFactoryIID, (void **) &iFactory);

  IUnknown *iUnknown;  
  nsITestCom *iTestCom;

  nsresult res;
  iFactory->LockServer(TRUE);
  res = iFactory->CreateInstance(NULL,
				 IID_IUnknown, 
				 (void **) &iUnknown);
  iFactory->LockServer(FALSE);

  GUID testGUID = NS_ITEST_COM_IID;
  HRESULT hres;
  hres= iUnknown->QueryInterface(testGUID, 
				 (void **) &iTestCom);

  iTestCom->Test();

  iUnknown->Release();
  iTestCom->Release();
  iFactory->Release();

  return 0;
}

