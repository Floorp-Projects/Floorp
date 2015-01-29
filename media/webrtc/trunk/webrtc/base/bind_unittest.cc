/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/bind.h"
#include "webrtc/base/gunit.h"

namespace rtc {

namespace {

struct MethodBindTester {
  void NullaryVoid() { ++call_count; }
  int NullaryInt() { ++call_count; return 1; }
  int NullaryConst() const { ++call_count; return 2; }
  void UnaryVoid(int dummy) { ++call_count; }
  template <class T> T Identity(T value) { ++call_count; return value; }
  int UnaryByRef(int& value) const { ++call_count; return ++value; }  // NOLINT
  int Multiply(int a, int b) const { ++call_count; return a * b; }
  mutable int call_count;
};

int Return42() { return 42; }
int Negate(int a) { return -a; }
int Multiply(int a, int b) { return a * b; }

}  // namespace

TEST(BindTest, BindToMethod) {
  MethodBindTester object = {0};
  EXPECT_EQ(0, object.call_count);
  Bind(&MethodBindTester::NullaryVoid, &object)();
  EXPECT_EQ(1, object.call_count);
  EXPECT_EQ(1, Bind(&MethodBindTester::NullaryInt, &object)());
  EXPECT_EQ(2, object.call_count);
  EXPECT_EQ(2, Bind(&MethodBindTester::NullaryConst,
                    static_cast<const MethodBindTester*>(&object))());
  EXPECT_EQ(3, object.call_count);
  Bind(&MethodBindTester::UnaryVoid, &object, 5)();
  EXPECT_EQ(4, object.call_count);
  EXPECT_EQ(100, Bind(&MethodBindTester::Identity<int>, &object, 100)());
  EXPECT_EQ(5, object.call_count);
  const std::string string_value("test string");
  EXPECT_EQ(string_value, Bind(&MethodBindTester::Identity<std::string>,
                               &object, string_value)());
  EXPECT_EQ(6, object.call_count);
  int value = 11;
  EXPECT_EQ(12, Bind(&MethodBindTester::UnaryByRef, &object, value)());
  EXPECT_EQ(12, value);
  EXPECT_EQ(7, object.call_count);
  EXPECT_EQ(56, Bind(&MethodBindTester::Multiply, &object, 7, 8)());
  EXPECT_EQ(8, object.call_count);
}

TEST(BindTest, BindToFunction) {
  EXPECT_EQ(42, Bind(&Return42)());
  EXPECT_EQ(3, Bind(&Negate, -3)());
  EXPECT_EQ(56, Bind(&Multiply, 8, 7)());
}

}  // namespace rtc
