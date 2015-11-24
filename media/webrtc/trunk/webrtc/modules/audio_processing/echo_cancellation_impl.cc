/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/echo_cancellation_impl.h"

#include <assert.h>
#include <string.h>

extern "C" {
#include "webrtc/modules/audio_processing/aec/aec_core.h"
}
#include "webrtc/modules/audio_processing/aec/include/echo_cancellation.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

typedef void Handle;

namespace {
int16_t MapSetting(EchoCancellation::SuppressionLevel level) {
  switch (level) {
    case EchoCancellation::kLowSuppression:
      return kAecNlpConservative;
    case EchoCancellation::kModerateSuppression:
      return kAecNlpModerate;
    case EchoCancellation::kHighSuppression:
      return kAecNlpAggressive;
  }
  assert(false);
  return -1;
}

AudioProcessing::Error MapError(int err) {
  switch (err) {
    case AEC_UNSUPPORTED_FUNCTION_ERROR:
      return AudioProcessing::kUnsupportedFunctionError;
    case AEC_BAD_PARAMETER_ERROR:
      return AudioProcessing::kBadParameterError;
    case AEC_BAD_PARAMETER_WARNING:
      return AudioProcessing::kBadStreamParameterWarning;
    default:
      // AEC_UNSPECIFIED_ERROR
      // AEC_UNINITIALIZED_ERROR
      // AEC_NULL_POINTER_ERROR
      return AudioProcessing::kUnspecifiedError;
  }
}
}  // namespace

EchoCancellationImpl::EchoCancellationImpl(const AudioProcessing* apm,
                                           CriticalSectionWrapper* crit)
  : ProcessingComponent(),
    apm_(apm),
    crit_(crit),
    drift_compensation_enabled_(false),
    metrics_enabled_(false),
    suppression_level_(kModerateSuppression),
    stream_drift_samples_(0),
    was_stream_drift_set_(false),
    stream_has_echo_(false),
    delay_logging_enabled_(false),
    extended_filter_enabled_(false),
    delay_agnostic_enabled_(false) {
}

EchoCancellationImpl::~EchoCancellationImpl() {}

int EchoCancellationImpl::ProcessRenderAudio(const AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_reverse_channels());

  int err = apm_->kNoError;

  // The ordering convention must be followed to pass to the correct AEC.
  size_t handle_index = 0;
  for (int i = 0; i < apm_->num_output_channels(); i++) {
    for (int j = 0; j < audio->num_channels(); j++) {
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      err = WebRtcAec_BufferFarend(
          my_handle,
          audio->split_bands_const_f(j)[kBand0To8kHz],
          audio->num_frames_per_band());

      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);  // TODO(ajm): warning possible?
      }

      handle_index++;
    }
  }

  return apm_->kNoError;
}

int EchoCancellationImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  if (!apm_->was_stream_delay_set()) {
    return apm_->kStreamParameterNotSetError;
  }

  if (drift_compensation_enabled_ && !was_stream_drift_set_) {
    return apm_->kStreamParameterNotSetError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_output_channels());

  int err = apm_->kNoError;

  // The ordering convention must be followed to pass to the correct AEC.
  size_t handle_index = 0;
  stream_has_echo_ = false;
  for (int i = 0; i < audio->num_channels(); i++) {
    for (int j = 0; j < apm_->num_reverse_channels(); j++) {
      Handle* my_handle = handle(handle_index);
      err = WebRtcAec_Process(
          my_handle,
          audio->split_bands_const_f(i),
          audio->num_bands(),
          audio->split_bands_f(i),
          audio->num_frames_per_band(),
          apm_->stream_delay_ms(),
          stream_drift_samples_);

      if (err != apm_->kNoError) {
        err = GetHandleError(my_handle);
        // TODO(ajm): Figure out how to return warnings properly.
        if (err != apm_->kBadStreamParameterWarning) {
          return err;
        }
      }

      int status = 0;
      err = WebRtcAec_get_echo_status(my_handle, &status);
      if (err != apm_->kNoError) {
        return GetHandleError(my_handle);
      }

      if (status == 1) {
        stream_has_echo_ = true;
      }

      handle_index++;
    }
  }

  was_stream_drift_set_ = false;
  return apm_->kNoError;
}

int EchoCancellationImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  // Ensure AEC and AECM are not both enabled.
  if (enable && apm_->echo_control_mobile()->is_enabled()) {
    return apm_->kBadParameterError;
  }

  return EnableComponent(enable);
}

bool EchoCancellationImpl::is_enabled() const {
  return is_component_enabled();
}

int EchoCancellationImpl::set_suppression_level(SuppressionLevel level) {
  CriticalSectionScoped crit_scoped(crit_);
  if (MapSetting(level) == -1) {
    return apm_->kBadParameterError;
  }

  suppression_level_ = level;
  return Configure();
}

EchoCancellation::SuppressionLevel EchoCancellationImpl::suppression_level()
    const {
  return suppression_level_;
}

int EchoCancellationImpl::enable_drift_compensation(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  drift_compensation_enabled_ = enable;
  return Configure();
}

bool EchoCancellationImpl::is_drift_compensation_enabled() const {
  return drift_compensation_enabled_;
}

void EchoCancellationImpl::set_stream_drift_samples(int drift) {
  was_stream_drift_set_ = true;
  stream_drift_samples_ = drift;
}

int EchoCancellationImpl::stream_drift_samples() const {
  return stream_drift_samples_;
}

int EchoCancellationImpl::enable_metrics(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  metrics_enabled_ = enable;
  return Configure();
}

bool EchoCancellationImpl::are_metrics_enabled() const {
  return metrics_enabled_;
}

// TODO(ajm): we currently just use the metrics from the first AEC. Think more
//            aboue the best way to extend this to multi-channel.
int EchoCancellationImpl::GetMetrics(Metrics* metrics) {
  CriticalSectionScoped crit_scoped(crit_);
  if (metrics == NULL) {
    return apm_->kNullPointerError;
  }

  if (!is_component_enabled() || !metrics_enabled_) {
    return apm_->kNotEnabledError;
  }

  AecMetrics my_metrics;
  memset(&my_metrics, 0, sizeof(my_metrics));
  memset(metrics, 0, sizeof(Metrics));

  Handle* my_handle = static_cast<Handle*>(handle(0));
  int err = WebRtcAec_GetMetrics(my_handle, &my_metrics);
  if (err != apm_->kNoError) {
    return GetHandleError(my_handle);
  }

  metrics->residual_echo_return_loss.instant = my_metrics.rerl.instant;
  metrics->residual_echo_return_loss.average = my_metrics.rerl.average;
  metrics->residual_echo_return_loss.maximum = my_metrics.rerl.max;
  metrics->residual_echo_return_loss.minimum = my_metrics.rerl.min;

  metrics->echo_return_loss.instant = my_metrics.erl.instant;
  metrics->echo_return_loss.average = my_metrics.erl.average;
  metrics->echo_return_loss.maximum = my_metrics.erl.max;
  metrics->echo_return_loss.minimum = my_metrics.erl.min;

  metrics->echo_return_loss_enhancement.instant = my_metrics.erle.instant;
  metrics->echo_return_loss_enhancement.average = my_metrics.erle.average;
  metrics->echo_return_loss_enhancement.maximum = my_metrics.erle.max;
  metrics->echo_return_loss_enhancement.minimum = my_metrics.erle.min;

  metrics->a_nlp.instant = my_metrics.aNlp.instant;
  metrics->a_nlp.average = my_metrics.aNlp.average;
  metrics->a_nlp.maximum = my_metrics.aNlp.max;
  metrics->a_nlp.minimum = my_metrics.aNlp.min;

  return apm_->kNoError;
}

bool EchoCancellationImpl::stream_has_echo() const {
  return stream_has_echo_;
}

int EchoCancellationImpl::enable_delay_logging(bool enable) {
  CriticalSectionScoped crit_scoped(crit_);
  delay_logging_enabled_ = enable;
  return Configure();
}

