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

// The |sourceFrame| is scaled to |targetwidth_|,|targetheight_|, using the
// filter mode set to |mode|. The |expected_psnr| is used to verify basic
// quality when the resampled frame is scaled back up/down to the
// original/source size. |expected_psnr| is set to be  ~0.1/0.05dB lower than
// actual PSNR verified under the same conditions.
void TestSize(const I420VideoFrame& sourceFrame, int targetwidth_,
              int targetheight_, int mode, double expected_psnr,
              VideoProcessingModule* vpm);
bool CompareFrames(const webrtc::I420VideoFrame& frame1,
                  const webrtc::I420VideoFrame& frame2);

VideoProcessingModuleTest::VideoProcessingModuleTest()
    : vpm_(NULL),
      source_file_(NULL),
      width_(352),
      half_width_((width_ + 1) / 2),
      height_(288),
      size_y_(width_ * height_),
      size_uv_(half_width_ * ((height_ + 1) / 2)),
      frame_length_(CalcBufferSize(kI420, width_, height_)) {}

void VideoProcessingModuleTest::SetUp() {
  vpm_ = VideoProcessingModule::Create(0);
  ASSERT_TRUE(vpm_ != NULL);

  ASSERT_EQ(0, video_frame_.CreateEmptyFrame(width_, height_, width_,
                                            half_width_, half_width_));
  // Clear video frame so DrMemory/Valgrind will allow reads of the buffer.
  memset(video_frame_.buffer(kYPlane), 0, video_frame_.allocated_size(kYPlane));
  memset(video_frame_.buffer(kUPlane), 0, video_frame_.allocated_size(kUPlane));
  memset(video_frame_.buffer(kVPlane), 0, video_frame_.allocated_size(kVPlane));
  const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  source_file_  = fopen(video_file.c_str(),"rb");
  ASSERT_TRUE(source_file_ != NULL) <<
      "Cannot read source file: " + video_file + "\n";
}

void VideoProcessingModuleTest::TearDown() {
  if (source_file_ != NULL)  {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;

  if (vpm_ != NULL)  {
    VideoProcessingModule::Destroy(vpm_);
  }
  vpm_ = NULL;
}

TEST_F(VideoProcessingModuleTest, HandleNullBuffer) {
  // TODO(mikhal/stefan): Do we need this one?
  VideoProcessingModule::FrameStats stats;
  // Video frame with unallocated buffer.
  I420VideoFrame videoFrame;
  videoFrame.set_width(width_);
  videoFrame.set_height(height_);

  EXPECT_EQ(-3, vpm_->GetFrameStats(&stats, videoFrame));

  EXPECT_EQ(-1, vpm_->ColorEnhancement(&videoFrame));

  EXPECT_EQ(-1, vpm_->Deflickering(&videoFrame, &stats));

  EXPECT_EQ(-3, vpm_->BrightnessDetection(videoFrame, stats));
}

TEST_F(VideoProcessingModuleTest, HandleBadStats) {
  VideoProcessingModule::FrameStats stats;
  scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_, fread(video_buffer.get(), 1, frame_length_,
                                 source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &video_frame_));

  EXPECT_EQ(-1, vpm_->Deflickering(&video_frame_, &stats));

  EXPECT_EQ(-3, vpm_->BrightnessDetection(video_frame_, stats));
}

TEST_F(VideoProcessingModuleTest, HandleBadSize) {
  VideoProcessingModule::FrameStats stats;

  video_frame_.ResetSize();
  video_frame_.set_width(width_);
  video_frame_.set_height(0);
  EXPECT_EQ(-3, vpm_->GetFrameStats(&stats, video_frame_));

  EXPECT_EQ(-1, vpm_->ColorEnhancement(&video_frame_));

  EXPECT_EQ(-1, vpm_->Deflickering(&video_frame_, &stats));

  EXPECT_EQ(-3, vpm_->BrightnessDetection(video_frame_, stats));

  EXPECT_EQ(VPM_PARAMETER_ERROR, vpm_->SetTargetResolution(0,0,0));

  I420VideoFrame *out_frame = NULL;
  EXPECT_EQ(VPM_PARAMETER_ERROR, vpm_->PreprocessFrame(video_frame_,
                                                       &out_frame));
}

