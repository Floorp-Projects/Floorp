/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/test/unit_test/video_processing_unittest.h"

#include <string>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

// The |sourceFrame| is scaled to |target_width|,|target_height|, using the
// filter mode set to |mode|. The |expected_psnr| is used to verify basic
// quality when the resampled frame is scaled back up/down to the
// original/source size. |expected_psnr| is set to be  ~0.1/0.05dB lower than
// actual PSNR verified under the same conditions.
void TestSize(const I420VideoFrame& sourceFrame, int target_width,
              int target_height, int mode, double expected_psnr,
              VideoProcessingModule* vpm);
bool CompareFrames(const webrtc::I420VideoFrame& frame1,
                  const webrtc::I420VideoFrame& frame2);

VideoProcessingModuleTest::VideoProcessingModuleTest() :
  _vpm(NULL),
  _sourceFile(NULL),
  _width(352),
  _half_width((_width + 1) / 2),
  _height(288),
  _size_y(_width * _height),
  _size_uv(_half_width * ((_height + 1) / 2)),
  _frame_length(CalcBufferSize(kI420, _width, _height))
{
}

void VideoProcessingModuleTest::SetUp()
{
  _vpm = VideoProcessingModule::Create(0);
  ASSERT_TRUE(_vpm != NULL);

  ASSERT_EQ(0, _videoFrame.CreateEmptyFrame(_width, _height, _width,
                                            _half_width, _half_width));

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
  // TODO(mikhal/stefan): Do we need this one?
  VideoProcessingModule::FrameStats stats;
  // Video frame with unallocated buffer.
  I420VideoFrame videoFrame;
  videoFrame.set_width(_width);
  videoFrame.set_height(_height);

  EXPECT_EQ(-3, _vpm->GetFrameStats(&stats, videoFrame));

  EXPECT_EQ(-1, _vpm->ColorEnhancement(&videoFrame));

  EXPECT_EQ(-1, _vpm->Deflickering(&videoFrame, &stats));

  EXPECT_EQ(-1, _vpm->Denoising(&videoFrame));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(videoFrame, stats));
}

TEST_F(VideoProcessingModuleTest, HandleBadStats)
{
  VideoProcessingModule::FrameStats stats;
  scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
  ASSERT_EQ(_frame_length, fread(video_buffer.get(), 1, _frame_length,
                                 _sourceFile));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             _width, _height,
                             0, kRotateNone, &_videoFrame));

  EXPECT_EQ(-1, _vpm->Deflickering(&_videoFrame, &stats));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame, stats));
}

TEST_F(VideoProcessingModuleTest, HandleBadSize)
{
  VideoProcessingModule::FrameStats stats;

  _videoFrame.ResetSize();
  _videoFrame.set_width(_width);
  _videoFrame.set_height(0);
  EXPECT_EQ(-3, _vpm->GetFrameStats(&stats, _videoFrame));

  EXPECT_EQ(-1, _vpm->ColorEnhancement(&_videoFrame));

  EXPECT_EQ(-1, _vpm->Deflickering(&_videoFrame, &stats));

  EXPECT_EQ(-1, _vpm->Denoising(&_videoFrame));

  EXPECT_EQ(-3, _vpm->BrightnessDetection(_videoFrame, stats));

  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->SetTargetResolution(0,0,0));
  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->SetMaxFrameRate(0));

  I420VideoFrame *outFrame = NULL;
  EXPECT_EQ(VPM_PARAMETER_ERROR, _vpm->PreprocessFrame(_videoFrame,
                                                       &outFrame));
}

