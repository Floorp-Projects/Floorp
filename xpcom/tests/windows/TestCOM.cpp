/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <windows.h>
#include <unknwn.h>
#include <stdio.h>
#include "nsISupports.h"
#include "nsIFactory.h"

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

// {5846BA30-B856-11d1-A98A-00805F8A7AC4}
#define NS_ITEST_COM_IID \
{ 0x5846ba30, 0xb856, 0x11d1, \
  { 0xa9, 0x8a, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } }

class nsITestCom: public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITEST_COM_IID)
  NS_IMETHOD Test() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITestCom, NS_ITEST_COM_IID)

/*
 * nsTestCom
 */

class nsTestCom: public nsITestCom {
  NS_DECL_ISUPPORTS

public:
  nsTestCom() {
  }

  NS_IMETHOD Test() {
    printf("Accessed nsITestCom::Test() from COM\n");
    return NS_OK;
  }

private:
  ~nsTestCom() {
    printf("nsTestCom instance successfully deleted\n");
  }
};

NS_IMPL_QUERY_INTERFACE1(nsTestCom, nsITestCom)

nsrefcnt nsTestCom::AddRef() 
{
  nsrefcnt res = ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "nsTestCom", sizeof(*this));
  printf("nsTestCom: Adding ref = %d\n", res);
  return res;
}

nsrefcnt nsTestCom::Release() 
{
  nsrefcnt res = --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "nsTestCom");
  printf("nsTestCom: Releasing = %d\n", res);
  if (res == 0) {
    delete this;
  }
  return res;
}

class nsTestComFactory: public nsIFactory {
  NS_DECL_ISUPPORTS
public:
  nsTestComFactory() {
  }
  
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(bool aLock) {
    printf("nsTestComFactory: ");
    printf("%s", (aLock ? "Locking server" : "Unlocking server"));
    printf("\n");
    return S_OK;
  }
};

NS_IMPL_ISUPPORTS1(nsTestComFactory, nsIFactory)

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
  
  NS_ADDREF(t);
  nsresult res = t->QueryInterface(aIID, aResult);
  NS_RELEASE(t);

  if (NS_SUCCEEDED(res)) {
    printf("nsTestComFactory: successfully created nsTestCom instance\n");
  }

  return res;
}

/*
 * main
 */

int main(int argc, char *argv[])
{
  nsTestComFactory *inst = new nsTestComFactory();
  IClassFactory *iFactory;
  inst->QueryInterface(NS_GET_IID(nsIFactory), (void **) &iFactory);

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

