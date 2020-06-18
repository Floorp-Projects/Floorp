/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/ReverseIterator.h"

using namespace mozilla;

TEST(ReverseIterator, Const_RangeBasedFor)
{
  const std::vector<int> in = {1, 2, 3, 4};
  const auto reversedRange =
      detail::IteratorRange<ReverseIterator<std::vector<int>::const_iterator>>{
          ReverseIterator{in.end()}, ReverseIterator{in.begin()}};

  const std::vector<int> expected = {4, 3, 2, 1};
  std::vector<int> out;
  for (auto i : reversedRange) {
    out.emplace_back(i);
  }

  EXPECT_EQ(expected, out);
}

TEST(ReverseIterator, NonConst_RangeBasedFor)
{
  std::vector<int> in = {1, 2, 3, 4};
  auto reversedRange =
      detail::IteratorRange<ReverseIterator<std::vector<int>::iterator>>{
          ReverseIterator{in.end()}, ReverseIterator{in.begin()}};

  const std::vector<int> expected = {-1, -2, -3, -4};
  for (auto& i : reversedRange) {
    i = -i;
  }

  EXPECT_EQ(expected, in);
}

TEST(ReverseIterator, Difference)
{
  const std::vector<int> in = {1, 2, 3, 4};
  using reverse_iterator = ReverseIterator<std::vector<int>::const_iterator>;

  reverse_iterator rbegin = reverse_iterator{in.end()},
                   rend = reverse_iterator{in.begin()};
  EXPECT_EQ(4, rend - rbegin);
  EXPECT_EQ(0, rend - rend);
  EXPECT_EQ(0, rbegin - rbegin);

  --rend;
  EXPECT_EQ(3, rend - rbegin);

  ++rbegin;
  EXPECT_EQ(2, rend - rbegin);

  rend--;
  EXPECT_EQ(1, rend - rbegin);

  rbegin++;
  EXPECT_EQ(0, rend - rbegin);
}

TEST(ReverseIterator, Comparison)
{
  const std::vector<int> in = {1, 2, 3, 4};
  using reverse_iterator = ReverseIterator<std::vector<int>::const_iterator>;

  reverse_iterator rbegin = reverse_iterator{in.end()},
                   rend = reverse_iterator{in.begin()};
  EXPECT_TRUE(rbegin < rend);
  EXPECT_FALSE(rend < rbegin);
  EXPECT_FALSE(rend < rend);
  EXPECT_FALSE(rbegin < rbegin);

  EXPECT_TRUE(rend > rbegin);
  EXPECT_FALSE(rbegin > rend);
  EXPECT_FALSE(rend > rend);
  EXPECT_FALSE(rbegin > rbegin);

  EXPECT_TRUE(rbegin <= rend);
  EXPECT_FALSE(rend <= rbegin);
  EXPECT_TRUE(rend <= rend);
  EXPECT_TRUE(rbegin <= rbegin);

  EXPECT_TRUE(rend >= rbegin);
  EXPECT_FALSE(rbegin >= rend);
  EXPECT_TRUE(rend >= rend);
  EXPECT_TRUE(rbegin >= rbegin);

  EXPECT_FALSE(rend == rbegin);
  EXPECT_FALSE(rbegin == rend);
  EXPECT_TRUE(rend == rend);
  EXPECT_TRUE(rbegin == rbegin);

  EXPECT_TRUE(rend != rbegin);
  EXPECT_TRUE(rbegin != rend);
  EXPECT_FALSE(rend != rend);
  EXPECT_FALSE(rbegin != rbegin);
}
