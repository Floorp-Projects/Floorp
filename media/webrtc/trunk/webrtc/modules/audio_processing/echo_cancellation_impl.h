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
// Use to enable the delay correction feature. This now engages an extended
// filter mode in the AEC, along with robustness measures around the reported
// system delays. It comes with a significant increase in AEC complexity, but is
// much more robust to unreliable reported delays.
//
// Detailed changes to the algorithm:
// - The filter length is changed from 48 to 128 ms. This comes with tuning of
//   several parameters: i) filter adaptation stepsize and error threshold;
//   ii) non-linear processing smoothing and overdrive.
// - Option to ignore the reported delays on platforms which we deem
//   sufficiently unreliable. See WEBRTC_UNTRUSTED_DELAY in echo_cancellation.c.
// - Faster startup times by removing the excessive "startup phase" processing
//   of reported delays.
// - Much more conservative adjustments to the far-end read pointer. We smooth
//   the delay difference more heavily, and back off from the difference more.
//   Adjustments force a readaptation of the filter, so they should be avoided
//   except when really necessary.
struct DelayCorrection {
  DelayCorrection() : enabled(false) {}
  DelayCorrection(bool enabled) : enabled(enabled) {}

  bool enabled;
};

class AudioProcessingImpl;
class AudioBuffer;

class EchoCancellationImpl : public EchoCancellation,
                             public ProcessingComponent {
 public:
  explicit EchoCancellationImpl(const AudioProcessingImpl* apm);
  virtual ~EchoCancellationImpl();

  int ProcessRenderAudio(const AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // EchoCancellation implementation.
  virtual bool is_enabled() const;
  virtual int device_sample_rate_hz() const;
  virtual int stream_drift_samples() const;

  // ProcessingComponent implementation.
  virtual int Initialize();
  //  virtual void SetExtraOptions(const Config& config) OVERRIDE;

 private:
  // EchoCancellation implementation.
  virtual int Enable(bool enable);
  virtual int enable_drift_compensation(bool enable);
  virtual bool is_drift_compensation_enabled() const;
  virtual int set_device_sample_rate_hz(int rate);
  virtual void set_stream_drift_samples(int drift);
  virtual int set_suppression_level(SuppressionLevel level);
  virtual SuppressionLevel suppression_level() const;
  virtual int enable_metrics(bool enable);
  virtual bool are_metrics_enabled() const;
  virtual bool stream_has_echo() const;
  virtual int GetMetrics(Metrics* metrics);
  virtual int enable_delay_logging(bool enable);
  virtual bool is_delay_logging_enabled() const;
  virtual int GetDelayMetrics(int* median, int* std);
  virtual struct AecCore* aec_core() const;

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
  bool drift_compensation_enabled_;
  bool metrics_enabled_;
  SuppressionLevel suppression_level_;
  int device_sample_rate_hz_;
  int stream_drift_samples_;
  bool was_stream_drift_set_;
  bool stream_has_echo_;
  bool delay_logging_enabled_;
  bool delay_correction_enabled_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_ECHO_CANCELLATION_IMPL_H_
