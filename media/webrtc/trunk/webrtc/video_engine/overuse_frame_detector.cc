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

#include <assert.h>
#include <math.h>

#include <algorithm>
#include <list>
#include <map>

#include "webrtc/base/checks.h"
#include "webrtc/base/exp_filter.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

// TODO(mflodman) Test different values for all of these to trigger correctly,
// avoid fluctuations etc.
namespace {
const int64_t kProcessIntervalMs = 5000;

// Weight factor to apply to the standard deviation.
const float kWeightFactor = 0.997f;
// Weight factor to apply to the average.
const float kWeightFactorMean = 0.98f;

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

// TODO(asapersson): Remove this class. Not used.
Statistics::Statistics() :
    sum_(0.0),
    count_(0),
    filtered_samples_(new rtc::ExpFilter(kWeightFactorMean)),
    filtered_variance_(new rtc::ExpFilter(kWeightFactor)) {
  Reset();
}

void Statistics::SetOptions(const CpuOveruseOptions& options) {
  options_ = options;
}

void Statistics::Reset() {
  sum_ =  0.0;
  count_ = 0;
  filtered_variance_->Reset(kWeightFactor);
  filtered_variance_->Apply(1.0f, InitialVariance());
}

void Statistics::AddSample(float sample_ms) {
  sum_ += sample_ms;
  ++count_;

  if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
    // Initialize filtered samples.
    filtered_samples_->Reset(kWeightFactorMean);
    filtered_samples_->Apply(1.0f, InitialMean());
    return;
  }

  float exp = sample_ms / kSampleDiffMs;
  exp = std::min(exp, kMaxExp);
  filtered_samples_->Apply(exp, sample_ms);
  filtered_variance_->Apply(exp, (sample_ms - filtered_samples_->filtered()) *
                                 (sample_ms - filtered_samples_->filtered()));
}

float Statistics::InitialMean() const {
  if (count_ == 0)
    return 0.0;
  return sum_ / count_;
}

float Statistics::InitialVariance() const {
  // Start in between the underuse and overuse threshold.
  float average_stddev = (options_.low_capture_jitter_threshold_ms +
                          options_.high_capture_jitter_threshold_ms) / 2.0f;
  return average_stddev * average_stddev;
}

float Statistics::Mean() const { return filtered_samples_->filtered(); }

float Statistics::StdDev() const {
  return sqrt(std::max(filtered_variance_->filtered(), 0.0f));
}

uint64_t Statistics::Count() const { return count_; }


// Class for calculating the average encode time.
class OveruseFrameDetector::EncodeTimeAvg {
 public:
  EncodeTimeAvg()
      : kWeightFactor(0.5f),
        kInitialAvgEncodeTimeMs(5.0f),
        filtered_encode_time_ms_(new rtc::ExpFilter(kWeightFactor)) {
    filtered_encode_time_ms_->Apply(1.0f, kInitialAvgEncodeTimeMs);
  }
  ~EncodeTimeAvg() {}

  void AddSample(float encode_time_ms, int64_t diff_last_sample_ms) {
    float exp =  diff_last_sample_ms / kSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_encode_time_ms_->Apply(exp, encode_time_ms);
  }

  int Value() const {
    return static_cast<int>(filtered_encode_time_ms_->filtered() + 0.5);
  }

 private:
  const float kWeightFactor;
  const float kInitialAvgEncodeTimeMs;
  rtc::scoped_ptr<rtc::ExpFilter> filtered_encode_time_ms_;
};

// Class for calculating the processing usage on the send-side (the average
// processing time of a frame divided by the average time difference between
// captured frames).
class OveruseFrameDetector::SendProcessingUsage {
 public:
  SendProcessingUsage()
      : kWeightFactorFrameDiff(0.998f),
        kWeightFactorProcessing(0.995f),
        kInitialSampleDiffMs(40.0f),
        kMaxSampleDiffMs(45.0f),
        count_(0),
        filtered_processing_ms_(new rtc::ExpFilter(kWeightFactorProcessing)),
        filtered_frame_diff_ms_(new rtc::ExpFilter(kWeightFactorFrameDiff)) {
    Reset();
  }
  ~SendProcessingUsage() {}

  void SetOptions(const CpuOveruseOptions& options) {
    options_ = options;
  }

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
  CpuOveruseOptions options_;
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
  int NumFrames() const { return frame_times_.size(); }
  int last_processing_time_ms() const { return last_processing_time_ms_; }

