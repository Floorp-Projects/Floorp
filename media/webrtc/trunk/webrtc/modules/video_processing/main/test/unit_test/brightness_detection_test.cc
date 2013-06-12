/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test.h"
#include "video_processing.h"

using namespace webrtc;

TEST_F(VideoProcessingModuleTest, BrightnessDetection)
{
    WebRtc_UWord32 frameNum = 0;
    WebRtc_Word32 brightnessWarning = 0;
    WebRtc_UWord32 warningCount = 0;
    scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
    while (fread(video_buffer.get(), 1, _frame_length, _sourceFile) ==
           _frame_length)
    {
        _videoFrame.CreateFrame(_size_y, video_buffer.get(),
                                _size_uv, video_buffer.get() + _size_y,
                                _size_uv, video_buffer.get() + _size_y +
                                _size_uv,
                                _width, _height,
                                _width, _half_width, _half_width);
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
        _videoFrame.CreateFrame(_size_y, video_buffer.get(),
                                _size_uv, video_buffer.get() + _size_y,
                                _size_uv, video_buffer.get() + _size_y +
                                _size_uv,
                                _width, _height,
                                _width, _half_width, _half_width);
        frameNum++;

        WebRtc_UWord8* frame = _videoFrame.buffer(kYPlane);
        WebRtc_UWord32 yTmp = 0;
        for (int yIdx = 0; yIdx < _width * _height; yIdx++)
        {
            yTmp = frame[yIdx] << 1;
            if (yTmp > 255)
            {
                yTmp = 255;
            }
            frame[yIdx] = static_cast<WebRtc_UWord8>(yTmp);
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
        _videoFrame.CreateFrame(_size_y, video_buffer.get(),
                                _size_uv, video_buffer.get() + _size_y,
                                _size_uv, video_buffer.get() + _size_y +
                                _size_uv,
                                _width, _height,
                                _width, _half_width, _half_width);
        frameNum++;

        WebRtc_UWord8* y_plane = _videoFrame.buffer(kYPlane);
        WebRtc_Word32 yTmp = 0;
        for (int yIdx = 0; yIdx < _width * _height; yIdx++)
        {
            yTmp = y_plane[yIdx] >> 1;
            y_plane[yIdx] = static_cast<WebRtc_UWord8>(yTmp);
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