bool EchoCancellationImpl::is_delay_logging_enabled() const {
  return delay_logging_enabled_;
}

bool EchoCancellationImpl::is_delay_agnostic_enabled() const {
  return delay_agnostic_enabled_;
}

bool EchoCancellationImpl::is_extended_filter_enabled() const {
  return extended_filter_enabled_;
}

// TODO(bjornv): How should we handle the multi-channel case?
int EchoCancellationImpl::GetDelayMetrics(int* median, int* std) {
  float fraction_poor_delays = 0;
  return GetDelayMetrics(median, std, &fraction_poor_delays);
}

int EchoCancellationImpl::GetDelayMetrics(int* median, int* std,
                                          float* fraction_poor_delays) {
  CriticalSectionScoped crit_scoped(crit_);
  if (median == NULL) {
    return apm_->kNullPointerError;
  }
  if (std == NULL) {
    return apm_->kNullPointerError;
  }

  if (!is_component_enabled() || !delay_logging_enabled_) {
    return apm_->kNotEnabledError;
  }

  Handle* my_handle = static_cast<Handle*>(handle(0));
  if (WebRtcAec_GetDelayMetrics(my_handle, median, std, fraction_poor_delays) !=
      apm_->kNoError) {
    return GetHandleError(my_handle);
  }

  return apm_->kNoError;
}

struct AecCore* EchoCancellationImpl::aec_core() const {
  CriticalSectionScoped crit_scoped(crit_);
  if (!is_component_enabled()) {
    return NULL;
  }
  Handle* my_handle = static_cast<Handle*>(handle(0));
  return WebRtcAec_aec_core(my_handle);
}

int EchoCancellationImpl::Initialize() {
  int err = ProcessingComponent::Initialize();
  if (err != apm_->kNoError || !is_component_enabled()) {
    return err;
  }

  return apm_->kNoError;
}

void EchoCancellationImpl::SetExtraOptions(const Config& config) {
  extended_filter_enabled_ = config.Get<ExtendedFilter>().enabled;
  delay_agnostic_enabled_ = config.Get<DelayAgnostic>().enabled;
  Configure();
}

void* EchoCancellationImpl::CreateHandle() const {
  return WebRtcAec_Create();
}

void EchoCancellationImpl::DestroyHandle(void* handle) const {
  assert(handle != NULL);
  WebRtcAec_Free(static_cast<Handle*>(handle));
}

int EchoCancellationImpl::InitializeHandle(void* handle) const {
  assert(handle != NULL);
  // TODO(ajm): Drift compensation is disabled in practice. If restored, it
  // should be managed internally and not depend on the hardware sample rate.
  // For now, just hardcode a 48 kHz value.
  return WebRtcAec_Init(static_cast<Handle*>(handle),
                       apm_->proc_sample_rate_hz(),
                       48000);
}

int EchoCancellationImpl::ConfigureHandle(void* handle) const {
  assert(handle != NULL);
  AecConfig config;
  config.metricsMode = metrics_enabled_;
  config.nlpMode = MapSetting(suppression_level_);
  config.skewMode = drift_compensation_enabled_;
  config.delay_logging = delay_logging_enabled_;

  WebRtcAec_enable_extended_filter(
      WebRtcAec_aec_core(static_cast<Handle*>(handle)),
      extended_filter_enabled_ ? 1 : 0);
  WebRtcAec_enable_delay_agnostic(
      WebRtcAec_aec_core(static_cast<Handle*>(handle)),
      delay_agnostic_enabled_ ? 1 : 0);
  return WebRtcAec_set_config(static_cast<Handle*>(handle), config);
}

int EchoCancellationImpl::num_handles_required() const {
  return apm_->num_output_channels() *
         apm_->num_reverse_channels();
}

int EchoCancellationImpl::GetHandleError(void* handle) const {
  assert(handle != NULL);
  return AudioProcessing::kUnspecifiedError;
}
}  // namespace webrtc
