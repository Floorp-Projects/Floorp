/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Original author: bcampen@mozilla.com */

#include "simpletokenbucket.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using mozilla::SimpleTokenBucket;

class TestSimpleTokenBucket : public SimpleTokenBucket {
 public:
  TestSimpleTokenBucket(size_t bucketSize, size_t tokensPerSecond)
      : SimpleTokenBucket(bucketSize, tokensPerSecond) {}

  void fastForward(int32_t timeMilliSeconds) {
    if (timeMilliSeconds >= 0) {
      last_time_tokens_added_ -= PR_MillisecondsToInterval(timeMilliSeconds);
    } else {
      last_time_tokens_added_ += PR_MillisecondsToInterval(-timeMilliSeconds);
    }
  }
};

TEST(SimpleTokenBucketTest, TestConstruct)
{ TestSimpleTokenBucket b(10, 1); }

TEST(SimpleTokenBucketTest, TestGet)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(5U, b.getTokens(5));
}

TEST(SimpleTokenBucketTest, TestGetAll)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(10U, b.getTokens(10));
}

TEST(SimpleTokenBucketTest, TestGetInsufficient)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(5U, b.getTokens(5));
  ASSERT_EQ(5U, b.getTokens(6));
}

TEST(SimpleTokenBucketTest, TestGetBucketCount)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(10U, b.getTokens(UINT32_MAX));
  ASSERT_EQ(5U, b.getTokens(5));
  ASSERT_EQ(5U, b.getTokens(UINT32_MAX));
}

TEST(SimpleTokenBucketTest, TestTokenRefill)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(5U, b.getTokens(5));
  b.fastForward(1000);
  ASSERT_EQ(6U, b.getTokens(6));
}

TEST(SimpleTokenBucketTest, TestNoTimeWasted)
{
  // Makes sure that when the time elapsed is insufficient to add any
  // tokens to the bucket, the internal timestamp that is used in this
  // calculation is not updated (ie; two subsequent 0.5 second elapsed times
  // counts as a full second)
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(5U, b.getTokens(5));
  b.fastForward(500);
  ASSERT_EQ(5U, b.getTokens(6));
  b.fastForward(500);
  ASSERT_EQ(6U, b.getTokens(6));
}

TEST(SimpleTokenBucketTest, TestNegativeTime)
{
  TestSimpleTokenBucket b(10, 1);
  b.fastForward(-1000);
  // Make sure we don't end up with an invalid number of tokens, but otherwise
  // permit anything.
  ASSERT_GT(11U, b.getTokens(100));
}

TEST(SimpleTokenBucketTest, TestEmptyBucket)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(10U, b.getTokens(10));
  ASSERT_EQ(0U, b.getTokens(10));
}

TEST(SimpleTokenBucketTest, TestEmptyThenFillBucket)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(10U, b.getTokens(10));
  ASSERT_EQ(0U, b.getTokens(1));
  b.fastForward(50000);
  ASSERT_EQ(10U, b.getTokens(10));
}

TEST(SimpleTokenBucketTest, TestNoOverflow)
{
  TestSimpleTokenBucket b(10, 1);
  ASSERT_EQ(10U, b.getTokens(10));
  ASSERT_EQ(0U, b.getTokens(1));
  b.fastForward(50000);
  ASSERT_EQ(10U, b.getTokens(11));
}
