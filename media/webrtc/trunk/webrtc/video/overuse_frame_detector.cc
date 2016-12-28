/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/overuse_frame_detector.h"

#include <assert.h>
#include <math.h>

#include <algorithm>
#include <list>
#include <map>

#include "webrtc/base/checks.h"
#include "webrtc/base/exp_filter.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

namespace {
const int64_t kProcessIntervalMs = 5000;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 40 * 1000;
const int kMaxRampUpDelayMs = 240 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// Max number of overuses detected before always applying the rampup delay.
const int kMaxOverusesBeforeApplyRampupDelay = 4;

// The maximum exponent to use in VCMExpFilter.
const float kSampleDiffMs = 33.0f;
const float kMaxExp = 7.0f;

}  // namespace

// Class for calculating the processing usage on the send-side (the average
// processing time of a frame divided by the average time difference between
// captured frames).
class OveruseFrameDetector::SendProcessingUsage {
 public:
  explicit SendProcessingUsage(const CpuOveruseOptions& options)
      : kWeightFactorFrameDiff(0.998f),
        kWeightFactorProcessing(0.995f),
        kInitialSampleDiffMs(40.0f),
        kMaxSampleDiffMs(45.0f),
        count_(0),
        options_(options),
        filtered_processing_ms_(new rtc::ExpFilter(kWeightFactorProcessing)),
        filtered_frame_diff_ms_(new rtc::ExpFilter(kWeightFactorFrameDiff)) {
    Reset();
  }
  ~SendProcessingUsage() {}

  void Reset() {
    count_ = 0;
    filtered_frame_diff_ms_->Reset(kWeightFactorFrameDiff);
    filtered_frame_diff_ms_->Apply(1.0f, kInitialSampleDiffMs);
    filtered_processing_ms_->Reset(kWeightFactorProcessing);
    filtered_processing_ms_->Apply(1.0f, InitialProcessingMs());
  }

  void AddCaptureSample(float sample_ms) {
    float exp = sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_frame_diff_ms_->Apply(exp, sample_ms);
  }

  void AddSample(float processing_ms, int64_t diff_last_sample_ms) {
    ++count_;
    float exp = diff_last_sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_processing_ms_->Apply(exp, processing_ms);
  }

  int Value() const {
    if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
      return static_cast<int>(InitialUsageInPercent() + 0.5f);
    }
    float frame_diff_ms = std::max(filtered_frame_diff_ms_->filtered(), 1.0f);
    frame_diff_ms = std::min(frame_diff_ms, kMaxSampleDiffMs);
    float encode_usage_percent =
        100.0f * filtered_processing_ms_->filtered() / frame_diff_ms;
    return static_cast<int>(encode_usage_percent + 0.5);
  }

 private:
  float InitialUsageInPercent() const {
    // Start in between the underuse and overuse threshold.
    return (options_.low_encode_usage_threshold_percent +
            options_.high_encode_usage_threshold_percent) / 2.0f;
  }

  float InitialProcessingMs() const {
    return InitialUsageInPercent() * kInitialSampleDiffMs / 100;
  }

  const float kWeightFactorFrameDiff;
  const float kWeightFactorProcessing;
  const float kInitialSampleDiffMs;
  const float kMaxSampleDiffMs;
  uint64_t count_;
  const CpuOveruseOptions options_;
  rtc::scoped_ptr<rtc::ExpFilter> filtered_processing_ms_;
  rtc::scoped_ptr<rtc::ExpFilter> filtered_frame_diff_ms_;
};

// Class for calculating the processing time of frames.
class OveruseFrameDetector::FrameQueue {
 public:
  FrameQueue() : last_processing_time_ms_(-1) {}
  ~FrameQueue() {}

  // Called when a frame is captured.
  // Starts the measuring of the processing time of the frame.
  void Start(int64_t capture_time, int64_t now) {
    const size_t kMaxSize = 90;  // Allows for processing time of 1.5s at 60fps.
    if (frame_times_.size() > kMaxSize) {
      LOG(LS_WARNING) << "Max size reached, removed oldest frame.";
      frame_times_.erase(frame_times_.begin());
    }
    if (frame_times_.find(capture_time) != frame_times_.end()) {
      // Frame should not exist.
      assert(false);
      return;
    }
    frame_times_[capture_time] = now;
  }

  // Called when the processing of a frame has finished.
  // Returns the processing time of the frame.
  int End(int64_t capture_time, int64_t now) {
    std::map<int64_t, int64_t>::iterator it = frame_times_.find(capture_time);
    if (it == frame_times_.end()) {
      return -1;
    }
    // Remove any old frames up to current.
    // Old frames have been skipped by the capture process thread.
    // TODO(asapersson): Consider measuring time from first frame in list.
    last_processing_time_ms_ = now - (*it).second;
    frame_times_.erase(frame_times_.begin(), ++it);
    return last_processing_time_ms_;
  }

  void Reset() { frame_times_.clear(); }
  int NumFrames() const { return static_cast<int>(frame_times_.size()); }
  int last_processing_time_ms() const { return last_processing_time_ms_; }

 private:
  // Captured frames mapped by the capture time.
  std::map<int64_t, int64_t> frame_times_;
  int last_processing_time_ms_;
};


