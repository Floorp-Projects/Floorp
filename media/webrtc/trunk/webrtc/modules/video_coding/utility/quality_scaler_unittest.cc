/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/quality_scaler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {
namespace {
static const int kNumSeconds = 10;
static const int kWidth = 1920;
static const int kHalfWidth = kWidth / 2;
static const int kHeight = 1080;
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kNormalQp = 30;
static const int kMaxQp = 56;
}  // namespace

class QualityScalerTest : public ::testing::Test {
 protected:
  enum ScaleDirection { kScaleDown, kScaleUp };

  QualityScalerTest() {
    input_frame_.CreateEmptyFrame(
        kWidth, kHeight, kWidth, kHalfWidth, kHalfWidth);
    qs_.Init(kMaxQp);
    qs_.ReportFramerate(kFramerate);
  }

  void TriggerScale(ScaleDirection scale_direction) {
    int initial_width = qs_.GetScaledResolution(input_frame_).width;
    for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
      switch (scale_direction) {
        case kScaleUp:
          qs_.ReportEncodedFrame(kLowQp);
          break;
        case kScaleDown:
          qs_.ReportDroppedFrame();
          break;
      }

      if (qs_.GetScaledResolution(input_frame_).width != initial_width)
        return;
    }

    FAIL() << "No downscale within " << kNumSeconds << " seconds.";
  }

  void ExpectOriginalFrame() {
    EXPECT_EQ(&input_frame_, &qs_.GetScaledFrame(input_frame_))
        << "Using scaled frame instead of original input.";
  }

  void ExpectScaleUsingReportedResolution() {
    QualityScaler::Resolution res = qs_.GetScaledResolution(input_frame_);
    const I420VideoFrame& scaled_frame = qs_.GetScaledFrame(input_frame_);
    EXPECT_EQ(res.width, scaled_frame.width());
    EXPECT_EQ(res.height, scaled_frame.height());
  }

  void ContinuouslyDownscalesByHalfDimensionsAndBackUp();

  void DoesNotDownscaleFrameDimensions(int width, int height);

  QualityScaler qs_;
  I420VideoFrame input_frame_;
};

TEST_F(QualityScalerTest, UsesOriginalFrameInitially) {
  ExpectOriginalFrame();
}

TEST_F(QualityScalerTest, ReportsOriginalResolutionInitially) {
  QualityScaler::Resolution res = qs_.GetScaledResolution(input_frame_);
  EXPECT_EQ(input_frame_.width(), res.width);
  EXPECT_EQ(input_frame_.height(), res.height);
}

TEST_F(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  TriggerScale(kScaleDown);
  QualityScaler::Resolution res = qs_.GetScaledResolution(input_frame_);
  EXPECT_LT(res.width, input_frame_.width());
  EXPECT_LT(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 3; ++i) {
    qs_.ReportEncodedFrame(kNormalQp);
    qs_.ReportDroppedFrame();
    qs_.ReportDroppedFrame();
    if (qs_.GetScaledResolution(input_frame_).width < input_frame_.width())
      return;
  }

  FAIL() << "No downscale within " << kNumSeconds << " seconds.";
}

TEST_F(QualityScalerTest, DoesNotDownscaleOnNormalQp) {
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportEncodedFrame(kNormalQp);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution(input_frame_).width)
        << "Unexpected scale on half framedrop.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 2; ++i) {
    qs_.ReportEncodedFrame(kNormalQp);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution(input_frame_).width)
        << "Unexpected scale on half framedrop.";

    qs_.ReportDroppedFrame();
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution(input_frame_).width)
        << "Unexpected scale on half framedrop.";
  }
}

void QualityScalerTest::ContinuouslyDownscalesByHalfDimensionsAndBackUp() {
  const int initial_min_dimension = input_frame_.width() < input_frame_.height()
                                  ? input_frame_.width()
                                  : input_frame_.height();
  int min_dimension = initial_min_dimension;
  int current_shift = 0;
  // Drop all frames to force-trigger downscaling.
  while (min_dimension > 16) {
    TriggerScale(kScaleDown);
    QualityScaler::Resolution res = qs_.GetScaledResolution(input_frame_);
    min_dimension = res.width < res.height ? res.width : res.height;
    ++current_shift;
    ASSERT_EQ(input_frame_.width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_.height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Make sure we can scale back with good-quality frames.
  while (min_dimension < initial_min_dimension) {
    TriggerScale(kScaleUp);
    QualityScaler::Resolution res = qs_.GetScaledResolution(input_frame_);
    min_dimension = res.width < res.height ? res.width : res.height;
    --current_shift;
    ASSERT_EQ(input_frame_.width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_.height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Verify we don't start upscaling after further low use.
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportEncodedFrame(kLowQp);
    ExpectOriginalFrame();
  }
}

TEST_F(QualityScalerTest, ContinuouslyDownscalesByHalfDimensionsAndBackUp) {
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

TEST_F(QualityScalerTest,
       ContinuouslyDownscalesOddResolutionsByHalfDimensionsAndBackUp) {
  const int kOddWidth = 517;
  const int kHalfOddWidth = (kOddWidth + 1) / 2;
  const int kOddHeight = 1239;
  input_frame_.CreateEmptyFrame(
      kOddWidth, kOddHeight, kOddWidth, kHalfOddWidth, kHalfOddWidth);
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

void QualityScalerTest::DoesNotDownscaleFrameDimensions(int width, int height) {
  input_frame_.CreateEmptyFrame(
      width, height, width, (width + 1) / 2, (width + 1) / 2);

  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportDroppedFrame();
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution(input_frame_).width)
        << "Unexpected scale of minimal-size frame.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1PxWidth) {
  DoesNotDownscaleFrameDimensions(1, kHeight);
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1PxHeight) {
  DoesNotDownscaleFrameDimensions(kWidth, 1);
}

TEST_F(QualityScalerTest, DoesNotDownscaleFrom1Px) {
  DoesNotDownscaleFrameDimensions(1, 1);
}

}  // namespace webrtc
