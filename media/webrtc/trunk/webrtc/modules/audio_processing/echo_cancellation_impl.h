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

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/processing_component.h"

namespace webrtc {

class AudioBuffer;
class CriticalSectionWrapper;

class EchoCancellationImpl : public EchoCancellation,
                             public ProcessingComponent {
 public:
  EchoCancellationImpl(const AudioProcessing* apm,
                       CriticalSectionWrapper* crit);
  virtual ~EchoCancellationImpl();

  int ProcessRenderAudio(const AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // EchoCancellation implementation.
  bool is_enabled() const override;
  int stream_drift_samples() const override;

  // ProcessingComponent implementation.
  int Initialize() override;
  void SetExtraOptions(const Config& config) override;

 private:
  // EchoCancellation implementation.
  int Enable(bool enable) override;
  int enable_drift_compensation(bool enable) override;
  bool is_drift_compensation_enabled() const override;
  void set_stream_drift_samples(int drift) override;
  int set_suppression_level(SuppressionLevel level) override;
  SuppressionLevel suppression_level() const override;
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
  int num_handles_required() const override;
  int GetHandleError(void* handle) const override;

  const AudioProcessing* apm_;
  CriticalSectionWrapper* crit_;
  bool drift_compensation_enabled_;
  bool metrics_enabled_;
  SuppressionLevel suppression_level_;
  int stream_drift_samples_;
  bool was_stream_drift_set_;
  bool stream_has_echo_;
  bool delay_logging_enabled_;
  bool delay_correction_enabled_;
  bool reported_delay_enabled_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_
