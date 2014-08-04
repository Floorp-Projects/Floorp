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

#include "nssgtest.h"
#include "nss.h"
#include "pkixtestutil.h"
#include "prinit.h"

using namespace std;
using namespace testing;

namespace mozilla { namespace pkix { namespace test {

ostream&
operator<<(ostream& os, SECStatusWithPRErrorCode const& value)
{
  switch (value.mRv)
  {
    case SECSuccess:
      os << "SECSuccess";
      break;
    case SECWouldBlock:
      os << "SECWouldBlock";
      break;
    case SECFailure:
      os << "SECFailure";
      break;
    default:
      os << "[Invalid SECStatus: " << static_cast<int64_t>(value.mRv) << ']';
      break;
  }

  if (value.mRv != SECSuccess) {
    os << '(';
    const char* name = PR_ErrorToName(value.mErrorCode);
    if (name) {
      os << name;
    } else {
      os << value.mErrorCode;
    }
    os << ')';
  }

  return os;
}

AssertionResult
Pred_SECFailure(const char* expectedExpr, const char* actualExpr,
                PRErrorCode expectedErrorCode, SECStatus actual)
{
  if (SECFailure == actual && expectedErrorCode == PR_GetError()) {
    return AssertionSuccess();
  }

  return AssertionFailure()
      << "Expected: (" << expectedExpr << ") == (" << actualExpr
      << "), actual: " << SECFailure << " != " << actual;
}

/*static*/ void
NSSTest::SetUpTestCase()
{
  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    PR_Abort();
  }

  now = Now();
  pr_now = PR_Now();
  pr_oneDayBeforeNow = pr_now - ONE_DAY;
  pr_oneDayAfterNow = pr_now + ONE_DAY;
}

NSSTest::NSSTest()
  : arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE))
{
  if (!arena) {
    PR_Abort();
  }
}

/*static*/ mozilla::pkix::Time NSSTest::now(Now());
/*static*/ PRTime NSSTest::pr_now;
/*static*/ PRTime NSSTest::pr_oneDayBeforeNow;
/*static*/ PRTime NSSTest::pr_oneDayAfterNow;

} } } // namespace mozilla::pkix::test
