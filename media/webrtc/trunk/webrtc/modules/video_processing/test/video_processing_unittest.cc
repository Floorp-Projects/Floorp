/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/test/video_processing_unittest.h"

#include <gflags/gflags.h>

#include <string>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

namespace {

// Define command line flag 'gen_files' (default value: false).
DEFINE_bool(gen_files, false, "Output files for visual inspection.");

}  // namespace

static void PreprocessFrameAndVerify(const VideoFrame& source,
                                     int target_width,
                                     int target_height,
                                     VideoProcessing* vpm,
                                     const VideoFrame* out_frame);
static void CropFrame(const uint8_t* source_data,
                      int source_width,
                      int source_height,
                      int offset_x,
                      int offset_y,
                      int cropped_width,
                      int cropped_height,
                      VideoFrame* cropped_frame);
// The |source_data| is cropped and scaled to |target_width| x |target_height|,
// and then scaled back to the expected cropped size. |expected_psnr| is used to
// verify basic quality, and is set to be ~0.1/0.05dB lower than actual PSNR
// verified under the same conditions.
static void TestSize(const VideoFrame& source_frame,
                     const VideoFrame& cropped_source_frame,
                     int target_width,
                     int target_height,
                     double expected_psnr,
                     VideoProcessing* vpm);
static bool CompareFrames(const webrtc::VideoFrame& frame1,
                          const webrtc::VideoFrame& frame2);
static void WriteProcessedFrameForVisualInspection(const VideoFrame& source,
                                                   const VideoFrame& processed);

VideoProcessingTest::VideoProcessingTest()
    : vp_(NULL),
      source_file_(NULL),
      width_(352),
      half_width_((width_ + 1) / 2),
      height_(288),
      size_y_(width_ * height_),
      size_uv_(half_width_ * ((height_ + 1) / 2)),
      frame_length_(CalcBufferSize(kI420, width_, height_)) {}

void VideoProcessingTest::SetUp() {
  vp_ = VideoProcessing::Create();
  ASSERT_TRUE(vp_ != NULL);

  ASSERT_EQ(0, video_frame_.CreateEmptyFrame(width_, height_, width_,
                                             half_width_, half_width_));
  // Clear video frame so DrMemory/Valgrind will allow reads of the buffer.
  memset(video_frame_.buffer(kYPlane), 0, video_frame_.allocated_size(kYPlane));
  memset(video_frame_.buffer(kUPlane), 0, video_frame_.allocated_size(kUPlane));
  memset(video_frame_.buffer(kVPlane), 0, video_frame_.allocated_size(kVPlane));
  const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  source_file_ = fopen(video_file.c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL)
      << "Cannot read source file: " + video_file + "\n";
}

