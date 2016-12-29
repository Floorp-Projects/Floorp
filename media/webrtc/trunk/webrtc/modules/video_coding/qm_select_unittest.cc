/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes unit tests the QmResolution class
 * In particular, for the selection of spatial and/or temporal down-sampling.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/qm_select.h"

namespace webrtc {

// Representative values of content metrics for: low/high/medium(default) state,
// based on parameters settings in qm_select_data.h.
const float kSpatialLow = 0.01f;
const float kSpatialMedium = 0.03f;
const float kSpatialHigh = 0.1f;
const float kTemporalLow = 0.01f;
const float kTemporalMedium = 0.06f;
const float kTemporalHigh = 0.1f;

class QmSelectTest : public ::testing::Test {
 protected:
  QmSelectTest()
      : qm_resolution_(new VCMQmResolution()),
        content_metrics_(new VideoContentMetrics()),
        qm_scale_(NULL) {}
  VCMQmResolution* qm_resolution_;
  VideoContentMetrics* content_metrics_;
  VCMResolutionScale* qm_scale_;

  void InitQmNativeData(float initial_bit_rate,
                        int user_frame_rate,
                        int native_width,
                        int native_height,
                        int num_layers);

  void UpdateQmEncodedFrame(size_t* encoded_size, size_t num_updates);

  void UpdateQmRateData(int* target_rate,
                        int* encoder_sent_rate,
                        int* incoming_frame_rate,
                        uint8_t* fraction_lost,
                        int num_updates);

  void UpdateQmContentData(float motion_metric,
                           float spatial_metric,
                           float spatial_metric_horiz,
                           float spatial_metric_vert);

  bool IsSelectedActionCorrect(VCMResolutionScale* qm_scale,
                               float fac_width,
                               float fac_height,
                               float fac_temp,
                               uint16_t new_width,
                               uint16_t new_height,
                               float new_frame_rate);

  void TearDown() {
    delete qm_resolution_;
    delete content_metrics_;
  }
};

TEST_F(QmSelectTest, HandleInputs) {
  // Expect parameter error. Initialize with invalid inputs.
  EXPECT_EQ(-4, qm_resolution_->Initialize(1000, 0, 640, 480, 1));
  EXPECT_EQ(-4, qm_resolution_->Initialize(1000, 30, 640, 0, 1));
  EXPECT_EQ(-4, qm_resolution_->Initialize(1000, 30, 0, 480, 1));

  // Expect uninitialized error.: No valid initialization before selection.
  EXPECT_EQ(-7, qm_resolution_->SelectResolution(&qm_scale_));

  VideoContentMetrics* content_metrics = NULL;
  EXPECT_EQ(0, qm_resolution_->Initialize(1000, 30, 640, 480, 1));
  qm_resolution_->UpdateContent(content_metrics);
  // Content metrics are NULL: Expect success and no down-sampling action.
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0, 1.0, 1.0, 640, 480, 30.0f));
}

// TODO(marpan): Add a test for number of temporal layers > 1.

// No down-sampling action at high rates.
TEST_F(QmSelectTest, NoActionHighRate) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(800, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {800, 800, 800};
  int encoder_sent_rate[] = {800, 800, 800};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  UpdateQmContentData(kTemporalLow, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(0, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 30.0f));
}

