/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_processing/main/test/unit_test/unit_test.h"

#include <string>

#include "common_video/libyuv/include/libyuv.h"
#include "system_wrappers/interface/tick_util.h"
#include "testsupport/fileutils.h"

namespace webrtc {

void TestSize(VideoFrame& sourceFrame,
              WebRtc_UWord32 targetWidth, WebRtc_UWord32 targetHeight,
              WebRtc_UWord32 mode, VideoProcessingModule *vpm);

VideoProcessingModuleTest::VideoProcessingModuleTest() :
  _vpm(NULL),
  _sourceFile(NULL),
  _width(352),
  _height(288),
  _frameLength(CalcBufferSize(kI420, 352, 288))
{
}

void VideoProcessingModuleTest::SetUp()
{
  _vpm = VideoProcessingModule::Create(0);
  ASSERT_TRUE(_vpm != NULL);

  ASSERT_EQ(0, _videoFrame.VerifyAndAllocate(_frameLength));
  _videoFrame.SetWidth(_width);
  _videoFrame.SetHeight(_height);

  const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  _sourceFile  = fopen(video_file.c_str(),"rb");
  ASSERT_TRUE(_sourceFile != NULL) <<
      "Cannot read source file: " + video_file + "\n";
}

void VideoProcessingModuleTest::TearDown()
{
  if (_sourceFile != NULL)  {
    ASSERT_EQ(0, fclose(_sourceFile));
  }
  _sourceFile = NULL;

  if (_vpm != NULL)  {
    VideoProcessingModule::Destroy(_vpm);
  }
  _vpm = NULL;
}

TEST_F(VideoProcessingModuleTest, HandleNullBuffer)
{
  VideoProcessingModule::FrameStats stats;
  ASSERT_EQ(0, _vpm->GetFrameStats(stats, _videoFrame));
  // Video frame with unallocated buffer.
  VideoFrame videoFrame;
  videoFrame.SetWidth(_width);
  videoFrame.SetHeight(_height);

  EXPECT_EQ(-3, _vpm->GetFrameStats(stats, NULL, _width, _height));
  EXPECT_EQ(-3, _vpm->GetFrameStats(stats, videoFrame));

  EXPECT_EQ(-1, _vpm->ColorEnhancement(NULL, _width, _height));
  EXPECT_EQ(-1, _vpm->ColorEnhancement(videoFrame));

  EXPECT_EQ(-1, _vpm->Deflickering(NULL, _width, _height, 0, stats));
  EXPECT_EQ(-1, _vpm->Deflickering(videoFrame, stats));

  EXPECT_EQ(-1, _vpm->Denoising(NULL, _width, _height));
  EXPECT_EQ(-1, _vpm->Denoising(videoFrame));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(NULL, _width, _height, stats));
  EXPECT_EQ(-3, _vpm->BrightnessDetection(videoFrame, stats));

  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->PreprocessFrame(NULL, NULL));
}

TEST_F(VideoProcessingModuleTest, HandleBadStats)
{
  VideoProcessingModule::FrameStats stats;

  ASSERT_EQ(_frameLength, fread(_videoFrame.Buffer(), 1, _frameLength,
                                _sourceFile));

  EXPECT_EQ(-1, _vpm->Deflickering(_videoFrame.Buffer(), _width, _height, 0,
                                   stats));
  EXPECT_EQ(-1, _vpm->Deflickering(_videoFrame, stats));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame.Buffer(), _width,
                                          _height, stats));
  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame, stats));
}

TEST_F(VideoProcessingModuleTest, HandleBadSize)
{
  VideoProcessingModule::FrameStats stats;
  ASSERT_EQ(0, _vpm->GetFrameStats(stats, _videoFrame));

  // Bad width
  _videoFrame.SetWidth(0);
  EXPECT_EQ(-3, _vpm->GetFrameStats(stats, _videoFrame.Buffer(), 0, _height));
  EXPECT_EQ(-3, _vpm->GetFrameStats(stats, _videoFrame));

  EXPECT_EQ(-1, _vpm->ColorEnhancement(_videoFrame.Buffer(), 0, _height));
  EXPECT_EQ(-1, _vpm->ColorEnhancement(_videoFrame));

  EXPECT_EQ(-1, _vpm->Deflickering(_videoFrame.Buffer(), 0, _height, 0,
                                   stats));
  EXPECT_EQ(-1, _vpm->Deflickering(_videoFrame, stats));

  EXPECT_EQ(-1, _vpm->Denoising(_videoFrame.Buffer(), 0, _height));
  EXPECT_EQ(-1, _vpm->Denoising(_videoFrame));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame.Buffer(), 0, _height,
                                          stats));
  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame, stats));


  // Bad height
  _videoFrame.SetWidth(_width);
  _videoFrame.SetHeight(0);
  EXPECT_EQ(-3, _vpm->GetFrameStats(stats, _videoFrame.Buffer(), _width, 0));
  EXPECT_EQ(-3, _vpm->GetFrameStats(stats, _videoFrame));

  EXPECT_EQ(-1, _vpm->ColorEnhancement(_videoFrame.Buffer(), _width, 0));
  EXPECT_EQ(-1, _vpm->ColorEnhancement(_videoFrame));

  EXPECT_EQ(-1, _vpm->Deflickering(_videoFrame.Buffer(), _width, 0, 0,
                                   stats));
  EXPECT_EQ(-1, _vpm->Deflickering(_videoFrame, stats));

  EXPECT_EQ(-1, _vpm->Denoising(_videoFrame.Buffer(), _width, 0));
  EXPECT_EQ(-1, _vpm->Denoising(_videoFrame));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame.Buffer(), _width, 0,
                                          stats));
  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame, stats));

  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->SetTargetResolution(0,0,0));
  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->SetMaxFrameRate(0));

  VideoFrame *outFrame = NULL;
  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->PreprocessFrame(&_videoFrame,
                                                       &outFrame));
}

