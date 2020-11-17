/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "mozilla/Assertions.h"
#include "mozilla/Tainting.h"

using mozilla::Tainted;

#define EXPECTED_VALUE 10

static bool externalFunction(int arg) { return arg > 2; }

static void TestTainting() {
  int bar;
  Tainted<int> foo = Tainted<int>(EXPECTED_VALUE);

  // ==================================================================
  // MOZ_VALIDATE_AND_GET =============================================

  bar = MOZ_VALIDATE_AND_GET(foo, foo < 20);
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // This test is for comparison to an external variable, testing the
  // default capture mode of the lambda used inside the macro.
  int comparisonVariable = 20;
  bar = MOZ_VALIDATE_AND_GET(foo, foo < comparisonVariable);
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  bar = MOZ_VALIDATE_AND_GET(
      foo, foo < 20,
      "foo must be less than 20 because higher values represent decibel"
      "levels greater than a a jet engine inside your ear.");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // Test an external variable with a comment.
  bar = MOZ_VALIDATE_AND_GET(foo, foo < comparisonVariable, "Test comment");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // Test an external function with a comment.
  bar = MOZ_VALIDATE_AND_GET(foo, externalFunction(foo), "Test comment");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // Lambda Tests
  bar = MOZ_VALIDATE_AND_GET(foo, ([&foo]() {
                               bool intermediateResult = externalFunction(foo);
                               if (intermediateResult) {
                                 return true;
                               }
                               return false;
                             }()));
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // This test is for the lambda variant with a supplied assertion
  // string.
  bar = MOZ_VALIDATE_AND_GET(foo, ([&foo]() {
                               bool intermediateResult = externalFunction(foo);
                               if (intermediateResult) {
                                 return true;
                               }
                               return false;
                             }()),
                             "This tests a comment");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // This test is for the lambda variant with a captured variable
  bar =
      MOZ_VALIDATE_AND_GET(foo, ([&foo, &comparisonVariable] {
                             bool intermediateResult = externalFunction(foo);
                             if (intermediateResult || comparisonVariable < 4) {
                               return true;
                             }
                             return false;
                           }()),
                           "This tests a comment");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // This test is for the lambda variant with full capture mode
  bar =
      MOZ_VALIDATE_AND_GET(foo, ([&] {
                             bool intermediateResult = externalFunction(foo);
                             if (intermediateResult || comparisonVariable < 4) {
                               return true;
                             }
                             return false;
                           }()),
                           "This tests a comment");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // External lambdas
  auto lambda1 = [](int foo) {
    bool intermediateResult = externalFunction(foo);
    if (intermediateResult) {
      return true;
    }
    return false;
  };
  bar = MOZ_VALIDATE_AND_GET(foo, lambda1(foo));
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // Test with a comment
  bar = MOZ_VALIDATE_AND_GET(foo, lambda1(foo), "Test comment.");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // Test with a default capture mode
  auto lambda2 = [&](int foo) {
    bool intermediateResult = externalFunction(foo);
    if (intermediateResult || comparisonVariable < 4) {
      return true;
    }
    return false;
  };
  bar = MOZ_VALIDATE_AND_GET(foo, lambda2(foo), "Test comment.");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // Test with an explicit capture
  auto lambda3 = [&comparisonVariable](int foo) {
    bool intermediateResult = externalFunction(foo);
    if (intermediateResult || comparisonVariable < 4) {
      return true;
    }
    return false;
  };
  bar = MOZ_VALIDATE_AND_GET(foo, lambda3(foo), "Test comment.");
  MOZ_RELEASE_ASSERT(bar == EXPECTED_VALUE);

  // We can't test MOZ_VALIDATE_AND_GET failing, because that triggers
  // a release assert.

  // ==================================================================
  // MOZ_IS_VALID =====================================================
  if (MOZ_IS_VALID(foo, foo < 20)) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  if (MOZ_IS_VALID(foo, foo > 20)) {
    MOZ_RELEASE_ASSERT(false);
  } else {
    MOZ_RELEASE_ASSERT(true);
  }

  if (MOZ_IS_VALID(foo, foo < comparisonVariable)) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  if (MOZ_IS_VALID(foo, ([&foo]() {
                     bool intermediateResult = externalFunction(foo);
                     if (intermediateResult) {
                       return true;
                     }
                     return false;
                   }()))) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  if (MOZ_IS_VALID(foo, ([&foo, &comparisonVariable]() {
                     bool intermediateResult = externalFunction(foo);
                     if (intermediateResult || comparisonVariable < 4) {
                       return true;
                     }
                     return false;
                   }()))) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  if (MOZ_IS_VALID(foo, lambda1(foo))) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  if (MOZ_IS_VALID(foo, lambda2(foo))) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  if (MOZ_IS_VALID(foo, lambda3(foo))) {
    MOZ_RELEASE_ASSERT(true);
  } else {
    MOZ_RELEASE_ASSERT(false);
  }

  // ==================================================================
  // MOZ_VALIDATE_OR ==================================================

  int result;

  result = MOZ_VALIDATE_OR(foo, foo < 20, 100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  result = MOZ_VALIDATE_OR(foo, foo > 20, 100);
  MOZ_RELEASE_ASSERT(result == 100);

  result = MOZ_VALIDATE_OR(foo, foo < comparisonVariable, 100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  result = MOZ_VALIDATE_OR(foo, lambda1(foo), 100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  result = MOZ_VALIDATE_OR(foo, lambda2(foo), 100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  result = MOZ_VALIDATE_OR(foo, lambda3(foo), 100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  result = MOZ_VALIDATE_OR(foo, ([&foo]() {
                             bool intermediateResult = externalFunction(foo);
                             if (intermediateResult) {
                               return true;
                             }
                             return false;
                           }()),
                           100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  // This test is for the lambda variant with a supplied assertion
  // string.
  result = MOZ_VALIDATE_OR(foo, ([&foo] {
                             bool intermediateResult = externalFunction(foo);
                             if (intermediateResult) {
                               return true;
                             }
                             return false;
                           }()),
                           100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  // This test is for the lambda variant with a captured variable
  result = MOZ_VALIDATE_OR(foo, ([&foo, &comparisonVariable] {
                             bool intermediateResult = externalFunction(foo);
                             if (intermediateResult || comparisonVariable < 4) {
                               return true;
                             }
                             return false;
                           }()),
                           100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  // This test is for the lambda variant with full capture mode
  result = MOZ_VALIDATE_OR(foo, ([&] {
                             bool intermediateResult = externalFunction(foo);
                             if (intermediateResult || comparisonVariable < 4) {
                               return true;
                             }
                             return false;
                           }()),
                           100);
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);

  // ==================================================================
  // MOZ_NO_VALIDATE ==================================================
  result = MOZ_NO_VALIDATE(
      foo,
      "Value is used to match against a dictionary key in the parent."
      "If there's no key present, there won't be a match."
      "There is no risk of grabbing a cross-origin value from the dictionary,"
      "because the IPC actor is instatiated per-content-process and the "
      "dictionary is not shared between actors.");
  MOZ_RELEASE_ASSERT(result == EXPECTED_VALUE);
}

int main() {
  TestTainting();

  return 0;
}