// Rate is well below transition, down-sampling action is taken,
// depending on the content state.
TEST_F(QmSelectTest, DownActionLowRate) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial: 2x2 spatial expected.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, low spatial: 2/3 temporal is expected.
  UpdateQmContentData(kTemporalLow, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(0, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 640, 480, 20.5f));

  qm_resolution_->ResetDownSamplingState();
  // Medium motion, low spatial: 2x2 spatial expected.
  UpdateQmContentData(kTemporalMedium, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // High motion, high spatial: 2/3 temporal expected.
  UpdateQmContentData(kTemporalHigh, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(4, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 640, 480, 20.5f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial: 1/2 temporal expected.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f, 640, 480, 15.5f));

  qm_resolution_->ResetDownSamplingState();
  // Medium motion, high spatial: 1/2 temporal expected.
  UpdateQmContentData(kTemporalMedium, kSpatialHigh, kSpatialHigh,
                      kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(7, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f, 640, 480, 15.5f));

  qm_resolution_->ResetDownSamplingState();
  // High motion, medium spatial: 2x2 spatial expected.
  UpdateQmContentData(kTemporalHigh, kSpatialMedium, kSpatialMedium,
                      kSpatialMedium);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(5, qm_resolution_->ComputeContentClass());
  // Target frame rate for frame dropper should be the same as previous == 15.
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, medium spatial: high frame rate, so 1/2 temporal expected.
  UpdateQmContentData(kTemporalLow, kSpatialMedium, kSpatialMedium,
                      kSpatialMedium);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(2, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f, 640, 480, 15.5f));

  qm_resolution_->ResetDownSamplingState();
  // Medium motion, medium spatial: high frame rate, so 2/3 temporal expected.
  UpdateQmContentData(kTemporalMedium, kSpatialMedium, kSpatialMedium,
                      kSpatialMedium);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(8, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 640, 480, 20.5f));
}

// Rate mis-match is high, and we have over-shooting.
// since target rate is below max for down-sampling, down-sampling is selected.
TEST_F(QmSelectTest, DownActionHighRateMMOvershoot) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(300, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {300, 300, 300};
  int encoder_sent_rate[] = {900, 900, 900};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 4.0f / 3.0f, 4.0f / 3.0f, 1.0f,
                                      480, 360, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 640, 480, 20.5f));
}

// Rate mis-match is high, target rate is below max for down-sampling,
// but since we have consistent under-shooting, no down-sampling action.
TEST_F(QmSelectTest, NoActionHighRateMMUndershoot) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(300, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {300, 300, 300};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kEasyEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 30.0f));
}

// Buffer is underflowing, and target rate is below max for down-sampling,
// so action is taken.
TEST_F(QmSelectTest, DownActionBufferUnderflow) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(300, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update with encoded size over a number of frames.
  // per-frame bandwidth = 15 = 450/30: simulate (decoder) buffer underflow:
  size_t encoded_size[] = {200, 100, 50, 30, 60, 40, 20, 30, 20, 40};
  UpdateQmEncodedFrame(encoded_size, GTEST_ARRAY_SIZE_(encoded_size));

  // Update rates for a sequence of intervals.
  int target_rate[] = {300, 300, 300};
  int encoder_sent_rate[] = {450, 450, 450};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 4.0f / 3.0f, 4.0f / 3.0f, 1.0f,
                                      480, 360, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 640, 480, 20.5f));
}

// Target rate is below max for down-sampling, but buffer level is stable,
// so no action is taken.
TEST_F(QmSelectTest, NoActionBufferStable) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(350, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update with encoded size over a number of frames.
  // per-frame bandwidth = 15 = 450/30: simulate stable (decoder) buffer levels.
  size_t encoded_size[] = {40, 10, 10, 16, 18, 20, 17, 20, 16, 15};
  UpdateQmEncodedFrame(encoded_size, GTEST_ARRAY_SIZE_(encoded_size));

  // Update rates for a sequence of intervals.
  int target_rate[] = {350, 350, 350};
  int encoder_sent_rate[] = {350, 450, 450};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 30.0f));

  qm_resolution_->ResetDownSamplingState();
  // Low motion, high spatial
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 30.0f));
}

// Very low rate, but no spatial down-sampling below some size (QCIF).
TEST_F(QmSelectTest, LimitDownSpatialAction) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(10, 30, 176, 144, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 176;
  uint16_t codec_height = 144;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(0, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {10, 10, 10};
  int encoder_sent_rate[] = {10, 10, 10};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 176, 144, 30.0f));
}

// Very low rate, but no frame reduction below some frame_rate (8fps).
TEST_F(QmSelectTest, LimitDownTemporalAction) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(10, 8, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(8.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {10, 10, 10};
  int encoder_sent_rate[] = {10, 10, 10};
  int incoming_frame_rate[] = {8, 8, 8};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, medium spatial.
  UpdateQmContentData(kTemporalLow, kSpatialMedium, kSpatialMedium,
                      kSpatialMedium);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(2, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 8.0f));
}

