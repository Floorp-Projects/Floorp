/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLiteralString.h"
#include "nsTSubstringTuple.h"
#include "gtest/gtest.h"

namespace TestSubstringTuple {

static const auto kFooLiteral = u"foo"_ns;

static const auto kFoo = nsCString("foo");
static const auto kBar = nsCString("bar");
static const auto kBaz = nsCString("baz");

// The test must be done in a macro to ensure that tuple is always a temporary.
#define DO_SUBSTRING_TUPLE_TEST(tuple, dependentString, expectedLength, \
                                expectedDependency)                     \
  const auto length = (tuple).Length();                                 \
  const auto isDependentOn = (tuple).IsDependentOn(                     \
      dependentString.BeginReading(), dependentString.EndReading());    \
                                                                        \
  EXPECT_EQ((expectedLength), length);                                  \
  EXPECT_EQ((expectedDependency), isDependentOn);                       \
                                                                        \
  const auto [combinedIsDependentOn, combinedLength] =                  \
      (tuple).IsDependentOnWithLength(dependentString.BeginReading(),   \
                                      dependentString.EndReading());    \
                                                                        \
  EXPECT_EQ(length, combinedLength);                                    \
  EXPECT_EQ(isDependentOn, combinedIsDependentOn);

TEST(SubstringTuple, IsDependentOnAndLength_NonDependent_Literal_ZeroLength)
{ DO_SUBSTRING_TUPLE_TEST(u""_ns + u""_ns, kFooLiteral, 0u, false); }

TEST(SubstringTuple, IsDependentOnAndLength_NonDependent_Literal_NonZeroLength)
{ DO_SUBSTRING_TUPLE_TEST(u"bar"_ns + u"baz"_ns, kFooLiteral, 6u, false); }

TEST(SubstringTuple, IsDependentOnAndLength_NonDependent_NonZeroLength)
{ DO_SUBSTRING_TUPLE_TEST(kBar + kBaz, kFoo, 6u, false); }

TEST(SubstringTuple,
     IsDependentOnAndLength_NonDependent_NonZeroLength_ThreeParts)
{ DO_SUBSTRING_TUPLE_TEST(kBar + kBaz + kBar, kFoo, 9u, false); }

TEST(SubstringTuple, IsDependentOnAndLength_Dependent_NonZeroLength)
{ DO_SUBSTRING_TUPLE_TEST(kBar + kBaz, kBar, 6u, true); }

TEST(SubstringTuple, IsDependentOnAndLength_Dependent_NonZeroLength_ThreeParts)
{ DO_SUBSTRING_TUPLE_TEST(kBar + kBaz + kFoo, kBar, 9u, true); }

}  // namespace TestSubstringTuple
