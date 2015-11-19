/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
#define WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/exp_filter.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

class Clock;
class CpuOveruseObserver;

// TODO(pbos): Move this somewhere appropriate.
class Statistics {
 public:
  Statistics();

  void AddSample(float sample_ms);
  void Reset();
  void SetOptions(const CpuOveruseOptions& options);

  float Mean() const;
  float StdDev() const;
  uint64_t Count() const;

 private:
  float InitialMean() const;
  float InitialVariance() const;

  float sum_;
  uint64_t count_;
  CpuOveruseOptions options_;
  rtc::scoped_ptr<rtc::ExpFilter> filtered_samples_;
  rtc::scoped_ptr<rtc::ExpFilter> filtered_variance_;
};

// Use to detect system overuse based on jitter in incoming frames.
class OveruseFrameDetector : public Module {
 public:
  OveruseFrameDetector(Clock* clock,
                       CpuOveruseMetricsObserver* metrics_observer);
  ~OveruseFrameDetector();

  // Registers an observer receiving overuse and underuse callbacks. Set
  // 'observer' to NULL to disable callbacks.
  void SetObserver(CpuOveruseObserver* observer);

  // Sets options for overuse detection.
  void SetOptions(const CpuOveruseOptions& options);

  // Called for each captured frame.
  void FrameCaptured(int width, int height, int64_t capture_time_ms);

  // Called when the processing of a captured frame is started.
  void FrameProcessingStarted();

  // Called for each encoded frame.
  void FrameEncoded(int encode_time_ms);

  // Called for each sent frame.
  void FrameSent(int64_t capture_time_ms);

  // Only public for testing.
  int CaptureQueueDelayMsPerS() const;
  int LastProcessingTimeMs() const;
  int FramesInQueue() const;

  // Implements Module.
  int64_t TimeUntilNextProcess() override;
  int32_t Process() override;

 private:
  class EncodeTimeAvg;
  class SendProcessingUsage;
  class CaptureQueueDelay;
  class FrameQueue;

  void UpdateCpuOveruseMetrics() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // TODO(asapersson): This method is only used on one thread, so it shouldn't
  // need a guard.
  void AddProcessingTime(int elapsed_ms) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // TODO(asapersson): This method is always called on the processing thread.
  // If locking is required, consider doing that locking inside the
  // implementation and reduce scope as much as possible.  We should also
  // see if we can avoid calling out to other methods while holding the lock.
  bool IsOverusing() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  bool IsUnderusing(int64_t time_now) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  bool FrameTimeoutDetected(int64_t now) const EXCLUSIVE_LOCKS_REQUIRED(crit_);
  bool FrameSizeChanged(int num_pixels) const EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void ResetAll(int num_pixels) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Protecting all members except const and those that are only accessed on the
  // processing thread.
  // TODO(asapersson): See if we can reduce locking.  As is, video frame
  // processing contends with reading stats and the processing thread.
  mutable rtc::CriticalSection crit_;

  // Observer getting overuse reports.
  CpuOveruseObserver* observer_ GUARDED_BY(crit_);

  CpuOveruseOptions options_ GUARDED_BY(crit_);

  // Stats metrics.
  CpuOveruseMetricsObserver* const metrics_observer_;
  CpuOveruseMetrics metrics_ GUARDED_BY(crit_);

  Clock* const clock_;
  int64_t next_process_time_;  // Only accessed on the processing thread.
  int64_t num_process_times_ GUARDED_BY(crit_);

  Statistics capture_deltas_ GUARDED_BY(crit_);
  int64_t last_capture_time_ GUARDED_BY(crit_);

  // These six members are only accessed on the processing thread.
  int64_t last_overuse_time_;
  int checks_above_threshold_;
  int num_overuse_detections_;

  int64_t last_rampup_time_;
  bool in_quick_rampup_;
  int current_rampup_delay_ms_;

  // Number of pixels of last captured frame.
  int num_pixels_ GUARDED_BY(crit_);

  int64_t last_encode_sample_ms_;  // Only accessed by one thread.

  // TODO(asapersson): Can these be regular members (avoid separate heap
  // allocs)?
  const rtc::scoped_ptr<EncodeTimeAvg> encode_time_ GUARDED_BY(crit_);
  const rtc::scoped_ptr<SendProcessingUsage> usage_ GUARDED_BY(crit_);
  const rtc::scoped_ptr<FrameQueue> frame_queue_ GUARDED_BY(crit_);

  int64_t last_sample_time_ms_;  // Only accessed by one thread.

  const rtc::scoped_ptr<CaptureQueueDelay> capture_queue_delay_
      GUARDED_BY(crit_);

  rtc::ThreadChecker processing_thread_;

  DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