// Two stages: spatial down-sample and then back up spatially,
// as rate as increased.
TEST_F(QmSelectTest, 2StageDownSpatialUpSpatial) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  // Reset and go up in rate: expected to go back up, in 2 stages of 3/4.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecParameters(30.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {400, 400, 400, 400, 400};
  int encoder_sent_rate2[] = {400, 400, 400, 400, 400};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  float scale = (4.0f / 3.0f) / 2.0f;
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, scale, scale, 1.0f, 480, 360, 30.0f));

  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 3.0f / 4.0f, 3.0f / 4.0f, 1.0f,
                                      640, 480, 30.0f));
}

// Two stages: spatial down-sample and then back up spatially, since encoder
// is under-shooting target even though rate has not increased much.
TEST_F(QmSelectTest, 2StageDownSpatialUpSpatialUndershoot) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  // Reset rates and simulate under-shooting scenario.: expect to go back up.
  // Goes up spatially in two stages for 1/2x1/2 down-sampling.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecParameters(30.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {200, 200, 200, 200, 200};
  int encoder_sent_rate2[] = {50, 50, 50, 50, 50};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kEasyEncoding, qm_resolution_->GetEncoderState());
  float scale = (4.0f / 3.0f) / 2.0f;
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, scale, scale, 1.0f, 480, 360, 30.0f));

  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 3.0f / 4.0f, 3.0f / 4.0f, 1.0f,
                                      640, 480, 30.0f));
}

// Two stages: spatial down-sample and then no action to go up,
// as encoding rate mis-match is too high.
TEST_F(QmSelectTest, 2StageDownSpatialNoActionUp) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  // Reset and simulate large rate mis-match: expect no action to go back up.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecParameters(30.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {400, 400, 400, 400, 400};
  int encoder_sent_rate2[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 320, 240, 30.0f));
}

// Two stages: temporally down-sample and then back up temporally,
// as rate as increased.
TEST_F(QmSelectTest, 2StatgeDownTemporalUpTemporal) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f, 640, 480, 15.5f));

  // Reset rates and go up in rate: expect to go back up.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {400, 400, 400, 400, 400};
  int encoder_sent_rate2[] = {400, 400, 400, 400, 400};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 0.5f, 640, 480, 30.0f));
}

// Two stages: temporal down-sample and then back up temporally, since encoder
// is under-shooting target even though rate has not increased much.
TEST_F(QmSelectTest, 2StatgeDownTemporalUpTemporalUndershoot) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f, 640, 480, 15.5f));

  // Reset rates and simulate under-shooting scenario.: expect to go back up.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {150, 150, 150, 150, 150};
  int encoder_sent_rate2[] = {50, 50, 50, 50, 50};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kEasyEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 0.5f, 640, 480, 30.0f));
}

// Two stages: temporal down-sample and then no action to go up,
// as encoding rate mis-match is too high.
TEST_F(QmSelectTest, 2StageDownTemporalNoActionUp) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(50, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {50, 50, 50};
  int encoder_sent_rate[] = {50, 50, 50};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1, 1, 2, 640, 480, 15.5f));

  // Reset and simulate large rate mis-match: expect no action to go back up.
  qm_resolution_->UpdateCodecParameters(15.0f, codec_width, codec_height);
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate2[] = {600, 600, 600, 600, 600};
  int encoder_sent_rate2[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate2[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStressedEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 640, 480, 15.0f));
}
// 3 stages: spatial down-sample, followed by temporal down-sample,
// and then go up to full state, as encoding rate has increased.
TEST_F(QmSelectTest, 3StageDownSpatialTemporlaUpSpatialTemporal) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(80, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {80, 80, 80};
  int encoder_sent_rate[] = {80, 80, 80};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  // Change content data: expect temporal down-sample.
  qm_resolution_->UpdateCodecParameters(30.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));

  // Reset rates and go lower in rate.
  qm_resolution_->ResetRates();
  int target_rate2[] = {40, 40, 40, 40, 40};
  int encoder_sent_rate2[] = {40, 40, 40, 40, 40};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 320, 240, 20.5f));

  // Reset rates and go high up in rate: expect to go back up both spatial
  // and temporally. The 1/2x1/2 spatial is undone in two stages.
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate3[] = {1000, 1000, 1000, 1000, 1000};
  int encoder_sent_rate3[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate3[] = {20, 20, 20, 20, 20};
  uint8_t fraction_lost3[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate3, encoder_sent_rate3, incoming_frame_rate3,
                   fraction_lost3, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  float scale = (4.0f / 3.0f) / 2.0f;
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, scale, scale, 2.0f / 3.0f, 480,
                                      360, 30.0f));

  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 3.0f / 4.0f, 3.0f / 4.0f, 1.0f,
                                      640, 480, 30.0f));
}

