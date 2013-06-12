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
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/test/unit_test/unit_test.h"

using namespace webrtc;

TEST_F(VideoProcessingModuleTest, BrightnessDetection)
{
    uint32_t frameNum = 0;
    int32_t brightnessWarning = 0;
    uint32_t warningCount = 0;
    scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
    while (fread(video_buffer.get(), 1, _frame_length, _sourceFile) ==
           _frame_length)
    {
      EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                 _width, _height,
                                 0, kRotateNone, &_videoFrame));
        frameNum++;
        VideoProcessingModule::FrameStats stats;
        ASSERT_EQ(0, _vpm->GetFrameStats(&stats, _videoFrame));
        ASSERT_GE(brightnessWarning = _vpm->BrightnessDetection(_videoFrame,
                                                                stats), 0);
        if (brightnessWarning != VideoProcessingModule::kNoWarning)
        {
            warningCount++;
        }
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

    // Expect few warnings
    float warningProportion = static_cast<float>(warningCount) / frameNum * 100;
    printf("\nWarning proportions:\n");
    printf("Stock foreman: %.1f %%\n", warningProportion);
    EXPECT_LT(warningProportion, 10);

    rewind(_sourceFile);
    frameNum = 0;
    warningCount = 0;
    while (fread(video_buffer.get(), 1, _frame_length, _sourceFile) ==
        _frame_length &&
        frameNum < 300)
    {
        EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                   _width, _height,
                                   0, kRotateNone, &_videoFrame));
        frameNum++;

        uint8_t* frame = _videoFrame.buffer(kYPlane);
        uint32_t yTmp = 0;
        for (int yIdx = 0; yIdx < _width * _height; yIdx++)
        {
            yTmp = frame[yIdx] << 1;
            if (yTmp > 255)
            {
                yTmp = 255;
            }
            frame[yIdx] = static_cast<uint8_t>(yTmp);
        }

        VideoProcessingModule::FrameStats stats;
        ASSERT_EQ(0, _vpm->GetFrameStats(&stats, _videoFrame));
        ASSERT_GE(brightnessWarning = _vpm->BrightnessDetection(_videoFrame,
                                                                stats), 0);
        EXPECT_NE(VideoProcessingModule::kDarkWarning, brightnessWarning);
        if (brightnessWarning == VideoProcessingModule::kBrightWarning)
        {
            warningCount++;
        }
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

    // Expect many brightness warnings
    warningProportion = static_cast<float>(warningCount) / frameNum * 100;
    printf("Bright foreman: %.1f %%\n", warningProportion);
    EXPECT_GT(warningProportion, 95);

    rewind(_sourceFile);
    frameNum = 0;
    warningCount = 0;
    while (fread(video_buffer.get(), 1, _frame_length, _sourceFile) ==
        _frame_length && frameNum < 300)
    {
        EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                   _width, _height,
                                   0, kRotateNone, &_videoFrame));
        frameNum++;

        uint8_t* y_plane = _videoFrame.buffer(kYPlane);
        int32_t yTmp = 0;
        for (int yIdx = 0; yIdx < _width * _height; yIdx++)
        {
            yTmp = y_plane[yIdx] >> 1;
            y_plane[yIdx] = static_cast<uint8_t>(yTmp);
        }

        VideoProcessingModule::FrameStats stats;
        ASSERT_EQ(0, _vpm->GetFrameStats(&stats, _videoFrame));
        ASSERT_GE(brightnessWarning = _vpm->BrightnessDetection(_videoFrame,
                                                                stats), 0);
        EXPECT_NE(VideoProcessingModule::kBrightWarning, brightnessWarning);
        if (brightnessWarning == VideoProcessingModule::kDarkWarning)
        {
            warningCount++;
        }
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

    // Expect many darkness warnings
    warningProportion = static_cast<float>(warningCount) / frameNum * 100;
    printf("Dark foreman: %.1f %%\n\n", warningProportion);
    EXPECT_GT(warningProportion, 90);
}
