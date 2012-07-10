/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoRef.h"
#include "TestHarness.h"

#define TEST(aCondition, aMsg) \
  if (!(aCondition)) { fail("TestAutoRef: "#aMsg); exit(1); }

struct TestObjectA {
public:
  TestObjectA() : mRefCnt(0) {
  }

  ~TestObjectA() {
    TEST(mRefCnt == 0, "mRefCnt in destructor");
  }

public:
  int mRefCnt;
};

template <>
class nsAutoRefTraits<TestObjectA> : public nsPointerRefTraits<TestObjectA>
{
public:
  static int mTotalRefsCnt;

  static void Release(TestObjectA *ptr) {
    ptr->mRefCnt--;
    if (ptr->mRefCnt == 0) {
      delete ptr;
    }
  }

  static void AddRef(TestObjectA *ptr) {
    ptr->mRefCnt++;
  }
};

int nsAutoRefTraits<TestObjectA>::mTotalRefsCnt = 0;

int main()
{
  {
    nsCountedRef<TestObjectA> a(new TestObjectA());
    TEST(a->mRefCnt == 1, "nsCountedRef instantiation with valid RawRef");

    nsCountedRef<TestObjectA> b;
    TEST(b.get() == nsnull, "nsCountedRef instantiation with invalid RawRef");

    a.swap(b);
    TEST(b->mRefCnt, "nsAutoRef::swap() t1");
    TEST(a.get() == nsnull, "nsAutoRef::swap() t2");
  }

  TEST(true, "All tests pass");
  return 0;
}
