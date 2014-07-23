/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WeakPtr.h"

using mozilla::SupportsWeakPtr;
using mozilla::WeakPtr;

// To have a class C support weak pointers, inherit from SupportsWeakPtr<C>.
class C : public SupportsWeakPtr<C>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(C)
  int mNum;
  void act() {}
};

static void
Example()
{
  C* ptr = new C();

  // Get weak pointers to ptr. The first time asWeakPtr is called
  // a reference counted WeakReference object is created that
  // can live beyond the lifetime of 'ptr'. The WeakReference
  // object will be notified of 'ptr's destruction.
  WeakPtr<C> weak = ptr->asWeakPtr();
  WeakPtr<C> other = ptr->asWeakPtr();

  // Test a weak pointer for validity before using it.
  if (weak) {
    weak->mNum = 17;
    weak->act();
  }

  // Destroying the underlying object clears weak pointers to it.
  delete ptr;

  MOZ_RELEASE_ASSERT(!weak, "Deleting |ptr| clears weak pointers to it.");
  MOZ_RELEASE_ASSERT(!other, "Deleting |ptr| clears all weak pointers to it.");
}

struct A : public SupportsWeakPtr<A>
{
  MOZ_DECLARE_REFCOUNTED_TYPENAME(A)
  int mData;
};

int
main()
{
  A* a = new A;

  // a2 is unused to test the case when we haven't initialized
  // the internal WeakReference pointer.
  A* a2 = new A;

  a->mData = 5;
  WeakPtr<A> ptr = a->asWeakPtr();
  {
    WeakPtr<A> ptr2 = a->asWeakPtr();
    MOZ_RELEASE_ASSERT(ptr->mData == 5);
    WeakPtr<A> ptr3 = a->asWeakPtr();
    MOZ_RELEASE_ASSERT(ptr->mData == 5);
  }

  delete a;
  MOZ_RELEASE_ASSERT(!ptr);

  delete a2;

  Example();

  return 0;
}
