/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "UTFStrings.h"
#include "nsUnicharUtils.h"
#include "mozilla/HashFunctions.h"

#include "gtest/gtest.h"

using namespace mozilla;

namespace TestUTF {

TEST(UTF, Valid)
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentCString str8(ValidStrings[i].m8);
    nsDependentString str16(ValidStrings[i].m16);

    EXPECT_TRUE(NS_ConvertUTF16toUTF8(str16).Equals(str8));

    EXPECT_TRUE(NS_ConvertUTF8toUTF16(str8).Equals(str16));

    nsCString tmp8("string ");
    AppendUTF16toUTF8(str16, tmp8);
    EXPECT_TRUE(tmp8.Equals(NS_LITERAL_CSTRING("string ") + str8));

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    EXPECT_TRUE(tmp16.Equals(NS_LITERAL_STRING("string ") + str16));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Invalid16)
{
  for (unsigned int i = 0; i < ArrayLength(Invalid16Strings); ++i) {
    nsDependentString str16(Invalid16Strings[i].m16);
    nsDependentCString str8(Invalid16Strings[i].m8);

    EXPECT_TRUE(NS_ConvertUTF16toUTF8(str16).Equals(str8));

    nsCString tmp8("string ");
    AppendUTF16toUTF8(str16, tmp8);
    EXPECT_TRUE(tmp8.Equals(NS_LITERAL_CSTRING("string ") + str8));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Invalid8)
{
  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsDependentString str16(Invalid8Strings[i].m16);
    nsDependentCString str8(Invalid8Strings[i].m8);

    EXPECT_TRUE(NS_ConvertUTF8toUTF16(str8).Equals(str16));

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    EXPECT_TRUE(tmp16.Equals(NS_LITERAL_STRING("string ") + str16));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Malformed8)
{
// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsDependentCString str8(Malformed8Strings[i]);

    EXPECT_TRUE(NS_ConvertUTF8toUTF16(str8).IsEmpty());

    nsString tmp16(NS_LITERAL_STRING("string"));
    AppendUTF8toUTF16(str8, tmp16);
    EXPECT_TRUE(tmp16.EqualsLiteral("string"));

    EXPECT_NE(CompareUTF8toUTF16(str8, EmptyString()), 0);
  }
#endif
}

TEST(UTF, Hash16)
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentCString str8(ValidStrings[i].m8);
    bool err;
    EXPECT_EQ(HashString(ValidStrings[i].m16),
              HashUTF8AsUTF16(str8.get(), str8.Length(), &err));
    EXPECT_FALSE(err);
  }

  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsDependentCString str8(Invalid8Strings[i].m8);
    bool err;
    EXPECT_EQ(HashString(Invalid8Strings[i].m16),
              HashUTF8AsUTF16(str8.get(), str8.Length(), &err));
    EXPECT_FALSE(err);
  }

// Don't run this test in debug builds as that intentionally asserts.
#ifndef DEBUG
  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsDependentCString str8(Malformed8Strings[i]);
    bool err;
    EXPECT_EQ(HashUTF8AsUTF16(str8.get(), str8.Length(), &err), 0u);
    EXPECT_TRUE(err);
  }
#endif
}

} // namespace TestUTF
