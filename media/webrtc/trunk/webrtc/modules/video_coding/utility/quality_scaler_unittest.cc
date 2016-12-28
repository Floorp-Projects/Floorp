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
static const int kHighQp = 40;
static const int kMaxQp = 56;
}  // namespace

class QualityScalerTest : public ::testing::Test {
 public:
  // Temporal and spatial resolution.
  struct Resolution {
    int framerate;
    int width;
    int height;
  };

 protected:
  enum ScaleDirection {
    kKeepScaleAtHighQp,
    kScaleDown,
    kScaleDownAboveHighQp,
    kScaleUp
  };
  enum BadQualityMetric { kDropFrame, kReportLowQP };

  QualityScalerTest() {
    input_frame_.CreateEmptyFrame(kWidth, kHeight, kWidth, kHalfWidth,
                                  kHalfWidth);
    qs_.Init(kMaxQp / QualityScaler::kDefaultLowQpDenominator, kHighQp, false);
    qs_.ReportFramerate(kFramerate);
    qs_.OnEncodeFrame(input_frame_);
  }

  bool TriggerScale(ScaleDirection scale_direction) {
    qs_.OnEncodeFrame(input_frame_);
    int initial_width = qs_.GetScaledResolution().width;
    for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
      switch (scale_direction) {
        case kScaleUp:
          qs_.ReportQP(kLowQp);
          break;
        case kScaleDown:
          qs_.ReportDroppedFrame();
          break;
        case kKeepScaleAtHighQp:
          qs_.ReportQP(kHighQp);
          break;
        case kScaleDownAboveHighQp:
          qs_.ReportQP(kHighQp + 1);
          break;
      }
      qs_.OnEncodeFrame(input_frame_);
      if (qs_.GetScaledResolution().width != initial_width)
        return true;
    }

    return false;
  }

  void ExpectOriginalFrame() {
    EXPECT_EQ(&input_frame_, &qs_.GetScaledFrame(input_frame_))
        << "Using scaled frame instead of original input.";
  }

  void ExpectScaleUsingReportedResolution() {
    qs_.OnEncodeFrame(input_frame_);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    const VideoFrame& scaled_frame = qs_.GetScaledFrame(input_frame_);
    EXPECT_EQ(res.width, scaled_frame.width());
    EXPECT_EQ(res.height, scaled_frame.height());
  }

  void ContinuouslyDownscalesByHalfDimensionsAndBackUp();

  void DoesNotDownscaleFrameDimensions(int width, int height);

  Resolution TriggerResolutionChange(BadQualityMetric dropframe_lowqp,
                                     int num_second,
                                     int initial_framerate);

  void VerifyQualityAdaptation(int initial_framerate,
                               int seconds,
                               bool expect_spatial_resize,
                               bool expect_framerate_reduction);

  void DownscaleEndsAt(int input_width,
                       int input_height,
                       int end_width,
                       int end_height);

  QualityScaler qs_;
  VideoFrame input_frame_;
};

TEST_F(QualityScalerTest, UsesOriginalFrameInitially) {
  ExpectOriginalFrame();
}

TEST_F(QualityScalerTest, ReportsOriginalResolutionInitially) {
  qs_.OnEncodeFrame(input_frame_);
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_EQ(input_frame_.width(), res.width);
  EXPECT_EQ(input_frame_.height(), res.height);
}

TEST_F(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  EXPECT_TRUE(TriggerScale(kScaleDown)) << "No downscale within " << kNumSeconds
                                        << " seconds.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_LT(res.width, input_frame_.width());
  EXPECT_LT(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, KeepsScaleAtHighQp) {
  EXPECT_FALSE(TriggerScale(kKeepScaleAtHighQp))
      << "Downscale at high threshold which should keep scale.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_EQ(res.width, input_frame_.width());
  EXPECT_EQ(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, DownscalesAboveHighQp) {
  EXPECT_TRUE(TriggerScale(kScaleDownAboveHighQp))
      << "No downscale within " << kNumSeconds << " seconds.";
  QualityScaler::Resolution res = qs_.GetScaledResolution();
  EXPECT_LT(res.width, input_frame_.width());
  EXPECT_LT(res.height, input_frame_.height());
}

TEST_F(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 3; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.ReportDroppedFrame();
    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_);
    if (qs_.GetScaledResolution().width < input_frame_.width())
      return;
  }

  FAIL() << "No downscale within " << kNumSeconds << " seconds.";
}

