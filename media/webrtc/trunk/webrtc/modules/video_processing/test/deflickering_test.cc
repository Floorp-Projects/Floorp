/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_processing/test/video_processing_unittest.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_Deflickering) {
#else
TEST_F(VideoProcessingTest, Deflickering) {
#endif
  enum { NumRuns = 30 };
  uint32_t frameNum = 0;
  const uint32_t frame_rate = 15;

  int64_t min_runtime = 0;
  int64_t avg_runtime = 0;

  // Close automatically opened Foreman.
  fclose(source_file_);
  const std::string input_file =
      webrtc::test::ResourcePath("deflicker_before_cif_short", "yuv");
  source_file_ = fopen(input_file.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read input file: " << input_file
                                    << "\n";

  const std::string output_file =
      webrtc::test::OutputPath() + "deflicker_output_cif_short.yuv";
  FILE* deflickerFile = fopen(output_file.c_str(), "wb");
  ASSERT_TRUE(deflickerFile != NULL)
      << "Could not open output file: " << output_file << "\n";

  printf("\nRun time [us / frame]:\n");
  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  for (uint32_t run_idx = 0; run_idx < NumRuns; run_idx++) {
    TickTime t0;
    TickTime t1;
    TickInterval acc_ticks;
    uint32_t timeStamp = 1;

    frameNum = 0;
    while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
           frame_length_) {
      frameNum++;
      EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_,
                                 height_, 0, kVideoRotation_0, &video_frame_));
      video_frame_.set_timestamp(timeStamp);

      t0 = TickTime::Now();
      VideoProcessing::FrameStats stats;
      vp_->GetFrameStats(video_frame_, &stats);
      EXPECT_GT(stats.num_pixels, 0u);
      ASSERT_EQ(0, vp_->Deflickering(&video_frame_, &stats));
      t1 = TickTime::Now();
      acc_ticks += (t1 - t0);

      if (run_idx == 0) {
        if (PrintVideoFrame(video_frame_, deflickerFile) < 0) {
          return;
        }
      }
      timeStamp += (90000 / frame_rate);
    }
    ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

    printf("%u\n", static_cast<int>(acc_ticks.Microseconds() / frameNum));
    if (acc_ticks.Microseconds() < min_runtime || run_idx == 0) {
      min_runtime = acc_ticks.Microseconds();
    }
    avg_runtime += acc_ticks.Microseconds();

    rewind(source_file_);
  }
  ASSERT_EQ(0, fclose(deflickerFile));
  // TODO(kjellander): Add verification of deflicker output file.

  printf("\nAverage run time = %d us / frame\n",
         static_cast<int>(avg_runtime / frameNum / NumRuns));
  printf("Min run time = %d us / frame\n\n",
         static_cast<int>(min_runtime / frameNum));
}

}  // namespace webrtc
