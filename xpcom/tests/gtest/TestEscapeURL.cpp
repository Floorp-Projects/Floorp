/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEscape.h"
#include "gtest/gtest.h"

using namespace mozilla;

// Testing for failure here would be somewhat hard in automation. Locally you
// could use something like ulimit to create a failure.

TEST(EscapeURL, FallibleNoEscape)
{
  // Tests the fallible version of NS_EscapeURL works as expected when no
  // escaping is necessary.
  nsCString toEscape("data:,Hello%2C%20World!");
  nsCString escaped;
  nsresult rv = NS_EscapeURL(toEscape, esc_OnlyNonASCII, escaped, fallible);
  EXPECT_EQ(rv, NS_OK);
  // Nothing should have been escaped, they should be the same string.
  EXPECT_STREQ(toEscape.BeginReading(), escaped.BeginReading());
  // We expect them to point at the same buffer.
  EXPECT_EQ(toEscape.BeginReading(), escaped.BeginReading());
}

TEST(EscapeURL, FallibleEscape)
{
  // Tests the fallible version of NS_EscapeURL works as expected when
  // escaping is necessary.
  nsCString toEscape("data:,Hello%2C%20World!\xC4\x9F");
  nsCString escaped;
  nsresult rv = NS_EscapeURL(toEscape, esc_OnlyNonASCII, escaped, fallible);
  EXPECT_EQ(rv, NS_OK);
  EXPECT_STRNE(toEscape.BeginReading(), escaped.BeginReading());
  const char* const kExpected = "data:,Hello%2C%20World!%C4%9F";
  EXPECT_STREQ(escaped.BeginReading(), kExpected);
}