TEST_F(VideoProcessingModuleTest, IdenticalResultsAfterReset)
{
  VideoFrame videoFrame2;
  VideoProcessingModule::FrameStats stats;

  ASSERT_EQ(0, videoFrame2.VerifyAndAllocate(_frameLength));
  videoFrame2.SetWidth(_width);
  videoFrame2.SetHeight(_height);

  // Only testing non-static functions here.
  ASSERT_EQ(_frameLength, fread(_videoFrame.Buffer(), 1, _frameLength,
                                _sourceFile));
  ASSERT_EQ(0, _vpm->GetFrameStats(stats, _videoFrame));
  memcpy(videoFrame2.Buffer(), _videoFrame.Buffer(), _frameLength);
  ASSERT_EQ(0, _vpm->Deflickering(_videoFrame, stats));
  _vpm->Reset();
  // Retrieve frame stats again in case Deflickering() has zeroed them.
  ASSERT_EQ(0, _vpm->GetFrameStats(stats, videoFrame2));
  ASSERT_EQ(0, _vpm->Deflickering(videoFrame2, stats));
  EXPECT_EQ(0, memcmp(_videoFrame.Buffer(), videoFrame2.Buffer(),
                      _frameLength));

  ASSERT_EQ(_frameLength, fread(_videoFrame.Buffer(), 1, _frameLength,
                                _sourceFile));
  memcpy(videoFrame2.Buffer(), _videoFrame.Buffer(), _frameLength);
  ASSERT_GE(_vpm->Denoising(_videoFrame), 0);
  _vpm->Reset();
  ASSERT_GE(_vpm->Denoising(videoFrame2), 0);
  EXPECT_EQ(0, memcmp(_videoFrame.Buffer(), videoFrame2.Buffer(),
                      _frameLength));

  ASSERT_EQ(_frameLength, fread(_videoFrame.Buffer(), 1, _frameLength,
                                _sourceFile));
  ASSERT_EQ(0, _vpm->GetFrameStats(stats, _videoFrame));
  memcpy(videoFrame2.Buffer(), _videoFrame.Buffer(), _frameLength);
  ASSERT_EQ(0, _vpm->BrightnessDetection(_videoFrame, stats));
  _vpm->Reset();
  ASSERT_EQ(0, _vpm->BrightnessDetection(videoFrame2, stats));
  EXPECT_EQ(0, memcmp(_videoFrame.Buffer(), videoFrame2.Buffer(),
                      _frameLength));
}

TEST_F(VideoProcessingModuleTest, FrameStats)
{
  VideoProcessingModule::FrameStats stats;
  ASSERT_EQ(_frameLength, fread(_videoFrame.Buffer(), 1, _frameLength,
                                _sourceFile));

  EXPECT_FALSE(_vpm->ValidFrameStats(stats));
  EXPECT_EQ(0, _vpm->GetFrameStats(stats, _videoFrame));
  EXPECT_TRUE(_vpm->ValidFrameStats(stats));

  printf("\nFrameStats\n");
  printf("mean: %u\nnumPixels: %u\nsubSamplWidth: "
         "%u\nsumSamplHeight: %u\nsum: %u\n\n",
         static_cast<unsigned int>(stats.mean),
         static_cast<unsigned int>(stats.numPixels),
         static_cast<unsigned int>(stats.subSamplHeight),
         static_cast<unsigned int>(stats.subSamplWidth),
         static_cast<unsigned int>(stats.sum));

  _vpm->ClearFrameStats(stats);
  EXPECT_FALSE(_vpm->ValidFrameStats(stats));
}

TEST_F(VideoProcessingModuleTest, PreprocessorLogic)
{
  // Disable temporal sampling
  _vpm->EnableTemporalDecimation(false);
  ASSERT_EQ(VPM_OK, _vpm->SetMaxFrameRate(30));
  ASSERT_EQ(VPM_OK, _vpm->SetTargetResolution(100, 100, 15));
  // Revert
  _vpm->EnableTemporalDecimation(true);
  ASSERT_EQ(VPM_OK, _vpm->SetTargetResolution(100, 100, 30));
  // Disable spatial sampling
  _vpm->SetInputFrameResampleMode(kNoRescaling);
  ASSERT_EQ(VPM_OK, _vpm->SetTargetResolution(100, 100, 30));
  VideoFrame *outFrame = NULL;
  ASSERT_EQ(VPM_OK, _vpm->PreprocessFrame(&_videoFrame, &outFrame));
  // No rescaling=> output frame = NULL
  ASSERT_TRUE(outFrame == NULL);
}