 private:
  // Captured frames mapped by the capture time.
  std::map<int64_t, int64_t> frame_times_;
  int last_processing_time_ms_;
};

// TODO(asapersson): Remove this class. Not used.
// Class for calculating the capture queue delay change.
class OveruseFrameDetector::CaptureQueueDelay {
 public:
  CaptureQueueDelay()
      : kWeightFactor(0.5f),
        delay_ms_(0),
        filtered_delay_ms_per_s_(new rtc::ExpFilter(kWeightFactor)) {
    filtered_delay_ms_per_s_->Apply(1.0f, 0.0f);
  }
  ~CaptureQueueDelay() {}

  void FrameCaptured(int64_t now) {
    const size_t kMaxSize = 200;
    if (frames_.size() > kMaxSize) {
      frames_.pop_front();
    }
    frames_.push_back(now);
  }

  void FrameProcessingStarted(int64_t now) {
    if (frames_.empty()) {
      return;
    }
    delay_ms_ = now - frames_.front();
    frames_.pop_front();
  }

  void CalculateDelayChange(int64_t diff_last_sample_ms) {
    if (diff_last_sample_ms <= 0) {
      return;
    }
    float exp = static_cast<float>(diff_last_sample_ms) / kProcessIntervalMs;
    exp = std::min(exp, kMaxExp);
    filtered_delay_ms_per_s_->Apply(exp,
                                    delay_ms_ * 1000.0f / diff_last_sample_ms);
    ClearFrames();
  }

  void ClearFrames() {
    frames_.clear();
  }

  int delay_ms() const {
    return delay_ms_;
  }

  int Value() const {
    return static_cast<int>(filtered_delay_ms_per_s_->filtered() + 0.5);
  }

 private:
  const float kWeightFactor;
  std::list<int64_t> frames_;
  int delay_ms_;
  rtc::scoped_ptr<rtc::ExpFilter> filtered_delay_ms_per_s_;
};

OveruseFrameDetector::OveruseFrameDetector(
    Clock* clock,
    CpuOveruseMetricsObserver* metrics_observer)
    : observer_(NULL),
      metrics_observer_(metrics_observer),
      clock_(clock),
      next_process_time_(clock_->TimeInMilliseconds()),
      num_process_times_(0),
      last_capture_time_(0),
      last_overuse_time_(0),
      checks_above_threshold_(0),
      num_overuse_detections_(0),
      last_rampup_time_(0),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      num_pixels_(0),
      last_encode_sample_ms_(0),
      encode_time_(new EncodeTimeAvg()),
      usage_(new SendProcessingUsage()),
      frame_queue_(new FrameQueue()),
      last_sample_time_ms_(0),
      capture_queue_delay_(new CaptureQueueDelay()) {
  DCHECK(metrics_observer != nullptr);
  processing_thread_.DetachFromThread();
}

OveruseFrameDetector::~OveruseFrameDetector() {
}

void OveruseFrameDetector::SetObserver(CpuOveruseObserver* observer) {
  rtc::CritScope cs(&crit_);
  observer_ = observer;
}

void OveruseFrameDetector::SetOptions(const CpuOveruseOptions& options) {
  assert(options.min_frame_samples > 0);
  rtc::CritScope cs(&crit_);
  if (options_.Equals(options)) {
    return;
  }
  options_ = options;
  capture_deltas_.SetOptions(options);
  usage_->SetOptions(options);
  ResetAll(num_pixels_);
}

int OveruseFrameDetector::CaptureQueueDelayMsPerS() const {
  rtc::CritScope cs(&crit_);
  return capture_queue_delay_->delay_ms();
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
  metrics_.capture_jitter_ms = static_cast<int>(capture_deltas_.StdDev() + 0.5);
  metrics_.avg_encode_time_ms = encode_time_->Value();
  metrics_.encode_usage_percent = usage_->Value();
  metrics_.capture_queue_delay_ms_per_s = capture_queue_delay_->Value();

  metrics_observer_->CpuOveruseMetricsUpdated(metrics_);
}

