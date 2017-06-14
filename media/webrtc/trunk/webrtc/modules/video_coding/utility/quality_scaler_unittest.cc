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

#include <memory>

#include "webrtc/base/event.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
static const int kFramerate = 30;
static const int kLowQp = 15;
static const int kLowQpThreshold = 18;
static const int kHighQp = 40;
static const size_t kDefaultTimeoutMs = 150;
}  // namespace

class MockScaleObserver : public ScalingObserverInterface {
 public:
  MockScaleObserver() : event(false, false) {}
  virtual ~MockScaleObserver() {}

  void ScaleUp(ScaleReason r) override {
    scaled_up++;
    event.Set();
  }
  void ScaleDown(ScaleReason r) override {
    scaled_down++;
    event.Set();
  }

  rtc::Event event;
  int scaled_up = 0;
  int scaled_down = 0;
};

// Pass a lower sampling period to speed up the tests.
class QualityScalerUnderTest : public QualityScaler {
 public:
  explicit QualityScalerUnderTest(ScalingObserverInterface* observer,
                                  VideoEncoder::QpThresholds thresholds)
      : QualityScaler(observer, thresholds, 5) {}
};

class QualityScalerTest : public ::testing::Test {
 protected:
  enum ScaleDirection {
    kKeepScaleAtHighQp,
    kScaleDown,
    kScaleDownAboveHighQp,
    kScaleUp
  };

  QualityScalerTest()
      : q_(new rtc::TaskQueue("QualityScalerTestQueue")),
        observer_(new MockScaleObserver()) {
    rtc::Event event(false, false);
    q_->PostTask([this, &event] {
      qs_ = std::unique_ptr<QualityScaler>(new QualityScalerUnderTest(
          observer_.get(),
          VideoEncoder::QpThresholds(kLowQpThreshold, kHighQp)));
      event.Set();
    });
    EXPECT_TRUE(event.Wait(kDefaultTimeoutMs));
  }

  ~QualityScalerTest() {
    rtc::Event event(false, false);
    q_->PostTask([this, &event] {
      qs_.reset(nullptr);
      event.Set();
    });
    EXPECT_TRUE(event.Wait(kDefaultTimeoutMs));
  }

  void TriggerScale(ScaleDirection scale_direction) {
    for (int i = 0; i < kFramerate * 5; ++i) {
      switch (scale_direction) {
        case kScaleUp:
          qs_->ReportQP(kLowQp);
          break;
        case kScaleDown:
          qs_->ReportDroppedFrame();
          break;
        case kKeepScaleAtHighQp:
          qs_->ReportQP(kHighQp);
          break;
        case kScaleDownAboveHighQp:
          qs_->ReportQP(kHighQp + 1);
          break;
      }
    }
  }

  std::unique_ptr<rtc::TaskQueue> q_;
  std::unique_ptr<QualityScaler> qs_;
  std::unique_ptr<MockScaleObserver> observer_;
};

#define DISABLED_TEST(basename, test) TEST_F(basename, DISABLED_##test)
DISABLED_TEST(QualityScalerTest, DownscalesAfterContinuousFramedrop) {
  q_->PostTask([this] { TriggerScale(kScaleDown); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->scaled_down);
}

DISABLED_TEST(QualityScalerTest, KeepsScaleAtHighQp) {
  q_->PostTask([this] { TriggerScale(kKeepScaleAtHighQp); });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->scaled_down);
  EXPECT_EQ(0, observer_->scaled_up);
}

DISABLED_TEST(QualityScalerTest, DownscalesAboveHighQp) {
  q_->PostTask([this] { TriggerScale(kScaleDownAboveHighQp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->scaled_down);
  EXPECT_EQ(0, observer_->scaled_up);
}

DISABLED_TEST(QualityScalerTest, DownscalesAfterTwoThirdsFramedrop) {
  q_->PostTask([this] {
    qs_->ReportDroppedFrame();
    qs_->ReportDroppedFrame();
    qs_->ReportQP(kHighQp);
  });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->scaled_down);
  EXPECT_EQ(0, observer_->scaled_up);
}

DISABLED_TEST(QualityScalerTest, DoesNotDownscaleOnNormalQp) {
  q_->PostTask([this] { TriggerScale(kScaleDownAboveHighQp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->scaled_down);
  EXPECT_EQ(0, observer_->scaled_up);
}

DISABLED_TEST(QualityScalerTest, DoesNotDownscaleAfterHalfFramedrop) {
  q_->PostTask([this] {
    qs_->ReportDroppedFrame();
    qs_->ReportQP(kHighQp);
  });
  EXPECT_FALSE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->scaled_down);
  EXPECT_EQ(0, observer_->scaled_up);
}

DISABLED_TEST(QualityScalerTest, UpscalesAfterLowQp) {
  q_->PostTask([this] { TriggerScale(kScaleUp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(0, observer_->scaled_down);
  EXPECT_EQ(1, observer_->scaled_up);
}

DISABLED_TEST(QualityScalerTest, ScalesDownAndBackUp) {
  q_->PostTask([this] { TriggerScale(kScaleDown); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->scaled_down);
  EXPECT_EQ(0, observer_->scaled_up);
  q_->PostTask([this] { TriggerScale(kScaleUp); });
  EXPECT_TRUE(observer_->event.Wait(kDefaultTimeoutMs));
  EXPECT_EQ(1, observer_->scaled_down);
  EXPECT_EQ(1, observer_->scaled_up);
}
#undef DISABLED_TEST
}  // namespace webrtc