OveruseFrameDetector::OveruseFrameDetector(
    Clock* clock,
    const CpuOveruseOptions& options,
    CpuOveruseObserver* observer,
    CpuOveruseMetricsObserver* metrics_observer)
    : options_(options),
      observer_(observer),
      metrics_observer_(metrics_observer),
      clock_(clock),
      num_process_times_(0),
      last_capture_time_(0),
      num_pixels_(0),
      next_process_time_(clock_->TimeInMilliseconds()),
      last_overuse_time_(0),
      checks_above_threshold_(0),
      num_overuse_detections_(0),
      last_rampup_time_(0),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      last_sample_time_ms_(0),
      usage_(new SendProcessingUsage(options)),
      frame_queue_(new FrameQueue()) {
  RTC_DCHECK(metrics_observer != nullptr);
  // Make sure stats are initially up-to-date. This simplifies unit testing
  // since we don't have to trigger an update using one of the methods which
  // would also alter the overuse state.
  UpdateCpuOveruseMetrics();
  processing_thread_.DetachFromThread();
}

OveruseFrameDetector::~OveruseFrameDetector() {
}

int OveruseFrameDetector::LastProcessingTimeMs() const {
  rtc::CritScope cs(&crit_);
  return frame_queue_->last_processing_time_ms();
}

int OveruseFrameDetector::FramesInQueue() const {
  rtc::CritScope cs(&crit_);
  return frame_queue_->NumFrames();
}

void OveruseFrameDetector::UpdateCpuOveruseMetrics() {
  metrics_.encode_usage_percent = usage_->Value();

  metrics_observer_->CpuOveruseMetricsUpdated(metrics_);
}

int64_t OveruseFrameDetector::TimeUntilNextProcess() {
  RTC_DCHECK(processing_thread_.CalledOnValidThread());
  return next_process_time_ - clock_->TimeInMilliseconds();
}

bool OveruseFrameDetector::FrameSizeChanged(int num_pixels) const {
  if (num_pixels != num_pixels_) {
    return true;
  }
  return false;
}

bool OveruseFrameDetector::FrameTimeoutDetected(int64_t now) const {
  if (last_capture_time_ == 0) {
    return false;
  }
  return (now - last_capture_time_) > options_.frame_timeout_interval_ms;
}

void OveruseFrameDetector::ResetAll(int num_pixels) {
  num_pixels_ = num_pixels;
  usage_->Reset();
  frame_queue_->Reset();
  last_capture_time_ = 0;
  num_process_times_ = 0;
  UpdateCpuOveruseMetrics();
}

void OveruseFrameDetector::FrameCaptured(int width,
                                         int height,
                                         int64_t capture_time_ms) {
  rtc::CritScope cs(&crit_);

  int64_t now = clock_->TimeInMilliseconds();
  if (FrameSizeChanged(width * height) || FrameTimeoutDetected(now)) {
    ResetAll(width * height);
  }

  if (last_capture_time_ != 0)
    usage_->AddCaptureSample(now - last_capture_time_);

  last_capture_time_ = now;

  frame_queue_->Start(capture_time_ms, now);
}

void OveruseFrameDetector::FrameSent(int64_t capture_time_ms) {
  rtc::CritScope cs(&crit_);
  int delay_ms = frame_queue_->End(capture_time_ms,
                                   clock_->TimeInMilliseconds());
  if (delay_ms > 0) {
    AddProcessingTime(delay_ms);
  }
}

void OveruseFrameDetector::AddProcessingTime(int elapsed_ms) {
  int64_t now = clock_->TimeInMilliseconds();
  if (last_sample_time_ms_ != 0) {
    int64_t diff_ms = now - last_sample_time_ms_;
    usage_->AddSample(elapsed_ms, diff_ms);
  }
  last_sample_time_ms_ = now;
  UpdateCpuOveruseMetrics();
}

int32_t OveruseFrameDetector::Process() {
  RTC_DCHECK(processing_thread_.CalledOnValidThread());

  int64_t now = clock_->TimeInMilliseconds();

  // Used to protect against Process() being called too often.
  if (now < next_process_time_)
    return 0;

  next_process_time_ = now + kProcessIntervalMs;

  CpuOveruseMetrics current_metrics;
  {
    rtc::CritScope cs(&crit_);
    ++num_process_times_;

    current_metrics = metrics_;
    if (num_process_times_ <= options_.min_process_count)
      return 0;
  }

  if (IsOverusing(current_metrics)) {
    // If the last thing we did was going up, and now have to back down, we need
    // to check if this peak was short. If so we should back off to avoid going
    // back and forth between this load, the system doesn't seem to handle it.
    bool check_for_backoff = last_rampup_time_ > last_overuse_time_;
    if (check_for_backoff) {
      if (now - last_rampup_time_ < kStandardRampUpDelayMs ||
          num_overuse_detections_ > kMaxOverusesBeforeApplyRampupDelay) {
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
    ++num_overuse_detections_;

    if (observer_ != NULL)
      observer_->OveruseDetected();
  } else if (IsUnderusing(current_metrics, now)) {
    last_rampup_time_ = now;
    in_quick_rampup_ = true;

    if (observer_ != NULL)
      observer_->NormalUsage();
  }

  int rampup_delay =
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;

  LOG(LS_VERBOSE) << " Frame stats: "
                  << " encode usage " << current_metrics.encode_usage_percent
                  << " overuse detections " << num_overuse_detections_
                  << " rampup delay " << rampup_delay;

  return 0;
}

bool OveruseFrameDetector::IsOverusing(const CpuOveruseMetrics& metrics) {
  if (metrics.encode_usage_percent >=
      options_.high_encode_usage_threshold_percent) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }
  return checks_above_threshold_ >= options_.high_threshold_consecutive_count;
}

bool OveruseFrameDetector::IsUnderusing(const CpuOveruseMetrics& metrics,
                                        int64_t time_now) {
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ + delay)
    return false;

  return metrics.encode_usage_percent <
         options_.low_encode_usage_threshold_percent;
}
}  // namespace webrtc
