/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <type_traits>

#include "gtest/gtest.h"
#include "mozilla/DbgMacro.h"
#include "mozilla/Unused.h"

using namespace mozilla;

#define TEST_MOZ_DBG_TYPE_IS(type_, expression_...)                    \
  static_assert(std::is_same_v<type_, decltype(MOZ_DBG(expression_))>, \
                "MOZ_DBG should return the indicated type")

#define TEST_MOZ_DBG_TYPE_SAME(expression_...)                                 \
  static_assert(                                                               \
      std::is_same_v<decltype((expression_)), decltype(MOZ_DBG(expression_))>, \
      "MOZ_DBG should return the same type")

struct Number {
  explicit Number(int aValue) : mValue(aValue) {}

  Number(const Number& aOther) = default;

  Number(Number&& aOther) : mValue(aOther.mValue) { aOther.mValue = 0; }

  Number& operator=(const Number& aOther) = default;

  Number& operator=(Number&& aOther) {
    mValue = aOther.mValue;
    aOther.mValue = 0;
    return *this;
  }

  ~Number() { mValue = -999; }

  int mValue;

  MOZ_DEFINE_DBG(Number, mValue)
};

struct MoveOnly {
  explicit MoveOnly(int aValue) : mValue(aValue) {}

  MoveOnly(const MoveOnly& aOther) = delete;

  MoveOnly(MoveOnly&& aOther) : mValue(aOther.mValue) { aOther.mValue = 0; }

  MoveOnly& operator=(MoveOnly& aOther) = default;

  MoveOnly& operator=(MoveOnly&& aOther) {
    mValue = aOther.mValue;
    aOther.mValue = 0;
    return *this;
  }

  int mValue;

  MOZ_DEFINE_DBG(MoveOnly)
};

void StaticAssertions() {
  int x = 123;
  Number y(123);
  Number z(234);
  MoveOnly w(456);

  // Static assertions.

  // lvalues
  TEST_MOZ_DBG_TYPE_SAME(x);        // int&
  TEST_MOZ_DBG_TYPE_SAME(y);        // Number&
  TEST_MOZ_DBG_TYPE_SAME(x = 234);  // int&
  TEST_MOZ_DBG_TYPE_SAME(y = z);    // Number&
  TEST_MOZ_DBG_TYPE_SAME(w);        // MoveOnly&

  // prvalues (which MOZ_DBG turns into xvalues by creating objects for them)
  TEST_MOZ_DBG_TYPE_IS(int&&, 123);
  TEST_MOZ_DBG_TYPE_IS(int&&, 1 + 2);
  TEST_MOZ_DBG_TYPE_IS(int*&&, &x);
  TEST_MOZ_DBG_TYPE_IS(int&&, x++);
  TEST_MOZ_DBG_TYPE_IS(Number&&, Number(123));
  TEST_MOZ_DBG_TYPE_IS(MoveOnly&&, MoveOnly(123));

  // xvalues
  TEST_MOZ_DBG_TYPE_SAME(std::move(y));  // int&&
  TEST_MOZ_DBG_TYPE_SAME(std::move(y));  // Number&&
  TEST_MOZ_DBG_TYPE_SAME(std::move(w));  // MoveOnly&

  Unused << x;
  Unused << y;
  Unused << z;
}

TEST(MozDbg, ObjectValues)
{
  // Test that moves and assignments all operate correctly with MOZ_DBG wrapped
  // around various parts of the expression.

  Number a(1);
  Number b(4);

  ASSERT_EQ(a.mValue, 1);

  MOZ_DBG(a.mValue);
  ASSERT_EQ(a.mValue, 1);

  MOZ_DBG(a.mValue + 1);
  ASSERT_EQ(a.mValue, 1);

  MOZ_DBG(a.mValue = 2);
  ASSERT_EQ(a.mValue, 2);

  MOZ_DBG(a).mValue = 3;
  ASSERT_EQ(a.mValue, 3);

  MOZ_DBG(a = b);
  ASSERT_EQ(a.mValue, 4);
  ASSERT_EQ(b.mValue, 4);

  b.mValue = 5;
  MOZ_DBG(a) = b;
  ASSERT_EQ(a.mValue, 5);
  ASSERT_EQ(b.mValue, 5);

  b.mValue = 6;
  MOZ_DBG(a = std::move(b));
  ASSERT_EQ(a.mValue, 6);
  ASSERT_EQ(b.mValue, 0);

  b.mValue = 7;
  MOZ_DBG(a) = std::move(b);
  ASSERT_EQ(a.mValue, 7);
  ASSERT_EQ(b.mValue, 0);

  b.mValue = 8;
  a = std::move(MOZ_DBG(b));
  ASSERT_EQ(a.mValue, 8);
  ASSERT_EQ(b.mValue, 0);

  a = MOZ_DBG(Number(9));
  ASSERT_EQ(a.mValue, 9);

  MoveOnly c(1);
  MoveOnly d(2);

  c = std::move(MOZ_DBG(d));
  ASSERT_EQ(c.mValue, 2);
  ASSERT_EQ(d.mValue, 0);

  c.mValue = 3;
  d.mValue = 4;
  c = MOZ_DBG(std::move(d));
  ASSERT_EQ(c.mValue, 4);
  ASSERT_EQ(d.mValue, 0);

  c.mValue = 5;
  d.mValue = 6;
  MOZ_DBG(c = std::move(d));
  ASSERT_EQ(c.mValue, 6);
  ASSERT_EQ(d.mValue, 0);

  c = MOZ_DBG(MoveOnly(7));
  ASSERT_EQ(c.mValue, 7);
}
