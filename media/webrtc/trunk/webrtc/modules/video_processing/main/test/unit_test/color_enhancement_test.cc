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

namespace webrtc {

TEST_F(VideoProcessingModuleTest, ColorEnhancement)
{
    TickTime t0;
    TickTime t1;
    TickInterval acc_ticks;

    // Use a shorter version of the Foreman clip for this test.
    fclose(source_file_);
    const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif_short", "yuv");
    source_file_  = fopen(video_file.c_str(), "rb");
    ASSERT_TRUE(source_file_ != NULL) <<
        "Cannot read source file: " + video_file + "\n";

    std::string output_file = webrtc::test::OutputPath() +
        "foremanColorEnhancedVPM_cif_short.yuv";
    FILE* modFile = fopen(output_file.c_str(), "w+b");
    ASSERT_TRUE(modFile != NULL) << "Could not open output file.\n";

    uint32_t frameNum = 0;
    scoped_array<uint8_t> video_buffer(new uint8_t[frame_length_]);
    while (fread(video_buffer.get(), 1, frame_length_, source_file_) ==
        frame_length_)
    {
        // Using ConvertToI420 to add stride to the image.
        EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                   width_, height_,
                                   0, kRotateNone, &video_frame_));
        frameNum++;
        t0 = TickTime::Now();
        ASSERT_EQ(0, VideoProcessingModule::ColorEnhancement(&video_frame_));
        t1 = TickTime::Now();
        acc_ticks += t1 - t0;
        if (PrintI420VideoFrame(video_frame_, modFile) < 0) {
          return;
        }
    }
    ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

    printf("\nTime per frame: %d us \n",
        static_cast<int>(acc_ticks.Microseconds() / frameNum));
    rewind(modFile);

    printf("Comparing files...\n\n");
    std::string reference_filename =
        webrtc::test::ResourcePath("foremanColorEnhanced_cif_short", "yuv");
    FILE* refFile = fopen(reference_filename.c_str(), "rb");
    ASSERT_TRUE(refFile != NULL) << "Cannot open reference file: " <<
        reference_filename << "\n"
        "Create the reference by running Matlab script createTable.m.";

    // get file lenghts
    ASSERT_EQ(0, fseek(refFile, 0L, SEEK_END));
    long refLen = ftell(refFile);
    ASSERT_NE(-1L, refLen);
    rewind(refFile);
    ASSERT_EQ(0, fseek(modFile, 0L, SEEK_END));
    long testLen = ftell(modFile);
    ASSERT_NE(-1L, testLen);
    rewind(modFile);
    ASSERT_EQ(refLen, testLen) << "File lengths differ.";

    I420VideoFrame refVideoFrame;
    refVideoFrame.CreateEmptyFrame(width_, height_,
                                   width_, half_width_, half_width_);

    // Compare frame-by-frame.
    scoped_array<uint8_t> ref_buffer(new uint8_t[frame_length_]);
    while (fread(video_buffer.get(), 1, frame_length_, modFile) ==
        frame_length_)
    {
        // Using ConvertToI420 to add stride to the image.
        EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                                   width_, height_,
                                   0, kRotateNone, &video_frame_));
        ASSERT_EQ(frame_length_, fread(ref_buffer.get(), 1, frame_length_,
                                       refFile));
        EXPECT_EQ(0, ConvertToI420(kI420, ref_buffer.get(), 0, 0,
                                   width_, height_,
                                    0, kRotateNone, &refVideoFrame));
        EXPECT_EQ(0, memcmp(video_frame_.buffer(kYPlane),
                            refVideoFrame.buffer(kYPlane),
                            size_y_));
        EXPECT_EQ(0, memcmp(video_frame_.buffer(kUPlane),
                            refVideoFrame.buffer(kUPlane),
                            size_uv_));
        EXPECT_EQ(0, memcmp(video_frame_.buffer(kVPlane),
                            refVideoFrame.buffer(kVPlane),
                            size_uv_));
    }
    ASSERT_NE(0, feof(source_file_)) << "Error reading source file";

    // Verify that all color pixels are enhanced, and no luminance values are
    // altered.

    scoped_array<uint8_t> testFrame(new uint8_t[frame_length_]);

    // Use value 128 as probe value, since we know that this will be changed
    // in the enhancement.
    memset(testFrame.get(), 128, frame_length_);

    I420VideoFrame testVideoFrame;
    testVideoFrame.CreateEmptyFrame(width_, height_,
                                    width_, half_width_, half_width_);
    EXPECT_EQ(0, ConvertToI420(kI420, testFrame.get(), 0, 0,
                               width_, height_, 0, kRotateNone,
                               &testVideoFrame));

    ASSERT_EQ(0, VideoProcessingModule::ColorEnhancement(&testVideoFrame));

    EXPECT_EQ(0, memcmp(testVideoFrame.buffer(kYPlane), testFrame.get(),
                        size_y_))
      << "Function is modifying the luminance.";

    EXPECT_NE(0, memcmp(testVideoFrame.buffer(kUPlane),
                        testFrame.get() + size_y_, size_uv_)) <<
                        "Function is not modifying all chrominance pixels";
    EXPECT_NE(0, memcmp(testVideoFrame.buffer(kVPlane),
                        testFrame.get() + size_y_ + size_uv_, size_uv_)) <<
                        "Function is not modifying all chrominance pixels";

    ASSERT_EQ(0, fclose(refFile));
    ASSERT_EQ(0, fclose(modFile));
}

}  // namespace webrtc
