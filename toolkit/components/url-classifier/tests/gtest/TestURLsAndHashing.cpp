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
      "a.b.c/1/2.html?param=1"_ns,
      "a.b.c/1/2.html"_ns,
      "a.b.c/"_ns,
      "a.b.c/1/"_ns,
      "b.c/1/2.html?param=1"_ns,
      "b.c/1/2.html"_ns,
      "b.c/"_ns,
      "b.c/1/"_ns,
  };

  VerifyFragments(url, expect);
}

// This testcase references SafeBrowsing spec:
// https://developers.google.com/safe-browsing/v4/urls-hashing#suffixprefix-expressions
TEST(URLsAndHashing, FragmentURLWithoutQuery)
{
  const nsLiteralCString url("a.b.c.d.e.f.g/1.html");
  nsTArray<nsCString> expect = {
      "a.b.c.d.e.f.g/1.html"_ns, "a.b.c.d.e.f.g/"_ns,
      "c.d.e.f.g/1.html"_ns,     "c.d.e.f.g/"_ns,
      "d.e.f.g/1.html"_ns,       "d.e.f.g/"_ns,
      "e.f.g/1.html"_ns,         "e.f.g/"_ns,
      "f.g/1.html"_ns,           "f.g/"_ns,
  };

  VerifyFragments(url, expect);
}

TEST(URLsAndHashing, FragmentURLEndWithoutPath)
{
  const nsLiteralCString url("1.2.3.4/?query=string");
  nsTArray<nsCString> expect = {
      "1.2.3.4/?query=string"_ns,
      "1.2.3.4/"_ns,
  };

  VerifyFragments(url, expect);
}
