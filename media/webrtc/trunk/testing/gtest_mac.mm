// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "gtest_mac.h"

#include <gtest/internal/gtest-port.h>
#include <gtest/internal/gtest-string.h>
#include <gtest/gtest.h>

#ifdef GTEST_OS_MAC

#import <Foundation/Foundation.h>

namespace testing {
namespace internal {

// This overloaded version allows comparison between ObjC objects that conform
// to the NSObject protocol. Used to implement {ASSERT|EXPECT}_EQ().
GTEST_API_ AssertionResult CmpHelperNSEQ(const char* expected_expression,
                                         const char* actual_expression,
                                         id<NSObject> expected,
                                         id<NSObject> actual) {
  if (expected == actual || [expected isEqual:actual]) {
    return AssertionSuccess();
  }
  return EqFailure(expected_expression,
                   actual_expression,
                   String([[expected description] UTF8String]),
                   String([[actual description] UTF8String]),
                   false);
}

// This overloaded version allows comparison between ObjC objects that conform
// to the NSObject protocol. Used to implement {ASSERT|EXPECT}_NE().
GTEST_API_ AssertionResult CmpHelperNSNE(const char* expected_expression,
                                         const char* actual_expression,
                                         id<NSObject> expected,
                                         id<NSObject> actual) {
  if (expected != actual && ![expected isEqual:actual]) {
    return AssertionSuccess();
  }
  Message msg;
  msg << "Expected: (" << expected_expression << ") != (" << actual_expression
      << "), actual: " << String([[expected description] UTF8String])
      << " vs " << String([[actual description] UTF8String]);
  return AssertionFailure(msg);
}

}  // namespace internal
}  // namespace testing

#endif  // GTEST_OS_MAC
