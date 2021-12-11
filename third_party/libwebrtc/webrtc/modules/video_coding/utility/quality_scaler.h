/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
#define MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_

#include <utility>

#include "api/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/utility/moving_average.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {

// An interface for signaling requests to limit or increase the resolution or
// framerate of the captured video stream.
class AdaptationObserverInterface {
 public:
  // Indicates if the adaptation is due to overuse of the CPU resources, or if
  // the quality of the encoded frames have dropped too low.
  enum AdaptReason : size_t { kQuality = 0, kCpu = 1 };
  static const size_t kScaleReasonSize = 2;
  // Called to signal that we can handle larger or more frequent frames.
  virtual void AdaptUp(AdaptReason reason) = 0;
  // Called to signal that the source should reduce the resolution or framerate.
  virtual void AdaptDown(AdaptReason reason) = 0;

 protected:
  virtual ~AdaptationObserverInterface() {}
};

// QualityScaler runs asynchronously and monitors QP values of encoded frames.
// It holds a reference to an AdaptationObserverInterface implementation to
// signal an intent to scale up or down.
class QualityScaler {
 public:
  // Construct a QualityScaler with a given |observer|.
  // This starts the quality scaler periodically checking what the average QP
  // has been recently.
  QualityScaler(AdaptationObserverInterface* observer,
                VideoCodecType codec_type);
  // If specific thresholds are desired these can be supplied as |thresholds|.
  QualityScaler(AdaptationObserverInterface* observer,
                VideoEncoder::QpThresholds thresholds);
  virtual ~QualityScaler();
  // Should be called each time the encoder drops a frame
  void ReportDroppedFrame();
  // Inform the QualityScaler of the last seen QP.
  void ReportQP(int qp);

  // The following members declared protected for testing purposes
 protected:
  QualityScaler(AdaptationObserverInterface* observer,
                VideoEncoder::QpThresholds thresholds,
                int64_t sampling_period);

 private:
  class CheckQPTask;
  void CheckQP();
  void ClearSamples();
  void ReportQPLow();
  void ReportQPHigh();
  int64_t GetSamplingPeriodMs() const;

  CheckQPTask* check_qp_task_ RTC_GUARDED_BY(&task_checker_);
  AdaptationObserverInterface* const observer_ RTC_GUARDED_BY(&task_checker_);
  rtc::SequencedTaskChecker task_checker_;

  const int64_t sampling_period_ms_;
  bool fast_rampup_ RTC_GUARDED_BY(&task_checker_);
  MovingAverage average_qp_ RTC_GUARDED_BY(&task_checker_);
  MovingAverage framedrop_percent_ RTC_GUARDED_BY(&task_checker_);

  VideoEncoder::QpThresholds thresholds_ RTC_GUARDED_BY(&task_checker_);
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
