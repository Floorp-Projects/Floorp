/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/NonDereferenceable.h"

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"

using mozilla::Move;
using mozilla::NonDereferenceable;

#define CHECK MOZ_RELEASE_ASSERT

void
TestNonDereferenceableSimple()
{
  // Default construction.
  NonDereferenceable<int> nd0;
  CHECK(!nd0);
  CHECK(!nd0.value());

  int i = 1;
  int i2 = 2;

  // Construction with pointer.
  NonDereferenceable<int> nd1(&i);
  CHECK(!!nd1);
  CHECK(nd1.value() == reinterpret_cast<uintptr_t>(&i));

  // Assignment with pointer.
  nd1 = &i2;
  CHECK(nd1.value() == reinterpret_cast<uintptr_t>(&i2));

  // Copy-construction.
  NonDereferenceable<int> nd2(nd1);
  CHECK(nd2.value() == reinterpret_cast<uintptr_t>(&i2));

  // Copy-assignment.
  nd2 = nd0;
  CHECK(!nd2.value());

  // Move-construction.
  NonDereferenceable<int> nd3{ NonDereferenceable<int>(&i) };
  CHECK(nd3.value() == reinterpret_cast<uintptr_t>(&i));

  // Move-assignment.
  nd3 = Move(nd1);
  CHECK(nd3.value() == reinterpret_cast<uintptr_t>(&i2));
  // Note: Not testing nd1's value because we don't want to assume what state
  // it is left in after move. But at least it should be reusable:
  nd1 = &i;
  CHECK(nd1.value() == reinterpret_cast<uintptr_t>(&i));
}

void
TestNonDereferenceableHierarchy()
{
  struct Base1
  {
    // Member variable, to make sure Base1 is not empty.
    int x1;
  };
  struct Base2
  {
    int x2;
  };
  struct Derived
    : Base1
    , Base2
  {
  };

  Derived d;

  // Construct NonDereferenceable from raw pointer.
  NonDereferenceable<Derived> ndd = NonDereferenceable<Derived>(&d);
  CHECK(ndd);
  CHECK(ndd.value() == reinterpret_cast<uintptr_t>(&d));

  // Cast Derived to Base1.
  NonDereferenceable<Base1> ndb1 = ndd;
  CHECK(ndb1);
  CHECK(ndb1.value() == reinterpret_cast<uintptr_t>(static_cast<Base1*>(&d)));

  // Cast Base1 back to Derived.
  NonDereferenceable<Derived> nddb1 = ndb1;
  CHECK(nddb1.value() == reinterpret_cast<uintptr_t>(&d));

  // Cast Derived to Base2.
  NonDereferenceable<Base2> ndb2 = ndd;
  CHECK(ndb2);
  CHECK(ndb2.value() == reinterpret_cast<uintptr_t>(static_cast<Base2*>(&d)));
  // Sanity check that Base2 should be offset from the start of Derived.
  CHECK(ndb2.value() != ndd.value());

  // Cast Base2 back to Derived.
  NonDereferenceable<Derived> nddb2 = ndb2;
  CHECK(nddb2.value() == reinterpret_cast<uintptr_t>(&d));

  // Note that it's not possible to jump between bases, as they're not obviously
  // related, i.e.: `NonDereferenceable<Base2> ndb22 = ndb1;` doesn't compile.
  // However it's possible to explicitly navigate through the derived object:
  NonDereferenceable<Base2> ndb22 = NonDereferenceable<Derived>(ndb1);
  CHECK(ndb22.value() == reinterpret_cast<uintptr_t>(static_cast<Base2*>(&d)));

  // Handling nullptr; should stay nullptr even for offset bases.
  ndd = nullptr;
  CHECK(!ndd);
  CHECK(!ndd.value());
  ndb1 = ndd;
  CHECK(!ndb1);
  CHECK(!ndb1.value());
  ndb2 = ndd;
  CHECK(!ndb2);
  CHECK(!ndb2.value());
  nddb2 = ndb2;
  CHECK(!nddb2);
  CHECK(!nddb2.value());
}

template<typename T, size_t Index>
struct CRTPBase
{
  // Convert `this` from `CRTPBase*` to `T*` while construction is still in
  // progress; normally UBSan -fsanitize=vptr would catch this, but using
  // NonDereferenceable should keep UBSan happy.
  CRTPBase()
    : mDerived(this)
  {
  }
  NonDereferenceable<T> mDerived;
};

void
TestNonDereferenceableCRTP()
{
  struct Derived
    : CRTPBase<Derived, 1>
    , CRTPBase<Derived, 2>
  {
  };
  using Base1 = Derived::CRTPBase<Derived, 1>;
  using Base2 = Derived::CRTPBase<Derived, 2>;

  Derived d;
  // Verify that base constructors have correctly captured the address of the
  // (at the time still incomplete) derived object.
  CHECK(d.Base1::mDerived.value() == reinterpret_cast<uintptr_t>(&d));
  CHECK(d.Base2::mDerived.value() == reinterpret_cast<uintptr_t>(&d));

  // Construct NonDereferenceable from raw pointer.
  NonDereferenceable<Derived> ndd = NonDereferenceable<Derived>(&d);
  CHECK(ndd);
  CHECK(ndd.value() == reinterpret_cast<uintptr_t>(&d));

  // Cast Derived to Base1.
  NonDereferenceable<Base1> ndb1 = ndd;
  CHECK(ndb1);
  CHECK(ndb1.value() == reinterpret_cast<uintptr_t>(static_cast<Base1*>(&d)));

  // Cast Base1 back to Derived.
  NonDereferenceable<Derived> nddb1 = ndb1;
  CHECK(nddb1.value() == reinterpret_cast<uintptr_t>(&d));

  // Cast Derived to Base2.
  NonDereferenceable<Base2> ndb2 = ndd;
  CHECK(ndb2);
  CHECK(ndb2.value() == reinterpret_cast<uintptr_t>(static_cast<Base2*>(&d)));
  // Sanity check that Base2 should be offset from the start of Derived.
  CHECK(ndb2.value() != ndd.value());

  // Cast Base2 back to Derived.
  NonDereferenceable<Derived> nddb2 = ndb2;
  CHECK(nddb2.value() == reinterpret_cast<uintptr_t>(&d));

  // Note that it's not possible to jump between bases, as they're not obviously
  // related, i.e.: `NonDereferenceable<Base2> ndb22 = ndb1;` doesn't compile.
  // However it's possible to explicitly navigate through the derived object:
  NonDereferenceable<Base2> ndb22 = NonDereferenceable<Derived>(ndb1);
  CHECK(ndb22.value() == reinterpret_cast<uintptr_t>(static_cast<Base2*>(&d)));
}

int
main()
{
  TestNonDereferenceableSimple();
  TestNonDereferenceableHierarchy();
  TestNonDereferenceableCRTP();

  return 0;
}