// No down-sampling below some total amount.
TEST_F(QmSelectTest, NoActionTooMuchDownSampling) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(150, 30, 1280, 720, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 1280;
  uint16_t codec_height = 720;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(7, qm_resolution_->GetImageType(codec_width, codec_height));

  // Update rates for a sequence of intervals.
  int target_rate[] = {150, 150, 150};
  int encoder_sent_rate[] = {150, 150, 150};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 640, 360, 30.0f));

  // Reset and lower rates to get another spatial action (3/4x3/4).
  // Lower the frame rate for spatial to be selected again.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecParameters(10.0f, 640, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(640, 360));
  // Update rates for a sequence of intervals.
  int target_rate2[] = {70, 70, 70, 70, 70};
  int encoder_sent_rate2[] = {70, 70, 70, 70, 70};
  int incoming_frame_rate2[] = {10, 10, 10, 10, 10};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, medium spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialMedium, kSpatialMedium,
                      kSpatialMedium);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(5, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 4.0f / 3.0f, 4.0f / 3.0f, 1.0f,
                                      480, 270, 10.0f));

  // Reset and go to very low rate: no action should be taken,
  // we went down too much already.
  qm_resolution_->ResetRates();
  qm_resolution_->UpdateCodecParameters(10.0f, 480, 270);
  EXPECT_EQ(3, qm_resolution_->GetImageType(480, 270));
  // Update rates for a sequence of intervals.
  int target_rate3[] = {10, 10, 10, 10, 10};
  int encoder_sent_rate3[] = {10, 10, 10, 10, 10};
  int incoming_frame_rate3[] = {10, 10, 10, 10, 10};
  uint8_t fraction_lost3[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate3, encoder_sent_rate3, incoming_frame_rate3,
                   fraction_lost3, 5);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(5, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.0f, 480, 270, 10.0f));
}

