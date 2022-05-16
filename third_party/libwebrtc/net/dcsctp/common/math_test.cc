/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/common/math.h"

#include "test/gmock.h"

namespace dcsctp {
namespace {

TEST(MathUtilTest, CanRoundUpTo4) {
  EXPECT_EQ(RoundUpTo4(0), 0);
  EXPECT_EQ(RoundUpTo4(1), 4);
  EXPECT_EQ(RoundUpTo4(2), 4);
  EXPECT_EQ(RoundUpTo4(3), 4);
  EXPECT_EQ(RoundUpTo4(4), 4);
  EXPECT_EQ(RoundUpTo4(5), 8);
  EXPECT_EQ(RoundUpTo4(6), 8);
  EXPECT_EQ(RoundUpTo4(7), 8);
  EXPECT_EQ(RoundUpTo4(8), 8);
  EXPECT_EQ(RoundUpTo4(10000000000), 10000000000);
  EXPECT_EQ(RoundUpTo4(10000000001), 10000000004);
}

}  // namespace
}  // namespace dcsctp
