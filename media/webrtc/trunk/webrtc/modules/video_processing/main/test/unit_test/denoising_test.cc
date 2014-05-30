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
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/test/unit_test/video_processing_unittest.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace webrtc {

TEST_F(VideoProcessingModuleTest, DISABLED_ON_ANDROID(Denoising))
{
    enum { NumRuns = 10 };
    uint32_t frameNum = 0;

    int64_t min_runtime = 0;
    int64_t avg_runtime = 0;

    const std::string denoise_filename =
        webrtc::test::OutputPath() + "denoise_testfile.yuv";
    FILE* denoiseFile = fopen(denoise_filename.c_str(), "wb");
    ASSERT_TRUE(denoiseFile != NULL) <<
        "Could not open output file: " << denoise_filename << "\n";

    const std::string noise_filename =
        webrtc::test::OutputPath() + "noise_testfile.yuv";
    FILE* noiseFile = fopen(noise_filename.c_str(), "wb");
    ASSERT_TRUE(noiseFile != NULL) <<
        "Could not open noisy file: " << noise_filename << "\n";

    printf("\nRun time [us / frame]:\n");
    for (uint32_t run_idx = 0; run_idx < NumRuns; run_idx++)
    {
        TickTime t0;
        TickTime t1;
        TickInterval acc_ticks;
        int32_t modifiedPixels = 0;

        frameNum = 0;
        scoped_array<uint8_t> video_buffer(new uint8_t[frame_length_]);
        while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
            frame_length_)
        {
            EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                       width_, height_,
                                       0, kRotateNone, &video_frame_));
            frameNum++;
            uint8_t* sourceBuffer = video_frame_.buffer(kYPlane);

            // Add noise to a part in video stream
            // Random noise
            // TODO: investigate the effectiveness of this test.

            for (int ir = 0; ir < height_; ir++)
            {
                uint32_t ik = ir * width_;
                for (int ic = 0; ic < width_; ic++)
                {
                    uint8_t r = rand() % 16;
                    r -= 8;
                    if (ir < height_ / 4)
                        r = 0;
                    if (ir >= 3 * height_ / 4)
                        r = 0;
                    if (ic < width_ / 4)
                        r = 0;
                    if (ic >= 3 * width_ / 4)
                        r = 0;

                    /*uint8_t pixelValue = 0;
                    if (ir >= height_ / 2)
                    { // Region 3 or 4
                        pixelValue = 170;
                    }
                    if (ic >= width_ / 2)
                    { // Region 2 or 4
                        pixelValue += 85;
                    }
                    pixelValue += r;
                    sourceBuffer[ik + ic] = pixelValue;
                    */
                    sourceBuffer[ik + ic] += r;
                }
            }

            if (run_idx == 0)
            {
              if (PrintI420VideoFrame(video_frame_, noiseFile) < 0) {
                return;
              }
            }

            t0 = TickTime::Now();
            ASSERT_GE(modifiedPixels = vpm_->Denoising(&video_frame_), 0);
            t1 = TickTime::Now();
            acc_ticks += (t1 - t0);

            if (run_idx == 0)
            {
              if (PrintI420VideoFrame(video_frame_, noiseFile) < 0) {
                return;
              }
            }
        }
        ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

        printf("%u\n", static_cast<int>(acc_ticks.Microseconds() / frameNum));
        if (acc_ticks.Microseconds() < min_runtime || run_idx == 0)
        {
            min_runtime = acc_ticks.Microseconds();
        }
        avg_runtime += acc_ticks.Microseconds();

        rewind(source_file_);
    }
    ASSERT_EQ(0, fclose(denoiseFile));
    ASSERT_EQ(0, fclose(noiseFile));
    printf("\nAverage run time = %d us / frame\n",
        static_cast<int>(avg_runtime / frameNum / NumRuns));
    printf("Min run time = %d us / frame\n\n",
        static_cast<int>(min_runtime / frameNum));
}

}  // namespace webrtc
