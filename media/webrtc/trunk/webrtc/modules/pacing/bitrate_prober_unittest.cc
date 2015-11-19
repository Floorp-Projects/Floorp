/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/pacing/bitrate_prober.h"

namespace webrtc {

TEST(BitrateProberTest, VerifyStatesAndTimeBetweenProbes) {
  BitrateProber prober;
  EXPECT_FALSE(prober.IsProbing());
  int64_t now_ms = 0;
  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));

  prober.SetEnabled(true);
  EXPECT_FALSE(prober.IsProbing());

  prober.MaybeInitializeProbe(300000);
  EXPECT_TRUE(prober.IsProbing());

  EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
  prober.PacketSent(now_ms, 1000);

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(8, prober.TimeUntilNextProbe(now_ms));
    now_ms += 4;
    EXPECT_EQ(4, prober.TimeUntilNextProbe(now_ms));
    now_ms += 4;
    EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
    prober.PacketSent(now_ms, 1000);
  }
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(4, prober.TimeUntilNextProbe(now_ms));
    now_ms += 4;
    EXPECT_EQ(0, prober.TimeUntilNextProbe(now_ms));
    prober.PacketSent(now_ms, 1000);
  }

  EXPECT_EQ(-1, prober.TimeUntilNextProbe(now_ms));
  EXPECT_FALSE(prober.IsProbing());
}
}  // namespace webrtc