// Multiple down-sampling stages and then undo all of them.
// Spatial down-sample 3/4x3/4, followed by temporal down-sample 2/3,
// followed by spatial 3/4x3/4. Then go up to full state,
// as encoding rate has increased.
TEST_F(QmSelectTest, MultipleStagesCheckActionHistory1) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(150, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Go down spatial 3/4x3/4.
  // Update rates for a sequence of intervals.
  int target_rate[] = {150, 150, 150};
  int encoder_sent_rate[] = {150, 150, 150};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Medium motion, low spatial.
  UpdateQmContentData(kTemporalMedium, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 4.0f / 3.0f, 4.0f / 3.0f, 1.0f,
                                      480, 360, 30.0f));
  // Go down 2/3 temporal.
  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  qm_resolution_->ResetRates();
  int target_rate2[] = {100, 100, 100, 100, 100};
  int encoder_sent_rate2[] = {100, 100, 100, 100, 100};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 480, 360, 20.5f));

  // Go down 3/4x3/4 spatial:
  qm_resolution_->UpdateCodecParameters(20.0f, 480, 360);
  qm_resolution_->ResetRates();
  int target_rate3[] = {80, 80, 80, 80, 80};
  int encoder_sent_rate3[] = {80, 80, 80, 80, 80};
  int incoming_frame_rate3[] = {20, 20, 20, 20, 20};
  uint8_t fraction_lost3[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate3, encoder_sent_rate3, incoming_frame_rate3,
                   fraction_lost3, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // High motion, low spatial.
  UpdateQmContentData(kTemporalHigh, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  // The two spatial actions of 3/4x3/4 are converted to 1/2x1/2,
  // so scale factor is 2.0.
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 20.0f));

  // Reset rates and go high up in rate: expect to go up:
  // 1/2x1x2 spatial and 1/2 temporally.

  // Go up 1/2x1/2 spatially and 1/2 temporally. Spatial is done in 2 stages.
  qm_resolution_->UpdateCodecParameters(15.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate4[] = {1000, 1000, 1000, 1000, 1000};
  int encoder_sent_rate4[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate4[] = {15, 15, 15, 15, 15};
  uint8_t fraction_lost4[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate4, encoder_sent_rate4, incoming_frame_rate4,
                   fraction_lost4, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(3, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  float scale = (4.0f / 3.0f) / 2.0f;
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, scale, scale, 2.0f / 3.0f, 480,
                                      360, 30.0f));

  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 3.0f / 4.0f, 3.0f / 4.0f, 1.0f,
                                      640, 480, 30.0f));
}

// Multiple down-sampling and up-sample stages, with partial undoing.
// Spatial down-sample 1/2x1/2, followed by temporal down-sample 2/3, undo the
// temporal, then another temporal, and then undo both spatial and temporal.
TEST_F(QmSelectTest, MultipleStagesCheckActionHistory2) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(80, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Go down 1/2x1/2 spatial.
  // Update rates for a sequence of intervals.
  int target_rate[] = {80, 80, 80};
  int encoder_sent_rate[] = {80, 80, 80};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Medium motion, low spatial.
  UpdateQmContentData(kTemporalMedium, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));

  // Go down 2/3 temporal.
  qm_resolution_->UpdateCodecParameters(30.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));
  qm_resolution_->ResetRates();
  int target_rate2[] = {40, 40, 40, 40, 40};
  int encoder_sent_rate2[] = {40, 40, 40, 40, 40};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // Medium motion, high spatial.
  UpdateQmContentData(kTemporalMedium, kSpatialHigh, kSpatialHigh,
                      kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(7, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 320, 240, 20.5f));

  // Go up 2/3 temporally.
  qm_resolution_->UpdateCodecParameters(20.0f, 320, 240);
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate3[] = {150, 150, 150, 150, 150};
  int encoder_sent_rate3[] = {150, 150, 150, 150, 150};
  int incoming_frame_rate3[] = {20, 20, 20, 20, 20};
  uint8_t fraction_lost3[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate3, encoder_sent_rate3, incoming_frame_rate3,
                   fraction_lost3, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(7, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f / 3.0f, 320,
                                      240, 30.0f));

  // Go down 2/3 temporal.
  qm_resolution_->UpdateCodecParameters(30.0f, 320, 240);
  EXPECT_EQ(2, qm_resolution_->GetImageType(320, 240));
  qm_resolution_->ResetRates();
  int target_rate4[] = {40, 40, 40, 40, 40};
  int encoder_sent_rate4[] = {40, 40, 40, 40, 40};
  int incoming_frame_rate4[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost4[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate4, encoder_sent_rate4, incoming_frame_rate4,
                   fraction_lost4, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 320, 240, 20.5f));

  // Go up spatial and temporal. Spatial undoing is done in 2 stages.
  qm_resolution_->UpdateCodecParameters(20.5f, 320, 240);
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate5[] = {1000, 1000, 1000, 1000, 1000};
  int encoder_sent_rate5[] = {1000, 1000, 1000, 1000, 1000};
  int incoming_frame_rate5[] = {20, 20, 20, 20, 20};
  uint8_t fraction_lost5[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate5, encoder_sent_rate5, incoming_frame_rate5,
                   fraction_lost5, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  float scale = (4.0f / 3.0f) / 2.0f;
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, scale, scale, 2.0f / 3.0f, 480,
                                      360, 30.0f));

  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 3.0f / 4.0f, 3.0f / 4.0f, 1.0f,
                                      640, 480, 30.0f));
}