int64_t OveruseFrameDetector::TimeUntilNextProcess() {
  DCHECK(processing_thread_.CalledOnValidThread());
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
  capture_deltas_.Reset();
  usage_->Reset();
  frame_queue_->Reset();
  capture_queue_delay_->ClearFrames();
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

  if (last_capture_time_ != 0) {
    capture_deltas_.AddSample(now - last_capture_time_);
    usage_->AddCaptureSample(now - last_capture_time_);
  }
  last_capture_time_ = now;

  capture_queue_delay_->FrameCaptured(now);

  if (options_.enable_extended_processing_usage) {
    frame_queue_->Start(capture_time_ms, now);
  }
  UpdateCpuOveruseMetrics();
}

void OveruseFrameDetector::FrameProcessingStarted() {
  rtc::CritScope cs(&crit_);
  capture_queue_delay_->FrameProcessingStarted(clock_->TimeInMilliseconds());
}

void OveruseFrameDetector::FrameEncoded(int encode_time_ms) {
  rtc::CritScope cs(&crit_);
  int64_t now = clock_->TimeInMilliseconds();
  if (last_encode_sample_ms_ != 0) {
    int64_t diff_ms = now - last_encode_sample_ms_;
    encode_time_->AddSample(encode_time_ms, diff_ms);
  }
  last_encode_sample_ms_ = now;

  if (!options_.enable_extended_processing_usage) {
    AddProcessingTime(encode_time_ms);
  }
  UpdateCpuOveruseMetrics();
}

void OveruseFrameDetector::FrameSent(int64_t capture_time_ms) {
  rtc::CritScope cs(&crit_);
  if (!options_.enable_extended_processing_usage) {
    return;
  }
  int delay_ms = frame_queue_->End(capture_time_ms,
                                   clock_->TimeInMilliseconds());
  if (delay_ms > 0) {
    AddProcessingTime(delay_ms);
  }
  UpdateCpuOveruseMetrics();
}

void OveruseFrameDetector::AddProcessingTime(int elapsed_ms) {
  int64_t now = clock_->TimeInMilliseconds();
  if (last_sample_time_ms_ != 0) {
    int64_t diff_ms = now - last_sample_time_ms_;
    usage_->AddSample(elapsed_ms, diff_ms);
  }
  last_sample_time_ms_ = now;
}

int32_t OveruseFrameDetector::Process() {
  DCHECK(processing_thread_.CalledOnValidThread());

  int64_t now = clock_->TimeInMilliseconds();

  // Used to protect against Process() being called too often.
  if (now < next_process_time_)
    return 0;

  int64_t diff_ms = now - next_process_time_ + kProcessIntervalMs;
  next_process_time_ = now + kProcessIntervalMs;

  rtc::CritScope cs(&crit_);
  ++num_process_times_;

  capture_queue_delay_->CalculateDelayChange(diff_ms);
  UpdateCpuOveruseMetrics();

  if (num_process_times_ <= options_.min_process_count) {
    return 0;
  }

  if (IsOverusing()) {
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
  } else if (IsUnderusing(now)) {
    last_rampup_time_ = now;
    in_quick_rampup_ = true;

    if (observer_ != NULL)
      observer_->NormalUsage();
  }

  int rampup_delay =
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  LOG(LS_VERBOSE) << " Frame stats: capture avg: " << capture_deltas_.Mean()
                  << " capture stddev " << capture_deltas_.StdDev()
                  << " encode usage " << usage_->Value()
                  << " overuse detections " << num_overuse_detections_
                  << " rampup delay " << rampup_delay;

  return 0;
}

bool OveruseFrameDetector::IsOverusing() {
  bool overusing = false;
  if (options_.enable_capture_jitter_method) {
    overusing = capture_deltas_.StdDev() >=
        options_.high_capture_jitter_threshold_ms;
  } else if (options_.enable_encode_usage_method) {
    overusing = usage_->Value() >= options_.high_encode_usage_threshold_percent;
  }

  if (overusing) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }
  return checks_above_threshold_ >= options_.high_threshold_consecutive_count;
}

bool OveruseFrameDetector::IsUnderusing(int64_t time_now) {
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ + delay)
    return false;

  bool underusing = false;
  if (options_.enable_capture_jitter_method) {
    underusing = capture_deltas_.StdDev() <
        options_.low_capture_jitter_threshold_ms;
  } else if (options_.enable_encode_usage_method) {
    underusing = usage_->Value() < options_.low_encode_usage_threshold_percent;
  }
  return underusing;
}
}  // namespace webrtc
