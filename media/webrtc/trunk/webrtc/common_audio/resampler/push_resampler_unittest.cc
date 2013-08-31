/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"

// Quality testing of PushResampler is handled through output_mixer_unittest.cc.

namespace webrtc {

TEST(PushResamplerTest, VerifiesInputParameters) {
  PushResampler resampler;
  EXPECT_EQ(-1, resampler.InitializeIfNeeded(-1, 16000, 1));
  EXPECT_EQ(-1, resampler.InitializeIfNeeded(16000, -1, 1));
  EXPECT_EQ(-1, resampler.InitializeIfNeeded(16000, 16000, 0));
  EXPECT_EQ(-1, resampler.InitializeIfNeeded(16000, 16000, 3));
  EXPECT_EQ(0, resampler.InitializeIfNeeded(16000, 16000, 1));
  EXPECT_EQ(0, resampler.InitializeIfNeeded(16000, 16000, 2));
}

}  // namespace webrtc
