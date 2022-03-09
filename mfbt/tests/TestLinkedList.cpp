
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"

using mozilla::AutoCleanLinkedList;
using mozilla::LinkedList;
using mozilla::LinkedListElement;

struct SomeClass : public LinkedListElement<SomeClass> {
  unsigned int mValue;
  explicit SomeClass(int aValue = 0) : mValue(aValue) {}
  SomeClass(SomeClass&&) = default;
  SomeClass& operator=(SomeClass&&) = default;
  void incr() { ++mValue; }
};

template <size_t N>
static void CheckListValues(LinkedList<SomeClass>& list,
                            unsigned int (&values)[N]) {
  size_t count = 0;
  for (SomeClass* x : list) {
    MOZ_RELEASE_ASSERT(x->mValue == values[count]);
    ++count;
  }
  MOZ_RELEASE_ASSERT(count == N);
}

static void TestList() {
  LinkedList<SomeClass> list;

  SomeClass one(1), two(2), three(3);

  MOZ_RELEASE_ASSERT(list.isEmpty());
  MOZ_RELEASE_ASSERT(list.length() == 0);
  MOZ_RELEASE_ASSERT(!list.getFirst());
  MOZ_RELEASE_ASSERT(!list.getLast());
  MOZ_RELEASE_ASSERT(!list.popFirst());
  MOZ_RELEASE_ASSERT(!list.popLast());

  for (SomeClass* x : list) {
    MOZ_RELEASE_ASSERT(x);
    MOZ_RELEASE_ASSERT(false);
  }

  list.insertFront(&one);
  {
    unsigned int check[]{1};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(one.isInList());
  MOZ_RELEASE_ASSERT(!two.isInList());
  MOZ_RELEASE_ASSERT(!three.isInList());

  MOZ_RELEASE_ASSERT(list.contains(&one));
  MOZ_RELEASE_ASSERT(!list.contains(&two));
  MOZ_RELEASE_ASSERT(!list.contains(&three));

  MOZ_RELEASE_ASSERT(!list.isEmpty());
  MOZ_RELEASE_ASSERT(list.length() == 1);
  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 1);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 1);

  list.insertFront(&two);
  {
    unsigned int check[]{2, 1};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(list.length() == 2);
  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 2);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 1);

  list.insertBack(&three);
  {
    unsigned int check[]{2, 1, 3};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(list.length() == 3);
  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 2);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 3);

  one.removeFrom(list);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  three.setPrevious(&one);
  {
    unsigned int check[]{2, 1, 3};
    CheckListValues(list, check);
  }

  three.removeFrom(list);
  {
    unsigned int check[]{2, 1};
    CheckListValues(list, check);
  }

  two.setPrevious(&three);
  {
    unsigned int check[]{3, 2, 1};
    CheckListValues(list, check);
  }

  three.removeFrom(list);
  {
    unsigned int check[]{2, 1};
    CheckListValues(list, check);
  }

  two.setNext(&three);
  {
    unsigned int check[]{2, 3, 1};
    CheckListValues(list, check);
  }

  one.remove();
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  two.remove();
  {
    unsigned int check[]{3};
    CheckListValues(list, check);
  }

  three.setPrevious(&two);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  three.remove();
  {
    unsigned int check[]{2};
    CheckListValues(list, check);
  }

  two.remove();

  list.insertBack(&three);
  {
    unsigned int check[]{3};
    CheckListValues(list, check);
  }

  list.insertFront(&two);
  {
    unsigned int check[]{2, 3};
    CheckListValues(list, check);
  }

  for (SomeClass* x : list) {
    x->incr();
  }

  MOZ_RELEASE_ASSERT(list.length() == 2);
  MOZ_RELEASE_ASSERT(list.getFirst() == &two);
  MOZ_RELEASE_ASSERT(list.getLast() == &three);
  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 3);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 4);

  const LinkedList<SomeClass>& constList = list;
  for (const SomeClass* x : constList) {
    MOZ_RELEASE_ASSERT(x);
  }
}

static void TestExtendLists() {
  AutoCleanLinkedList<SomeClass> list1, list2;

  constexpr unsigned int N = 5;
  for (unsigned int i = 0; i < N; ++i) {
    list1.insertBack(new SomeClass(static_cast<int>(i)));

    AutoCleanLinkedList<SomeClass> singleItemList;
    singleItemList.insertFront(new SomeClass(static_cast<int>(i + N)));
    list2.extendBack(std::move(singleItemList));
  }
  // list1 = { 0, 1, 2, 3, 4 }
  // list2 = { 5, 6, 7, 8, 9 }

  list1.extendBack(AutoCleanLinkedList<SomeClass>());
  list1.extendBack(std::move(list2));

  // Make sure the line above has properly emptied |list2|.
  MOZ_RELEASE_ASSERT(list2.isEmpty());  // NOLINT(bugprone-use-after-move)

  size_t i = 0;
  for (SomeClass* x : list1) {
    MOZ_RELEASE_ASSERT(x->mValue == i++);
  }
  MOZ_RELEASE_ASSERT(i == N * 2);
}

