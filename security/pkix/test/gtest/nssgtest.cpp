/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
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

} } } // namespace mozilla::pkix::test
