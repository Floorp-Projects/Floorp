/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LookupCache.h"

#include "Common.h"

static void VerifyFragments(const nsACString& aURL,
                            const nsTArray<nsCString>& aExpected) {
  nsTArray<nsCString> fragments;
  nsresult rv = LookupCache::GetLookupFragments(aURL, &fragments);
  ASSERT_EQ(rv, NS_OK) << "GetLookupFragments should not fail";

  ASSERT_EQ(aExpected.Length(), fragments.Length())
      << "Fragments generated from " << aURL.BeginReading()
      << " are not the same as expected";

  for (const auto& fragment : fragments) {
    ASSERT_TRUE(aExpected.Contains(fragment))
    << "Fragments generated from " << aURL.BeginReading()
    << " are not the same as expected";
  }
}

// This testcase references SafeBrowsing spec:
// https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions
TEST(URLsAndHashing, FragmentURLWithQuery)
{
  const nsLiteralCString url("a.b.c/1/2.html?param=1");
  nsTArray<nsCString> expect = {
      nsLiteralCString("a.b.c/1/2.html?param=1"),
      nsLiteralCString("a.b.c/1/2.html"),
      nsLiteralCString("a.b.c/"),
      nsLiteralCString("a.b.c/1/"),
      nsLiteralCString("b.c/1/2.html?param=1"),
      nsLiteralCString("b.c/1/2.html"),
      nsLiteralCString("b.c/"),
      nsLiteralCString("b.c/1/"),
  };

  VerifyFragments(url, expect);
}

// This testcase references SafeBrowsing spec:
// https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions
TEST(URLsAndHashing, FragmentURLWithoutQuery)
{
  const nsLiteralCString url("a.b.c.d.e.f.g/1.html");
  nsTArray<nsCString> expect = {
      nsLiteralCString("a.b.c.d.e.f.g/1.html"),
      nsLiteralCString("a.b.c.d.e.f.g/"),
      nsLiteralCString("c.d.e.f.g/1.html"),
      nsLiteralCString("c.d.e.f.g/"),
      nsLiteralCString("d.e.f.g/1.html"),
      nsLiteralCString("d.e.f.g/"),
      nsLiteralCString("e.f.g/1.html"),
      nsLiteralCString("e.f.g/"),
      nsLiteralCString("f.g/1.html"),
      nsLiteralCString("f.g/"),
  };

  VerifyFragments(url, expect);
}

TEST(URLsAndHashing, FragmentURLEndWithoutPath)
{
  const nsLiteralCString url("1.2.3.4/?query=string");
  nsTArray<nsCString> expect = {
      nsLiteralCString("1.2.3.4/?query=string"),
      nsLiteralCString("1.2.3.4/"),
  };

  VerifyFragments(url, expect);
}
