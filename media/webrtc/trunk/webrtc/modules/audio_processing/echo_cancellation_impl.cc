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
#include "webrtc/modules/audio_processing/aec/echo_cancellation.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"

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

// Maximum length that a frame of samples can have.
static const size_t kMaxAllowedValuesOfSamplesPerFrame = 160;
// Maximum number of frames to buffer in the render queue.
// TODO(peah): Decrease this once we properly handle hugely unbalanced
// reverse and forward call numbers.
static const size_t kMaxNumFramesToBuffer = 100;
}  // namespace

EchoCancellationImpl::EchoCancellationImpl(const AudioProcessing* apm,
                                           rtc::CriticalSection* crit_render,
                                           rtc::CriticalSection* crit_capture)
    : ProcessingComponent(),
      apm_(apm),
      crit_render_(crit_render),
      crit_capture_(crit_capture),
      drift_compensation_enabled_(false),
      metrics_enabled_(false),
      suppression_level_(kModerateSuppression),
      stream_drift_samples_(0),
      was_stream_drift_set_(false),
      stream_has_echo_(false),
      delay_logging_enabled_(false),
      extended_filter_enabled_(false),
      delay_agnostic_enabled_(false),
      render_queue_element_max_size_(0) {
  RTC_DCHECK(apm);
  RTC_DCHECK(crit_render);
  RTC_DCHECK(crit_capture);
}

EchoCancellationImpl::~EchoCancellationImpl() {}

int EchoCancellationImpl::ProcessRenderAudio(const AudioBuffer* audio) {
  rtc::CritScope cs_render(crit_render_);
  if (!is_component_enabled()) {
    return AudioProcessing::kNoError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_reverse_channels());

  int err = AudioProcessing::kNoError;

  // The ordering convention must be followed to pass to the correct AEC.
  size_t handle_index = 0;
  render_queue_buffer_.clear();
  for (size_t i = 0; i < apm_->num_output_channels(); i++) {
    for (size_t j = 0; j < audio->num_channels(); j++) {
      Handle* my_handle = static_cast<Handle*>(handle(handle_index));
      // Retrieve any error code produced by the buffering of the farend
      // signal
      err = WebRtcAec_GetBufferFarendError(
          my_handle, audio->split_bands_const_f(j)[kBand0To8kHz],
          audio->num_frames_per_band());

      if (err != AudioProcessing::kNoError) {
        return MapError(err);  // TODO(ajm): warning possible?
      }

      // Buffer the samples in the render queue.
      render_queue_buffer_.insert(render_queue_buffer_.end(),
                                  audio->split_bands_const_f(j)[kBand0To8kHz],
                                  (audio->split_bands_const_f(j)[kBand0To8kHz] +
                                   audio->num_frames_per_band()));
    }
  }

  // Insert the samples into the queue.
  if (!render_signal_queue_->Insert(&render_queue_buffer_)) {
    // The data queue is full and needs to be emptied.
    ReadQueuedRenderData();

    // Retry the insert (should always work).
    RTC_DCHECK_EQ(render_signal_queue_->Insert(&render_queue_buffer_), true);
  }

  return AudioProcessing::kNoError;
}

// Read chunks of data that were received and queued on the render side from
// a queue. All the data chunks are buffered into the farend signal of the AEC.
void EchoCancellationImpl::ReadQueuedRenderData() {
  rtc::CritScope cs_capture(crit_capture_);
  if (!is_component_enabled()) {
    return;
  }

  while (render_signal_queue_->Remove(&capture_queue_buffer_)) {
    size_t handle_index = 0;
    size_t buffer_index = 0;
    const size_t num_frames_per_band =
        capture_queue_buffer_.size() /
        (apm_->num_output_channels() * apm_->num_reverse_channels());
    for (size_t i = 0; i < apm_->num_output_channels(); i++) {
      for (size_t j = 0; j < apm_->num_reverse_channels(); j++) {
        Handle* my_handle = static_cast<Handle*>(handle(handle_index));
        WebRtcAec_BufferFarend(my_handle, &capture_queue_buffer_[buffer_index],
                               num_frames_per_band);

        buffer_index += num_frames_per_band;
        handle_index++;
      }
    }
  }
}

int EchoCancellationImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  rtc::CritScope cs_capture(crit_capture_);
  if (!is_component_enabled()) {
    return AudioProcessing::kNoError;
  }

  if (!apm_->was_stream_delay_set()) {
    return AudioProcessing::kStreamParameterNotSetError;
  }

  if (drift_compensation_enabled_ && !was_stream_drift_set_) {
    return AudioProcessing::kStreamParameterNotSetError;
  }

  assert(audio->num_frames_per_band() <= 160);
  assert(audio->num_channels() == apm_->num_proc_channels());

  int err = AudioProcessing::kNoError;

  // The ordering convention must be followed to pass to the correct AEC.
  size_t handle_index = 0;
  stream_has_echo_ = false;
  for (size_t i = 0; i < audio->num_channels(); i++) {
    for (size_t j = 0; j < apm_->num_reverse_channels(); j++) {
      Handle* my_handle = handle(handle_index);
      err = WebRtcAec_Process(my_handle, audio->split_bands_const_f(i),
                              audio->num_bands(), audio->split_bands_f(i),
                              audio->num_frames_per_band(),
                              apm_->stream_delay_ms(), stream_drift_samples_);

      if (err != AudioProcessing::kNoError) {
        err = MapError(err);
        // TODO(ajm): Figure out how to return warnings properly.
        if (err != AudioProcessing::kBadStreamParameterWarning) {
          return err;
        }
      }

      int status = 0;
      err = WebRtcAec_get_echo_status(my_handle, &status);
      if (err != AudioProcessing::kNoError) {
        return MapError(err);
      }

      if (status == 1) {
        stream_has_echo_ = true;
      }

      handle_index++;
    }
  }

  was_stream_drift_set_ = false;
  return AudioProcessing::kNoError;
}

int EchoCancellationImpl::Enable(bool enable) {
  // Run in a single-threaded manner.
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
  // Ensure AEC and AECM are not both enabled.
  // The is_enabled call is safe from a deadlock perspective
  // as both locks are already held in the correct order.
  if (enable && apm_->echo_control_mobile()->is_enabled()) {
    return AudioProcessing::kBadParameterError;
  }

  return EnableComponent(enable);
}

bool EchoCancellationImpl::is_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return is_component_enabled();
}

int EchoCancellationImpl::set_suppression_level(SuppressionLevel level) {
  {
    if (MapSetting(level) == -1) {
      return AudioProcessing::kBadParameterError;
    }
    rtc::CritScope cs(crit_capture_);
    suppression_level_ = level;
  }
  return Configure();
}

EchoCancellation::SuppressionLevel EchoCancellationImpl::suppression_level()
    const {
  rtc::CritScope cs(crit_capture_);
  return suppression_level_;
}

int EchoCancellationImpl::enable_drift_compensation(bool enable) {
  {
    rtc::CritScope cs(crit_capture_);
    drift_compensation_enabled_ = enable;
  }
  return Configure();
}

bool EchoCancellationImpl::is_drift_compensation_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return drift_compensation_enabled_;
}

void EchoCancellationImpl::set_stream_drift_samples(int drift) {
  rtc::CritScope cs(crit_capture_);
  was_stream_drift_set_ = true;
  stream_drift_samples_ = drift;
}

int EchoCancellationImpl::stream_drift_samples() const {
  rtc::CritScope cs(crit_capture_);
  return stream_drift_samples_;
}

int EchoCancellationImpl::enable_metrics(bool enable) {
  {
    rtc::CritScope cs(crit_capture_);
    metrics_enabled_ = enable;
  }
  return Configure();
}

bool EchoCancellationImpl::are_metrics_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return metrics_enabled_;
}

// TODO(ajm): we currently just use the metrics from the first AEC. Think more
//            aboue the best way to extend this to multi-channel.
int EchoCancellationImpl::GetMetrics(Metrics* metrics) {
  rtc::CritScope cs(crit_capture_);
  if (metrics == NULL) {
    return AudioProcessing::kNullPointerError;
  }

  if (!is_component_enabled() || !metrics_enabled_) {
    return AudioProcessing::kNotEnabledError;
  }

  AecMetrics my_metrics;
  memset(&my_metrics, 0, sizeof(my_metrics));
  memset(metrics, 0, sizeof(Metrics));

  Handle* my_handle = static_cast<Handle*>(handle(0));
  int err = WebRtcAec_GetMetrics(my_handle, &my_metrics);
  if (err != AudioProcessing::kNoError) {
    return MapError(err);
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

  return AudioProcessing::kNoError;
}

bool EchoCancellationImpl::stream_has_echo() const {
  rtc::CritScope cs(crit_capture_);
  return stream_has_echo_;
}

int EchoCancellationImpl::enable_delay_logging(bool enable) {
  {
    rtc::CritScope cs(crit_capture_);
    delay_logging_enabled_ = enable;
  }
  return Configure();
}

bool EchoCancellationImpl::is_delay_logging_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return delay_logging_enabled_;
}

