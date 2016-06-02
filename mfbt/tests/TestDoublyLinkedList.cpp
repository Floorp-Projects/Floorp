/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/DoublyLinkedList.h"

using mozilla::DoublyLinkedList;
using mozilla::DoublyLinkedListElement;

struct SomeClass : public DoublyLinkedListElement<SomeClass> {
  unsigned int mValue;
  explicit SomeClass(int aValue) : mValue(aValue) {}
  void incr() { ++mValue; }
  bool operator==(const SomeClass& other) { return mValue == other.mValue; }
};

template <typename ListType, size_t N>
static void
CheckListValues(ListType& list, unsigned int (&values)[N])
{
  size_t count = 0;
  for (auto& x : list) {
    MOZ_RELEASE_ASSERT(x.mValue == values[count]);
    ++count;
  }
  MOZ_RELEASE_ASSERT(count == N);
}

static void
TestDoublyLinkedList()
{
  DoublyLinkedList<SomeClass> list;

  SomeClass one(1), two(2), three(3);

  MOZ_RELEASE_ASSERT(list.isEmpty());
  MOZ_RELEASE_ASSERT(!list.begin());
  MOZ_RELEASE_ASSERT(!list.end());

  for (SomeClass& x : list) {
    MOZ_RELEASE_ASSERT(x.mValue);
    MOZ_RELEASE_ASSERT(false);
  }

  list.pushFront(&one);
  { unsigned int check[] { 1 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(list.contains(one));
  MOZ_RELEASE_ASSERT(!list.contains(two));
  MOZ_RELEASE_ASSERT(!list.contains(three));

  MOZ_RELEASE_ASSERT(!list.isEmpty());
  MOZ_RELEASE_ASSERT(list.begin()->mValue == 1);
  MOZ_RELEASE_ASSERT(!list.end());

  list.pushFront(&two);
  { unsigned int check[] { 2, 1 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(list.begin()->mValue == 2);
  MOZ_RELEASE_ASSERT(!list.end());
  MOZ_RELEASE_ASSERT(!list.contains(three));

  list.pushBack(&three);
  { unsigned int check[] { 2, 1, 3 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(list.begin()->mValue == 2);
  MOZ_RELEASE_ASSERT(!list.end());

  list.remove(&one);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  list.insertBefore(list.find(three), &one);
  { unsigned int check[] { 2, 1, 3 }; CheckListValues(list, check); }

  list.remove(&three);
  { unsigned int check[] { 2, 1 }; CheckListValues(list, check); }

  list.insertBefore(list.find(two), &three);
  { unsigned int check[] { 3, 2, 1 }; CheckListValues(list, check); }

  list.remove(&three);
  { unsigned int check[] { 2, 1 }; CheckListValues(list, check); }

  list.insertBefore(++list.find(two), &three);
  { unsigned int check[] { 2, 3, 1 }; CheckListValues(list, check); }

  list.remove(&one);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  list.remove(&two);
  { unsigned int check[] { 3 }; CheckListValues(list, check); }

  list.insertBefore(list.find(three), &two);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  list.remove(&three);
  { unsigned int check[] { 2 }; CheckListValues(list, check); }

  list.remove(&two);
  MOZ_RELEASE_ASSERT(list.isEmpty());

  list.pushBack(&three);
  { unsigned int check[] { 3 }; CheckListValues(list, check); }

  list.pushFront(&two);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  // This should modify the values of |two| and |three| as pointers to them are
  // stored in the list, not copies.
  for (SomeClass& x : list) {
    x.incr();
  }

  MOZ_RELEASE_ASSERT(*list.begin() == two);
  MOZ_RELEASE_ASSERT(*++list.begin() == three);

  SomeClass four(4);
  MOZ_RELEASE_ASSERT(++list.begin() == list.find(four));
}

static void
TestCustomAccessor()
{
  struct InTwoLists {
    explicit InTwoLists(unsigned int aValue) : mValue(aValue) {}
    DoublyLinkedListElement<InTwoLists> mListOne;
    DoublyLinkedListElement<InTwoLists> mListTwo;
    unsigned int mValue;
  };

  struct ListOneSiblingAccess {
    static void SetNext(InTwoLists* aElm, InTwoLists* aNext) { aElm->mListOne.mNext = aNext; }
    static InTwoLists* GetNext(InTwoLists* aElm) { return aElm->mListOne.mNext; }
    static void SetPrev(InTwoLists* aElm, InTwoLists* aPrev) { aElm->mListOne.mPrev = aPrev; }
    static InTwoLists* GetPrev(InTwoLists* aElm) { return aElm->mListOne.mPrev; }
  };

  struct ListTwoSiblingAccess {
    static void SetNext(InTwoLists* aElm, InTwoLists* aNext) { aElm->mListTwo.mNext = aNext; }
    static InTwoLists* GetNext(InTwoLists* aElm) { return aElm->mListTwo.mNext; }
    static void SetPrev(InTwoLists* aElm, InTwoLists* aPrev) { aElm->mListTwo.mPrev = aPrev; }
    static InTwoLists* GetPrev(InTwoLists* aElm) { return aElm->mListTwo.mPrev; }
  };

  DoublyLinkedList<InTwoLists, ListOneSiblingAccess> listOne;
  DoublyLinkedList<InTwoLists, ListTwoSiblingAccess> listTwo;

  InTwoLists one(1);
  InTwoLists two(2);

  listOne.pushBack(&one);
  listOne.pushBack(&two);
  { unsigned int check[] { 1, 2 }; CheckListValues(listOne, check); }

  listTwo.pushBack(&one);
  listTwo.pushBack(&two);
  { unsigned int check[] { 1, 2 }; CheckListValues(listTwo, check); }

  (void)listTwo.popBack();
  { unsigned int check[] { 1, 2 }; CheckListValues(listOne, check); }
  { unsigned int check[] { 1 }; CheckListValues(listTwo, check); }
}

int
main()
{
  TestDoublyLinkedList();
  TestCustomAccessor();
  return 0;
}
