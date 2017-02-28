/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "ApplicationReputation.h"
#include "mozilla/Preferences.h"
#include "nsLiteralString.h"

using namespace mozilla;
using namespace mozilla::downloads;

TEST(PendingLookup, LookupTablesInPrefs)
{
  EXPECT_EQ(NS_OK, Preferences::SetCString("gtest.test", "goog-badbinurl-proto,goog-downloadwhite-proto,goog-badbinurl-shavar"));

  bool result;
  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-phish-proto,adafaf,,daf,goog-badbinurl-proto"), "gtest.test");
  ASSERT_TRUE(result);

  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-phish-proto,adafaf,,daf,goog-downloadwhite-proto"), "gtest.test");
  ASSERT_TRUE(result);

  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-phish-proto"), "gtest.test");
  ASSERT_FALSE(result);

  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-phish-proto,goog-badbinurl-proto,goog-phish-shavar"), "gtest.test");
  ASSERT_TRUE(result);

  EXPECT_EQ(NS_OK, Preferences::SetCString("gtest.test", "goog-badbinurl-proto"));

  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-badbinurl-proto"), "gtest.test");
  ASSERT_TRUE(result);

  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-phish-proto,goog-badbinurl-proto,goog-phish-shavar"), "gtest.test");
  ASSERT_TRUE(result);

  // Empty prefrence
  EXPECT_EQ(NS_OK, Preferences::SetCString("gtest.test", ""));

  result = LookupTablesInPrefs(NS_LITERAL_CSTRING("goog-phish-proto,goog-badbinurl-proto,goog-phish-shavar"), "gtest.test");
  ASSERT_FALSE(result);

  Preferences::ClearUser("gtest.test");
}

