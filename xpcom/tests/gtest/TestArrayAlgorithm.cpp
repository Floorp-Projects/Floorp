/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "nsTArray.h"

using namespace mozilla;
using std::begin;
using std::end;

namespace {
constexpr static int32_t arr1[3] = {1, 2, 3};
}

TEST(nsAlgorithm_TransformIntoNewArrayAbortOnErr, NoError_Fallible)
{
  auto res = TransformIntoNewArrayAbortOnErr(
      begin(arr1), end(arr1),
      [](const int32_t value) -> Result<int64_t, nsresult> {
        return value * 10;
      },
      fallible);
  ASSERT_TRUE(res.isOk());
  const nsTArray<int64_t>& out = res.inspect();

  const nsTArray<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, out);
}

TEST(nsAlgorithm_TransformIntoNewArrayAbortOnErr, NoError_Fallible_Range)
{
  auto res = TransformIntoNewArrayAbortOnErr(
      arr1,
      [](const int32_t value) -> Result<int64_t, nsresult> {
        return value * 10;
      },
      fallible);
  ASSERT_TRUE(res.isOk());
  const nsTArray<int64_t>& out = res.inspect();

  const nsTArray<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, out);
}

TEST(nsAlgorithm_TransformIntoNewArrayAbortOnErr, ErrorOnOther_Fallible)
{
  auto res = TransformIntoNewArrayAbortOnErr(
      begin(arr1), end(arr1),
      [](const int32_t value) -> Result<int64_t, nsresult> {
        if (value > 1) {
          return Err(NS_ERROR_FAILURE);
        }
        return value * 10;
      },
      fallible);
  ASSERT_TRUE(res.isErr());
  ASSERT_EQ(NS_ERROR_FAILURE, res.inspectErr());
}

TEST(nsAlgorithm_TransformIntoNewArray, NoError)
{
  auto res = TransformIntoNewArray(
      begin(arr1), end(arr1),
      [](const int32_t value) -> int64_t { return value * 10; });

  const nsTArray<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, res);
}

TEST(nsAlgorithm_TransformIntoNewArray, NoError_Range)
{
  auto res = TransformIntoNewArray(
      arr1, [](const int32_t value) -> int64_t { return value * 10; });

  const nsTArray<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, res);
}

TEST(nsAlgorithm_TransformIntoNewArray, NoError_Fallible)
{
  auto res = TransformIntoNewArray(
      begin(arr1), end(arr1),
      [](const int32_t value) -> int64_t { return value * 10; }, fallible);
  ASSERT_TRUE(res.isOk());
  const nsTArray<int64_t>& out = res.inspect();

  const nsTArray<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, out);
}

TEST(nsAlgorithm_TransformIntoNewArray, NoError_Fallible_Range)
{
  auto res = TransformIntoNewArray(
      arr1, [](const int32_t value) -> int64_t { return value * 10; },
      fallible);
  ASSERT_TRUE(res.isOk());
  const nsTArray<int64_t>& out = res.inspect();

  const nsTArray<int64_t> expected = {10, 20, 30};
  ASSERT_EQ(expected, out);
}
