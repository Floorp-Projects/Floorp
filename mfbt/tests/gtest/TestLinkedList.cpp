/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/LinkedList.h"
#include "mozilla/RefPtr.h"

using mozilla::AutoCleanLinkedList;
using mozilla::LinkedList;
using mozilla::LinkedListElement;

class PtrClass : public LinkedListElement<PtrClass> {
 public:
  bool* mResult;

  explicit PtrClass(bool* result) : mResult(result) { EXPECT_TRUE(!*mResult); }

  virtual ~PtrClass() { *mResult = true; }
};

class InheritedPtrClass : public PtrClass {
 public:
  bool* mInheritedResult;

  InheritedPtrClass(bool* result, bool* inheritedResult)
      : PtrClass(result), mInheritedResult(inheritedResult) {
    EXPECT_TRUE(!*mInheritedResult);
  }

  virtual ~InheritedPtrClass() { *mInheritedResult = true; }
};

TEST(LinkedList, AutoCleanLinkedList)
{
  bool rv1 = false;
  bool rv2 = false;
  bool rv3 = false;
  {
    AutoCleanLinkedList<PtrClass> list;
    list.insertBack(new PtrClass(&rv1));
    list.insertBack(new InheritedPtrClass(&rv2, &rv3));
  }

  EXPECT_TRUE(rv1);
  EXPECT_TRUE(rv2);
  EXPECT_TRUE(rv3);
}

class CountedClass final : public LinkedListElement<RefPtr<CountedClass>> {
 public:
  int mCount;
  void AddRef() { mCount++; }
  void Release() { mCount--; }

  CountedClass() : mCount(0) {}
  ~CountedClass() { EXPECT_TRUE(mCount == 0); }
};

TEST(LinkedList, AutoCleanLinkedListRefPtr)
{
  RefPtr<CountedClass> elt1 = new CountedClass;
  CountedClass* elt2 = new CountedClass;
  {
    AutoCleanLinkedList<RefPtr<CountedClass>> list;
    list.insertBack(elt1);
    list.insertBack(elt2);

    EXPECT_TRUE(elt1->mCount == 2);
    EXPECT_TRUE(elt2->mCount == 1);
  }

  EXPECT_TRUE(elt1->mCount == 1);
  EXPECT_TRUE(elt2->mCount == 0);
}
