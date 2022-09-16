/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozAssertions.h"
#include "mozilla/ErrorNames.h"
#include "nsString.h"

namespace mozilla::gtest {

static testing::AssertionResult NsresultFailureHelper(const char* expr,
                                                      const char* expected,
                                                      nsresult rv) {
  nsAutoCString name;
  GetErrorName(rv, name);

  return testing::AssertionFailure()
         << "Expected: " << expr << " " << expected << ".\n"
         << "  Actual: " << name << "\n";
}

testing::AssertionResult IsNsresultSuccess(const char* expr, nsresult rv) {
  if (NS_SUCCEEDED(rv)) {
    return testing::AssertionSuccess();
  }
  return NsresultFailureHelper(expr, "succeeds", rv);
}

testing::AssertionResult IsNsresultFailure(const char* expr, nsresult rv) {
  if (NS_FAILED(rv)) {
    return testing::AssertionSuccess();
  }
  return NsresultFailureHelper(expr, "failed", rv);
}

}  // namespace mozilla::gtest
