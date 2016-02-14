/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

using mozilla::detail::VectorTesting;
using mozilla::MakeUnique;
using mozilla::Move;
using mozilla::UniquePtr;
using mozilla::Vector;

struct mozilla::detail::VectorTesting
{
  static void testReserved();
  static void testConstRange();
  static void testEmplaceBack();
  static void testReverse();
};

void
mozilla::detail::VectorTesting::testReserved()
{
#ifdef DEBUG
  Vector<bool> bv;
  MOZ_RELEASE_ASSERT(bv.reserved() == 0);

  MOZ_RELEASE_ASSERT(bv.append(true));
  MOZ_RELEASE_ASSERT(bv.reserved() == 1);

  Vector<bool> otherbv;
  MOZ_RELEASE_ASSERT(otherbv.append(false));
  MOZ_RELEASE_ASSERT(otherbv.append(true));
  MOZ_RELEASE_ASSERT(bv.appendAll(otherbv));
  MOZ_RELEASE_ASSERT(bv.reserved() == 3);

  MOZ_RELEASE_ASSERT(bv.reserve(5));
  MOZ_RELEASE_ASSERT(bv.reserved() == 5);

  MOZ_RELEASE_ASSERT(bv.reserve(1));
  MOZ_RELEASE_ASSERT(bv.reserved() == 5);

  Vector<bool> bv2(Move(bv));
  MOZ_RELEASE_ASSERT(bv.reserved() == 0);
  MOZ_RELEASE_ASSERT(bv2.reserved() == 5);

  bv2.clearAndFree();
  MOZ_RELEASE_ASSERT(bv2.reserved() == 0);

  Vector<int, 42> iv;
  MOZ_RELEASE_ASSERT(iv.reserved() == 0);

  MOZ_RELEASE_ASSERT(iv.append(17));
  MOZ_RELEASE_ASSERT(iv.reserved() == 1);

  Vector<int, 42> otheriv;
  MOZ_RELEASE_ASSERT(otheriv.append(42));
  MOZ_RELEASE_ASSERT(otheriv.append(37));
  MOZ_RELEASE_ASSERT(iv.appendAll(otheriv));
  MOZ_RELEASE_ASSERT(iv.reserved() == 3);

  MOZ_RELEASE_ASSERT(iv.reserve(5));
  MOZ_RELEASE_ASSERT(iv.reserved() == 5);

  MOZ_RELEASE_ASSERT(iv.reserve(1));
  MOZ_RELEASE_ASSERT(iv.reserved() == 5);

  MOZ_RELEASE_ASSERT(iv.reserve(55));
  MOZ_RELEASE_ASSERT(iv.reserved() == 55);

  Vector<int, 42> iv2(Move(iv));
  MOZ_RELEASE_ASSERT(iv.reserved() == 0);
  MOZ_RELEASE_ASSERT(iv2.reserved() == 55);

  iv2.clearAndFree();
  MOZ_RELEASE_ASSERT(iv2.reserved() == 0);
#endif
}

void
mozilla::detail::VectorTesting::testConstRange()
{
#ifdef DEBUG
  Vector<int> vec;

  for (int i = 0; i < 10; i++) {
    MOZ_RELEASE_ASSERT(vec.append(i));
  }

  const auto &vecRef = vec;

  Vector<int>::ConstRange range = vecRef.all();
  for (int i = 0; i < 10; i++) {
    MOZ_RELEASE_ASSERT(!range.empty());
    MOZ_RELEASE_ASSERT(range.front() == i);
    range.popFront();
  }
#endif
}

namespace {

struct S
{
  size_t            j;
  UniquePtr<size_t> k;

  static size_t constructCount;
  static size_t moveCount;

  S(size_t j, size_t k)
    : j(j)
    , k(MakeUnique<size_t>(k))
  {
    constructCount++;
  }

  S(S&& rhs)
    : j(rhs.j)
    , k(Move(rhs.k))
  {
    rhs.~S();
    moveCount++;
  }

  S(const S&) = delete;
  S& operator=(const S&) = delete;
};

size_t S::constructCount = 0;
size_t S::moveCount = 0;

}

void
mozilla::detail::VectorTesting::testEmplaceBack()
{
  Vector<S> vec;
  MOZ_RELEASE_ASSERT(vec.reserve(20));

  for (size_t i = 0; i < 10; i++) {
    S s(i, i*i);
    MOZ_RELEASE_ASSERT(vec.append(Move(s)));
  }

  MOZ_RELEASE_ASSERT(vec.length() == 10);
  MOZ_RELEASE_ASSERT(S::constructCount == 10);
  MOZ_RELEASE_ASSERT(S::moveCount == 10);

  for (size_t i = 10; i < 20; i++) {
    MOZ_RELEASE_ASSERT(vec.emplaceBack(i, i*i));
  }

  MOZ_RELEASE_ASSERT(vec.length() == 20);
  MOZ_RELEASE_ASSERT(S::constructCount == 20);
  MOZ_RELEASE_ASSERT(S::moveCount == 10);

  for (size_t i = 0; i < 20; i++) {
    MOZ_RELEASE_ASSERT(vec[i].j == i);
    MOZ_RELEASE_ASSERT(*vec[i].k == i*i);
  }
}

void
mozilla::detail::VectorTesting::testReverse()
{
  // Use UniquePtr to make sure that reverse() can handler move-only types.
  Vector<UniquePtr<uint8_t>, 0> vec;

  // Reverse an odd number of elements.

  for (uint8_t i = 0; i < 5; i++) {
    auto p = MakeUnique<uint8_t>(i);
    MOZ_RELEASE_ASSERT(p);
    MOZ_RELEASE_ASSERT(vec.append(mozilla::Move(p)));
  }

  vec.reverse();

  MOZ_RELEASE_ASSERT(*vec[0] == 4);
  MOZ_RELEASE_ASSERT(*vec[1] == 3);
  MOZ_RELEASE_ASSERT(*vec[2] == 2);
  MOZ_RELEASE_ASSERT(*vec[3] == 1);
  MOZ_RELEASE_ASSERT(*vec[4] == 0);

  // Reverse an even number of elements.

  vec.popBack();
  vec.reverse();

  MOZ_RELEASE_ASSERT(*vec[0] == 1);
  MOZ_RELEASE_ASSERT(*vec[1] == 2);
  MOZ_RELEASE_ASSERT(*vec[2] == 3);
  MOZ_RELEASE_ASSERT(*vec[3] == 4);

  // Reverse an empty vector.

  vec.clear();
  MOZ_RELEASE_ASSERT(vec.length() == 0);
  vec.reverse();
  MOZ_RELEASE_ASSERT(vec.length() == 0);

  // Reverse a vector using only inline storage.

  Vector<UniquePtr<uint8_t>, 5> vec2;
  for (uint8_t i = 0; i < 5; i++) {
    auto p = MakeUnique<uint8_t>(i);
    MOZ_RELEASE_ASSERT(p);
    MOZ_RELEASE_ASSERT(vec2.append(mozilla::Move(p)));
  }

  vec2.reverse();

  MOZ_RELEASE_ASSERT(*vec2[0] == 4);
  MOZ_RELEASE_ASSERT(*vec2[1] == 3);
  MOZ_RELEASE_ASSERT(*vec2[2] == 2);
  MOZ_RELEASE_ASSERT(*vec2[3] == 1);
  MOZ_RELEASE_ASSERT(*vec2[4] == 0);
}

int
main()
{
  VectorTesting::testReserved();
  VectorTesting::testConstRange();
  VectorTesting::testEmplaceBack();
  VectorTesting::testReverse();
}
