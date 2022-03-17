/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "mozilla/ThreadSafeWeakPtr.h"

using mozilla::SupportsThreadSafeWeakPtr;
using mozilla::ThreadSafeWeakPtr;

// To have a class C support weak pointers, inherit from
// SupportsThreadSafeWeakPtr<C>.
class C : public SupportsThreadSafeWeakPtr<C> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(C)

  int mNum;

  C() : mNum(0) {}

  ~C() {
    // Setting mNum in the destructor allows us to test against use-after-free
    // below
    mNum = 0xDEAD;
  }

  void act() {}
};

// Test that declaring a ThreadSafeWeakPtr pointing to an incomplete type
// builds.
class Incomplete;
class D {
  ThreadSafeWeakPtr<Incomplete> mMember;
};

int main() {
  RefPtr<C> c1 = new C;
  MOZ_RELEASE_ASSERT(c1->mNum == 0);

  // Get weak pointers to c1. The first time,
  // a reference-counted ThreadSafeWeakReference object is created that
  // can live beyond the lifetime of 'c1'. The ThreadSafeWeakReference
  // object will be notified of 'c1's destruction.
  ThreadSafeWeakPtr<C> w1(c1);
  {
    RefPtr<C> s1(w1);
    // Test a weak pointer for validity before using it.
    MOZ_RELEASE_ASSERT(s1);
    MOZ_RELEASE_ASSERT(s1 == c1);
    s1->mNum = 1;
    s1->act();
  }

  // Test taking another ThreadSafeWeakPtr<C> to c1
  ThreadSafeWeakPtr<C> w2(c1);
  {
    RefPtr<C> s2(w2);
    MOZ_RELEASE_ASSERT(s2);
    MOZ_RELEASE_ASSERT(s2 == c1);
    MOZ_RELEASE_ASSERT(w1 == s2);
    MOZ_RELEASE_ASSERT(s2->mNum == 1);
  }

  // Test that when a ThreadSafeWeakPtr is destroyed, it does not destroy the
  // object that it points to, and it does not affect other ThreadSafeWeakPtrs
  // pointing to the same object (e.g. it does not destroy the
  // ThreadSafeWeakReference object).
  {
    ThreadSafeWeakPtr<C> w4local(c1);
    MOZ_RELEASE_ASSERT(w4local == c1);
  }
  // Now w4local has gone out of scope. If that had destroyed c1, then the
  // following would fail for sure (see C::~C()).
  MOZ_RELEASE_ASSERT(c1->mNum == 1);
  // Check that w4local going out of scope hasn't affected other
  // ThreadSafeWeakPtr's pointing to c1
  MOZ_RELEASE_ASSERT(w1 == c1);
  MOZ_RELEASE_ASSERT(w2 == c1);

  // Now construct another C object and test changing what object a
  // ThreadSafeWeakPtr points to
  RefPtr<C> c2 = new C;
  c2->mNum = 2;
  {
    RefPtr<C> s2(w2);
    MOZ_RELEASE_ASSERT(s2->mNum == 1);  // w2 was pointing to c1
  }
  w2 = c2;
  {
    RefPtr<C> s2(w2);
    MOZ_RELEASE_ASSERT(s2);
    MOZ_RELEASE_ASSERT(s2 == c2);
    MOZ_RELEASE_ASSERT(s2 != c1);
    MOZ_RELEASE_ASSERT(w1 != s2);
    MOZ_RELEASE_ASSERT(s2->mNum == 2);
  }

  // Destroying the underlying object clears weak pointers to it.
  // It should not affect pointers that are not currently pointing to it.
  c1 = nullptr;
  {
    RefPtr<C> s1(w1);
    MOZ_RELEASE_ASSERT(
        !s1, "Deleting an object should clear ThreadSafeWeakPtr's to it.");
    MOZ_RELEASE_ASSERT(w1.IsDead(), "The weak pointer is now dead");
    MOZ_RELEASE_ASSERT(!w1.IsNull(), "The weak pointer isn't null");

    RefPtr<C> s2(w2);
    MOZ_RELEASE_ASSERT(s2,
                       "Deleting an object should not clear ThreadSafeWeakPtr "
                       "that are not pointing to it.");
    MOZ_RELEASE_ASSERT(!w2.IsDead(), "The weak pointer isn't dead");
    MOZ_RELEASE_ASSERT(!w2.IsNull(), "The weak pointer isn't null");
  }

  c2 = nullptr;
  {
    RefPtr<C> s2(w2);
    MOZ_RELEASE_ASSERT(
        !s2, "Deleting an object should clear ThreadSafeWeakPtr's to it.");
    MOZ_RELEASE_ASSERT(w2.IsDead(), "The weak pointer is now dead");
    MOZ_RELEASE_ASSERT(!w2.IsNull(), "The weak pointer isn't null");
  }
}
