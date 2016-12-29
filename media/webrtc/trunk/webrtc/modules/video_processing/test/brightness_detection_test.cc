/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_processing/test/video_processing_unittest.h"

namespace webrtc {

#if defined(WEBRTC_IOS)
#define MAYBE_BrightnessDetection DISABLED_BrightnessDetection
#else
#define MAYBE_BrightnessDetection BrightnessDetection
#endif
TEST_F(VideoProcessingTest, MAYBE_BrightnessDetection) {
  uint32_t frameNum = 0;
  int32_t brightnessWarning = 0;
  uint32_t warningCount = 0;
  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
         frame_length_) {
    EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                               0, kVideoRotation_0, &video_frame_));
    frameNum++;
    VideoProcessing::FrameStats stats;
    vp_->GetFrameStats(video_frame_, &stats);
    EXPECT_GT(stats.num_pixels, 0u);
    ASSERT_GE(brightnessWarning = vp_->BrightnessDetection(video_frame_, stats),
              0);
    if (brightnessWarning != VideoProcessing::kNoWarning) {
      warningCount++;
    }
  }
  ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

  // Expect few warnings
  float warningProportion = static_cast<float>(warningCount) / frameNum * 100;
  printf("\nWarning proportions:\n");
  printf("Stock foreman: %.1f %%\n", warningProportion);
  EXPECT_LT(warningProportion, 10);

  rewind(source_file_);
  frameNum = 0;
  warningCount = 0;
  while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
             frame_length_ &&
         frameNum < 300) {
    EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                               0, kVideoRotation_0, &video_frame_));
    frameNum++;

    uint8_t* frame = video_frame_.buffer(kYPlane);
    uint32_t yTmp = 0;
    for (int yIdx = 0; yIdx < width_ * height_; yIdx++) {
      yTmp = frame[yIdx] << 1;
      if (yTmp > 255) {
        yTmp = 255;
      }
      frame[yIdx] = static_cast<uint8_t>(yTmp);
    }

    VideoProcessing::FrameStats stats;
    vp_->GetFrameStats(video_frame_, &stats);
    EXPECT_GT(stats.num_pixels, 0u);
    ASSERT_GE(brightnessWarning = vp_->BrightnessDetection(video_frame_, stats),
              0);
    EXPECT_NE(VideoProcessing::kDarkWarning, brightnessWarning);
    if (brightnessWarning == VideoProcessing::kBrightWarning) {
      warningCount++;
    }
  }
  ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

  // Expect many brightness warnings
  warningProportion = static_cast<float>(warningCount) / frameNum * 100;
  printf("Bright foreman: %.1f %%\n", warningProportion);
  EXPECT_GT(warningProportion, 95);

  rewind(source_file_);
  frameNum = 0;
  warningCount = 0;
  while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
             frame_length_ &&
         frameNum < 300) {
    EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                               0, kVideoRotation_0, &video_frame_));
    frameNum++;

    uint8_t* y_plane = video_frame_.buffer(kYPlane);
    int32_t yTmp = 0;
    for (int yIdx = 0; yIdx < width_ * height_; yIdx++) {
      yTmp = y_plane[yIdx] >> 1;
      y_plane[yIdx] = static_cast<uint8_t>(yTmp);
    }

    VideoProcessing::FrameStats stats;
    vp_->GetFrameStats(video_frame_, &stats);
    EXPECT_GT(stats.num_pixels, 0u);
    ASSERT_GE(brightnessWarning = vp_->BrightnessDetection(video_frame_, stats),
              0);
    EXPECT_NE(VideoProcessing::kBrightWarning, brightnessWarning);
    if (brightnessWarning == VideoProcessing::kDarkWarning) {
      warningCount++;
    }
  }
  ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

  // Expect many darkness warnings
  warningProportion = static_cast<float>(warningCount) / frameNum * 100;
  printf("Dark foreman: %.1f %%\n\n", warningProportion);
  EXPECT_GT(warningProportion, 90);
}
}  // namespace webrtc