TEST_F(VideoProcessingModuleTest, IdenticalResultsAfterReset)
{
  I420VideoFrame videoFrame2;
  VideoProcessingModule::FrameStats stats;
  // Only testing non-static functions here.
  scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
  ASSERT_EQ(_frame_length, fread(video_buffer.get(), 1, _frame_length,
                                _sourceFile));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             _width, _height,
                             0, kRotateNone, &_videoFrame));
  ASSERT_EQ(0, _vpm->GetFrameStats(&stats, _videoFrame));
  ASSERT_EQ(0, videoFrame2.CopyFrame(_videoFrame));
  ASSERT_EQ(0, _vpm->Deflickering(&_videoFrame, &stats));
  _vpm->Reset();
  // Retrieve frame stats again in case Deflickering() has zeroed them.
  ASSERT_EQ(0, _vpm->GetFrameStats(&stats, videoFrame2));
  ASSERT_EQ(0, _vpm->Deflickering(&videoFrame2, &stats));
  EXPECT_TRUE(CompareFrames(_videoFrame, videoFrame2));

  ASSERT_EQ(_frame_length, fread(video_buffer.get(), 1, _frame_length,
                                 _sourceFile));
  // Using ConvertToI420 to add stride to the image.
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             _width, _height,
                             0, kRotateNone, &_videoFrame));
  videoFrame2.CopyFrame(_videoFrame);
  EXPECT_TRUE(CompareFrames(_videoFrame, videoFrame2));
  ASSERT_GE(_vpm->Denoising(&_videoFrame), 0);
  _vpm->Reset();
  ASSERT_GE(_vpm->Denoising(&videoFrame2), 0);
  EXPECT_TRUE(CompareFrames(_videoFrame, videoFrame2));

  ASSERT_EQ(_frame_length, fread(video_buffer.get(), 1, _frame_length,
                                 _sourceFile));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             _width, _height,
                             0, kRotateNone, &_videoFrame));
  ASSERT_EQ(0, _vpm->GetFrameStats(&stats, _videoFrame));
  videoFrame2.CopyFrame(_videoFrame);
  ASSERT_EQ(0, _vpm->BrightnessDetection(_videoFrame, stats));
  _vpm->Reset();
  ASSERT_EQ(0, _vpm->BrightnessDetection(videoFrame2, stats));
  EXPECT_TRUE(CompareFrames(_videoFrame, videoFrame2));
}

TEST_F(VideoProcessingModuleTest, FrameStats)
{
  VideoProcessingModule::FrameStats stats;
  scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
  ASSERT_EQ(_frame_length, fread(video_buffer.get(), 1, _frame_length,
                                 _sourceFile));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             _width, _height,
                             0, kRotateNone, &_videoFrame));

  EXPECT_FALSE(_vpm->ValidFrameStats(stats));
  EXPECT_EQ(0, _vpm->GetFrameStats(&stats, _videoFrame));
  EXPECT_TRUE(_vpm->ValidFrameStats(stats));

  printf("\nFrameStats\n");
  printf("mean: %u\nnumPixels: %u\nsubSamplWidth: "
         "%u\nsumSamplHeight: %u\nsum: %u\n\n",
         static_cast<unsigned int>(stats.mean),
         static_cast<unsigned int>(stats.numPixels),
         static_cast<unsigned int>(stats.subSamplHeight),
         static_cast<unsigned int>(stats.subSamplWidth),
         static_cast<unsigned int>(stats.sum));

  _vpm->ClearFrameStats(&stats);
  EXPECT_FALSE(_vpm->ValidFrameStats(stats));
}

