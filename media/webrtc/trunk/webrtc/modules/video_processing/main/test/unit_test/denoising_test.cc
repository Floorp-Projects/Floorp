/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <cstdlib>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_processing/main/interface/video_processing.h"
#include "modules/video_processing/main/test/unit_test/unit_test.h"
#include "system_wrappers/interface/tick_util.h"
#include "testsupport/fileutils.h"

namespace webrtc {

TEST_F(VideoProcessingModuleTest, Denoising)
{
    enum { NumRuns = 10 };
    WebRtc_UWord32 frameNum = 0;

    WebRtc_Word64 minRuntime = 0;
    WebRtc_Word64 avgRuntime = 0;

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
    for (WebRtc_UWord32 runIdx = 0; runIdx < NumRuns; runIdx++)
    {
        TickTime t0;
        TickTime t1;
        TickInterval accTicks;
        WebRtc_Word32 modifiedPixels = 0;

        frameNum = 0;
        scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
        while (fread(video_buffer.get(), 1, _frame_length, _sourceFile) ==
            _frame_length)
        {
          _videoFrame.CreateFrame(_size_y, video_buffer.get(),
                                  _size_uv, video_buffer.get() + _size_y,
                                  _size_uv,
                                  video_buffer.get() + _size_y + _size_uv,
                                  _width, _height,
                                  _width, _half_width, _half_width);
            frameNum++;
            WebRtc_UWord8* sourceBuffer = _videoFrame.buffer(kYPlane);

            // Add noise to a part in video stream
            // Random noise
            // TODO: investigate the effectiveness of this test.

            for (int ir = 0; ir < _height; ir++)
            {
                WebRtc_UWord32 ik = ir * _width;
                for (int ic = 0; ic < _width; ic++)
                {
                    WebRtc_UWord8 r = rand() % 16;
                    r -= 8;
                    if (ir < _height / 4)
                        r = 0;
                    if (ir >= 3 * _height / 4)
                        r = 0;
                    if (ic < _width / 4)
                        r = 0;
                    if (ic >= 3 * _width / 4)
                        r = 0;

                    /*WebRtc_UWord8 pixelValue = 0;
                    if (ir >= _height / 2)
                    { // Region 3 or 4
                        pixelValue = 170;
                    }
                    if (ic >= _width / 2)
                    { // Region 2 or 4
                        pixelValue += 85;
                    }
                    pixelValue += r;
                    sourceBuffer[ik + ic] = pixelValue;
                    */
                    sourceBuffer[ik + ic] += r;
                }
            }

            if (runIdx == 0)
            {
              if (PrintI420VideoFrame(_videoFrame, noiseFile) < 0) {
                return;
              }
            }

            t0 = TickTime::Now();
            ASSERT_GE(modifiedPixels = _vpm->Denoising(&_videoFrame), 0);
            t1 = TickTime::Now();
            accTicks += (t1 - t0);

            if (runIdx == 0)
            {
              if (PrintI420VideoFrame(_videoFrame, noiseFile) < 0) {
                return;
              }
            }
        }
        ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

        printf("%u\n", static_cast<int>(accTicks.Microseconds() / frameNum));
        if (accTicks.Microseconds() < minRuntime || runIdx == 0)
        {
            minRuntime = accTicks.Microseconds();
        }
        avgRuntime += accTicks.Microseconds();

        rewind(_sourceFile);
    }
    ASSERT_EQ(0, fclose(denoiseFile));
    ASSERT_EQ(0, fclose(noiseFile));
    printf("\nAverage run time = %d us / frame\n",
        static_cast<int>(avgRuntime / frameNum / NumRuns));
    printf("Min run time = %d us / frame\n\n",
        static_cast<int>(minRuntime / frameNum));
}

}  // namespace webrtc
