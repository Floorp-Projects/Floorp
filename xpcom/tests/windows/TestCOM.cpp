/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <unknwn.h>
#include <stdio.h>
#include "nsISupports.h"
#include "nsIFactory.h"

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include "gtest/gtest.h"

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

class nsTestCom final : public nsITestCom {
  NS_DECL_ISUPPORTS

public:
  nsTestCom() {
  }

  NS_IMETHOD Test() {
    return NS_OK;
  }

  static int sDestructions;

private:
  ~nsTestCom() {
    sDestructions++;
  }
};

int nsTestCom::sDestructions;

NS_IMPL_QUERY_INTERFACE(nsTestCom, nsITestCom)

MozExternalRefCountType nsTestCom::AddRef()
{
  nsrefcnt res = ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "nsTestCom", sizeof(*this));
  return res;
}

MozExternalRefCountType nsTestCom::Release()
{
  nsrefcnt res = --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "nsTestCom");
  if (res == 0) {
    delete this;
  }
  return res;
}

class nsTestComFactory final : public nsIFactory {
  ~nsTestComFactory() { sDestructions++; }
  NS_DECL_ISUPPORTS
public:
  nsTestComFactory() {
  }

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(bool aLock) {
    return NS_OK;
  }

  static int sDestructions;
};

int nsTestComFactory::sDestructions;

NS_IMPL_ISUPPORTS(nsTestComFactory, nsIFactory)

nsresult nsTestComFactory::CreateInstance(nsISupports *aOuter,
					  const nsIID &aIID,
					  void **aResult)
{
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsTestCom *t = new nsTestCom();

  if (t == nullptr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(t);
  nsresult res = t->QueryInterface(aIID, aResult);
  NS_RELEASE(t);

  return res;
}

TEST(TestCOM, WindowsInterop)
{
  nsTestComFactory *inst = new nsTestComFactory();

  // Test we can QI nsIFactory to an IClassFactory.
  IClassFactory *iFactory = nullptr;
  nsresult rv = inst->QueryInterface(NS_GET_IID(nsIFactory),
                                      (void **) &iFactory);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(iFactory);

  // Test we can CreateInstance with an IUnknown.
  IUnknown *iUnknown = nullptr;

  HRESULT hr = iFactory->LockServer(TRUE);
  ASSERT_TRUE(SUCCEEDED(hr));
  hr = iFactory->CreateInstance(nullptr, IID_IUnknown, (void **) &iUnknown);
  ASSERT_TRUE(SUCCEEDED(hr));
  ASSERT_TRUE(iUnknown);
  hr = iFactory->LockServer(FALSE);
  ASSERT_TRUE(SUCCEEDED(hr));

  // Test we can QI an IUnknown to nsITestCom.
  nsITestCom *iTestCom = nullptr;
  GUID testGUID = NS_ITEST_COM_IID;
  hr = iUnknown->QueryInterface(testGUID,
				 (void **) &iTestCom);
  ASSERT_TRUE(SUCCEEDED(hr));
  ASSERT_TRUE(iTestCom);

  // Make sure we can call our test function (and the pointer is valid).
  rv = iTestCom->Test();
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  iUnknown->Release();
  iTestCom->Release();
  iFactory->Release();

  ASSERT_EQ(nsTestComFactory::sDestructions, 1);
  ASSERT_EQ(nsTestCom::sDestructions, 1);
}