// Multiple down-sampling and up-sample stages, with partial undoing.
// Spatial down-sample 3/4x3/4, followed by temporal down-sample 2/3,
// undo the temporal 2/3, and then undo the spatial.
TEST_F(QmSelectTest, MultipleStagesCheckActionHistory3) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(100, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Go down 3/4x3/4 spatial.
  // Update rates for a sequence of intervals.
  int target_rate[] = {100, 100, 100};
  int encoder_sent_rate[] = {100, 100, 100};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Medium motion, low spatial.
  UpdateQmContentData(kTemporalMedium, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 4.0f / 3.0f, 4.0f / 3.0f, 1.0f,
                                      480, 360, 30.0f));

  // Go down 2/3 temporal.
  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  qm_resolution_->ResetRates();
  int target_rate2[] = {100, 100, 100, 100, 100};
  int encoder_sent_rate2[] = {100, 100, 100, 100, 100};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // Low motion, high spatial.
  UpdateQmContentData(kTemporalLow, kSpatialHigh, kSpatialHigh, kSpatialHigh);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 1.5f, 480, 360, 20.5f));

  // Go up 2/3 temporal.
  qm_resolution_->UpdateCodecParameters(20.5f, 480, 360);
  qm_resolution_->ResetRates();
  // Update rates for a sequence of intervals.
  int target_rate3[] = {250, 250, 250, 250, 250};
  int encoder_sent_rate3[] = {250, 250, 250, 250, 250};
  int incoming_frame_rate3[] = {20, 20, 20, 20, 120};
  uint8_t fraction_lost3[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate3, encoder_sent_rate3, incoming_frame_rate3,
                   fraction_lost3, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(1, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 1.0f, 1.0f, 2.0f / 3.0f, 480,
                                      360, 30.0f));

  // Go up spatial.
  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  qm_resolution_->ResetRates();
  int target_rate4[] = {500, 500, 500, 500, 500};
  int encoder_sent_rate4[] = {500, 500, 500, 500, 500};
  int incoming_frame_rate4[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost4[] = {30, 30, 30, 30, 30};
  UpdateQmRateData(target_rate4, encoder_sent_rate4, incoming_frame_rate4,
                   fraction_lost4, 5);

  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 3.0f / 4.0f, 3.0f / 4.0f, 1.0f,
                                      640, 480, 30.0f));
}

