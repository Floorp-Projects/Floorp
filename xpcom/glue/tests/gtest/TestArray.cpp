/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsISupportsArray.h"
#include "gtest/gtest.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

namespace TestArray {

class IFoo : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

class Foo final : public IFoo {
public:

  explicit Foo(int32_t aID);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IFoo implementation
  NS_IMETHOD_(nsrefcnt) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return mID; }

  static int32_t gCount;

  int32_t mID;

private:
  ~Foo();
};

int32_t Foo::gCount;

Foo::Foo(int32_t aID)
{
  mID = aID;
  ++gCount;
}

Foo::~Foo()
{
  --gCount;
}

NS_IMPL_ISUPPORTS(Foo, IFoo)

void CheckArray(nsISupportsArray* aArray, int32_t aExpectedCount, int32_t aElementIDs[], int32_t aExpectedTotal)
{
  uint32_t cnt = 0;
#ifdef DEBUG
  nsresult rv =
#endif
    aArray->Count(&cnt);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  int32_t count = cnt;
  int32_t index;

  EXPECT_EQ(Foo::gCount, aExpectedTotal);
  EXPECT_EQ(count, aExpectedCount);

  for (index = 0; (index < count) && (index < aExpectedCount); index++) {
    nsCOMPtr<IFoo> foo = do_QueryElementAt(aArray, index);
    EXPECT_EQ(foo->ID(), aElementIDs[index]);
  }
}

void FillArray(nsISupportsArray* aArray, int32_t aCount)
{
  int32_t index;
  for (index = 0; index < aCount; index++) {
    nsCOMPtr<IFoo> foo = new Foo(index);
    aArray->AppendElement(foo);
  }
}

}

using namespace TestArray;

TEST(Array, main)
{
  nsISupportsArray* array;
  nsresult  rv;

  if (NS_OK == (rv = NS_NewISupportsArray(&array))) {
    FillArray(array, 10);
    int32_t   fillResult[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    CheckArray(array, 10, fillResult, 10);

    // test insert
    nsCOMPtr<IFoo> foo = do_QueryElementAt(array, 3);
    array->InsertElementAt(foo, 5);
    int32_t   insertResult[11] = {0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    CheckArray(array, 11, insertResult, 10);
    array->InsertElementAt(foo, 0);
    int32_t   insertResult2[12] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    CheckArray(array, 12, insertResult2, 10);
    array->AppendElement(foo);
    int32_t   appendResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9, 3};
    CheckArray(array, 13, appendResult, 10);


    // test IndexOf && LastIndexOf
    int32_t expectedIndex[5] = {0, 4, 6, 12, -1};
    int32_t count = 0;
    int32_t index = array->IndexOf(foo);
    EXPECT_EQ(index, expectedIndex[count]);
    while (-1 != index) {
      count++;
      index = array->IndexOfStartingAt(foo, index + 1);
      if (-1 != index)
        EXPECT_EQ(index, expectedIndex[count]);
    }
    index = array->LastIndexOf(foo);
    count--;
    EXPECT_EQ(index, expectedIndex[count]);

    // test ReplaceElementAt
    array->ReplaceElementAt(foo, 8);
    int32_t   replaceResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    CheckArray(array, 13, replaceResult, 9);

    // test RemoveElementAt, RemoveElement RemoveLastElement
    array->RemoveElementAt(0);
    int32_t   removeResult[12] = {0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    CheckArray(array, 12, removeResult, 9);
    array->RemoveElementAt(7);
    int32_t   removeResult2[11] = {0, 1, 2, 3, 4, 3, 5, 7, 8, 9, 3};
    CheckArray(array, 11, removeResult2, 9);
    array->RemoveElement(foo);
    int32_t   removeResult3[10] = {0, 1, 2, 4, 3, 5, 7, 8, 9, 3};
    CheckArray(array, 10, removeResult3, 9);
    array->RemoveLastElement(foo);
    int32_t   removeResult4[9] = {0, 1, 2, 4, 3, 5, 7, 8, 9};
    CheckArray(array, 9, removeResult4, 9);

    foo = nullptr;

    // test clear
    array->Clear();
    FillArray(array, 4);
    CheckArray(array, 4, fillResult, 4);

    // test compact
    array->Compact();
    CheckArray(array, 4, fillResult, 4);

    // test delete
    NS_RELEASE(array);
  }
}
