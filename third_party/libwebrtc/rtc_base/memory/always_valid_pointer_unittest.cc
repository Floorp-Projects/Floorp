/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/memory/always_valid_pointer.h"

#include <string>

#include "test/gtest.h"

namespace webrtc {

TEST(AlwaysValidPointerTest, DefaultToEmptyValue) {
  AlwaysValidPointer<std::string> ptr(nullptr);
  EXPECT_EQ(*ptr, "");
}
TEST(AlwaysValidPointerTest, DefaultWithForwardedArgument) {
  AlwaysValidPointer<std::string> ptr(nullptr, "test");
  EXPECT_EQ(*ptr, "test");
}
TEST(AlwaysValidPointerTest, DefaultToSubclass) {
  struct A {
    virtual ~A() {}
    virtual int f() = 0;
  };
  struct B : public A {
    int b = 0;
    explicit B(int val) : b(val) {}
    virtual ~B() {}
    int f() override { return b; }
  };
  AlwaysValidPointer<A, B> ptr(nullptr, 3);
  EXPECT_EQ(ptr->f(), 3);
  EXPECT_EQ((*ptr).f(), 3);
  EXPECT_EQ(ptr.get()->f(), 3);
}
TEST(AlwaysValidPointerTest, NonDefaultValue) {
  std::string str("keso");
  AlwaysValidPointer<std::string> ptr(&str, "test");
  EXPECT_EQ(*ptr, "keso");
}

}  // namespace webrtc