TEST_F(VideoProcessingModuleTest, IdenticalResultsAfterReset) {
  I420VideoFrame video_frame2;
  VideoProcessingModule::FrameStats stats;
  // Only testing non-static functions here.
  scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_, fread(video_buffer.get(), 1, frame_length_,
                                source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &video_frame_));
  ASSERT_EQ(0, vpm_->GetFrameStats(&stats, video_frame_));
  ASSERT_EQ(0, video_frame2.CopyFrame(video_frame_));
  ASSERT_EQ(0, vpm_->Deflickering(&video_frame_, &stats));
  vpm_->Reset();
  // Retrieve frame stats again in case Deflickering() has zeroed them.
  ASSERT_EQ(0, vpm_->GetFrameStats(&stats, video_frame2));
  ASSERT_EQ(0, vpm_->Deflickering(&video_frame2, &stats));
  EXPECT_TRUE(CompareFrames(video_frame_, video_frame2));

  ASSERT_EQ(frame_length_, fread(video_buffer.get(), 1, frame_length_,
                                 source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &video_frame_));
  ASSERT_EQ(0, vpm_->GetFrameStats(&stats, video_frame_));
  video_frame2.CopyFrame(video_frame_);
  ASSERT_EQ(0, vpm_->BrightnessDetection(video_frame_, stats));
  vpm_->Reset();
  ASSERT_EQ(0, vpm_->BrightnessDetection(video_frame2, stats));
  EXPECT_TRUE(CompareFrames(video_frame_, video_frame2));
}

TEST_F(VideoProcessingModuleTest, FrameStats) {
  VideoProcessingModule::FrameStats stats;
  scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_, fread(video_buffer.get(), 1, frame_length_,
                                 source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &video_frame_));

  EXPECT_FALSE(vpm_->ValidFrameStats(stats));
  EXPECT_EQ(0, vpm_->GetFrameStats(&stats, video_frame_));
  EXPECT_TRUE(vpm_->ValidFrameStats(stats));

  printf("\nFrameStats\n");
  printf("mean: %u\nnum_pixels: %u\nsubSamplWidth: "
         "%u\nsumSamplHeight: %u\nsum: %u\n\n",
         static_cast<unsigned int>(stats.mean),
         static_cast<unsigned int>(stats.num_pixels),
         static_cast<unsigned int>(stats.subSamplHeight),
         static_cast<unsigned int>(stats.subSamplWidth),
         static_cast<unsigned int>(stats.sum));

  vpm_->ClearFrameStats(&stats);
  EXPECT_FALSE(vpm_->ValidFrameStats(stats));
}

TEST_F(VideoProcessingModuleTest, PreprocessorLogic) {
  // Disable temporal sampling (frame dropping).
  vpm_->EnableTemporalDecimation(false);
  int resolution = 100;
  EXPECT_EQ(VPM_OK, vpm_->SetTargetResolution(resolution, resolution, 15));
  EXPECT_EQ(VPM_OK, vpm_->SetTargetResolution(resolution, resolution, 30));
  // Disable spatial sampling.
  vpm_->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_EQ(VPM_OK, vpm_->SetTargetResolution(resolution, resolution, 30));
  I420VideoFrame* out_frame = NULL;
  // Set rescaling => output frame != NULL.
  vpm_->SetInputFrameResampleMode(kFastRescaling);
  EXPECT_EQ(VPM_OK, vpm_->SetTargetResolution(resolution, resolution, 30));
  EXPECT_EQ(VPM_OK, vpm_->PreprocessFrame(video_frame_, &out_frame));
  EXPECT_FALSE(out_frame == NULL);
  if (out_frame) {
    EXPECT_EQ(resolution, out_frame->width());
    EXPECT_EQ(resolution, out_frame->height());
  }
  // No rescaling=> output frame = NULL.
  vpm_->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_EQ(VPM_OK, vpm_->PreprocessFrame(video_frame_, &out_frame));
  EXPECT_TRUE(out_frame == NULL);
}

