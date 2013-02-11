/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests for the bitrate estimator.
 */

#include <gtest/gtest.h>

#include "typedefs.h"
#include "bitrate_estimator.h"

namespace {

using webrtc::BitRateStats;

class BitRateStatsTest : public ::testing::Test
{
protected:
    BitRateStatsTest() {};
    BitRateStats bitRate;
};

TEST_F(BitRateStatsTest, TestStrictMode)
{
    WebRtc_Word64 nowMs = 0;
    // Should be initialized to 0.
    EXPECT_EQ(0u, bitRate.BitRate(nowMs));
    bitRate.Update(1500, nowMs);
    // Expecting 24 kbps given a 500 ms window with one 1500 bytes packet.
    EXPECT_EQ(24000u, bitRate.BitRate(nowMs));
    bitRate.Init();
    // Expecting 0 after init.
    EXPECT_EQ(0u, bitRate.BitRate(nowMs));
    for (int i = 0; i < 100000; ++i)
    {
        if (nowMs % 10 == 0)
            bitRate.Update(1500, nowMs);
        // Approximately 1200 kbps expected. Not exact since when packets
        // are removed we will jump 10 ms to the next packet.
        if (nowMs > 0 && nowMs % 500 == 0)
            EXPECT_NEAR(1200000u, bitRate.BitRate(nowMs), 24000u);
        nowMs += 1;
    }
    nowMs += 500;
    // The window is 2 seconds. If nothing has been received for that time
    // the estimate should be 0.
    EXPECT_EQ(0u, bitRate.BitRate(nowMs));
}

}