void VideoProcessingTest::TearDown() {
  if (source_file_ != NULL) {
    ASSERT_EQ(0, fclose(source_file_));
  }
  source_file_ = NULL;
  delete vp_;
  vp_ = NULL;
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_HandleNullBuffer) {
#else
TEST_F(VideoProcessingTest, HandleNullBuffer) {
#endif
  // TODO(mikhal/stefan): Do we need this one?
  VideoProcessing::FrameStats stats;
  // Video frame with unallocated buffer.
  VideoFrame videoFrame;

  vp_->GetFrameStats(videoFrame, &stats);
  EXPECT_EQ(stats.num_pixels, 0u);

  EXPECT_EQ(-1, vp_->Deflickering(&videoFrame, &stats));

  EXPECT_EQ(-3, vp_->BrightnessDetection(videoFrame, stats));
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_HandleBadStats) {
#else
TEST_F(VideoProcessingTest, HandleBadStats) {
#endif
  VideoProcessing::FrameStats stats;
  vp_->ClearFrameStats(&stats);
  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_,
            fread(video_buffer.get(), 1, frame_length_, source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                             0, kVideoRotation_0, &video_frame_));

  EXPECT_EQ(-1, vp_->Deflickering(&video_frame_, &stats));

  EXPECT_EQ(-3, vp_->BrightnessDetection(video_frame_, stats));
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_IdenticalResultsAfterReset) {
#else
TEST_F(VideoProcessingTest, IdenticalResultsAfterReset) {
#endif
  VideoFrame video_frame2;
  VideoProcessing::FrameStats stats;
  // Only testing non-static functions here.
  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_,
            fread(video_buffer.get(), 1, frame_length_, source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                             0, kVideoRotation_0, &video_frame_));
  vp_->GetFrameStats(video_frame_, &stats);
  EXPECT_GT(stats.num_pixels, 0u);
  ASSERT_EQ(0, video_frame2.CopyFrame(video_frame_));
  ASSERT_EQ(0, vp_->Deflickering(&video_frame_, &stats));

  // Retrieve frame stats again in case Deflickering() has zeroed them.
  vp_->GetFrameStats(video_frame2, &stats);
  EXPECT_GT(stats.num_pixels, 0u);
  ASSERT_EQ(0, vp_->Deflickering(&video_frame2, &stats));
  EXPECT_TRUE(CompareFrames(video_frame_, video_frame2));

  ASSERT_EQ(frame_length_,
            fread(video_buffer.get(), 1, frame_length_, source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                             0, kVideoRotation_0, &video_frame_));
  vp_->GetFrameStats(video_frame_, &stats);
  EXPECT_GT(stats.num_pixels, 0u);
  video_frame2.CopyFrame(video_frame_);
  ASSERT_EQ(0, vp_->BrightnessDetection(video_frame_, stats));

  ASSERT_EQ(0, vp_->BrightnessDetection(video_frame2, stats));
  EXPECT_TRUE(CompareFrames(video_frame_, video_frame2));
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_FrameStats) {
#else
TEST_F(VideoProcessingTest, FrameStats) {
#endif
  VideoProcessing::FrameStats stats;
  vp_->ClearFrameStats(&stats);
  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_,
            fread(video_buffer.get(), 1, frame_length_, source_file_));
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                             0, kVideoRotation_0, &video_frame_));

  EXPECT_FALSE(vp_->ValidFrameStats(stats));
  vp_->GetFrameStats(video_frame_, &stats);
  EXPECT_GT(stats.num_pixels, 0u);
  EXPECT_TRUE(vp_->ValidFrameStats(stats));

  printf("\nFrameStats\n");
  printf("mean: %u\nnum_pixels: %u\nsubSamplFactor: %u\nsum: %u\n\n",
         static_cast<unsigned int>(stats.mean),
         static_cast<unsigned int>(stats.num_pixels),
         static_cast<unsigned int>(stats.sub_sampling_factor),
         static_cast<unsigned int>(stats.sum));

  vp_->ClearFrameStats(&stats);
  EXPECT_FALSE(vp_->ValidFrameStats(stats));
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_PreprocessorLogic) {
#else
TEST_F(VideoProcessingTest, PreprocessorLogic) {
#endif
  // Disable temporal sampling (frame dropping).
  vp_->EnableTemporalDecimation(false);
  int resolution = 100;
  EXPECT_EQ(VPM_OK, vp_->SetTargetResolution(resolution, resolution, 15));
  EXPECT_EQ(VPM_OK, vp_->SetTargetResolution(resolution, resolution, 30));
  // Disable spatial sampling.
  vp_->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_EQ(VPM_OK, vp_->SetTargetResolution(resolution, resolution, 30));
  VideoFrame* out_frame = NULL;
  // Set rescaling => output frame != NULL.
  vp_->SetInputFrameResampleMode(kFastRescaling);
  PreprocessFrameAndVerify(video_frame_, resolution, resolution, vp_,
                           out_frame);
  // No rescaling=> output frame = NULL.
  vp_->SetInputFrameResampleMode(kNoRescaling);
  EXPECT_TRUE(vp_->PreprocessFrame(video_frame_) != nullptr);
}

#if defined(WEBRTC_IOS)
TEST_F(VideoProcessingTest, DISABLED_Resampler) {
#else
TEST_F(VideoProcessingTest, Resampler) {
#endif
  enum { NumRuns = 1 };

  int64_t min_runtime = 0;
  int64_t total_runtime = 0;

  rewind(source_file_);
  ASSERT_TRUE(source_file_ != NULL) << "Cannot read input file \n";

  // CA not needed here
  vp_->EnableContentAnalysis(false);
  // no temporal decimation
  vp_->EnableTemporalDecimation(false);

  // Reading test frame
  rtc::scoped_ptr<uint8_t[]> video_buffer(new uint8_t[frame_length_]);
  ASSERT_EQ(frame_length_,
            fread(video_buffer.get(), 1, frame_length_, source_file_));
  // Using ConvertToI420 to add stride to the image.
  EXPECT_EQ(0, ConvertToI420(kI420, video_buffer.get(), 0, 0, width_, height_,
                             0, kVideoRotation_0, &video_frame_));
  // Cropped source frame that will contain the expected visible region.
  VideoFrame cropped_source_frame;
  cropped_source_frame.CopyFrame(video_frame_);

  for (uint32_t run_idx = 0; run_idx < NumRuns; run_idx++) {
    // Initiate test timer.
    const TickTime time_start = TickTime::Now();

    // Init the sourceFrame with a timestamp.
    video_frame_.set_render_time_ms(time_start.MillisecondTimestamp());
    video_frame_.set_timestamp(time_start.MillisecondTimestamp() * 90);

    // Test scaling to different sizes: source is of |width|/|height| = 352/288.
    // Pure scaling:
    TestSize(video_frame_, video_frame_, width_ / 4, height_ / 4, 25.2, vp_);
    TestSize(video_frame_, video_frame_, width_ / 2, height_ / 2, 28.1, vp_);
    // No resampling:
    TestSize(video_frame_, video_frame_, width_, height_, -1, vp_);
    TestSize(video_frame_, video_frame_, 2 * width_, 2 * height_, 32.2, vp_);

    // Scaling and cropping. The cropped source frame is the largest center
    // aligned region that can be used from the source while preserving aspect
    // ratio.
    CropFrame(video_buffer.get(), width_, height_, 0, 56, 352, 176,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 100, 50, 24.0, vp_);

    CropFrame(video_buffer.get(), width_, height_, 0, 30, 352, 225,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 400, 256, 31.3, vp_);

    CropFrame(video_buffer.get(), width_, height_, 68, 0, 216, 288,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 480, 640, 32.15, vp_);

    CropFrame(video_buffer.get(), width_, height_, 0, 12, 352, 264,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 960, 720, 32.2, vp_);

    CropFrame(video_buffer.get(), width_, height_, 0, 44, 352, 198,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 1280, 720, 32.15, vp_);

    // Upsampling to odd size.
    CropFrame(video_buffer.get(), width_, height_, 0, 26, 352, 233,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 501, 333, 32.05, vp_);
    // Downsample to odd size.
    CropFrame(video_buffer.get(), width_, height_, 0, 34, 352, 219,
              &cropped_source_frame);
    TestSize(video_frame_, cropped_source_frame, 281, 175, 29.3, vp_);

    // Stop timer.
    const int64_t runtime = (TickTime::Now() - time_start).Microseconds();
    if (runtime < min_runtime || run_idx == 0) {
      min_runtime = runtime;
    }
    total_runtime += runtime;
  }

  printf("\nAverage run time = %d us / frame\n",
         static_cast<int>(total_runtime));
  printf("Min run time = %d us / frame\n\n", static_cast<int>(min_runtime));
}

void PreprocessFrameAndVerify(const VideoFrame& source,
                              int target_width,
                              int target_height,
                              VideoProcessing* vpm,
                              const VideoFrame* out_frame) {
  ASSERT_EQ(VPM_OK, vpm->SetTargetResolution(target_width, target_height, 30));
  out_frame = vpm->PreprocessFrame(source);
  EXPECT_TRUE(out_frame != nullptr);

  // If no resizing is needed, expect the original frame.
  if (target_width == source.width() && target_height == source.height()) {
    EXPECT_EQ(&source, out_frame);
    return;
  }

  // Verify the resampled frame.
  EXPECT_TRUE(out_frame != NULL);
  EXPECT_EQ(source.render_time_ms(), (out_frame)->render_time_ms());
  EXPECT_EQ(source.timestamp(), (out_frame)->timestamp());
  EXPECT_EQ(target_width, (out_frame)->width());
  EXPECT_EQ(target_height, (out_frame)->height());
}

void CropFrame(const uint8_t* source_data,
               int source_width,
               int source_height,
               int offset_x,
               int offset_y,
               int cropped_width,
               int cropped_height,
               VideoFrame* cropped_frame) {
  cropped_frame->CreateEmptyFrame(cropped_width, cropped_height, cropped_width,
                                  (cropped_width + 1) / 2,
                                  (cropped_width + 1) / 2);
  EXPECT_EQ(0,
            ConvertToI420(kI420, source_data, offset_x, offset_y, source_width,
                          source_height, 0, kVideoRotation_0, cropped_frame));
}

void TestSize(const VideoFrame& source_frame,
              const VideoFrame& cropped_source_frame,
              int target_width,
              int target_height,
              double expected_psnr,
              VideoProcessing* vpm) {
  // Resample source_frame to out_frame.
  VideoFrame* out_frame = NULL;
  vpm->SetInputFrameResampleMode(kBox);
  PreprocessFrameAndVerify(source_frame, target_width, target_height, vpm,
                           out_frame);
  if (out_frame == NULL)
    return;
  WriteProcessedFrameForVisualInspection(source_frame, *out_frame);

  // Scale |resampled_source_frame| back to the source scale.
  VideoFrame resampled_source_frame;
  resampled_source_frame.CopyFrame(*out_frame);
  PreprocessFrameAndVerify(resampled_source_frame, cropped_source_frame.width(),
                           cropped_source_frame.height(), vpm, out_frame);
  WriteProcessedFrameForVisualInspection(resampled_source_frame, *out_frame);

  // Compute PSNR against the cropped source frame and check expectation.
  double psnr = I420PSNR(&cropped_source_frame, out_frame);
  EXPECT_GT(psnr, expected_psnr);
  printf(
      "PSNR: %f. PSNR is between source of size %d %d, and a modified "
      "source which is scaled down/up to: %d %d, and back to source size \n",
      psnr, source_frame.width(), source_frame.height(), target_width,
      target_height);
}

bool CompareFrames(const webrtc::VideoFrame& frame1,
                   const webrtc::VideoFrame& frame2) {
  for (int plane = 0; plane < webrtc::kNumOfPlanes; plane++) {
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

void WriteProcessedFrameForVisualInspection(const VideoFrame& source,
                                            const VideoFrame& processed) {
  // Skip if writing to files is not enabled.
  if (!FLAGS_gen_files)
    return;
  // Write the processed frame to file for visual inspection.
  std::ostringstream filename;
  filename << webrtc::test::OutputPath() << "Resampler_from_" << source.width()
           << "x" << source.height() << "_to_" << processed.width() << "x"
           << processed.height() << "_30Hz_P420.yuv";
  std::cout << "Watch " << filename.str() << " and verify that it is okay."
            << std::endl;
  FILE* stand_alone_file = fopen(filename.str().c_str(), "wb");
  if (PrintVideoFrame(processed, stand_alone_file) < 0)
    std::cerr << "Failed to write: " << filename.str() << std::endl;
  if (stand_alone_file)
    fclose(stand_alone_file);
}

}  // namespace webrtc
