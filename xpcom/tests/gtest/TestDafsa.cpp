/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Dafsa.h"
#include "gtest/gtest.h"

#include "nsString.h"

using mozilla::Dafsa;

namespace dafsa_test_1 {
#include "dafsa_test_1.inc"  // kDafsa
}

TEST(Dafsa, Constructor)
{ Dafsa d(dafsa_test_1::kDafsa); }

TEST(Dafsa, StringsFound)
{
  Dafsa d(dafsa_test_1::kDafsa);

  int tag = d.Lookup(NS_LITERAL_CSTRING("foo.bar.baz"));
  EXPECT_EQ(tag, 1);

  tag = d.Lookup(NS_LITERAL_CSTRING("a.test.string"));
  EXPECT_EQ(tag, 0);

  tag = d.Lookup(NS_LITERAL_CSTRING("a.test.string2"));
  EXPECT_EQ(tag, 2);

  tag = d.Lookup(NS_LITERAL_CSTRING("aaaa"));
  EXPECT_EQ(tag, 4);
}

TEST(Dafsa, StringsNotFound)
{
  Dafsa d(dafsa_test_1::kDafsa);

  // Matches all but last letter.
  int tag = d.Lookup(NS_LITERAL_CSTRING("foo.bar.ba"));
  EXPECT_EQ(tag, Dafsa::kKeyNotFound);

  // Matches prefix with extra letter.
  tag = d.Lookup(NS_LITERAL_CSTRING("a.test.strings"));
  EXPECT_EQ(tag, Dafsa::kKeyNotFound);

  // Matches small portion.
  tag = d.Lookup(NS_LITERAL_CSTRING("a.test"));
  EXPECT_EQ(tag, Dafsa::kKeyNotFound);

  // Matches repeating pattern with extra letters.
  tag = d.Lookup(NS_LITERAL_CSTRING("aaaaa"));
  EXPECT_EQ(tag, Dafsa::kKeyNotFound);

  // Empty string.
  tag = d.Lookup(NS_LITERAL_CSTRING(""));
  EXPECT_EQ(tag, Dafsa::kKeyNotFound);
}

TEST(Dafsa, HugeString)
{
  Dafsa d(dafsa_test_1::kDafsa);

  int tag = d.Lookup(NS_LITERAL_CSTRING(
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "
      "This is a very long string that is larger than the dafsa itself. "));
  EXPECT_EQ(tag, Dafsa::kKeyNotFound);
}
