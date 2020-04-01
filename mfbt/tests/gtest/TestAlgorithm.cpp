/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/Algorithm.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"

#include <iterator>
#include <vector>

using namespace mozilla;
using std::begin;
using std::end;

namespace {
struct MoveOnly {
  explicit MoveOnly(int32_t aValue) : mValue{Some(aValue)} {}

  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;

  Maybe<int32_t> mValue;
};

struct TestError {};

constexpr static int32_t arr1[3] = {1, 2, 3};
}  // namespace

TEST(MFBT_Algorithm_TransformAbortOnErr, NoError)
{
  std::vector<int64_t> out;
  auto res = TransformAbortOnErr(
      begin(arr1), end(arr1), std::back_inserter(out),
      [](const int32_t value) -> Result<int64_t, TestError> {
        return value * 10;
      });
  ASSERT_TRUE(res.isOk());

  const std::vector<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformAbortOnErr, NoError_Range)
{
  std::vector<int64_t> out;
  auto res = TransformAbortOnErr(
      arr1, std::back_inserter(out),
      [](const int32_t value) -> Result<int64_t, TestError> {
        return value * 10;
      });
  ASSERT_TRUE(res.isOk());

  const std::vector<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformAbortOnErr, ErrorOnFirst)
{
  std::vector<int64_t> out;
  auto res = TransformAbortOnErr(
      begin(arr1), end(arr1), std::back_inserter(out),
      [](const int32_t value) -> Result<int64_t, TestError> {
        return Err(TestError{});
      });
  ASSERT_TRUE(res.isErr());
  ASSERT_TRUE(out.empty());
}

TEST(MFBT_Algorithm_TransformAbortOnErr, ErrorOnOther)
{
  std::vector<int64_t> out;
  auto res = TransformAbortOnErr(
      begin(arr1), end(arr1), std::back_inserter(out),
      [](const int32_t value) -> Result<int64_t, TestError> {
        if (value > 2) {
          return Err(TestError{});
        }
        return value * 10;
      });
  ASSERT_TRUE(res.isErr());

  // XXX Should we assert on this, or is the content of out an implementation
  // detail?
  const std::vector<int64_t> expected = {10, 20};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformAbortOnErr, ErrorOnOther_Move)
{
  MoveOnly in[3] = {MoveOnly{1}, MoveOnly{2}, MoveOnly{3}};
  std::vector<int64_t> out;
  auto res = TransformAbortOnErr(
      std::make_move_iterator(begin(in)), std::make_move_iterator(end(in)),
      std::back_inserter(out),
      [](MoveOnly value) -> Result<int64_t, TestError> {
        if (*value.mValue > 1) {
          return Err(TestError{});
        }
        return *value.mValue * 10;
      });
  ASSERT_TRUE(res.isErr());

  ASSERT_FALSE(in[0].mValue);
  ASSERT_FALSE(in[1].mValue);
  ASSERT_TRUE(in[2].mValue);

  // XXX Should we assert on this, or is the content of out an implementation
  // detail?
  const std::vector<int64_t> expected = {10};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformIfAbortOnErr, NoError)
{
  std::vector<int64_t> out;
  auto res = TransformIfAbortOnErr(
      begin(arr1), end(arr1), std::back_inserter(out),
      [](const int32_t value) { return value % 2 == 1; },
      [](const int32_t value) -> Result<int64_t, TestError> {
        return value * 10;
      });
  ASSERT_TRUE(res.isOk());

  const std::vector<int64_t> expected = {10, 30};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformIfAbortOnErr, NoError_Range)
{
  std::vector<int64_t> out;
  auto res = TransformIfAbortOnErr(
      arr1, std::back_inserter(out),
      [](const int32_t value) { return value % 2 == 1; },
      [](const int32_t value) -> Result<int64_t, TestError> {
        return value * 10;
      });
  ASSERT_TRUE(res.isOk());

  const std::vector<int64_t> expected = {10, 30};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformIfAbortOnErr, ErrorOnOther)
{
  std::vector<int64_t> out;
  auto res = TransformIfAbortOnErr(
      begin(arr1), end(arr1), std::back_inserter(out),
      [](const int32_t value) { return value % 2 == 1; },
      [](const int32_t value) -> Result<int64_t, TestError> {
        if (value > 2) {
          return Err(TestError{});
        }
        return value * 10;
      });
  ASSERT_TRUE(res.isErr());

  const std::vector<int64_t> expected = {10};
  ASSERT_EQ(expected, out);
}

TEST(MFBT_Algorithm_TransformIfAbortOnErr, ErrorOnOther_Move)
{
  MoveOnly in[3] = {MoveOnly{1}, MoveOnly{2}, MoveOnly{3}};
  std::vector<int64_t> out;
  auto res = TransformIfAbortOnErr(
      std::make_move_iterator(begin(in)), std::make_move_iterator(end(in)),
      std::back_inserter(out),
      [](const MoveOnly& value) { return *value.mValue % 2 == 1; },
      [](MoveOnly value) -> Result<int64_t, TestError> {
        if (*value.mValue > 1) {
          return Err(TestError{});
        }
        return *value.mValue * 10;
      });
  ASSERT_TRUE(res.isErr());

  ASSERT_FALSE(in[0].mValue);
  ASSERT_TRUE(in[1].mValue);
  ASSERT_FALSE(in[2].mValue);

  // XXX Should we assert on this, or is the content of out an implementation
  // detail?
  const std::vector<int64_t> expected = {10};
  ASSERT_EQ(expected, out);
}
