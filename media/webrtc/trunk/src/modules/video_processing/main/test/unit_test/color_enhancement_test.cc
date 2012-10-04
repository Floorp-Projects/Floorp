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

#include "modules/video_processing/main/interface/video_processing.h"
#include "modules/video_processing/main/test/unit_test/unit_test.h"
#include "system_wrappers/interface/tick_util.h"
#include "testsupport/fileutils.h"

namespace webrtc {

TEST_F(VideoProcessingModuleTest, ColorEnhancement)
{
    TickTime t0;
    TickTime t1;
    TickInterval accTicks;

    // Use a shorter version of the Foreman clip for this test.
    fclose(_sourceFile);
    const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif_short", "yuv");
    _sourceFile  = fopen(video_file.c_str(), "rb");
    ASSERT_TRUE(_sourceFile != NULL) <<
        "Cannot read source file: " + video_file + "\n";

    std::string output_file = webrtc::test::OutputPath() +
        "foremanColorEnhancedVPM_cif_short.yuv";
    FILE* modFile = fopen(output_file.c_str(), "w+b");
    ASSERT_TRUE(modFile != NULL) << "Could not open output file.\n";

    WebRtc_UWord32 frameNum = 0;
    while (fread(_videoFrame.Buffer(), 1, _frameLength, _sourceFile) == _frameLength)
    {
        frameNum++;
        t0 = TickTime::Now();
        ASSERT_EQ(0, VideoProcessingModule::ColorEnhancement(_videoFrame));
        t1 = TickTime::Now();
        accTicks += t1 - t0;
        if (fwrite(_videoFrame.Buffer(), 1, _frameLength,
                   modFile) !=  _frameLength) {
          return;
        }
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

    printf("\nTime per frame: %d us \n",
        static_cast<int>(accTicks.Microseconds() / frameNum));
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

    VideoFrame refVideoFrame;
    refVideoFrame.VerifyAndAllocate(_frameLength);
    refVideoFrame.SetWidth(_width);
    refVideoFrame.SetHeight(_height);

    // Compare frame-by-frame.
    while (fread(_videoFrame.Buffer(), 1, _frameLength, modFile) == _frameLength)
    {
        ASSERT_EQ(_frameLength, fread(refVideoFrame.Buffer(), 1, _frameLength, refFile));
        EXPECT_EQ(0, memcmp(_videoFrame.Buffer(), refVideoFrame.Buffer(), _frameLength));
    }
    ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

    // Verify that all color pixels are enhanced, that no luminance values are altered,
    // and that the function does not write outside the vector.
    WebRtc_UWord32 safeGuard = 1000;
    WebRtc_UWord32 numPixels = 352*288; // CIF size
    WebRtc_UWord8 *testFrame = new WebRtc_UWord8[numPixels + (numPixels / 2) + (2 * safeGuard)];
    WebRtc_UWord8 *refFrame = new WebRtc_UWord8[numPixels + (numPixels / 2) + (2 * safeGuard)];

    // use value 128 as probe value, since we know that this will be changed in the enhancement
    memset(testFrame, 128, safeGuard);
    memset(&testFrame[safeGuard], 128, numPixels);
    memset(&testFrame[safeGuard + numPixels], 128, numPixels / 2);
    memset(&testFrame[safeGuard + numPixels + (numPixels / 2)], 128, safeGuard);

    memcpy(refFrame, testFrame, numPixels + (numPixels / 2) + (2 * safeGuard));

    ASSERT_EQ(0, VideoProcessingModule::ColorEnhancement(&testFrame[safeGuard], 352, 288));

    EXPECT_EQ(0, memcmp(testFrame, refFrame, safeGuard)) <<
        "Function is writing outside the frame memory.";
    
    EXPECT_EQ(0, memcmp(&testFrame[safeGuard + numPixels + (numPixels / 2)], 
        &refFrame[safeGuard + numPixels + (numPixels / 2)], safeGuard)) <<
        "Function is writing outside the frame memory.";

    EXPECT_EQ(0, memcmp(&testFrame[safeGuard], &refFrame[safeGuard], numPixels)) <<
        "Function is modifying the luminance.";

    EXPECT_NE(0, memcmp(&testFrame[safeGuard + numPixels],
        &refFrame[safeGuard + numPixels], numPixels / 2)) <<
        "Function is not modifying all chrominance pixels";

    ASSERT_EQ(0, fclose(refFile));
    ASSERT_EQ(0, fclose(modFile));
    delete [] testFrame;
    delete [] refFrame;
}

}  // namespace webrtc
