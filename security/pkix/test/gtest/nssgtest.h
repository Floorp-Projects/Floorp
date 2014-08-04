/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
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

#ifndef mozilla_pkix__nssgtest_h
#define mozilla_pkix__nssgtest_h

#include "stdint.h"
#include "gtest/gtest.h"
#include "pkix/pkixtypes.h"
#include "pkixtestutil.h"
#include "prerror.h"
#include "prtime.h"
#include "seccomon.h"

namespace mozilla { namespace pkix { namespace test {

inline void
PORT_FreeArena_false(PLArenaPool* arena)
{
  // PL_FreeArenaPool can't be used because it doesn't actually free the
  // memory, which doesn't work well with memory analysis tools
  return PORT_FreeArena(arena, PR_FALSE);
}

typedef ScopedPtr<PLArenaPool, PORT_FreeArena_false> ScopedPLArenaPool;

class SECStatusWithPRErrorCode
{
public:
  SECStatusWithPRErrorCode(SECStatus rv, PRErrorCode errorCode)
    : mRv(rv)
    , mErrorCode(errorCode)
  {
  }

  explicit SECStatusWithPRErrorCode(SECStatus rv)
    : mRv(rv)
    , mErrorCode(rv == SECSuccess ? 0 : PR_GetError())
  {
  }

  bool operator==(const SECStatusWithPRErrorCode& other) const
  {
    return mRv == other.mRv && mErrorCode == other.mErrorCode;
  }

private:
  const SECStatus mRv;
  const PRErrorCode mErrorCode;

  friend std::ostream& operator<<(std::ostream& os,
                                  SECStatusWithPRErrorCode const& value);

  void operator=(const SECStatusWithPRErrorCode&) /*= delete*/;
};

::std::ostream& operator<<(::std::ostream&,
                           SECStatusWithPRErrorCode const&);

#define ASSERT_SECSuccess(rv) \
  ASSERT_EQ(::mozilla::pkix::test::SECStatusWithPRErrorCode(SECSuccess, 0), \
            ::mozilla::pkix::test::SECStatusWithPRErrorCode(rv))
#define EXPECT_SECSuccess(rv) \
  EXPECT_EQ(::mozilla::pkix::test::SECStatusWithPRErrorCode(SECSuccess, 0), \
            ::mozilla::pkix::test::SECStatusWithPRErrorCode(rv))

#define ASSERT_SECFailure(expectedError, rv) \
  ASSERT_EQ(::mozilla::pkix::test::SECStatusWithPRErrorCode(SECFailure, \
                                                            expectedError), \
            ::mozilla::pkix::test::SECStatusWithPRErrorCode(rv))
#define EXPECT_SECFailure(expectedError, rv) \
  EXPECT_EQ(::mozilla::pkix::test::SECStatusWithPRErrorCode(SECFailure, \
                                                            expectedError), \
            ::mozilla::pkix::test::SECStatusWithPRErrorCode(rv))

class NSSTest : public ::testing::Test
{
public:
  static void SetUpTestCase();

protected:
  NSSTest();

  ScopedPLArenaPool arena;
  static mozilla::pkix::Time now;
  static PRTime pr_now;
  static PRTime pr_oneDayBeforeNow;
  static PRTime pr_oneDayAfterNow;
};

} } } // namespace mozilla::pkix::test

#endif // mozilla_pkix__nssgtest_h