bool EchoCancellationImpl::is_delay_agnostic_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return delay_agnostic_enabled_;
}

bool EchoCancellationImpl::is_extended_filter_enabled() const {
  rtc::CritScope cs(crit_capture_);
  return extended_filter_enabled_;
}

// TODO(bjornv): How should we handle the multi-channel case?
int EchoCancellationImpl::GetDelayMetrics(int* median, int* std) {
  rtc::CritScope cs(crit_capture_);
  float fraction_poor_delays = 0;
  return GetDelayMetrics(median, std, &fraction_poor_delays);
}

int EchoCancellationImpl::GetDelayMetrics(int* median, int* std,
                                          float* fraction_poor_delays) {
  rtc::CritScope cs(crit_capture_);
  if (median == NULL) {
    return AudioProcessing::kNullPointerError;
  }
  if (std == NULL) {
    return AudioProcessing::kNullPointerError;
  }

  if (!is_component_enabled() || !delay_logging_enabled_) {
    return AudioProcessing::kNotEnabledError;
  }

  Handle* my_handle = static_cast<Handle*>(handle(0));
  const int err =
      WebRtcAec_GetDelayMetrics(my_handle, median, std, fraction_poor_delays);
  if (err != AudioProcessing::kNoError) {
    return MapError(err);
  }

  return AudioProcessing::kNoError;
}

struct AecCore* EchoCancellationImpl::aec_core() const {
  rtc::CritScope cs(crit_capture_);
  if (!is_component_enabled()) {
    return NULL;
  }
  Handle* my_handle = static_cast<Handle*>(handle(0));
  return WebRtcAec_aec_core(my_handle);
}

int EchoCancellationImpl::Initialize() {
  int err = ProcessingComponent::Initialize();
  {
    rtc::CritScope cs(crit_capture_);
    if (err != AudioProcessing::kNoError || !is_component_enabled()) {
      return err;
    }
  }

  AllocateRenderQueue();

  return AudioProcessing::kNoError;
}

void EchoCancellationImpl::AllocateRenderQueue() {
  const size_t new_render_queue_element_max_size = std::max<size_t>(
      static_cast<size_t>(1),
      kMaxAllowedValuesOfSamplesPerFrame * num_handles_required());

  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);

  // Reallocate the queue if the queue item size is too small to fit the
  // data to put in the queue.
  if (render_queue_element_max_size_ < new_render_queue_element_max_size) {
    render_queue_element_max_size_ = new_render_queue_element_max_size;

    std::vector<float> template_queue_element(render_queue_element_max_size_);

    render_signal_queue_.reset(
        new SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>(
            kMaxNumFramesToBuffer, template_queue_element,
            RenderQueueItemVerifier<float>(render_queue_element_max_size_)));

    render_queue_buffer_.resize(render_queue_element_max_size_);
    capture_queue_buffer_.resize(render_queue_element_max_size_);
  } else {
    render_signal_queue_->Clear();
  }
}

void EchoCancellationImpl::SetExtraOptions(const Config& config) {
  {
    rtc::CritScope cs(crit_capture_);
    extended_filter_enabled_ = config.Get<ExtendedFilter>().enabled;
    delay_agnostic_enabled_ = config.Get<DelayAgnostic>().enabled;
  }
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
  // Not locked as it only relies on APM public API which is threadsafe.

  assert(handle != NULL);
  // TODO(ajm): Drift compensation is disabled in practice. If restored, it
  // should be managed internally and not depend on the hardware sample rate.
  // For now, just hardcode a 48 kHz value.
  return WebRtcAec_Init(static_cast<Handle*>(handle),
                        apm_->proc_sample_rate_hz(), 48000);
}

int EchoCancellationImpl::ConfigureHandle(void* handle) const {
  rtc::CritScope cs_render(crit_render_);
  rtc::CritScope cs_capture(crit_capture_);
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

size_t EchoCancellationImpl::num_handles_required() const {
  // Not locked as it only relies on APM public API which is threadsafe.
  return apm_->num_output_channels() * apm_->num_reverse_channels();
}

int EchoCancellationImpl::GetHandleError(void* handle) const {
  // Not locked as it does not rely on anything in the state.
  assert(handle != NULL);
  return AudioProcessing::kUnspecifiedError;
}
}  // namespace webrtc