TEST_F(VideoProcessingModuleTest, PreprocessorLogic)
{
  // Disable temporal sampling (frame dropping).
  _vpm->EnableTemporalDecimation(false);
  int resolution = 100;
  EXPECT_EQ(VPM_OK, _vpm->SetMaxFrameRate(30));
  EXPECT_EQ(VPM_OK, _vpm->SetTargetResolution(resolution, resolution, 15));
  EXPECT_EQ(VPM_OK, _vpm->SetTargetResolution(resolution, resolution, 30));
  // Disable spatial sampling.
  _vpm->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_EQ(VPM_OK, _vpm->SetTargetResolution(resolution, resolution, 30));
  I420VideoFrame* outFrame = NULL;
  // Set rescaling => output frame != NULL.
  _vpm->SetInputFrameResampleMode(kFastRescaling);
  EXPECT_EQ(VPM_OK, _vpm->SetTargetResolution(resolution, resolution, 30));
  EXPECT_EQ(VPM_OK, _vpm->PreprocessFrame(_videoFrame, &outFrame));
  EXPECT_FALSE(outFrame == NULL);
  if (outFrame) {
    EXPECT_EQ(resolution, outFrame->width());
    EXPECT_EQ(resolution, outFrame->height());
  }
  // No rescaling=> output frame = NULL.
  _vpm->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_EQ(VPM_OK, _vpm->PreprocessFrame(_videoFrame, &outFrame));
  EXPECT_TRUE(outFrame == NULL);
}

TEST_F(VideoProcessingModuleTest, Resampler)
{
  enum { NumRuns = 1 };

  int64_t minRuntime = 0;
  int64_t avgRuntime = 0;

  TickTime t0;
  TickTime t1;
  TickInterval accTicks;

  rewind(_sourceFile);
  ASSERT_TRUE(_sourceFile != NULL) <<
      "Cannot read input file \n";

  // CA not needed here
  _vpm->EnableContentAnalysis(false);
  // no temporal decimation
  _vpm->EnableTemporalDecimation(false);

  // Reading test frame
  scoped_array<uint8_t> video_buffer(new uint8_t[_frame_length]);
  ASSERT_EQ(_frame_length, fread(video_buffer.get(), 1, _frame_length,
                                 _sourceFile));
  // Using ConvertToI420 to add stride to the image.
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             _width, _height,
                             0, kRotateNone, &_videoFrame));

  for (uint32_t runIdx = 0; runIdx < NumRuns; runIdx++)
  {
    // initiate test timer
    t0 = TickTime::Now();

    // Init the sourceFrame with a timestamp.
    _videoFrame.set_render_time_ms(t0.MillisecondTimestamp());
    _videoFrame.set_timestamp(t0.MillisecondTimestamp() * 90);

    // Test scaling to different sizes: source is of |width|/|height| = 352/288.
    // Scaling mode in VPM is currently fixed to kScaleBox (mode = 3).
    TestSize(_videoFrame, 100, 50, 3, 24.0, _vpm);
    TestSize(_videoFrame, 352/4, 288/4, 3, 25.2, _vpm);
    TestSize(_videoFrame, 352/2, 288/2, 3, 28.1, _vpm);
    TestSize(_videoFrame, 352, 288, 3, -1, _vpm);  // no resampling.
    TestSize(_videoFrame, 2*352, 2*288, 3, 32.2, _vpm);
    TestSize(_videoFrame, 400, 256, 3, 31.3, _vpm);
    TestSize(_videoFrame, 480, 640, 3, 32.15, _vpm);
    TestSize(_videoFrame, 960, 720, 3, 32.2, _vpm);
    TestSize(_videoFrame, 1280, 720, 3, 32.15, _vpm);
    // Upsampling to odd size.
    TestSize(_videoFrame, 501, 333, 3, 32.05, _vpm);
    // Downsample to odd size.
    TestSize(_videoFrame, 281, 175, 3, 29.3, _vpm);

    // stop timer
    t1 = TickTime::Now();
    accTicks += (t1 - t0);

    if (accTicks.Microseconds() < minRuntime || runIdx == 0)  {
      minRuntime = accTicks.Microseconds();
    }
    avgRuntime += accTicks.Microseconds();
  }

  printf("\nAverage run time = %d us / frame\n",
         //static_cast<int>(avgRuntime / frameNum / NumRuns));
         static_cast<int>(avgRuntime));
  printf("Min run time = %d us / frame\n\n",
         //static_cast<int>(minRuntime / frameNum));
         static_cast<int>(minRuntime));
}