TEST_F(VideoProcessingModuleTest, Resampler)
{
  enum { NumRuns = 1 };

  WebRtc_Word64 minRuntime = 0;
  WebRtc_Word64 avgRuntime = 0;

  TickTime t0;
  TickTime t1;
  TickInterval accTicks;
  WebRtc_Word32 height = 288;
  WebRtc_Word32 width = 352;
  WebRtc_Word32 lengthSourceFrame = width*height*3/2;

  rewind(_sourceFile);
  ASSERT_TRUE(_sourceFile != NULL) <<
      "Cannot read input file \n";

  // CA not needed here
  _vpm->EnableContentAnalysis(false);
  // no temporal decimation
  _vpm->EnableTemporalDecimation(false);

  // Reading test frame
  VideoFrame sourceFrame;
  ASSERT_EQ(0, sourceFrame.VerifyAndAllocate(lengthSourceFrame));
  EXPECT_GT(fread(sourceFrame.Buffer(), 1, lengthSourceFrame, _sourceFile), 0u);
  ASSERT_EQ(0, sourceFrame.SetLength(lengthSourceFrame));
  sourceFrame.SetHeight(height);
  sourceFrame.SetWidth(width);

  for (WebRtc_UWord32 runIdx = 0; runIdx < NumRuns; runIdx++)
  {
    // initiate test timer
    t0 = TickTime::Now();

    // kFastRescaling
    _vpm->SetInputFrameResampleMode(kFastRescaling);
    // TESTING DIFFERENT SIZES
    TestSize(sourceFrame, 100, 50, 1, _vpm);  // Cut, decimation 1x, interpolate
    TestSize(sourceFrame, 352/2, 288/2, 1, _vpm);  // Even decimation
    TestSize(sourceFrame, 352, 288, 1, _vpm);      // No resampling
    TestSize(sourceFrame, 2*352, 2*288,1,  _vpm);  // Even upsampling
    TestSize(sourceFrame, 400, 256, 1, _vpm);      // Upsampling 1.5x and cut
    TestSize(sourceFrame, 960, 720, 1, _vpm);      // Upsampling 3.5x and cut
    TestSize(sourceFrame, 1280, 720, 1, _vpm);     // Upsampling 4x and cut

    // kBiLinear
    _vpm->SetInputFrameResampleMode(kBiLinear);
    // TESTING DIFFERENT SIZES
    TestSize(sourceFrame, 352/4, 288/4, 2, _vpm);
    TestSize(sourceFrame, 352/2, 288/2, 2, _vpm);
    TestSize(sourceFrame, 2*352, 2*288,2, _vpm);
    TestSize(sourceFrame, 480, 640, 2, _vpm);
    TestSize(sourceFrame, 960, 720, 2, _vpm);
    TestSize(sourceFrame, 1280, 720, 2, _vpm);
    // stop timer
    t1 = TickTime::Now();
    accTicks += t1 - t0;

    if (accTicks.Microseconds() < minRuntime || runIdx == 0)  {
      minRuntime = accTicks.Microseconds();
    }
    avgRuntime += accTicks.Microseconds();
  }

  sourceFrame.Free();

  printf("\nAverage run time = %d us / frame\n",
         //static_cast<int>(avgRuntime / frameNum / NumRuns));
         static_cast<int>(avgRuntime));
  printf("Min run time = %d us / frame\n\n",
         //static_cast<int>(minRuntime / frameNum));
         static_cast<int>(minRuntime));
}

void TestSize(VideoFrame& sourceFrame, WebRtc_UWord32 targetWidth,
              WebRtc_UWord32 targetHeight,
              WebRtc_UWord32 mode, VideoProcessingModule *vpm)
{
    VideoFrame *outFrame = NULL;
  std::ostringstream filename;
  filename << webrtc::test::OutputPath() << "Resampler_"<< mode << "_" <<
      targetWidth << "x" << targetHeight << "_30Hz_P420.yuv";
  // TODO(kjellander): Add automatic verification of these output files:
  std::cout << "Watch " << filename.str() << " and verify that it is okay."
            << std::endl;
  FILE* standAloneFile = fopen(filename.str().c_str(), "wb");
  ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(targetWidth, targetHeight, 30));
  ASSERT_EQ(VPM_OK, vpm->PreprocessFrame(&sourceFrame, &outFrame));
  // Length should be updated only if frame was resampled
  if (targetWidth != sourceFrame.Width() ||
      targetHeight != sourceFrame.Height())  {
    ASSERT_EQ((targetWidth * targetHeight * 3 / 2), outFrame->Length());
    // Write to file for visual inspection
    fwrite(outFrame->Buffer(), 1, outFrame->Length(), standAloneFile);
    outFrame->Free();
  }
  fclose(standAloneFile);
}

}  // namespace webrtc
