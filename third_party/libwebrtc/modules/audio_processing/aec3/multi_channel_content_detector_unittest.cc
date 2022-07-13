/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/multi_channel_content_detector.h"

#include "test/gtest.h"

namespace webrtc {

TEST(MultiChannelContentDetector, HandlingOfMono) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/1,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, HandlingOfMonoAndDetectionOff) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/false,
      /*num_render_input_channels=*/1,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, HandlingOfDetectionOff) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/false,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);

  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, InitialDetectionOfStereo) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, DetectionWhenFakeStereo) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f);
  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWhenStereo) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);
  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWhenStereoAfterAWhile) {
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));

  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f);
  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));

  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);

  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWithStereoBelowThreshold) {
  constexpr float kThreshold = 1.0f;
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/kThreshold,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f + kThreshold);

  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWithStereoAboveThreshold) {
  constexpr float kThreshold = 1.0f;
  MultiChannelContentDetector mc(
      /*detect_stereo_content=*/true,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/kThreshold,
      /*stereo_detection_timeout_threshold_seconds=*/0);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f + kThreshold + 0.1f);

  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

class MultiChannelContentDetectorTimeoutBehavior
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, int>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannelContentDetector,
                         MultiChannelContentDetectorTimeoutBehavior,
                         ::testing::Combine(::testing::Values(false, true),
                                            ::testing::Values(0, 1, 10)));

TEST_P(MultiChannelContentDetectorTimeoutBehavior,
       TimeOutBehaviorForNonTrueStereo) {
  constexpr int kNumFramesPerSecond = 100;
  const bool detect_stereo_content = std::get<0>(GetParam());
  const int stereo_stereo_detection_timeout_threshold_seconds =
      std::get<1>(GetParam());
  const int stereo_detection_timeout_threshold_frames =
      stereo_stereo_detection_timeout_threshold_seconds * kNumFramesPerSecond;

  MultiChannelContentDetector mc(
      detect_stereo_content,
      /*num_render_input_channels=*/2,
      /*detection_threshold=*/0.0f,
      stereo_stereo_detection_timeout_threshold_seconds);
  std::vector<std::vector<std::vector<float>>> true_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 101.0f)}};

  std::vector<std::vector<std::vector<float>>> fake_stereo_frame = {
      {std::vector<float>(160, 100.0f), std::vector<float>(160, 100.0f)}};

  // Pass fake stereo frames and verify the content detection.
  for (int k = 0; k < 10; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    if (detect_stereo_content) {
      EXPECT_FALSE(mc.IsMultiChannelContentDetected());
    } else {
      EXPECT_TRUE(mc.IsMultiChannelContentDetected());
    }
  }

  // Pass a true stereo frame and verify that it is properly detected.
  if (detect_stereo_content) {
    EXPECT_TRUE(mc.UpdateDetection(true_stereo_frame));
  } else {
    EXPECT_FALSE(mc.UpdateDetection(true_stereo_frame));
  }
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  // Pass fake stereo frames until any timeouts are about to occur.
  for (int k = 0; k < stereo_detection_timeout_threshold_frames - 1; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_TRUE(mc.IsMultiChannelContentDetected());
  }

  // Pass a fake stereo frame and verify that any timeouts properly occur.
  if (detect_stereo_content && stereo_detection_timeout_threshold_frames > 0) {
    EXPECT_TRUE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_FALSE(mc.IsMultiChannelContentDetected());
  } else {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    EXPECT_TRUE(mc.IsMultiChannelContentDetected());
  }

  // Pass fake stereo frames and verify the behavior after any timeout.
  for (int k = 0; k < 10; ++k) {
    EXPECT_FALSE(mc.UpdateDetection(fake_stereo_frame));
    if (detect_stereo_content &&
        stereo_detection_timeout_threshold_frames > 0) {
      EXPECT_FALSE(mc.IsMultiChannelContentDetected());
    } else {
      EXPECT_TRUE(mc.IsMultiChannelContentDetected());
    }
  }
}

}  // namespace webrtc
