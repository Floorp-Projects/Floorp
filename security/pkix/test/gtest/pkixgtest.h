/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef mozilla_pkix__pkixgtest_h
#define mozilla_pkix__pkixgtest_h

#include <ostream>

#include "gtest/gtest.h"
#include "pkixutil.h"
#include "prerror.h"
#include "stdint.h"

namespace mozilla { namespace pkix { namespace test {

class ResultWithPRErrorCode
{
public:
  ResultWithPRErrorCode(Result rv, PRErrorCode errorCode)
    : mRv(rv)
    , mErrorCode(errorCode)
  {
  }

  explicit ResultWithPRErrorCode(Result rv)
    : mRv(rv)
    , mErrorCode(rv == Success ? 0 : PR_GetError())
  {
  }

  bool operator==(const ResultWithPRErrorCode& other) const
  {
    return mRv == other.mRv && mErrorCode == other.mErrorCode;
  }

private:
  const Result mRv;
  const PRErrorCode mErrorCode;

  friend std::ostream& operator<<(std::ostream& os,
                                  const ResultWithPRErrorCode & value);

  void operator=(const ResultWithPRErrorCode&) /*= delete*/;
};

::std::ostream& operator<<(::std::ostream&, const ResultWithPRErrorCode &);

#define ASSERT_Success(rv) \
  ASSERT_EQ(::mozilla::pkix::test::ResultWithPRErrorCode( \
                ::mozilla::pkix::Success, 0), \
            ::mozilla::pkix::test::ResultWithPRErrorCode(rv))
#define EXPECT_Success(rv) \
  EXPECT_EQ(::mozilla::pkix::test::ResultWithPRErrorCode( \
                ::mozilla::pkix::Success, 0), \
            ::mozilla::pkix::test::ResultWithPRErrorCode(rv))

#define ASSERT_RecoverableError(expectedError, rv) \
  ASSERT_EQ(::mozilla::pkix::test::ResultWithPRErrorCode( \
                 ::mozilla::pkix::RecoverableError, expectedError), \
            ::mozilla::pkix::test::ResultWithPRErrorCode(rv))
#define EXPECT_RecoverableError(expectedError, rv) \
  EXPECT_EQ(::mozilla::pkix::test::ResultWithPRErrorCode( \
                 ::mozilla::pkix::RecoverableError, expectedError), \
            ::mozilla::pkix::test::ResultWithPRErrorCode(rv))

#define ASSERT_FatalError(expectedError, rv) \
  ASSERT_EQ(::mozilla::pkix::test::ResultWithPRErrorCode( \
                 ::mozilla::pkix::FatalError, expectedError), \
            ::mozilla::pkix::test::ResultWithPRErrorCode(rv))
#define EXPECT_FatalError(expectedError, rv) \
  EXPECT_EQ(::mozilla::pkix::test::ResultWithPRErrorCode( \
                 ::mozilla::pkix::FatalError, expectedError), \
            ::mozilla::pkix::test::ResultWithPRErrorCode(rv))

} } } // namespace mozilla::pkix::test

#endif // mozilla_pkix__pkixgtest_h
