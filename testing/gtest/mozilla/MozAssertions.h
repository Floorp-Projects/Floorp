/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gtest_MozAssertions_h__
#define mozilla_gtest_MozAssertions_h__

#include "gtest/gtest.h"
#include "nsError.h"

namespace mozilla::gtest {

testing::AssertionResult IsNsresultSuccess(const char* expr, nsresult rv);
testing::AssertionResult IsNsresultFailure(const char* expr, nsresult rv);

}  // namespace mozilla::gtest

#define EXPECT_NS_SUCCEEDED(expr) \
  EXPECT_PRED_FORMAT1(::mozilla::gtest::IsNsresultSuccess, (expr))

#define ASSERT_NS_SUCCEEDED(expr) \
  ASSERT_PRED_FORMAT1(::mozilla::gtest::IsNsresultSuccess, (expr))

#define EXPECT_NS_FAILED(expr) \
  EXPECT_PRED_FORMAT1(::mozilla::gtest::IsNsresultFailure, (expr))

#define ASSERT_NS_FAILED(expr) \
  ASSERT_PRED_FORMAT1(::mozilla::gtest::IsNsresultFailure, (expr))

#endif  // mozilla_gtest_MozAssertions_h__