void TestSize(const I420VideoFrame& source_frame, int target_width,
              int target_height, int mode, double expected_psnr,
              VideoProcessingModule* vpm) {
  int source_width = source_frame.width();
  int source_height = source_frame.height();
  I420VideoFrame* out_frame = NULL;

  ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(target_width, target_height, 30));
  ASSERT_EQ(VPM_OK, vpm->PreprocessFrame(source_frame, &out_frame));

  if (out_frame) {
    EXPECT_EQ(source_frame.render_time_ms(), out_frame->render_time_ms());
    EXPECT_EQ(source_frame.timestamp(), out_frame->timestamp());
  }

  // If the frame was resampled (scale changed) then:
  // (1) verify the new size and write out processed frame for viewing.
  // (2) scale the resampled frame (|out_frame|) back to the original size and
  // compute PSNR relative to |source_frame| (for automatic verification).
  // (3) write out the processed frame for viewing.
  if (target_width != static_cast<int>(source_width) ||
      target_height != static_cast<int>(source_height))  {
    // Write the processed frame to file for visual inspection.
    std::ostringstream filename;
    filename << webrtc::test::OutputPath() << "Resampler_"<< mode << "_" <<
        "from_" << source_width << "x" << source_height << "_to_" <<
        target_width << "x" << target_height << "_30Hz_P420.yuv";
    std::cout << "Watch " << filename.str() << " and verify that it is okay."
        << std::endl;
    FILE* stand_alone_file = fopen(filename.str().c_str(), "wb");
    if (PrintI420VideoFrame(*out_frame, stand_alone_file) < 0) {
      fprintf(stderr, "Failed to write frame for scaling to width/height: "
          " %d %d \n", target_width, target_height);
      return;
    }
    fclose(stand_alone_file);

    I420VideoFrame resampled_source_frame;
    resampled_source_frame.CopyFrame(*out_frame);

    // Scale |resampled_source_frame| back to original/source size.
    ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(source_width,
                                               source_height,
                                               30));
    ASSERT_EQ(VPM_OK, vpm->PreprocessFrame(resampled_source_frame,
                                           &out_frame));

    // Write the processed frame to file for visual inspection.
    std::ostringstream filename2;
    filename2 << webrtc::test::OutputPath() << "Resampler_"<< mode << "_" <<
          "from_" << target_width << "x" << target_height << "_to_" <<
          source_width << "x" << source_height << "_30Hz_P420.yuv";
    std::cout << "Watch " << filename2.str() << " and verify that it is okay."
                << std::endl;
    stand_alone_file = fopen(filename2.str().c_str(), "wb");
    if (PrintI420VideoFrame(*out_frame, stand_alone_file) < 0) {
      fprintf(stderr, "Failed to write frame for scaling to width/height "
              "%d %d \n", source_width, source_height);
      return;
    }
    fclose(stand_alone_file);

    // Compute the PSNR and check expectation.
    double psnr = I420PSNR(&source_frame, out_frame);
    EXPECT_GT(psnr, expected_psnr);
    printf("PSNR: %f. PSNR is between source of size %d %d, and a modified "
        "source which is scaled down/up to: %d %d, and back to source size \n",
        psnr, source_width, source_height, target_width, target_height);
  }
}

bool CompareFrames(const webrtc::I420VideoFrame& frame1,
                   const webrtc::I420VideoFrame& frame2) {
  for (int plane = 0; plane < webrtc::kNumOfPlanes; plane ++) {
    webrtc::PlaneType plane_type = static_cast<webrtc::PlaneType>(plane);
    int allocated_size1 = frame1.allocated_size(plane_type);
    int allocated_size2 = frame2.allocated_size(plane_type);
    if (allocated_size1 != allocated_size2)
      return false;
    const uint8_t* plane_buffer1 = frame1.buffer(plane_type);
    const uint8_t* plane_buffer2 = frame2.buffer(plane_type);
    if (memcmp(plane_buffer1, plane_buffer2, allocated_size1))
      return false;
  }
  return true;
}

}  // namespace webrtc
