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
  bool operator==(const SomeClass& other) const {
    return mValue == other.mValue;
  }
};

template <typename ListType, size_t N>
static void CheckListValues(ListType& list, unsigned int (&values)[N]) {
  size_t count = 0;
  for (auto& x : list) {
    MOZ_RELEASE_ASSERT(x.mValue == values[count]);
    ++count;
  }
  MOZ_RELEASE_ASSERT(count == N);
}

static void TestDoublyLinkedList() {
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
  {
    unsigned int check[]{1};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(list.contains(one));
  MOZ_RELEASE_ASSERT(!list.contains(two));
  MOZ_RELEASE_ASSERT(!list.contains(three));

  MOZ_RELEASE_ASSERT(!list.isEmpty());
  MOZ_RELEASE_ASSERT(list.begin()->mValue == 1);
  MOZ_RELEASE_ASSERT(!list.end());

  list.pushFront(&two);
  {
    unsigned int check[]{2, 1};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(list.begin()->mValue == 2);
  MOZ_RELEASE_ASSERT(!list.end());
  MOZ_RELEASE_ASSERT(!list.contains(three));

  list.pushBack(&three);
  {
    unsigned int check[]{2, 1, 3};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(list.begin()->mValue == 2);
  MOZ_RELEASE_ASSERT(!list.end());

  list.remove(&one);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  list.insertBefore(list.find(three), &one);
  {
    unsigned int check[]{2, 1, 3};
    CheckListValues(list, check);
  }

  list.remove(&three);
  {
    unsigned int check[]{2, 1};
    CheckListValues(list, check);
  }

  list.insertBefore(list.find(two), &three);
  {
    unsigned int check[]{3, 2, 1};
    CheckListValues(list, check);
  }

  list.remove(&three);
  {
    unsigned int check[]{2, 1};
    CheckListValues(list, check);
  }

  list.insertBefore(++list.find(two), &three);
  {
    unsigned int check[]{2, 3, 1};
    CheckListValues(list, check);
  }

  list.remove(&one);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  list.remove(&two);
  {
    unsigned int check[]{3};
    CheckListValues(list, check);
  }

  list.insertBefore(list.find(three), &two);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  list.remove(&three);
  {
    unsigned int check[]{2};
    CheckListValues(list, check);
  }

  list.remove(&two);
  MOZ_RELEASE_ASSERT(list.isEmpty());

  list.pushBack(&three);
  {
    unsigned int check[]{3};
    CheckListValues(list, check);
  }

  list.pushFront(&two);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

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

struct InTwoLists {
  explicit InTwoLists(unsigned int aValue) : mValue(aValue) {}
  DoublyLinkedListElement<InTwoLists> mListOne;
  DoublyLinkedListElement<InTwoLists> mListTwo;
  unsigned int mValue;

  struct GetListOneTrait {
    static DoublyLinkedListElement<InTwoLists>& Get(InTwoLists* aThis) {
      return aThis->mListOne;
    }
  };
};

namespace mozilla {

template <>
struct GetDoublyLinkedListElement<InTwoLists> {
  static DoublyLinkedListElement<InTwoLists>& Get(InTwoLists* aThis) {
    return aThis->mListTwo;
  }
};

}  // namespace mozilla

static void TestCustomAccessor() {
  DoublyLinkedList<InTwoLists, InTwoLists::GetListOneTrait> listOne;
  DoublyLinkedList<InTwoLists> listTwo;

  InTwoLists one(1);
  InTwoLists two(2);

  listOne.pushBack(&one);
  listOne.pushBack(&two);
  {
    unsigned int check[]{1, 2};
    CheckListValues(listOne, check);
  }

  listTwo.pushBack(&one);
  listTwo.pushBack(&two);
  {
    unsigned int check[]{1, 2};
    CheckListValues(listOne, check);
  }
  {
    unsigned int check[]{1, 2};
    CheckListValues(listTwo, check);
  }

  (void)listTwo.popBack();
  {
    unsigned int check[]{1, 2};
    CheckListValues(listOne, check);
  }
  {
    unsigned int check[]{1};
    CheckListValues(listTwo, check);
  }

  (void)listOne.popBack();
  {
    unsigned int check[]{1};
    CheckListValues(listOne, check);
  }
  {
    unsigned int check[]{1};
    CheckListValues(listTwo, check);
  }
}

static void TestSafeDoubleLinkedList() {
  mozilla::SafeDoublyLinkedList<SomeClass> list;
  auto* elt1 = new SomeClass(0);
  auto* elt2 = new SomeClass(0);
  auto* elt3 = new SomeClass(0);
  auto* elt4 = new SomeClass(0);
  list.pushBack(elt1);
  list.pushBack(elt2);
  list.pushBack(elt3);
  auto iter = list.begin();

  // basic tests for iterator validity
  MOZ_RELEASE_ASSERT(
      &*iter == elt1,
      "iterator returned by begin() must point to the first element!");
  MOZ_RELEASE_ASSERT(
      &*(iter.next()) == elt2,
      "iterator returned by begin() must have the second element as 'next'!");
  list.remove(elt2);
  MOZ_RELEASE_ASSERT(
      &*(iter.next()) == elt3,
      "After removal of the 2nd element 'next' must point to the 3rd element!");
  ++iter;
  MOZ_RELEASE_ASSERT(
      &*iter == elt3,
      "After advancing one step the current element must be the 3rd one!");
  MOZ_RELEASE_ASSERT(!iter.next(), "This is the last element of the list!");
  list.pushBack(elt4);
  MOZ_RELEASE_ASSERT(&*(iter.next()) == elt4,
                     "After adding an element at the end of the list the "
                     "iterator must be updated!");

  // advance to last element, then remove last element
  ++iter;
  list.popBack();
  MOZ_RELEASE_ASSERT(bool(iter) == false,
                     "After removing the last element, the iterator pointing "
                     "to the last element must be empty!");

  // iterate the whole remaining list, increment values
  for (auto& el : list) {
    el.incr();
  }
  MOZ_RELEASE_ASSERT(elt1->mValue == 1);
  MOZ_RELEASE_ASSERT(elt2->mValue == 0);
  MOZ_RELEASE_ASSERT(elt3->mValue == 1);
  MOZ_RELEASE_ASSERT(elt4->mValue == 0);

  // Removing the first element of the list while iterating must empty the
  // iterator
  for (auto it = list.begin(); it != list.end(); ++it) {
    MOZ_RELEASE_ASSERT(bool(it) == true, "The iterator must contain a value!");
    list.popFront();
    MOZ_RELEASE_ASSERT(
        bool(it) == false,
        "After removing the first element, the iterator must be empty!");
  }

  delete elt1;
  delete elt2;
  delete elt3;
  delete elt4;
}

int main() {
  TestDoublyLinkedList();
  TestCustomAccessor();
  TestSafeDoubleLinkedList();
  return 0;
}
