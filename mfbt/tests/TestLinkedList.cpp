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
  explicit SomeClass(int aValue) : mValue(aValue) {}
  void incr() { ++mValue; }
};

struct PrivateClass : private LinkedListElement<PrivateClass> {
  friend class LinkedList<PrivateClass>;
  friend class LinkedListElement<PrivateClass>;
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
}

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

int
main()
{
  TestList();
  TestPrivate();
  return 0;
}