TEST_F(VideoProcessingModuleTest, Resampler) {
  enum { NumRuns = 1 };

  int64_t min_runtime = 0;
  int64_t avg_runtime = 0;

  TickTime t0;
  TickTime t1;
  TickInterval acc_ticks;

  rewind(source_file_);
  ASSERT_TRUE(source_file_ != NULL) <<
      "Cannot read input file \n";

  // CA not needed here
  vpm_->EnableContentAnalysis(false);
  // no temporal decimation
  vpm_->EnableTemporalDecimation(false);

  // Reading test frame
  scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_, fread(video_buffer.get(), 1, frame_length_,
                                 source_file_));
  // Using ConvertToI420 to add stride to the image.
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0,
                             width_, height_,
                             0, kRotateNone, &video_frame_));

  for (uint32_t run_idx = 0; run_idx < NumRuns; run_idx++) {
    // Initiate test timer.
    t0 = TickTime::Now();

    // Init the sourceFrame with a timestamp.
    video_frame_.set_render_time_ms(t0.MillisecondTimestamp());
    video_frame_.set_timestamp(t0.MillisecondTimestamp() * 90);

    // Test scaling to different sizes: source is of |width|/|height| = 352/288.
    // Scaling mode in VPM is currently fixed to kScaleBox (mode = 3).
    TestSize(video_frame_, 100, 50, 3, 24.0, vpm_);
    TestSize(video_frame_, 352/4, 288/4, 3, 25.2, vpm_);
    TestSize(video_frame_, 352/2, 288/2, 3, 28.1, vpm_);
    TestSize(video_frame_, 352, 288, 3, -1, vpm_);  // no resampling.
    TestSize(video_frame_, 2*352, 2*288, 3, 32.2, vpm_);
    TestSize(video_frame_, 400, 256, 3, 31.3, vpm_);
    TestSize(video_frame_, 480, 640, 3, 32.15, vpm_);
    TestSize(video_frame_, 960, 720, 3, 32.2, vpm_);
    TestSize(video_frame_, 1280, 720, 3, 32.15, vpm_);
    // Upsampling to odd size.
    TestSize(video_frame_, 501, 333, 3, 32.05, vpm_);
    // Downsample to odd size.
    TestSize(video_frame_, 281, 175, 3, 29.3, vpm_);

    // stop timer
    t1 = TickTime::Now();
    acc_ticks += (t1 - t0);

    if (acc_ticks.Microseconds() < min_runtime || run_idx == 0)  {
      min_runtime = acc_ticks.Microseconds();
    }
    avg_runtime += acc_ticks.Microseconds();
  }

  printf("\nAverage run time = %d us / frame\n",
         //static_cast<int>(avg_runtime / frameNum / NumRuns));
         static_cast<int>(avg_runtime));
  printf("Min run time = %d us / frame\n\n",
         //static_cast<int>(min_runtime / frameNum));
         static_cast<int>(min_runtime));
}

void TestSize(const I420VideoFrame& source_frame, int targetwidth_,
              int targetheight_, int mode, double expected_psnr,
              VideoProcessingModule* vpm) {
  int sourcewidth_ = source_frame.width();
  int sourceheight_ = source_frame.height();
  I420VideoFrame* out_frame = NULL;

  ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(targetwidth_, targetheight_, 30));
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
  if (targetwidth_ != static_cast<int>(sourcewidth_) ||
      targetheight_ != static_cast<int>(sourceheight_))  {
    // Write the processed frame to file for visual inspection.
    std::ostringstream filename;
    filename << webrtc::test::OutputPath() << "Resampler_"<< mode << "_" <<
        "from_" << sourcewidth_ << "x" << sourceheight_ << "_to_" <<
        targetwidth_ << "x" << targetheight_ << "_30Hz_P420.yuv";
    std::cout << "Watch " << filename.str() << " and verify that it is okay."
        << std::endl;
    FILE* stand_alone_file = fopen(filename.str().c_str(), "wb");
    if (PrintI420VideoFrame(*out_frame, stand_alone_file) < 0) {
      fprintf(stderr, "Failed to write frame for scaling to width/height: "
          " %d %d \n", targetwidth_, targetheight_);
      return;
    }
    fclose(stand_alone_file);

    I420VideoFrame resampled_source_frame;
    resampled_source_frame.CopyFrame(*out_frame);

    // Scale |resampled_source_frame| back to original/source size.
    ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(sourcewidth_,
                                               sourceheight_,
                                               30));
    ASSERT_EQ(VPM_OK, vpm->PreprocessFrame(resampled_source_frame,
                                           &out_frame));

    // Write the processed frame to file for visual inspection.
    std::ostringstream filename2;
    filename2 << webrtc::test::OutputPath() << "Resampler_"<< mode << "_" <<
          "from_" << targetwidth_ << "x" << targetheight_ << "_to_" <<
          sourcewidth_ << "x" << sourceheight_ << "_30Hz_P420.yuv";
    std::cout << "Watch " << filename2.str() << " and verify that it is okay."
                << std::endl;
    stand_alone_file = fopen(filename2.str().c_str(), "wb");
    if (PrintI420VideoFrame(*out_frame, stand_alone_file) < 0) {
      fprintf(stderr, "Failed to write frame for scaling to width/height "
              "%d %d \n", sourcewidth_, sourceheight_);
      return;
    }
    fclose(stand_alone_file);

    // Compute the PSNR and check expectation.
    double psnr = I420PSNR(&source_frame, out_frame);
    EXPECT_GT(psnr, expected_psnr);
    printf("PSNR: %f. PSNR is between source of size %d %d, and a modified "
        "source which is scaled down/up to: %d %d, and back to source size \n",
        psnr, sourcewidth_, sourceheight_, targetwidth_, targetheight_);
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
