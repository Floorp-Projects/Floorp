/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEscape.h"
#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"

using namespace mozilla;

// Testing for failure here would be somewhat hard in automation. Locally you
// could use something like ulimit to create a failure.

TEST(Escape, FallibleNoEscape)
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

TEST(Escape, FallibleEscape)
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

TEST(Escape, BadEscapeSequences)
{
  {
    char bad[] = "%s\0fa";

    int32_t count = nsUnescapeCount(bad);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(bad, "%s");
  }
  {
    char bad[] = "%a";
    int32_t count = nsUnescapeCount(bad);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(bad, "%a");
  }
  {
    char bad[] = "%";
    int32_t count = nsUnescapeCount(bad);
    EXPECT_EQ(count, 1);
    EXPECT_STREQ(bad, "%");
  }
  {
    char bad[] = "%s/%s";
    int32_t count = nsUnescapeCount(bad);
    EXPECT_EQ(count, 5);
    EXPECT_STREQ(bad, "%s/%s");
  }
}

TEST(Escape, nsAppendEscapedHTML)
{
  const char* srcs[] = {
    "a",
    "bcdefgh",
    "<",
    ">",
    "&",
    "\"",
    "'",
    "'bad'",
    "Foo<T>& foo",
    "'\"&><abc",
    "",
  };

  const char* dsts1[] = {
    "a",
    "bcdefgh",
    "&lt;",
    "&gt;",
    "&amp;",
    "&quot;",
    "&#39;",
    "&#39;bad&#39;",
    "Foo&lt;T&gt;&amp; foo",
    "&#39;&quot;&amp;&gt;&lt;abc",
    "",
  };

  const char* dsts2[] = {
    "a",
    "abcdefgh",
    "abcdefgh&lt;",
    "abcdefgh&lt;&gt;",
    "abcdefgh&lt;&gt;&amp;",
    "abcdefgh&lt;&gt;&amp;&quot;",
    "abcdefgh&lt;&gt;&amp;&quot;&#39;",
    "abcdefgh&lt;&gt;&amp;&quot;&#39;&#39;bad&#39;",
    "abcdefgh&lt;&gt;&amp;&quot;&#39;&#39;bad&#39;Foo&lt;T&gt;&amp; foo",
    "abcdefgh&lt;&gt;&amp;&quot;&#39;&#39;bad&#39;Foo&lt;T&gt;&amp; foo&#39;&quot;&amp;&gt;&lt;abc",
    "abcdefgh&lt;&gt;&amp;&quot;&#39;&#39;bad&#39;Foo&lt;T&gt;&amp; foo&#39;&quot;&amp;&gt;&lt;abc",
  };

  ASSERT_EQ(ArrayLength(srcs), ArrayLength(dsts1));
  ASSERT_EQ(ArrayLength(srcs), ArrayLength(dsts2));

  // Test when the destination is empty.
  for (size_t i = 0; i < ArrayLength(srcs); i++) {
    nsCString src(srcs[i]);
    nsCString dst;
    nsAppendEscapedHTML(src, dst);
    ASSERT_TRUE(dst.Equals(dsts1[i]));
  }

  // Test when the destination is non-empty.
  nsCString dst;
  for (size_t i = 0; i < ArrayLength(srcs); i++) {
    nsCString src(srcs[i]);
    nsAppendEscapedHTML(src, dst);
    ASSERT_TRUE(dst.Equals(dsts2[i]));
  }
}

TEST(Escape, EscapeSpaces)
{
  // Tests the fallible version of NS_EscapeURL works as expected when no
  // escaping is necessary.
  nsCString toEscape("data:\x0D\x0A spa ces\xC4\x9F");
  nsCString escaped;
  nsresult rv = NS_EscapeURL(toEscape, esc_OnlyNonASCII, escaped, fallible);
  EXPECT_EQ(rv, NS_OK);
  // Only non-ASCII and C0
  EXPECT_STREQ(escaped.BeginReading(), "data:%0D%0A spa ces%C4%9F");

  escaped.Truncate();
  rv = NS_EscapeURL(toEscape, esc_OnlyNonASCII | esc_Spaces, escaped, fallible);
  EXPECT_EQ(rv, NS_OK);
  EXPECT_STREQ(escaped.BeginReading(), "data:%0D%0A%20spa%20ces%C4%9F");
}
