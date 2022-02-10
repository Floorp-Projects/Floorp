/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/Preferences.h"
#include "mozilla/PartitioningExceptionList.h"

using namespace mozilla;

static const char kPrefPartitioningExceptionList[] =
    "privacy.restrict3rdpartystorage.skip_list";

static const char kPrefEnableWebcompat[] =
    "privacy.antitracking.enableWebcompat";

TEST(TestPartitioningExceptionList, TestPrefBasic)
{
  nsAutoCString oldPartitioningExceptionList;
  Preferences::GetCString(kPrefEnableWebcompat, oldPartitioningExceptionList);
  bool oldEnableWebcompat = Preferences::GetBool(kPrefEnableWebcompat);

  for (uint32_t populateList = 0; populateList <= 1; populateList++) {
    for (uint32_t enableWebcompat = 0; enableWebcompat <= 1;
         enableWebcompat++) {
      if (populateList) {
        Preferences::SetCString(kPrefPartitioningExceptionList,
                                "https://example.com,https://example.net");
      } else {
        Preferences::SetCString(kPrefPartitioningExceptionList, "");
      }

      Preferences::SetBool(kPrefEnableWebcompat, enableWebcompat);

      EXPECT_FALSE(
          PartitioningExceptionList::Check(""_ns, "https://example.net"_ns));
      EXPECT_FALSE(
          PartitioningExceptionList::Check("https://example.com"_ns, ""_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check(""_ns, ""_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check("https://example.net"_ns,
                                                    "https://example.com"_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                    "https://example.org"_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                    "https://example.com"_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check("http://example.com"_ns,
                                                    "http://example.net"_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                    "http://example.net"_ns));
      EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com."_ns,
                                                    "https://example.net"_ns));

      bool result = PartitioningExceptionList::Check("https://example.com"_ns,
                                                     "https://example.net"_ns);
      EXPECT_TRUE(result == (populateList && enableWebcompat));
    }
  }

  Preferences::SetCString(kPrefPartitioningExceptionList,
                          oldPartitioningExceptionList);
  Preferences::SetBool(kPrefEnableWebcompat, oldEnableWebcompat);
}

TEST(TestPartitioningExceptionList, TestPrefWildcard)
{
  nsAutoCString oldPartitioningExceptionList;
  Preferences::GetCString(kPrefEnableWebcompat, oldPartitioningExceptionList);
  bool oldEnableWebcompat = Preferences::GetBool(kPrefEnableWebcompat);

  Preferences::SetCString(kPrefPartitioningExceptionList,
                          "https://example.com,https://example.net;"
                          "https://*.foo.com,https://bar.com;"
                          "https://*.foo.com,https://foobar.net;"
                          "https://test.net,https://*.example.com;"
                          "https://test.com,https://*.example.com;"
                          "https://*.test2.org,*;"
                          "*,http://notatracker.org");

  Preferences::SetBool(kPrefEnableWebcompat, true);

  EXPECT_TRUE(PartitioningExceptionList::Check("https://example.com"_ns,
                                               "https://example.net"_ns));

  EXPECT_TRUE(PartitioningExceptionList::Check("https://two.foo.com"_ns,
                                               "https://bar.com"_ns));
  EXPECT_TRUE(PartitioningExceptionList::Check("https://another.foo.com"_ns,
                                               "https://bar.com"_ns));
  EXPECT_TRUE(PartitioningExceptionList::Check("https://three.two.foo.com"_ns,
                                               "https://bar.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("https://two.foo.com"_ns,
                                                "https://example.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("https://foo.com"_ns,
                                                "https://bar.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("https://two.foo.com"_ns,
                                                "http://bar.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("http://two.foo.com"_ns,
                                                "https://bar.com"_ns));

  EXPECT_TRUE(PartitioningExceptionList::Check("https://a.foo.com"_ns,
                                               "https://foobar.net"_ns));

  EXPECT_TRUE(PartitioningExceptionList::Check("https://test.net"_ns,
                                               "https://test.example.com"_ns));
  EXPECT_TRUE(PartitioningExceptionList::Check(
      "https://test.net"_ns, "https://foo.bar.example.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("https://test.com"_ns,
                                                "https://foo.test.net"_ns));

  EXPECT_TRUE(PartitioningExceptionList::Check("https://one.test2.org"_ns,
                                               "https://example.net"_ns));
  EXPECT_TRUE(PartitioningExceptionList::Check("https://two.test2.org"_ns,
                                               "https://foo.example.net"_ns));
  EXPECT_TRUE(PartitioningExceptionList::Check("https://three.test2.org"_ns,
                                               "http://example.net"_ns));
  EXPECT_TRUE(PartitioningExceptionList::Check("https://four.sub.test2.org"_ns,
                                               "https://bar.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("https://four.sub.test2.com"_ns,
                                                "https://bar.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("http://four.sub.test2.org"_ns,
                                                "https://bar.com"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check(
      "https://four.sub.test2.org."_ns, "https://bar.com"_ns));

  EXPECT_TRUE(PartitioningExceptionList::Check("https://example.com"_ns,
                                               "http://notatracker.org"_ns));

  Preferences::SetCString(kPrefPartitioningExceptionList,
                          oldPartitioningExceptionList);
  Preferences::SetBool(kPrefEnableWebcompat, oldEnableWebcompat);
}

TEST(TestPartitioningExceptionList, TestInvalidEntries)
{
  nsAutoCString oldPartitioningExceptionList;
  Preferences::GetCString(kPrefEnableWebcompat, oldPartitioningExceptionList);
  bool oldEnableWebcompat = Preferences::GetBool(kPrefEnableWebcompat);

  Preferences::SetBool(kPrefEnableWebcompat, true);

  // Empty entries.
  Preferences::SetCString(kPrefPartitioningExceptionList, ";;;,;");

  EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                "https://example.net"_ns));

  // Schemeless entries.
  Preferences::SetCString(kPrefPartitioningExceptionList,
                          "example.com,example.net");

  EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                "https://example.net"_ns));

  //  Invalid entry should be skipped and not break other entries.
  Preferences::SetCString(kPrefPartitioningExceptionList,
                          "*,*;"
                          "https://example.com,https://example.net;"
                          "http://example.org,");

  EXPECT_TRUE(PartitioningExceptionList::Check("https://example.com"_ns,
                                               "https://example.net"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("https://foo.com"_ns,
                                                "https://bar.net"_ns));

  // Unsupported schemes should not be accepted.
  Preferences::SetCString(kPrefPartitioningExceptionList,
                          "ftp://example.com,ftp://example.net;");

  EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                "https://example.net"_ns));
  EXPECT_FALSE(PartitioningExceptionList::Check("ftp://example.com"_ns,
                                                "ftp://example.net"_ns));

  // Test invalid origins with trailing '/'.
  Preferences::SetCString(kPrefPartitioningExceptionList,
                          "https://example.com/,https://example.net/");
  EXPECT_FALSE(PartitioningExceptionList::Check("https://example.com"_ns,
                                                "https://example.net"_ns));

  Preferences::SetCString(kPrefPartitioningExceptionList,
                          oldPartitioningExceptionList);
  Preferences::SetBool(kPrefEnableWebcompat, oldEnableWebcompat);
}
