/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsStringFwd.h"

#include "mozilla/Preferences.h"
#include "mozilla/URLQueryStringStripper.h"

using namespace mozilla;

static const char kPrefQueryStrippingEnabled[] =
    "privacy.query_stripping.enabled";
static const char kPrefQueryStrippingList[] =
    "privacy.query_stripping.strip_list";

void DoTest(const nsACString& aTestURL, const nsACString& aExpectedURL,
            bool aExpectedResult) {
  nsCOMPtr<nsIURI> testURI;

  NS_NewURI(getter_AddRefs(testURI), aTestURL);

  bool result;
  nsCOMPtr<nsIURI> strippedURI;
  result = URLQueryStringStripper::Strip(testURI, strippedURI);

  EXPECT_TRUE(result == aExpectedResult);

  if (!result) {
    EXPECT_TRUE(!strippedURI);
  } else {
    EXPECT_TRUE(strippedURI->GetSpecOrDefault().Equals(aExpectedURL));
  }
}

TEST(TestURLQueryStringStripper, TestPrefDisabled)
{
  // Disable the query string stripping by the pref and make sure the stripping
  // is disabled.
  Preferences::SetCString(kPrefQueryStrippingList, "fooBar foobaz");
  Preferences::SetBool(kPrefQueryStrippingEnabled, false);

  DoTest("https://example.com/"_ns, ""_ns, false);
  DoTest("https://example.com/?Barfoo=123"_ns, ""_ns, false);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, ""_ns, false);
}

TEST(TestURLQueryStringStripper, TestEmptyStripList)
{
  // Make sure there is no error if the strip list is empty.
  Preferences::SetCString(kPrefQueryStrippingList, "");
  Preferences::SetBool(kPrefQueryStrippingEnabled, true);

  DoTest("https://example.com/"_ns, ""_ns, false);
  DoTest("https://example.com/?Barfoo=123"_ns, ""_ns, false);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, ""_ns, false);
}

TEST(TestURLQueryStringStripper, TestStripping)
{
  Preferences::SetCString(kPrefQueryStrippingList, "fooBar foobaz");
  Preferences::SetBool(kPrefQueryStrippingEnabled, true);

  // Test the stripping.
  DoTest("https://example.com/"_ns, ""_ns, false);
  DoTest("https://example.com/?Barfoo=123"_ns, ""_ns, false);

  DoTest("https://example.com/?fooBar=123"_ns, "https://example.com/"_ns, true);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, "https://example.com/"_ns,
         true);
  DoTest("https://example.com/?fooBar=123&Barfoo=456&foobaz"_ns,
         "https://example.com/?Barfoo=456"_ns, true);

  DoTest("https://example.com/?FOOBAR=123"_ns, "https://example.com/"_ns, true);
  DoTest("https://example.com/?barfoo=foobar"_ns,
         "https://example.com/?barfoo=foobar"_ns, false);
  DoTest("https://example.com/?foobar=123&nostrip=456&FooBar=789"_ns,
         "https://example.com/?nostrip=456"_ns, true);
  DoTest("https://example.com/?AfoobazB=123"_ns,
         "https://example.com/?AfoobazB=123"_ns, false);

  // Change the strip list pref to see if it is updated properly.
  Preferences::SetCString(kPrefQueryStrippingList, "Barfoo bazfoo");

  DoTest("https://example.com/?fooBar=123"_ns, ""_ns, false);
  DoTest("https://example.com/?fooBar=123&foobaz"_ns, ""_ns, false);

  DoTest("https://example.com/?bazfoo=123"_ns, "https://example.com/"_ns, true);
  DoTest("https://example.com/?fooBar=123&Barfoo=456&foobaz=abc"_ns,
         "https://example.com/?fooBar=123&foobaz=abc"_ns, true);
}
