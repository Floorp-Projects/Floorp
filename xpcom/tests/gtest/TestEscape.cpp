/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEscape.h"
#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "nsNetUtil.h"

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
      "a", "bcdefgh", "<",           ">",         "&", "\"",
      "'", "'bad'",   "Foo<T>& foo", "'\"&><abc", "",
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
      "abcdefgh&lt;&gt;&amp;&quot;&#39;&#39;bad&#39;Foo&lt;T&gt;&amp; "
      "foo&#39;&quot;&amp;&gt;&lt;abc",
      "abcdefgh&lt;&gt;&amp;&quot;&#39;&#39;bad&#39;Foo&lt;T&gt;&amp; "
      "foo&#39;&quot;&amp;&gt;&lt;abc",
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

TEST(Escape, AppleNSURLEscapeHash)
{
  nsCString toEscape("#");
  nsCString escaped;
  bool isEscapedOK = NS_Escape(toEscape, escaped, url_NSURLRef);
  EXPECT_EQ(isEscapedOK, true);
  EXPECT_STREQ(escaped.BeginReading(), "%23");
}

TEST(Escape, AppleNSURLEscapeNoDouble)
{
  // The '%' in "%23" shouldn't be encoded again.
  nsCString toEscape("%23");
  nsCString escaped;
  bool isEscapedOK = NS_Escape(toEscape, escaped, url_NSURLRef);
  EXPECT_EQ(isEscapedOK, true);
  EXPECT_STREQ(escaped.BeginReading(), "%23");
}

// Test escaping of URLs that shouldn't be changed by escaping.
TEST(Escape, AppleNSURLEscapeLists)
{
  // Pairs of URLs (un-encoded, encoded)
  nsTArray<std::pair<nsCString, nsCString>> pairs{
      {"https://chat.mozilla.org/#/room/#macdev:mozilla.org"_ns,
       "https://chat.mozilla.org/#/room/%23macdev:mozilla.org"_ns},
  };

  for (std::pair<nsCString, nsCString>& pair : pairs) {
    nsCString escaped;
    nsresult rv = NS_GetSpecWithNSURLEncoding(escaped, pair.first);
    EXPECT_EQ(rv, NS_OK);
    EXPECT_STREQ(pair.second.BeginReading(), escaped.BeginReading());
  }

  // A list of URLs that should not be changed by encoding.
  nsTArray<nsCString> unchangedURLs{
      // '=' In the query
      "https://bugzilla.mozilla.org/show_bug.cgi?id=1737854"_ns,
      // Escaped character in the fragment
      "https://html.spec.whatwg.org/multipage/dom.html#the-document%27s-address"_ns,
      // Misc query
      "https://www.google.com/search?q=firefox+web+browser&client=firefox-b-1-d&ei=abc&ved=abc&abc=5&oq=firefox+web+browser&gs_lcp=abc&sclient=gws-wiz"_ns,
      // Check for double encoding. % encoded octals should not be re-encoded.
      "https://chat.mozilla.org/#/room/%23macdev%3Amozilla.org"_ns,
      "https://searchfox.org/mozilla-central/search?q=symbol%3AE_%3CT_mozilla%3A%3AWebGLExtensionID%3E_EXT_color_buffer_half_float&path="_ns,
      // Other
      "https://site.com/script?foo=bar#this_ref"_ns,
  };

  for (nsCString& toEscape : unchangedURLs) {
    nsCString escaped;
    nsresult rv = NS_GetSpecWithNSURLEncoding(escaped, toEscape);
    EXPECT_EQ(rv, NS_OK);
    EXPECT_STREQ(toEscape.BeginReading(), escaped.BeginReading());
  }
}

// Test external handler URLs are properly escaped.
TEST(Escape, EscapeURLExternalHandlerURLs)
{
  const nsCString input[] = {
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ;/?:@&=+$,!'()*-._~#[]"_ns,
      " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"_ns,
      "custom_proto:Hello World"_ns,
      "custom_proto:Hello%20World"_ns,
      "myApp://\"foo\" 'bar' `foo`"_ns,
      "translator://en-de?view=übersicht"_ns,
      "foo:some\\path\\here"_ns,
      "web+foo://user:1234@example.com:8080?foo=bar"_ns,
      "ext+bar://id='myId'"_ns};

  const nsCString expected[] = {
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ;/?:@&=+$,!'()*-._~#[]"_ns,
      "%20!%22#$%&'()*+,-./0123456789:;%3C=%3E?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[%5C]%5E_%60abcdefghijklmnopqrstuvwxyz%7B%7C%7D~"_ns,
      "custom_proto:Hello%20World"_ns,
      "custom_proto:Hello%20World"_ns,
      "myApp://%22foo%22%20'bar'%20%60foo%60"_ns,
      "translator://en-de?view=%C3%BCbersicht"_ns,
      "foo:some%5Cpath%5Chere"_ns,
      "web+foo://user:1234@example.com:8080?foo=bar"_ns,
      "ext+bar://id='myId'"_ns};

  for (size_t i = 0; i < ArrayLength(input); i++) {
    nsCString src(input[i]);
    nsCString dst;
    nsresult rv =
        NS_EscapeURL(src, esc_ExtHandler | esc_AlwaysCopy, dst, fallible);
    EXPECT_EQ(rv, NS_OK);
    ASSERT_TRUE(dst.Equals(expected[i]));
  }
}