// Two stages of 3/4x3/4 converted to one stage of 1/2x1/2.
TEST_F(QmSelectTest, ConvertThreeQuartersToOneHalf) {
  // Initialize with bitrate, frame rate, native system width/height, and
  // number of temporal layers.
  InitQmNativeData(150, 30, 640, 480, 1);

  // Update with encoder frame size.
  uint16_t codec_width = 640;
  uint16_t codec_height = 480;
  qm_resolution_->UpdateCodecParameters(30.0f, codec_width, codec_height);
  EXPECT_EQ(5, qm_resolution_->GetImageType(codec_width, codec_height));

  // Go down 3/4x3/4 spatial.
  // Update rates for a sequence of intervals.
  int target_rate[] = {150, 150, 150};
  int encoder_sent_rate[] = {150, 150, 150};
  int incoming_frame_rate[] = {30, 30, 30};
  uint8_t fraction_lost[] = {10, 10, 10};
  UpdateQmRateData(target_rate, encoder_sent_rate, incoming_frame_rate,
                   fraction_lost, 3);

  // Update content: motion level, and 3 spatial prediction errors.
  // Medium motion, low spatial.
  UpdateQmContentData(kTemporalMedium, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(IsSelectedActionCorrect(qm_scale_, 4.0f / 3.0f, 4.0f / 3.0f, 1.0f,
                                      480, 360, 30.0f));

  // Set rates to go down another 3/4 spatial. Should be converted ton 1/2.
  qm_resolution_->UpdateCodecParameters(30.0f, 480, 360);
  EXPECT_EQ(4, qm_resolution_->GetImageType(480, 360));
  qm_resolution_->ResetRates();
  int target_rate2[] = {100, 100, 100, 100, 100};
  int encoder_sent_rate2[] = {100, 100, 100, 100, 100};
  int incoming_frame_rate2[] = {30, 30, 30, 30, 30};
  uint8_t fraction_lost2[] = {10, 10, 10, 10, 10};
  UpdateQmRateData(target_rate2, encoder_sent_rate2, incoming_frame_rate2,
                   fraction_lost2, 5);

  // Update content: motion level, and 3 spatial prediction errors.
  // Medium motion, low spatial.
  UpdateQmContentData(kTemporalMedium, kSpatialLow, kSpatialLow, kSpatialLow);
  EXPECT_EQ(0, qm_resolution_->SelectResolution(&qm_scale_));
  EXPECT_EQ(6, qm_resolution_->ComputeContentClass());
  EXPECT_EQ(kStableEncoding, qm_resolution_->GetEncoderState());
  EXPECT_TRUE(
      IsSelectedActionCorrect(qm_scale_, 2.0f, 2.0f, 1.0f, 320, 240, 30.0f));
}

void QmSelectTest::InitQmNativeData(float initial_bit_rate,
                                    int user_frame_rate,
                                    int native_width,
                                    int native_height,
                                    int num_layers) {
  EXPECT_EQ(
      0, qm_resolution_->Initialize(initial_bit_rate, user_frame_rate,
                                    native_width, native_height, num_layers));
}

void QmSelectTest::UpdateQmContentData(float motion_metric,
                                       float spatial_metric,
                                       float spatial_metric_horiz,
                                       float spatial_metric_vert) {
  content_metrics_->motion_magnitude = motion_metric;
  content_metrics_->spatial_pred_err = spatial_metric;
  content_metrics_->spatial_pred_err_h = spatial_metric_horiz;
  content_metrics_->spatial_pred_err_v = spatial_metric_vert;
  qm_resolution_->UpdateContent(content_metrics_);
}

void QmSelectTest::UpdateQmEncodedFrame(size_t* encoded_size,
                                        size_t num_updates) {
  for (size_t i = 0; i < num_updates; ++i) {
    // Convert to bytes.
    size_t encoded_size_update = 1000 * encoded_size[i] / 8;
    qm_resolution_->UpdateEncodedSize(encoded_size_update);
  }
}

void QmSelectTest::UpdateQmRateData(int* target_rate,
                                    int* encoder_sent_rate,
                                    int* incoming_frame_rate,
                                    uint8_t* fraction_lost,
                                    int num_updates) {
  for (int i = 0; i < num_updates; ++i) {
    float target_rate_update = target_rate[i];
    float encoder_sent_rate_update = encoder_sent_rate[i];
    float incoming_frame_rate_update = incoming_frame_rate[i];
    uint8_t fraction_lost_update = fraction_lost[i];
    qm_resolution_->UpdateRates(target_rate_update, encoder_sent_rate_update,
                                incoming_frame_rate_update,
                                fraction_lost_update);
  }
}

// Check is the selected action from the QmResolution class is the same
// as the expected scales from |fac_width|, |fac_height|, |fac_temp|.
bool QmSelectTest::IsSelectedActionCorrect(VCMResolutionScale* qm_scale,
                                           float fac_width,
                                           float fac_height,
                                           float fac_temp,
                                           uint16_t new_width,
                                           uint16_t new_height,
                                           float new_frame_rate) {
  if (qm_scale->spatial_width_fact == fac_width &&
      qm_scale->spatial_height_fact == fac_height &&
      qm_scale->temporal_fact == fac_temp &&
      qm_scale->codec_width == new_width &&
      qm_scale->codec_height == new_height &&
      qm_scale->frame_rate == new_frame_rate) {
    return true;
  } else {
    return false;
  }
}
}  // namespace webrtc
