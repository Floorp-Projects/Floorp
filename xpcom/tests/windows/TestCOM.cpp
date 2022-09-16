/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <unknwn.h>
#include <stdio.h>
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/RefPtr.h"

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include "gtest/gtest.h"

// {5846BA30-B856-11d1-A98A-00805F8A7AC4}
#define NS_ITEST_COM_IID                            \
  {                                                 \
    0x5846ba30, 0xb856, 0x11d1, {                   \
      0xa9, 0x8a, 0x0, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 \
    }                                               \
  }

class nsITestCom : public nsISupports {
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
  nsTestCom() {}

  NS_IMETHOD Test() override { return NS_OK; }

  static int sDestructions;

 private:
  ~nsTestCom() { sDestructions++; }
};

int nsTestCom::sDestructions;

NS_IMPL_QUERY_INTERFACE(nsTestCom, nsITestCom)

MozExternalRefCountType nsTestCom::AddRef() {
  nsrefcnt res = ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "nsTestCom", sizeof(*this));
  return res;
}

MozExternalRefCountType nsTestCom::Release() {
  nsrefcnt res = --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "nsTestCom");
  if (res == 0) {
    delete this;
  }
  return res;
}

TEST(TestCOM, WindowsInterop)
{
  // Test that we can QI an nsITestCom to an IUnknown.
  RefPtr<nsTestCom> t = new nsTestCom();
  IUnknown* iUnknown = nullptr;
  nsresult rv = t->QueryInterface(NS_GET_IID(nsISupports), (void**)&iUnknown);
  ASSERT_NS_SUCCEEDED(rv);
  ASSERT_TRUE(iUnknown);

  // Test we can QI an IUnknown to nsITestCom.
  nsCOMPtr<nsITestCom> iTestCom;
  GUID testGUID = NS_ITEST_COM_IID;
  HRESULT hr = iUnknown->QueryInterface(testGUID, getter_AddRefs(iTestCom));
  ASSERT_TRUE(SUCCEEDED(hr));
  ASSERT_TRUE(iTestCom);

  // Make sure we can call our test function (and the pointer is valid).
  rv = iTestCom->Test();
  ASSERT_NS_SUCCEEDED(rv);

  iUnknown->Release();
  iTestCom = nullptr;
  t = nullptr;

  ASSERT_EQ(nsTestCom::sDestructions, 1);
}
