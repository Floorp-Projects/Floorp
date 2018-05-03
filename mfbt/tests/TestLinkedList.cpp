/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/LinkedList.h"

using mozilla::LinkedList;
using mozilla::LinkedListElement;

struct SomeClass : public LinkedListElement<SomeClass> {
  unsigned int mValue;
  explicit SomeClass(int aValue = 0) : mValue(aValue) {}
  void incr() { ++mValue; }
};

template <size_t N>
static void
CheckListValues(LinkedList<SomeClass>& list, unsigned int (&values)[N])
{
  size_t count = 0;
  for (SomeClass* x : list) {
    MOZ_RELEASE_ASSERT(x->mValue == values[count]);
    ++count;
  }
  MOZ_RELEASE_ASSERT(count == N);
}

static void
TestList()
{
  LinkedList<SomeClass> list;

  SomeClass one(1), two(2), three(3);

  MOZ_RELEASE_ASSERT(list.isEmpty());
  MOZ_RELEASE_ASSERT(!list.getFirst());
  MOZ_RELEASE_ASSERT(!list.getLast());
  MOZ_RELEASE_ASSERT(!list.popFirst());
  MOZ_RELEASE_ASSERT(!list.popLast());

  for (SomeClass* x : list) {
    MOZ_RELEASE_ASSERT(x);
    MOZ_RELEASE_ASSERT(false);
  }

  list.insertFront(&one);
  { unsigned int check[] { 1 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(one.isInList());
  MOZ_RELEASE_ASSERT(!two.isInList());
  MOZ_RELEASE_ASSERT(!three.isInList());

  MOZ_RELEASE_ASSERT(!list.isEmpty());
  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 1);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 1);

  list.insertFront(&two);
  { unsigned int check[] { 2, 1 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 2);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 1);

  list.insertBack(&three);
  { unsigned int check[] { 2, 1, 3 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 2);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 3);

  one.removeFrom(list);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  three.setPrevious(&one);
  { unsigned int check[] { 2, 1, 3 }; CheckListValues(list, check); }

  three.removeFrom(list);
  { unsigned int check[] { 2, 1 }; CheckListValues(list, check); }

  two.setPrevious(&three);
  { unsigned int check[] { 3, 2, 1 }; CheckListValues(list, check); }

  three.removeFrom(list);
  { unsigned int check[] { 2, 1 }; CheckListValues(list, check); }

  two.setNext(&three);
  { unsigned int check[] { 2, 3, 1 }; CheckListValues(list, check); }

  one.remove();
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  two.remove();
  { unsigned int check[] { 3 }; CheckListValues(list, check); }

  three.setPrevious(&two);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  three.remove();
  { unsigned int check[] { 2 }; CheckListValues(list, check); }

  two.remove();

  list.insertBack(&three);
  { unsigned int check[] { 3 }; CheckListValues(list, check); }

  list.insertFront(&two);
  { unsigned int check[] { 2, 3 }; CheckListValues(list, check); }

  for (SomeClass* x : list) {
    x->incr();
  }

  MOZ_RELEASE_ASSERT(list.getFirst() == &two);
  MOZ_RELEASE_ASSERT(list.getLast() == &three);
  MOZ_RELEASE_ASSERT(list.getFirst()->mValue == 3);
  MOZ_RELEASE_ASSERT(list.getLast()->mValue == 4);

  const LinkedList<SomeClass>& constList = list;
  for (const SomeClass* x : constList) {
    MOZ_RELEASE_ASSERT(x);
  }
}

static void
TestMove()
{
  auto MakeSomeClass =
    [] (unsigned int aValue) -> SomeClass { return SomeClass(aValue); };

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
  c3 = Move(c2);
  MOZ_RELEASE_ASSERT(!c2.isInList());
  MOZ_RELEASE_ASSERT(c3.isInList());

  // Test move constructor for LinkedList.
  LinkedList<SomeClass> list2(Move(list1));
  { unsigned int check[] { 1, 2 }; CheckListValues(list2, check); }
  MOZ_RELEASE_ASSERT(list1.isEmpty());

  // Test move assignment for LinkedList.
  LinkedList<SomeClass> list3;
  list3 = Move(list2);
  { unsigned int check[] { 1, 2 }; CheckListValues(list3, check); }
  MOZ_RELEASE_ASSERT(list2.isEmpty());

  list3.clear();
}

static void
TestRemoveAndGet()
{
  LinkedList<SomeClass> list;

  SomeClass one(1), two(2), three(3);
  list.insertBack(&one);
  list.insertBack(&two);
  list.insertBack(&three);
  { unsigned int check[] { 1, 2, 3 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(two.removeAndGetNext() == &three);
  { unsigned int check[] { 1, 3 }; CheckListValues(list, check); }

  MOZ_RELEASE_ASSERT(three.removeAndGetPrevious() == &one);
  { unsigned int check[] { 1 }; CheckListValues(list, check); }
}

struct PrivateClass : private LinkedListElement<PrivateClass> {
  friend class mozilla::LinkedList<PrivateClass>;
  friend class mozilla::LinkedListElement<PrivateClass>;
};

static void
TestPrivate()
{
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

struct CountedClass : public LinkedListElement<RefPtr<CountedClass>>
{
  int mCount;
  void AddRef() { mCount++; }
  void Release() { mCount--; }

  CountedClass() : mCount(0) {}
  ~CountedClass() { MOZ_RELEASE_ASSERT(mCount == 0); }
};

static void
TestRefPtrList()
{
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
    ptr = Move(next);
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

int
main()
{
  TestList();
  TestPrivate();
  TestMove();
  TestRemoveAndGet();
  TestRefPtrList();
  return 0;
}
