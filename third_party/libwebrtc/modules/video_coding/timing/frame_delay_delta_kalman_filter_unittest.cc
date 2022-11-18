/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/frame_delay_delta_kalman_filter.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(FrameDelayDeltaKalmanFilterTest, InitialBandwidthStateIs512kbps) {
  FrameDelayDeltaKalmanFilter filter;

  // The slope corresponds to the estimated bandwidth, and the initial value
  // is set in the implementation.
  EXPECT_EQ(filter.GetSlope(), 1 / (512e3 / 8));
}

}  // namespace
}  // namespace webrtc
