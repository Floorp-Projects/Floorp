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
#include "webrtc/base/exp_filter.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_base.h"

namespace webrtc {

class Clock;
class CpuOveruseObserver;
class CriticalSectionWrapper;

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
  scoped_ptr<rtc::ExpFilter> filtered_samples_;
  scoped_ptr<rtc::ExpFilter> filtered_variance_;
};

// Use to detect system overuse based on jitter in incoming frames.
class OveruseFrameDetector : public Module {
 public:
  explicit OveruseFrameDetector(Clock* clock);
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

  // Accessors.

  // Returns CpuOveruseMetrics where
  // capture_jitter_ms: The estimated jitter based on incoming captured frames.
  // avg_encode_time_ms: Running average of reported encode time
  //                     (FrameEncoded()). Only used for stats.
  // TODO(asapersson): Rename metric.
  // encode_usage_percent: The average processing time of a frame on the
  //                       send-side divided by the average time difference
  //                       between incoming captured frames.
  // capture_queue_delay_ms_per_s: The current time delay between an incoming
  //                               captured frame (FrameCaptured()) until the
  //                               frame is being processed
  //                               (FrameProcessingStarted()). (Note: if a new
  //                               frame is received before an old frame has
  //                               been processed, the old frame is skipped).
  //                               The delay is expressed in ms delay per sec.
  //                               Only used for stats.
  void GetCpuOveruseMetrics(CpuOveruseMetrics* metrics) const;

  // Only public for testing.
  int CaptureQueueDelayMsPerS() const;
  int LastProcessingTimeMs() const;
  int FramesInQueue() const;

  // Implements Module.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

 private:
  class EncodeTimeAvg;
  class SendProcessingUsage;
  class CaptureQueueDelay;
  class FrameQueue;

  void AddProcessingTime(int elapsed_ms);

  bool IsOverusing();
  bool IsUnderusing(int64_t time_now);

  bool FrameTimeoutDetected(int64_t now) const;
  bool FrameSizeChanged(int num_pixels) const;

  void ResetAll(int num_pixels);

  // Protecting all members.
  scoped_ptr<CriticalSectionWrapper> crit_;

  // Observer getting overuse reports.
  CpuOveruseObserver* observer_;

  CpuOveruseOptions options_;

  Clock* clock_;
  int64_t next_process_time_;
  int64_t num_process_times_;

  Statistics capture_deltas_;
  int64_t last_capture_time_;

  int64_t last_overuse_time_;
  int checks_above_threshold_;
  int num_overuse_detections_;

  int64_t last_rampup_time_;
  bool in_quick_rampup_;
  int current_rampup_delay_ms_;

  // Number of pixels of last captured frame.
  int num_pixels_;

  int64_t last_encode_sample_ms_;
  scoped_ptr<EncodeTimeAvg> encode_time_;
  scoped_ptr<SendProcessingUsage> usage_;
  scoped_ptr<FrameQueue> frame_queue_;
  int64_t last_sample_time_ms_;

  scoped_ptr<CaptureQueueDelay> capture_queue_delay_;

  DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_OVERUSE_FRAME_DETECTOR_H_
