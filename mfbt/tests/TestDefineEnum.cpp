/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DefineEnum.h"

// Sanity test for MOZ_DEFINE_ENUM.

MOZ_DEFINE_ENUM(TestEnum1, (EnumeratorA, EnumeratorB, EnumeratorC));

static_assert(EnumeratorA == 0, "Unexpected enumerator value");
static_assert(EnumeratorB == 1, "Unexpected enumerator value");
static_assert(EnumeratorC == 2, "Unexpected enumerator value");
static_assert(kHighestTestEnum1 == EnumeratorC, "Incorrect highest value");
static_assert(kTestEnum1Count == 3, "Incorrect enumerator count");

// Sanity test for MOZ_DEFINE_ENUM_CLASS.

MOZ_DEFINE_ENUM_CLASS(TestEnum2, (A, B, C));

static_assert(TestEnum2::A == TestEnum2(0), "Unexpected enumerator value");
static_assert(TestEnum2::B == TestEnum2(1), "Unexpected enumerator value");
static_assert(TestEnum2::C == TestEnum2(2), "Unexpected enumerator value");
static_assert(kHighestTestEnum2 == TestEnum2::C, "Incorrect highest value");
static_assert(kTestEnum2Count == 3, "Incorrect enumerator count");

// TODO: Test that the _WITH_BASE variants generate enumerators with the
//       correct underlying types. To do this, we need an |UnderlyingType|
//       type trait, which needs compiler support (recent versions of
//       compilers in the GCC family provide an |__underlying_type| builtin
//       for this purpose.

// Sanity test for MOZ_DEFINE_ENUM[_CLASS]_AT_CLASS_SCOPE.

struct TestClass {
  MOZ_DEFINE_ENUM_AT_CLASS_SCOPE(
    TestEnum3, (
      EnumeratorA,
      EnumeratorB,
      EnumeratorC
  ));

  MOZ_DEFINE_ENUM_CLASS_AT_CLASS_SCOPE(
    TestEnum4, (
      A,
      B,
      C
  ));

  static_assert(EnumeratorA == 0, "Unexpected enumerator value");
  static_assert(EnumeratorB == 1, "Unexpected enumerator value");
  static_assert(EnumeratorC == 2, "Unexpected enumerator value");
  static_assert(sHighestTestEnum3 == EnumeratorC, "Incorrect highest value");
  static_assert(sTestEnum3Count == 3, "Incorrect enumerator count");

  static_assert(TestEnum4::A == TestEnum4(0), "Unexpected enumerator value");
  static_assert(TestEnum4::B == TestEnum4(1), "Unexpected enumerator value");
  static_assert(TestEnum4::C == TestEnum4(2), "Unexpected enumerator value");
  static_assert(sHighestTestEnum4 == TestEnum4::C, "Incorrect highest value");
  static_assert(sTestEnum4Count == 3, "Incorrect enumerator count");
};


// Test that MOZ_DEFINE_ENUM doesn't allow giving enumerators initializers.

#ifdef CONFIRM_COMPILATION_ERRORS
MOZ_DEFINE_ENUM_CLASS(EnumWithInitializer1, (A = -1, B, C))
MOZ_DEFINE_ENUM_CLASS(EnumWithInitializer2, (A = 1, B, C))
MOZ_DEFINE_ENUM_CLASS(EnumWithInitializer3, (A, B = 6, C))
#endif

int
main()
{
  // Nothing to do here, all tests are static_asserts.
  return 0;
}
