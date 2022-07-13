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
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/1,
                                 /*detection_threshold=*/0.0f);
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, HandlingOfMonoAndDetectionOff) {
  MultiChannelContentDetector mc(/*detect_stereo_content=*/false,
                                 /*num_render_input_channels=*/1,
                                 /*detection_threshold=*/0.0f);
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, HandlingOfDetectionOff) {
  MultiChannelContentDetector mc(/*detect_stereo_content=*/false,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/0.0f);
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
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/0.0f);
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());
}

TEST(MultiChannelContentDetector, DetectionWhenFakeStereo) {
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f);
  EXPECT_FALSE(mc.UpdateDetection(frame));
  EXPECT_FALSE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWhenStereo) {
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/0.0f);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 101.0f);
  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

TEST(MultiChannelContentDetector, DetectionWhenStereoAfterAWhile) {
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/0.0f);
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
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/kThreshold);
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
  MultiChannelContentDetector mc(/*detect_stereo_content=*/true,
                                 /*num_render_input_channels=*/2,
                                 /*detection_threshold=*/kThreshold);
  std::vector<std::vector<std::vector<float>>> frame(
      1, std::vector<std::vector<float>>(2, std::vector<float>(160, 0.0f)));
  std::fill(frame[0][0].begin(), frame[0][0].end(), 100.0f);
  std::fill(frame[0][1].begin(), frame[0][1].end(), 100.0f + kThreshold + 0.1f);

  EXPECT_TRUE(mc.UpdateDetection(frame));
  EXPECT_TRUE(mc.IsMultiChannelContentDetected());

  EXPECT_FALSE(mc.UpdateDetection(frame));
}

}  // namespace webrtc
