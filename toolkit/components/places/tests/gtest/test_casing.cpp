/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsUnicodeProperties.h"

// Verify the assertion in SQLFunctions.cpp / nextSearchCandidate that the
// only non-ASCII characters that lower-case to ASCII ones are:
//  * U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE
//  * U+212A KELVIN SIGN
TEST(MatchAutocompleteCasing, CaseAssumption)
{
  for (uint32_t c = 128; c < 0x110000; c++) {
    if (c != 304 && c != 8490) {
      ASSERT_GE(mozilla::unicode::GetLowercase(c), 128U);
    }
  }
}

// Verify the assertion that all ASCII characters lower-case to ASCII.
TEST(MatchAutocompleteCasing, CaseAssumption2)
{
  for (uint32_t c = 0; c < 128; c++) {
    ASSERT_LT(mozilla::unicode::GetLowercase(c), 128U);
  }
}
