/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoRef.h"
#include "gtest/gtest.h"

struct TestObjectA {
public:
  TestObjectA() : mRefCnt(0) {
  }

  ~TestObjectA() {
    EXPECT_EQ(mRefCnt, 0);
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

TEST(AutoRef, Assignment)
{
  {
    nsCountedRef<TestObjectA> a(new TestObjectA());
    ASSERT_EQ(a->mRefCnt, 1);

    nsCountedRef<TestObjectA> b;
    ASSERT_EQ(b.get(), nullptr);

    a.swap(b);
    ASSERT_EQ(b->mRefCnt, 1);
    ASSERT_EQ(a.get(), nullptr);
  }
}
