/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/overuse_frame_detector.h"

#include <algorithm>
#include <assert.h>
#include <math.h>

#include "webrtc/modules/video_coding/utility/include/exp_filter.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

// TODO(mflodman) Test different values for all of these to trigger correctly,
// avoid fluctuations etc.
namespace {
const int64_t kProcessIntervalMs = 5000;

// Consecutive checks above threshold to trigger overuse.
const int kConsecutiveChecksAboveThreshold = 2;

// Minimum samples required to perform a check.
const size_t kMinFrameSampleCount = 15;

// Weight factor to apply to the standard deviation.
const float kWeightFactor = 0.997f;

// Weight factor to apply to the average.
const float kWeightFactorMean = 0.98f;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 30 * 1000;
const int kMaxRampUpDelayMs = 120 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

}  // namespace

Statistics::Statistics() :
    sum_(0.0),
    count_(0),
    filtered_samples_(new VCMExpFilter(kWeightFactorMean)),
    filtered_variance_(new VCMExpFilter(kWeightFactor)) {
}

void Statistics::Reset() {
  sum_ =  0.0;
  count_ = 0;
}

void Statistics::AddSample(float sample_ms) {
  sum_ += sample_ms;
  ++count_;

  if (count_ < kMinFrameSampleCount) {
    // Initialize filtered samples.
    filtered_samples_->Reset(kWeightFactorMean);
    filtered_samples_->Apply(1.0f, InitialMean());
    filtered_variance_->Reset(kWeightFactor);
    filtered_variance_->Apply(1.0f, InitialVariance());
    return;
  }

  float exp = sample_ms/33.0f;
  exp = std::min(exp, 7.0f);
  filtered_samples_->Apply(exp, sample_ms);
  filtered_variance_->Apply(exp, (sample_ms - filtered_samples_->Value()) *
                                 (sample_ms - filtered_samples_->Value()));
}

float Statistics::InitialMean() const {
  if (count_ == 0)
    return 0.0;
  return sum_ / count_;
}

float Statistics::InitialVariance() const {
  // Start in between the underuse and overuse threshold.
  float average_stddev = (kNormalUseStdDevMs + kOveruseStdDevMs)/2.0f;
  return average_stddev * average_stddev;
}

float Statistics::Mean() const { return filtered_samples_->Value(); }

float Statistics::StdDev() const {
  return sqrt(std::max(filtered_variance_->Value(), 0.0f));
}

uint64_t Statistics::Count() const { return count_; }

OveruseFrameDetector::OveruseFrameDetector(Clock* clock,
                                           float normaluse_stddev_ms,
                                           float overuse_stddev_ms)
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      normaluse_stddev_ms_(normaluse_stddev_ms),
      overuse_stddev_ms_(overuse_stddev_ms),
      observer_(NULL),
      clock_(clock),
      next_process_time_(clock_->TimeInMilliseconds()),
      last_capture_time_(0),
      last_overuse_time_(0),
      checks_above_threshold_(0),
      last_rampup_time_(0),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      num_pixels_(0) {}

OveruseFrameDetector::~OveruseFrameDetector() {
}

void OveruseFrameDetector::SetObserver(CpuOveruseObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  observer_ = observer;
}

void OveruseFrameDetector::FrameCaptured(int width, int height) {
  CriticalSectionScoped cs(crit_.get());

  int num_pixels = width * height;
  if (num_pixels != num_pixels_) {
    // Frame size changed, reset statistics.
    num_pixels_ = num_pixels;
    capture_deltas_.Reset();
    last_capture_time_ = 0;
  }

  int64_t time = clock_->TimeInMilliseconds();
  if (last_capture_time_ != 0) {
    capture_deltas_.AddSample(time - last_capture_time_);
  }
  last_capture_time_ = time;
}

int32_t OveruseFrameDetector::TimeUntilNextProcess() {
  CriticalSectionScoped cs(crit_.get());
  return next_process_time_ - clock_->TimeInMilliseconds();
}

int32_t OveruseFrameDetector::Process() {
  CriticalSectionScoped cs(crit_.get());

  int64_t now = clock_->TimeInMilliseconds();

  // Used to protect against Process() being called too often.
  if (now < next_process_time_)
    return 0;

  next_process_time_ = now + kProcessIntervalMs;

  // Don't trigger overuse unless we've seen a certain number of frames.
  if (capture_deltas_.Count() < kMinFrameSampleCount)
    return 0;

  if (IsOverusing()) {
    // If the last thing we did was going up, and now have to back down, we need
    // to check if this peak was short. If so we should back off to avoid going
    // back and forth between this load, the system doesn't seem to handle it.
    bool check_for_backoff = last_rampup_time_ > last_overuse_time_;
    if (check_for_backoff) {
      if (now - last_rampup_time_ < kStandardRampUpDelayMs) {
        // Going up was not ok for very long, back off.
        current_rampup_delay_ms_ *= kRampUpBackoffFactor;
        if (current_rampup_delay_ms_ > kMaxRampUpDelayMs)
          current_rampup_delay_ms_ = kMaxRampUpDelayMs;
      } else {
        // Not currently backing off, reset rampup delay.
        current_rampup_delay_ms_ = kStandardRampUpDelayMs;
      }
    }

    last_overuse_time_ = now;
    in_quick_rampup_ = false;
    checks_above_threshold_ = 0;

    if (observer_ != NULL)
      observer_->OveruseDetected();
  } else if (IsUnderusing(now)) {
    last_rampup_time_ = now;
    in_quick_rampup_ = true;

    if (observer_ != NULL)
      observer_->NormalUsage();
  }

  WEBRTC_TRACE(
      webrtc::kTraceInfo,
      webrtc::kTraceVideo,
      -1,
      "Capture input stats: avg: %.2fms, std_dev: %.2fms (rampup delay: "
      "%dms, overuse: >=%.2fms, "
      "underuse: <%.2fms)",
      capture_deltas_.Mean(),
      capture_deltas_.StdDev(),
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_,
      overuse_stddev_ms_,
      normaluse_stddev_ms_);

  return 0;
}

bool OveruseFrameDetector::IsOverusing() {
  if (capture_deltas_.StdDev() >= overuse_stddev_ms_) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }

  return checks_above_threshold_ >= kConsecutiveChecksAboveThreshold;
}

bool OveruseFrameDetector::IsUnderusing(int64_t time_now) {
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ + delay)
    return false;

  return capture_deltas_.StdDev() < normaluse_stddev_ms_;
}
}  // namespace webrtc