TEST_F(QualityScalerTest, DoesNotDownscaleOnNormalQp) {
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";
  }
}

TEST_F(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  for (int i = 0; i < kFramerate * kNumSeconds / 2; ++i) {
    qs_.ReportQP(kNormalQp);
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
        << "Unexpected scale on half framedrop.";

    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
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
  while (min_dimension >= 2 * QualityScaler::kDefaultMinDownscaleDimension) {
    EXPECT_TRUE(TriggerScale(kScaleDown)) << "No downscale within "
                                          << kNumSeconds << " seconds.";
    qs_.OnEncodeFrame(input_frame_);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    min_dimension = res.width < res.height ? res.width : res.height;
    ++current_shift;
    ASSERT_EQ(input_frame_.width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_.height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Make sure we can scale back with good-quality frames.
  while (min_dimension < initial_min_dimension) {
    EXPECT_TRUE(TriggerScale(kScaleUp)) << "No upscale within " << kNumSeconds
                                        << " seconds.";
    qs_.OnEncodeFrame(input_frame_);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    min_dimension = res.width < res.height ? res.width : res.height;
    --current_shift;
    ASSERT_EQ(input_frame_.width() >> current_shift, res.width);
    ASSERT_EQ(input_frame_.height() >> current_shift, res.height);
    ExpectScaleUsingReportedResolution();
  }

  // Verify we don't start upscaling after further low use.
  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportQP(kLowQp);
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
  input_frame_.CreateEmptyFrame(kOddWidth, kOddHeight, kOddWidth, kHalfOddWidth,
                                kHalfOddWidth);
  ContinuouslyDownscalesByHalfDimensionsAndBackUp();
}

void QualityScalerTest::DoesNotDownscaleFrameDimensions(int width, int height) {
  input_frame_.CreateEmptyFrame(width, height, width, (width + 1) / 2,
                                (width + 1) / 2);

  for (int i = 0; i < kFramerate * kNumSeconds; ++i) {
    qs_.ReportDroppedFrame();
    qs_.OnEncodeFrame(input_frame_);
    ASSERT_EQ(input_frame_.width(), qs_.GetScaledResolution().width)
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

QualityScalerTest::Resolution QualityScalerTest::TriggerResolutionChange(
    BadQualityMetric dropframe_lowqp,
    int num_second,
    int initial_framerate) {
  QualityScalerTest::Resolution res;
  res.framerate = initial_framerate;
  qs_.OnEncodeFrame(input_frame_);
  res.width = qs_.GetScaledResolution().width;
  res.height = qs_.GetScaledResolution().height;
  for (int i = 0; i < kFramerate * num_second; ++i) {
    switch (dropframe_lowqp) {
      case kReportLowQP:
        qs_.ReportQP(kLowQp);
        break;
      case kDropFrame:
        qs_.ReportDroppedFrame();
        break;
    }
    qs_.OnEncodeFrame(input_frame_);
    // Simulate the case when SetRates is called right after reducing
    // framerate.
    qs_.ReportFramerate(initial_framerate);
    res.framerate = qs_.GetTargetFramerate();
    if (res.framerate != -1)
      qs_.ReportFramerate(res.framerate);
    res.width = qs_.GetScaledResolution().width;
    res.height = qs_.GetScaledResolution().height;
  }
  return res;
}

void QualityScalerTest::VerifyQualityAdaptation(
    int initial_framerate,
    int seconds,
    bool expect_spatial_resize,
    bool expect_framerate_reduction) {
  const int kDisabledBadQpThreshold = kMaxQp + 1;
  qs_.Init(kMaxQp / QualityScaler::kDefaultLowQpDenominator,
           kDisabledBadQpThreshold, true);
  qs_.OnEncodeFrame(input_frame_);
  int init_width = qs_.GetScaledResolution().width;
  int init_height = qs_.GetScaledResolution().height;

  // Test reducing framerate by dropping frame continuously.
  QualityScalerTest::Resolution res =
      TriggerResolutionChange(kDropFrame, seconds, initial_framerate);

  if (expect_framerate_reduction) {
    EXPECT_LT(res.framerate, initial_framerate);
  } else {
    // No framerate reduction, video decimator should be disabled.
    EXPECT_EQ(-1, res.framerate);
  }

  if (expect_spatial_resize) {
    EXPECT_LT(res.width, init_width);
    EXPECT_LT(res.height, init_height);
  } else {
    EXPECT_EQ(init_width, res.width);
    EXPECT_EQ(init_height, res.height);
  }

  // The "seconds * 1.5" is to ensure spatial resolution to recover.
  // For example, in 10 seconds test, framerate reduction happens in the first
  // 5 seconds from 30fps to 15fps and causes the buffer size to be half of the
  // original one. Then it will take only 75 samples to downscale (twice in 150
  // samples). So to recover the resolution changes, we need more than 10
  // seconds (i.e, seconds * 1.5). This is because the framerate increases
  // before spatial size recovers, so it will take 150 samples to recover
  // spatial size (300 for twice).
  res = TriggerResolutionChange(kReportLowQP, seconds * 1.5, initial_framerate);
  EXPECT_EQ(-1, res.framerate);
  EXPECT_EQ(init_width, res.width);
  EXPECT_EQ(init_height, res.height);
}

// In 5 seconds test, only framerate adjusting should happen.
TEST_F(QualityScalerTest, ChangeFramerateOnly) {
  VerifyQualityAdaptation(kFramerate, 5, false, true);
}

// In 10 seconds test, framerate adjusting and scaling are both
// triggered, it shows that scaling would happen after framerate
// adjusting.
TEST_F(QualityScalerTest, ChangeFramerateAndSpatialSize) {
  VerifyQualityAdaptation(kFramerate, 10, true, true);
}

// When starting from a low framerate, only spatial size will be changed.
TEST_F(QualityScalerTest, ChangeSpatialSizeOnly) {
  qs_.ReportFramerate(kFramerate >> 1);
  VerifyQualityAdaptation(kFramerate >> 1, 10, true, false);
}

TEST_F(QualityScalerTest, DoesNotDownscaleBelow2xDefaultMinDimensionsWidth) {
  DoesNotDownscaleFrameDimensions(
      2 * QualityScaler::kDefaultMinDownscaleDimension - 1, 1000);
}

TEST_F(QualityScalerTest, DoesNotDownscaleBelow2xDefaultMinDimensionsHeight) {
  DoesNotDownscaleFrameDimensions(
      1000, 2 * QualityScaler::kDefaultMinDownscaleDimension - 1);
}

void QualityScalerTest::DownscaleEndsAt(int input_width,
                                        int input_height,
                                        int end_width,
                                        int end_height) {
  // Create a frame with 2x expected end width/height to verify that we can
  // scale down to expected end width/height.
  input_frame_.CreateEmptyFrame(input_width, input_height, input_width,
                                (input_width + 1) / 2, (input_width + 1) / 2);

  int last_width = input_width;
  int last_height = input_height;
  // Drop all frames to force-trigger downscaling.
  while (true) {
    TriggerScale(kScaleDown);
    QualityScaler::Resolution res = qs_.GetScaledResolution();
    if (last_width == res.width) {
      EXPECT_EQ(last_height, res.height);
      EXPECT_EQ(end_width, res.width);
      EXPECT_EQ(end_height, res.height);
      break;
    }
    last_width = res.width;
    last_height = res.height;
  }
}

TEST_F(QualityScalerTest, DefaultDownscalesTo160x90) {
  DownscaleEndsAt(320, 180, 160, 90);
}

TEST_F(QualityScalerTest, DefaultDownscalesTo90x160) {
  DownscaleEndsAt(180, 320, 90, 160);
}

TEST_F(QualityScalerTest, DefaultDownscalesFrom1280x720To160x90) {
  DownscaleEndsAt(1280, 720, 160, 90);
}

TEST_F(QualityScalerTest, DefaultDoesntDownscaleBelow160x90) {
  DownscaleEndsAt(320 - 1, 180 - 1, 320 - 1, 180 - 1);
}

TEST_F(QualityScalerTest, DefaultDoesntDownscaleBelow90x160) {
  DownscaleEndsAt(180 - 1, 320 - 1, 180 - 1, 320 - 1);
}

TEST_F(QualityScalerTest, RespectsMinResolutionWidth) {
  // Should end at 200x100, as width can't go lower.
  qs_.SetMinResolution(200, 10);
  DownscaleEndsAt(1600, 800, 200, 100);
}

TEST_F(QualityScalerTest, RespectsMinResolutionHeight) {
  // Should end at 100x200, as height can't go lower.
  qs_.SetMinResolution(10, 200);
  DownscaleEndsAt(800, 1600, 100, 200);
}

}  // namespace webrtc
