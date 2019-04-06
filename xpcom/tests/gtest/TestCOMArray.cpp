/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "gtest/gtest.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID                                  \
  {                                                  \
    0x9e70a320, 0xbe02, 0x11d1, {                    \
      0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a \
    }                                                \
  }

class IFoo : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(MozExternalRefCountType) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

class Foo final : public IFoo {
  ~Foo();

 public:
  explicit Foo(int32_t aID);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IFoo implementation
  NS_IMETHOD_(MozExternalRefCountType) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return mID; }

  static int32_t gCount;

  int32_t mID;
};

int32_t Foo::gCount = 0;

Foo::Foo(int32_t aID) {
  mID = aID;
  ++gCount;
}

Foo::~Foo() { --gCount; }

NS_IMPL_ISUPPORTS(Foo, IFoo)

// {0e70a320-be02-11d1-8031-006008159b5a}
#define NS_IBAR_IID                                  \
  {                                                  \
    0x0e70a320, 0xbe02, 0x11d1, {                    \
      0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a \
    }                                                \
  }

class IBar : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBAR_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(IBar, NS_IBAR_IID)

class Bar final : public IBar {
 public:
  explicit Bar(nsCOMArray<IBar>& aArray);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  static int32_t sReleaseCalled;

 private:
  ~Bar();

  nsCOMArray<IBar>& mArray;
};

int32_t Bar::sReleaseCalled = 0;

typedef nsCOMArray<IBar> Array2;

Bar::Bar(Array2& aArray) : mArray(aArray) {}

Bar::~Bar() { EXPECT_FALSE(mArray.RemoveObject(this)); }

NS_IMPL_ADDREF(Bar)
NS_IMPL_QUERY_INTERFACE(Bar, IBar)

NS_IMETHODIMP_(MozExternalRefCountType)
Bar::Release(void) {
  ++Bar::sReleaseCalled;
  EXPECT_GT(int(mRefCnt), 0);
  NS_ASSERT_OWNINGTHREAD(_class);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "Bar");
  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
}

TEST(COMArray, Sizing)
{
  nsCOMArray<IFoo> arr;

  for (int32_t i = 0; i < 20; ++i) {
    nsCOMPtr<IFoo> foo = new Foo(i);
    arr.AppendObject(foo);
  }

  ASSERT_EQ(arr.Count(), int32_t(20));
  ASSERT_EQ(Foo::gCount, int32_t(20));

  arr.TruncateLength(10);

  ASSERT_EQ(arr.Count(), int32_t(10));
  ASSERT_EQ(Foo::gCount, int32_t(10));

  arr.SetCount(30);

  ASSERT_EQ(arr.Count(), int32_t(30));
  ASSERT_EQ(Foo::gCount, int32_t(10));

  for (int32_t i = 0; i < 10; ++i) {
    ASSERT_NE(arr[i], nullptr);
  }

  for (int32_t i = 10; i < 30; ++i) {
    ASSERT_EQ(arr[i], nullptr);
  }
}

TEST(COMArray, ObjectFunctions)
{
  int32_t base;
  {
    nsCOMArray<IBar> arr2;

    IBar *thirdObject = nullptr, *fourthObject = nullptr,
         *fifthObject = nullptr, *ninthObject = nullptr;
    for (int32_t i = 0; i < 20; ++i) {
      nsCOMPtr<IBar> bar = new Bar(arr2);
      switch (i) {
        case 2:
          thirdObject = bar;
          break;
        case 3:
          fourthObject = bar;
          break;
        case 4:
          fifthObject = bar;
          break;
        case 8:
          ninthObject = bar;
          break;
      }
      arr2.AppendObject(bar);
    }

    base = Bar::sReleaseCalled;

    arr2.SetCount(10);
    ASSERT_EQ(Bar::sReleaseCalled, base + 10);
    ASSERT_EQ(arr2.Count(), int32_t(10));

    arr2.RemoveObjectAt(9);
    ASSERT_EQ(Bar::sReleaseCalled, base + 11);
    ASSERT_EQ(arr2.Count(), int32_t(9));

    arr2.RemoveObject(ninthObject);
    ASSERT_EQ(Bar::sReleaseCalled, base + 12);
    ASSERT_EQ(arr2.Count(), int32_t(8));

    arr2.RemoveObjectsAt(2, 3);
    ASSERT_EQ(Bar::sReleaseCalled, base + 15);
    ASSERT_EQ(arr2.Count(), int32_t(5));
    for (int32_t j = 0; j < arr2.Count(); ++j) {
      ASSERT_NE(arr2.ObjectAt(j), thirdObject);
      ASSERT_NE(arr2.ObjectAt(j), fourthObject);
      ASSERT_NE(arr2.ObjectAt(j), fifthObject);
    }

    arr2.RemoveObjectsAt(4, 1);
    ASSERT_EQ(Bar::sReleaseCalled, base + 16);
    ASSERT_EQ(arr2.Count(), int32_t(4));

    arr2.Clear();
    ASSERT_EQ(Bar::sReleaseCalled, base + 20);
  }
}

TEST(COMArray, ElementFunctions)
{
  int32_t base;
  {
    nsCOMArray<IBar> arr2;

    IBar *thirdElement = nullptr, *fourthElement = nullptr,
         *fifthElement = nullptr, *ninthElement = nullptr;
    for (int32_t i = 0; i < 20; ++i) {
      nsCOMPtr<IBar> bar = new Bar(arr2);
      switch (i) {
        case 2:
          thirdElement = bar;
          break;
        case 3:
          fourthElement = bar;
          break;
        case 4:
          fifthElement = bar;
          break;
        case 8:
          ninthElement = bar;
          break;
      }
      arr2.AppendElement(bar);
    }

    base = Bar::sReleaseCalled;

    arr2.TruncateLength(10);
    ASSERT_EQ(Bar::sReleaseCalled, base + 10);
    ASSERT_EQ(arr2.Length(), uint32_t(10));

    arr2.RemoveElementAt(9);
    ASSERT_EQ(Bar::sReleaseCalled, base + 11);
    ASSERT_EQ(arr2.Length(), uint32_t(9));

    arr2.RemoveElement(ninthElement);
    ASSERT_EQ(Bar::sReleaseCalled, base + 12);
    ASSERT_EQ(arr2.Length(), uint32_t(8));

    arr2.RemoveElementsAt(2, 3);
    ASSERT_EQ(Bar::sReleaseCalled, base + 15);
    ASSERT_EQ(arr2.Length(), uint32_t(5));
    for (uint32_t j = 0; j < arr2.Length(); ++j) {
      ASSERT_NE(arr2.ElementAt(j), thirdElement);
      ASSERT_NE(arr2.ElementAt(j), fourthElement);
      ASSERT_NE(arr2.ElementAt(j), fifthElement);
    }

    arr2.RemoveElementsAt(4, 1);
    ASSERT_EQ(Bar::sReleaseCalled, base + 16);
    ASSERT_EQ(arr2.Length(), uint32_t(4));

    arr2.Clear();
    ASSERT_EQ(Bar::sReleaseCalled, base + 20);
  }
}

TEST(COMArray, Destructor)
{
  int32_t base;
  Bar::sReleaseCalled = 0;

  {
    nsCOMArray<IBar> arr2;

    for (int32_t i = 0; i < 20; ++i) {
      nsCOMPtr<IBar> bar = new Bar(arr2);
      arr2.AppendObject(bar);
    }

    base = Bar::sReleaseCalled;

    // Let arr2 be destroyed
  }
  ASSERT_EQ(Bar::sReleaseCalled, base + 20);
}
