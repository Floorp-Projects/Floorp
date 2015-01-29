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
  virtual bool is_enabled() const OVERRIDE;
  virtual int stream_drift_samples() const OVERRIDE;

  // ProcessingComponent implementation.
  virtual int Initialize() OVERRIDE;
  virtual void SetExtraOptions(const Config& config) OVERRIDE;

 private:
  // EchoCancellation implementation.
  virtual int Enable(bool enable) OVERRIDE;
  virtual int enable_drift_compensation(bool enable) OVERRIDE;
  virtual bool is_drift_compensation_enabled() const OVERRIDE;
  virtual void set_stream_drift_samples(int drift) OVERRIDE;
  virtual int set_suppression_level(SuppressionLevel level) OVERRIDE;
  virtual SuppressionLevel suppression_level() const OVERRIDE;
  virtual int enable_metrics(bool enable) OVERRIDE;
  virtual bool are_metrics_enabled() const OVERRIDE;
  virtual bool stream_has_echo() const OVERRIDE;
  virtual int GetMetrics(Metrics* metrics) OVERRIDE;
  virtual int enable_delay_logging(bool enable) OVERRIDE;
  virtual bool is_delay_logging_enabled() const OVERRIDE;
  virtual int GetDelayMetrics(int* median, int* std) OVERRIDE;
  virtual struct AecCore* aec_core() const OVERRIDE;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const OVERRIDE;
  virtual int InitializeHandle(void* handle) const OVERRIDE;
  virtual int ConfigureHandle(void* handle) const OVERRIDE;
  virtual void DestroyHandle(void* handle) const OVERRIDE;
  virtual int num_handles_required() const OVERRIDE;
  virtual int GetHandleError(void* handle) const OVERRIDE;

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
