/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include "TestFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIServiceManager.h"

NS_DEFINE_CID(kTestFactoryCID, NS_TESTFACTORY_CID);
NS_DEFINE_CID(kTestLoadedFactoryCID, NS_TESTLOADEDFACTORY_CID);


/**
 * ITestClass implementation
 */

class TestClassImpl: public ITestClass {
  NS_DECL_ISUPPORTS
public:
  TestClassImpl() {
  }

  void Test();
};

NS_IMPL_ISUPPORTS1(TestClassImpl, ITestClass)

void TestClassImpl::Test() {
  printf("hello, world!\n");
}

/**
 * TestFactory implementation
 */

class TestFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
  
public:
  TestFactory() {
  }

  NS_IMETHOD CreateInstance(nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock) { return NS_OK; }
};

NS_IMPL_ISUPPORTS1(TestFactory, nsIFactory)

nsresult TestFactory::CreateInstance(nsISupports *aDelegate,
                                     const nsIID &aIID,
                                     void **aResult) {
  if (aDelegate != NULL) {
    return NS_ERROR_NO_AGGREGATION;
  }

  TestClassImpl *t = new TestClassImpl();
  
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


int main(int argc, char **argv) {
  nsresult rv;

  {
    nsCOMPtr<nsIServiceManager> servMan;
    rv = NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    if (NS_FAILED(rv)) return -1;
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    NS_ASSERTION(registrar, "Null nsIComponentRegistrar");
    if (registrar)
      registrar->RegisterFactory(kTestFactoryCID,
                                 nsnull,
                                 nsnull,
                                 new TestFactory());

    ITestClass *t = NULL;
    CallCreateInstance(kTestFactoryCID, &t);

    if (t != NULL) {
      t->Test();
      t->Release();
    } else {
      printf("CreateInstance failed\n");
    }

    t = NULL;

    CallCreateInstance(kTestLoadedFactoryCID, &t);

    if (t != NULL) {
      t->Test();
      t->Release();
    } else {
      printf("Dynamic CreateInstance failed\n");
    }
  } // this scopes the nsCOMPtrs
  // no nsCOMPtrs are allowed to be alive when you call NS_ShutdownXPCOM
  rv = NS_ShutdownXPCOM(nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return 0;
}

