/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <XCTest/XCTest.h>

#include "sdk/objc/helpers/scoped_cftyperef.h"

#include "test/gtest.h"

namespace {
struct TestType {
  TestType() : has_value(true) {}
  TestType(bool b) : has_value(b) {}
  explicit operator bool() { return has_value; }
  bool has_value;
  int retain_count = 0;
};

typedef TestType* TestTypeRef;

struct TestTypeTraits {
  static TestTypeRef InvalidValue() { return TestTypeRef(false); }
  static void Release(TestTypeRef t) { t->retain_count--; }
  static TestTypeRef Retain(TestTypeRef t) {
    t->retain_count++;
    return t;
  }
};
}  // namespace

using ScopedTestType = rtc::internal::ScopedTypeRef<TestTypeRef, TestTypeTraits>;

// In these tests we sometime introduce variables just to
// observe side-effects. Ignore the compilers complaints.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

@interface ScopedTypeRefTests : XCTestCase
@end

@implementation ScopedTypeRefTests

- (void)testShouldNotRetainByDefault {
  TestType a;
  ScopedTestType ref(&a);
  EXPECT_EQ(0, a.retain_count);
}

- (void)testShouldRetainWithPolicy {
  TestType a;
  ScopedTestType ref(&a, rtc::RetainPolicy::RETAIN);
  EXPECT_EQ(1, a.retain_count);
}

- (void)testShouldReleaseWhenLeavingScope {
  TestType a;
  EXPECT_EQ(0, a.retain_count);
  {
    ScopedTestType ref(&a, rtc::RetainPolicy::RETAIN);
    EXPECT_EQ(1, a.retain_count);
  }
  EXPECT_EQ(0, a.retain_count);
}

- (void)testShouldBeCopyable {
  TestType a;
  EXPECT_EQ(0, a.retain_count);
  {
    ScopedTestType ref1(&a, rtc::RetainPolicy::RETAIN);
    EXPECT_EQ(1, a.retain_count);
    ScopedTestType ref2 = ref1;
    EXPECT_EQ(2, a.retain_count);
  }
  EXPECT_EQ(0, a.retain_count);
}

- (void)testCanReleaseOwnership {
  TestType a;
  EXPECT_EQ(0, a.retain_count);
  {
    ScopedTestType ref(&a, rtc::RetainPolicy::RETAIN);
    EXPECT_EQ(1, a.retain_count);
    TestTypeRef b = ref.release();
  }
  EXPECT_EQ(1, a.retain_count);
}

- (void)testShouldBeTestableForTruthiness {
  ScopedTestType ref;
  EXPECT_FALSE(ref);
  TestType a;
  ref = &a;
  EXPECT_TRUE(ref);
  ref.release();
  EXPECT_FALSE(ref);
}

- (void)testShouldProvideAccessToWrappedType {
  TestType a;
  ScopedTestType ref(&a);
  EXPECT_EQ(&(a.retain_count), &(ref->retain_count));
}

@end

#pragma clang diagnostic pop
