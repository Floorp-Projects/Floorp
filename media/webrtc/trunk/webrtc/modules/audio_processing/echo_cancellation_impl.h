/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/swap_queue.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {

class AudioBuffer;

class EchoCancellationImpl : public EchoCancellation,
                             public ProcessingComponent {
 public:
  EchoCancellationImpl(const AudioProcessing* apm,
                       rtc::CriticalSection* crit_render,
                       rtc::CriticalSection* crit_capture);
  virtual ~EchoCancellationImpl();

  int ProcessRenderAudio(const AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // EchoCancellation implementation.
  bool is_enabled() const override;
  int stream_drift_samples() const override;
  SuppressionLevel suppression_level() const override;
  bool is_drift_compensation_enabled() const override;

  // ProcessingComponent implementation.
  int Initialize() override;
  void SetExtraOptions(const Config& config) override;
  bool is_delay_agnostic_enabled() const;
  bool is_extended_filter_enabled() const;

  // Reads render side data that has been queued on the render call.
  // Called holding the capture lock.
  void ReadQueuedRenderData();

 private:
  // EchoCancellation implementation.
  int Enable(bool enable) override;
  int enable_drift_compensation(bool enable) override;
  void set_stream_drift_samples(int drift) override;
  int set_suppression_level(SuppressionLevel level) override;
  int enable_metrics(bool enable) override;
  bool are_metrics_enabled() const override;
  bool stream_has_echo() const override;
  int GetMetrics(Metrics* metrics) override;
  int enable_delay_logging(bool enable) override;
  bool is_delay_logging_enabled() const override;
  int GetDelayMetrics(int* median, int* std) override;
  int GetDelayMetrics(int* median,
                      int* std,
                      float* fraction_poor_delays) override;

  struct AecCore* aec_core() const override;

  // ProcessingComponent implementation.
  void* CreateHandle() const override;
  int InitializeHandle(void* handle) const override;
  int ConfigureHandle(void* handle) const override;
  void DestroyHandle(void* handle) const override;
  size_t num_handles_required() const override;
  int GetHandleError(void* handle) const override;

  void AllocateRenderQueue();

  // Not guarded as its public API is thread safe.
  const AudioProcessing* apm_;

  rtc::CriticalSection* const crit_render_ ACQUIRED_BEFORE(crit_capture_);
  rtc::CriticalSection* const crit_capture_;

  bool drift_compensation_enabled_ GUARDED_BY(crit_capture_);
  bool metrics_enabled_ GUARDED_BY(crit_capture_);
  SuppressionLevel suppression_level_ GUARDED_BY(crit_capture_);
  int stream_drift_samples_ GUARDED_BY(crit_capture_);
  bool was_stream_drift_set_ GUARDED_BY(crit_capture_);
  bool stream_has_echo_ GUARDED_BY(crit_capture_);
  bool delay_logging_enabled_ GUARDED_BY(crit_capture_);
  bool extended_filter_enabled_ GUARDED_BY(crit_capture_);
  bool delay_agnostic_enabled_ GUARDED_BY(crit_capture_);

  size_t render_queue_element_max_size_ GUARDED_BY(crit_render_)
      GUARDED_BY(crit_capture_);
  std::vector<float> render_queue_buffer_ GUARDED_BY(crit_render_);
  std::vector<float> capture_queue_buffer_ GUARDED_BY(crit_capture_);

  // Lock protection not needed.
  rtc::scoped_ptr<SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>>
      render_signal_queue_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_
