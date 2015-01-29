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
extern "C" {
#include "webrtc/modules/audio_processing/aec/aec_core.h"
}
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace webrtc {

TEST(EchoCancellationInternalTest, DelayCorrection) {
  scoped_ptr<AudioProcessing> ap(AudioProcessing::Create(0));
  EXPECT_TRUE(ap->echo_cancellation()->aec_core() == NULL);

  EXPECT_EQ(ap->kNoError, ap->echo_cancellation()->Enable(true));
  EXPECT_TRUE(ap->echo_cancellation()->is_enabled());

  AecCore* aec_core = ap->echo_cancellation()->aec_core();
  ASSERT_TRUE(aec_core != NULL);
  // Disabled by default.
  EXPECT_EQ(0, WebRtcAec_delay_correction_enabled(aec_core));

  Config config;
  config.Set<DelayCorrection>(new DelayCorrection(true));
  ap->SetExtraOptions(config);
  EXPECT_EQ(1, WebRtcAec_delay_correction_enabled(aec_core));

  // Retains setting after initialization.
  EXPECT_EQ(ap->kNoError, ap->Initialize());
  EXPECT_EQ(1, WebRtcAec_delay_correction_enabled(aec_core));

  config.Set<DelayCorrection>(new DelayCorrection(false));
  ap->SetExtraOptions(config);
  EXPECT_EQ(0, WebRtcAec_delay_correction_enabled(aec_core));

  // Retains setting after initialization.
  EXPECT_EQ(ap->kNoError, ap->Initialize());
  EXPECT_EQ(0, WebRtcAec_delay_correction_enabled(aec_core));
}

TEST(EchoCancellationInternalTest, ReportedDelay) {
  scoped_ptr<AudioProcessing> ap(AudioProcessing::Create(0));
  EXPECT_TRUE(ap->echo_cancellation()->aec_core() == NULL);

  EXPECT_EQ(ap->kNoError, ap->echo_cancellation()->Enable(true));
  EXPECT_TRUE(ap->echo_cancellation()->is_enabled());

  AecCore* aec_core = ap->echo_cancellation()->aec_core();
  ASSERT_TRUE(aec_core != NULL);
  // Enabled by default.
  EXPECT_EQ(1, WebRtcAec_reported_delay_enabled(aec_core));

  Config config;
  config.Set<ReportedDelay>(new ReportedDelay(false));
  ap->SetExtraOptions(config);
  EXPECT_EQ(0, WebRtcAec_reported_delay_enabled(aec_core));

  // Retains setting after initialization.
  EXPECT_EQ(ap->kNoError, ap->Initialize());
  EXPECT_EQ(0, WebRtcAec_reported_delay_enabled(aec_core));

  config.Set<ReportedDelay>(new ReportedDelay(true));
  ap->SetExtraOptions(config);
  EXPECT_EQ(1, WebRtcAec_reported_delay_enabled(aec_core));

  // Retains setting after initialization.
  EXPECT_EQ(ap->kNoError, ap->Initialize());
  EXPECT_EQ(1, WebRtcAec_reported_delay_enabled(aec_core));
}

}  // namespace webrtc
