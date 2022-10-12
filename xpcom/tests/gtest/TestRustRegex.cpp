/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/RustRegex.h"

// This file is adapted from the test.c file in the `rure` crate, but modified
// to use gtest and the `RustRegex` wrapper.

namespace mozilla {

TEST(TestRustRegex, IsMatch)
{
  RustRegex re("\\p{So}$");
  ASSERT_TRUE(re.IsValid());
  ASSERT_TRUE(re.IsMatch("snowman: \xE2\x98\x83"));
}

TEST(TestRustRegex, ShortestMatch)
{
  RustRegex re("a+");
  ASSERT_TRUE(re.IsValid());

  Maybe<size_t> match = re.ShortestMatch("aaaaa");
  ASSERT_TRUE(match);
  EXPECT_EQ(*match, 1u);
}

TEST(TestRustRegex, Find)
{
  RustRegex re("\\p{So}$");
  ASSERT_TRUE(re.IsValid());

  auto match = re.Find("snowman: \xE2\x98\x83");
  ASSERT_TRUE(match);
  EXPECT_EQ(match->start, 9u);
  EXPECT_EQ(match->end, 12u);
}

TEST(TestRustRegex, Captures)
{
  RustRegex re(".(.*(?P<snowman>\\p{So}))$");
  ASSERT_TRUE(re);

  auto captures = re.FindCaptures("snowman: \xE2\x98\x83");
  ASSERT_TRUE(captures);
  EXPECT_EQ(captures.Length(), 3u);
  EXPECT_EQ(re.CaptureNameIndex("snowman"), 2);

  auto match = captures[2];
  ASSERT_TRUE(match);
  EXPECT_EQ(match->start, 9u);
  EXPECT_EQ(match->end, 12u);
}

TEST(TestRustRegex, Iter)
{
  RustRegex re("\\w+(\\w)");
  ASSERT_TRUE(re);

  auto it = re.IterMatches("abc xyz");
  ASSERT_TRUE(it);

  auto match = it.Next();
  ASSERT_TRUE(match);
  EXPECT_EQ(match->start, 0u);
  EXPECT_EQ(match->end, 3u);

  auto captures = it.NextCaptures();
  ASSERT_TRUE(captures);

  auto capture = captures[1];
  ASSERT_TRUE(capture);
  EXPECT_EQ(capture->start, 6u);
  EXPECT_EQ(capture->end, 7u);
}

TEST(TestRustRegex, IterCaptureNames)
{
  RustRegex re("(?P<year>\\d{4})-(?P<month>\\d{2})-(?P<day>\\d{2})");
  ASSERT_TRUE(re);

  auto it = re.IterCaptureNames();
  Maybe<const char*> result = it.Next();
  ASSERT_TRUE(result.isSome());
  EXPECT_STREQ(*result, "");

  result = it.Next();
  ASSERT_TRUE(result.isSome());
  EXPECT_STREQ(*result, "year");

  result = it.Next();
  ASSERT_TRUE(result.isSome());
  EXPECT_STREQ(*result, "month");

  result = it.Next();
  ASSERT_TRUE(result.isSome());
  EXPECT_STREQ(*result, "day");

  result = it.Next();
  ASSERT_TRUE(result.isNothing());
}

/*
 * This tests whether we can set the flags correctly. In this case, we disable
 * all flags, which includes disabling Unicode mode. When we disable Unicode
 * mode, we can match arbitrary possibly invalid UTF-8 bytes, such as \xFF.
 * (When Unicode mode is enabled, \xFF won't match .)
 */
TEST(TestRustRegex, Flags)
{
  {
    RustRegex re(".");
    ASSERT_TRUE(re);
    ASSERT_FALSE(re.IsMatch("\xFF"));
  }
  {
    RustRegex re(".", RustRegexOptions().Unicode(false));
    ASSERT_TRUE(re);
    ASSERT_TRUE(re.IsMatch("\xFF"));
  }
}

TEST(TestRustRegex, CompileErrorSizeLimit)
{
  RustRegex re("\\w{100}", RustRegexOptions().SizeLimit(0));
  EXPECT_FALSE(re);
}

TEST(TestRustRegex, SetMatches)
{
  RustRegexSet set(nsTArray<std::string_view>{"foo", "barfoo", "\\w+", "\\d+",
                                              "foobar", "bar"});

  ASSERT_TRUE(set);
  EXPECT_EQ(set.Length(), 6u);
  EXPECT_TRUE(set.IsMatch("foobar"));
  EXPECT_FALSE(set.IsMatch(""));

  auto matches = set.Matches("foobar");
  EXPECT_TRUE(matches.matchedAny);
  EXPECT_EQ(matches.matches.Length(), 6u);

  nsTArray<bool> expectedMatches{true, false, true, false, true, true};
  EXPECT_EQ(matches.matches, expectedMatches);
}

TEST(TestRustRegex, SetMatchStart)
{
  RustRegexSet re(nsTArray<std::string_view>{"foo", "bar", "fooo"});
  EXPECT_TRUE(re);
  EXPECT_EQ(re.Length(), 3u);

  EXPECT_FALSE(re.IsMatch("foobiasdr", 2));

  {
    auto matches = re.Matches("fooobar");
    EXPECT_TRUE(matches.matchedAny);
    nsTArray<bool> expectedMatches{true, true, true};
    EXPECT_EQ(matches.matches, expectedMatches);
  }

  {
    auto matches = re.Matches("fooobar", 1);
    EXPECT_TRUE(matches.matchedAny);
    nsTArray<bool> expectedMatches{false, true, false};
    EXPECT_EQ(matches.matches, expectedMatches);
  }
}

TEST(TestRustRegex, RegexSetOptions)
{
  RustRegexSet re(nsTArray<std::string_view>{"\\w{100}"},
                  RustRegexOptions().SizeLimit(0));
  EXPECT_FALSE(re);
}

}  // namespace mozilla
