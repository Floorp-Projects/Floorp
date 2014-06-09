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

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/gtest_prod_util.h"

namespace webrtc {

class Clock;
class CpuOveruseObserver;
class CriticalSectionWrapper;
class VCMExpFilter;

// Limits on standard deviation for under/overuse.
#ifdef WEBRTC_LINUX
const float kOveruseStdDevMs = 15.0f;
const float kNormalUseStdDevMs = 7.0f;
#elif WEBRTC_MAC
const float kOveruseStdDevMs = 24.0f;
const float kNormalUseStdDevMs = 14.0f;
#else
const float kOveruseStdDevMs = 17.0f;
const float kNormalUseStdDevMs = 10.0f;
#endif

// TODO(pbos): Move this somewhere appropriate.
class Statistics {
 public:
  Statistics();

  void AddSample(float sample_ms);
  void Reset();

  float Mean() const;
  float StdDev() const;
  uint64_t Count() const;

 private:
  float InitialMean() const;
  float InitialVariance() const;

  float sum_;
  uint64_t count_;
  scoped_ptr<VCMExpFilter> filtered_samples_;
  scoped_ptr<VCMExpFilter> filtered_variance_;
};

// Use to detect system overuse based on jitter in incoming frames.
class OveruseFrameDetector : public Module {
 public:
  explicit OveruseFrameDetector(Clock* clock,
                                float normaluse_stddev_ms,
                                float overuse_stddev_ms);
  ~OveruseFrameDetector();

  // Registers an observer receiving overuse and underuse callbacks. Set
  // 'observer' to NULL to disable callbacks.
  void SetObserver(CpuOveruseObserver* observer);

  // Called for each captured frame.
  void FrameCaptured(int width, int height);

  // Called when the processing of a captured frame is started.
  void FrameProcessingStarted();

  // Called for each encoded frame.
  void FrameEncoded(int encode_time_ms);

  // Accessors.
  // The last estimated jitter based on the incoming captured frames.
  int last_capture_jitter_ms() const;

  // Running average of reported encode time (FrameEncoded()).
  // Only used for stats.
  int AvgEncodeTimeMs() const;

  // The average encode time divided by the average time difference between
  // incoming captured frames.
  // This variable is currently only used for statistics.
  int EncodeUsagePercent() const;

  // The current time delay between an incoming captured frame (FrameCaptured())
  // until the frame is being processed (FrameProcessingStarted()).
  // (Note: if a new frame is received before an old frame has been processed,
  // the old frame is skipped).
  // The delay is returned as the delay in ms per second.
  // This variable is currently only used for statistics.
  int AvgCaptureQueueDelayMsPerS() const;
  int CaptureQueueDelayMsPerS() const;

  // Implements Module.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(OveruseFrameDetectorTest, TriggerOveruse);
  FRIEND_TEST_ALL_PREFIXES(OveruseFrameDetectorTest, OveruseAndRecover);
  FRIEND_TEST_ALL_PREFIXES(OveruseFrameDetectorTest, DoubleOveruseAndRecover);
  FRIEND_TEST_ALL_PREFIXES(
      OveruseFrameDetectorTest, TriggerNormalUsageWithMinProcessCount);
  FRIEND_TEST_ALL_PREFIXES(
      OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage);
  FRIEND_TEST_ALL_PREFIXES(OveruseFrameDetectorTest, LastCaptureJitter);

  void set_min_process_count_before_reporting(int64_t count) {
    min_process_count_before_reporting_ = count;
  }

  class EncodeTimeAvg;
  class EncodeUsage;
  class CaptureQueueDelay;

  bool IsOverusing();
  bool IsUnderusing(int64_t time_now);

  bool DetectFrameTimeout(int64_t now) const;

  // Protecting all members.
  scoped_ptr<CriticalSectionWrapper> crit_;

  // Limits on standard deviation for under/overuse.
  const float normaluse_stddev_ms_;
  const float overuse_stddev_ms_;

  int64_t min_process_count_before_reporting_;

  // Observer getting overuse reports.
  CpuOveruseObserver* observer_;

  Clock* clock_;
  int64_t next_process_time_;
  int64_t num_process_times_;

  Statistics capture_deltas_;
  int64_t last_capture_time_;

  int64_t last_overuse_time_;
  int checks_above_threshold_;

  int64_t last_rampup_time_;
  bool in_quick_rampup_;
  int current_rampup_delay_ms_;

  // Number of pixels of last captured frame.
  int num_pixels_;

  int last_capture_jitter_ms_;

  int64_t last_encode_sample_ms_;
  scoped_ptr<EncodeTimeAvg> encode_time_;
  scoped_ptr<EncodeUsage> encode_usage_;

  scoped_ptr<CaptureQueueDelay> capture_queue_delay_;

  DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