void TestSplice() {
  AutoCleanLinkedList<SomeClass> list1, list2;
  for (unsigned int i = 1; i <= 5; ++i) {
    list1.insertBack(new SomeClass(static_cast<int>(i)));

    AutoCleanLinkedList<SomeClass> singleItemList;
    singleItemList.insertFront(new SomeClass(static_cast<int>(i * 10)));
    list2.extendBack(std::move(singleItemList));
  }
  // list1 = { 1, 2, 3, 4, 5 }
  // list2 = { 10, 20, 30, 40, 50 }

  list1.splice(2, list2, 0, 5);

  MOZ_RELEASE_ASSERT(list2.isEmpty());
  unsigned int kExpected1[]{1, 2, 10, 20, 30, 40, 50, 3, 4, 5};
  CheckListValues(list1, kExpected1);

  // Since aSourceLen=100 exceeds list1's end, the function transfers
  // three items [3, 4, 5].
  list2.splice(0, list1, 7, 100);

  unsigned int kExpected2[]{1, 2, 10, 20, 30, 40, 50};
  unsigned int kExpected3[]{3, 4, 5};
  CheckListValues(list1, kExpected2);
  CheckListValues(list2, kExpected3);

  // Since aDestinationPos=100 exceeds list2's end, the function transfers
  // items to list2's end.
  list2.splice(100, list1, 1, 1);

  unsigned int kExpected4[]{1, 10, 20, 30, 40, 50};
  unsigned int kExpected5[]{3, 4, 5, 2};
  CheckListValues(list1, kExpected4);
  CheckListValues(list2, kExpected5);
}

static void TestMove() {
  auto MakeSomeClass = [](unsigned int aValue) -> SomeClass {
    return SomeClass(aValue);
  };

  LinkedList<SomeClass> list1;

  // Test move constructor for LinkedListElement.
  SomeClass c1(MakeSomeClass(1));
  list1.insertBack(&c1);

  // Test move assignment for LinkedListElement from an element not in a
  // list.
  SomeClass c2;
  c2 = MakeSomeClass(2);
  list1.insertBack(&c2);

  // Test move assignment of LinkedListElement from an element already in a
  // list.
  SomeClass c3;
  c3 = std::move(c2);
  MOZ_RELEASE_ASSERT(!c2.isInList());
  MOZ_RELEASE_ASSERT(c3.isInList());

  // Test move constructor for LinkedList.
  LinkedList<SomeClass> list2(std::move(list1));
  {
    unsigned int check[]{1, 2};
    CheckListValues(list2, check);
  }
  MOZ_RELEASE_ASSERT(list1.isEmpty());

  // Test move assignment for LinkedList.
  LinkedList<SomeClass> list3;
  list3 = std::move(list2);
  {
    unsigned int check[]{1, 2};
    CheckListValues(list3, check);
  }
  MOZ_RELEASE_ASSERT(list2.isEmpty());

  list3.clear();
}

static void TestRemoveAndGet() {
  LinkedList<SomeClass> list;

  SomeClass one(1), two(2), three(3);
  list.insertBack(&one);
  list.insertBack(&two);
  list.insertBack(&three);
  {
    unsigned int check[]{1, 2, 3};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(two.removeAndGetNext() == &three);
  {
    unsigned int check[]{1, 3};
    CheckListValues(list, check);
  }

  MOZ_RELEASE_ASSERT(three.removeAndGetPrevious() == &one);
  {
    unsigned int check[]{1};
    CheckListValues(list, check);
  }
}

struct PrivateClass : private LinkedListElement<PrivateClass> {
  friend class mozilla::LinkedList<PrivateClass>;
  friend class mozilla::LinkedListElement<PrivateClass>;
};

static void TestPrivate() {
  LinkedList<PrivateClass> list;
  PrivateClass one, two;
  list.insertBack(&one);
  list.insertBack(&two);

  size_t count = 0;
  for (PrivateClass* p : list) {
    MOZ_RELEASE_ASSERT(p, "cannot have null elements in list");
    count++;
  }
  MOZ_RELEASE_ASSERT(count == 2);
}

struct CountedClass : public LinkedListElement<RefPtr<CountedClass>> {
  int mCount;
  void AddRef() { mCount++; }
  void Release() { mCount--; }

  CountedClass() : mCount(0) {}
  ~CountedClass() { MOZ_RELEASE_ASSERT(mCount == 0); }
};

static void TestRefPtrList() {
  LinkedList<RefPtr<CountedClass>> list;
  CountedClass* elt1 = new CountedClass;
  CountedClass* elt2 = new CountedClass;

  list.insertBack(elt1);
  list.insertBack(elt2);

  MOZ_RELEASE_ASSERT(elt1->mCount == 1);
  MOZ_RELEASE_ASSERT(elt2->mCount == 1);

  for (RefPtr<CountedClass> p : list) {
    MOZ_RELEASE_ASSERT(p->mCount == 2);
  }

  RefPtr<CountedClass> ptr = list.getFirst();
  while (ptr) {
    MOZ_RELEASE_ASSERT(ptr->mCount == 2);
    RefPtr<CountedClass> next = ptr->getNext();
    ptr->remove();
    ptr = std::move(next);
  }
  ptr = nullptr;

  MOZ_RELEASE_ASSERT(elt1->mCount == 0);
  MOZ_RELEASE_ASSERT(elt2->mCount == 0);

  list.insertBack(elt1);
  elt1->setNext(elt2);

  MOZ_RELEASE_ASSERT(elt1->mCount == 1);
  MOZ_RELEASE_ASSERT(elt2->mCount == 1);

  RefPtr<CountedClass> first = list.popFirst();

  MOZ_RELEASE_ASSERT(elt1->mCount == 1);
  MOZ_RELEASE_ASSERT(elt2->mCount == 1);

  RefPtr<CountedClass> second = list.popFirst();

  MOZ_RELEASE_ASSERT(elt1->mCount == 1);
  MOZ_RELEASE_ASSERT(elt2->mCount == 1);

  first = second = nullptr;

  delete elt1;
  delete elt2;
}

int main() {
  TestList();
  TestExtendLists();
  TestSplice();
  TestPrivate();
  TestMove();
  TestRemoveAndGet();
  TestRefPtrList();
  return 0;
}
