/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "high_pass_filter_impl.h"

#include <cassert>

#include "critical_section_wrapper.h"
#include "typedefs.h"
#include "signal_processing_library.h"

#include "audio_processing_impl.h"
#include "audio_buffer.h"

namespace webrtc {
namespace {
const int16_t kFilterCoefficients8kHz[5] =
    {3798, -7596, 3798, 7807, -3733};

const int16_t kFilterCoefficients[5] =
    {4012, -8024, 4012, 8002, -3913};

struct FilterState {
  int16_t y[4];
  int16_t x[2];
  const int16_t* ba;
};

int InitializeFilter(FilterState* hpf, int sample_rate_hz) {
  assert(hpf != NULL);

  if (sample_rate_hz == AudioProcessingImpl::kSampleRate8kHz) {
    hpf->ba = kFilterCoefficients8kHz;
  } else {
    hpf->ba = kFilterCoefficients;
  }

  WebRtcSpl_MemSetW16(hpf->x, 0, 2);
  WebRtcSpl_MemSetW16(hpf->y, 0, 4);

  return AudioProcessing::kNoError;
}

int Filter(FilterState* hpf, int16_t* data, int length) {
  assert(hpf != NULL);

  int32_t tmp_int32 = 0;
  int16_t* y = hpf->y;
  int16_t* x = hpf->x;
  const int16_t* ba = hpf->ba;

  for (int i = 0; i < length; i++) {
    //  y[i] = b[0] * x[i] + b[1] * x[i-1] + b[2] * x[i-2]
    //         + -a[1] * y[i-1] + -a[2] * y[i-2];

    tmp_int32 =
        WEBRTC_SPL_MUL_16_16(y[1], ba[3]); // -a[1] * y[i-1] (low part)
    tmp_int32 +=
        WEBRTC_SPL_MUL_16_16(y[3], ba[4]); // -a[2] * y[i-2] (low part)
    tmp_int32 = (tmp_int32 >> 15);
    tmp_int32 +=
        WEBRTC_SPL_MUL_16_16(y[0], ba[3]); // -a[1] * y[i-1] (high part)
    tmp_int32 +=
        WEBRTC_SPL_MUL_16_16(y[2], ba[4]); // -a[2] * y[i-2] (high part)
    tmp_int32 = (tmp_int32 << 1);

    tmp_int32 += WEBRTC_SPL_MUL_16_16(data[i], ba[0]); // b[0]*x[0]
    tmp_int32 += WEBRTC_SPL_MUL_16_16(x[0], ba[1]);    // b[1]*x[i-1]
    tmp_int32 += WEBRTC_SPL_MUL_16_16(x[1], ba[2]);    // b[2]*x[i-2]

    // Update state (input part)
    x[1] = x[0];
    x[0] = data[i];

    // Update state (filtered part)
    y[2] = y[0];
    y[3] = y[1];
    y[0] = static_cast<int16_t>(tmp_int32 >> 13);
    y[1] = static_cast<int16_t>((tmp_int32 -
        WEBRTC_SPL_LSHIFT_W32(static_cast<int32_t>(y[0]), 13)) << 2);

    // Rounding in Q12, i.e. add 2^11
    tmp_int32 += 2048;

    // Saturate (to 2^27) so that the HP filtered signal does not overflow
    tmp_int32 = WEBRTC_SPL_SAT(static_cast<int32_t>(134217727),
                               tmp_int32,
                               static_cast<int32_t>(-134217728));

    // Convert back to Q0 and use rounding
    data[i] = (int16_t)WEBRTC_SPL_RSHIFT_W32(tmp_int32, 12);

  }

  return AudioProcessing::kNoError;
}
}  // namespace

typedef FilterState Handle;

HighPassFilterImpl::HighPassFilterImpl(const AudioProcessingImpl* apm)
  : ProcessingComponent(apm),
    apm_(apm) {}

HighPassFilterImpl::~HighPassFilterImpl() {}

int HighPassFilterImpl::ProcessCaptureAudio(AudioBuffer* audio) {
  int err = apm_->kNoError;

  if (!is_component_enabled()) {
    return apm_->kNoError;
  }

  assert(audio->samples_per_split_channel() <= 160);

  for (int i = 0; i < num_handles(); i++) {
    Handle* my_handle = static_cast<Handle*>(handle(i));
    err = Filter(my_handle,
                 audio->low_pass_split_data(i),
                 audio->samples_per_split_channel());

    if (err != apm_->kNoError) {
      return GetHandleError(my_handle);
    }
  }

  return apm_->kNoError;
}

int HighPassFilterImpl::Enable(bool enable) {
  CriticalSectionScoped crit_scoped(apm_->crit());
  return EnableComponent(enable);
}

bool HighPassFilterImpl::is_enabled() const {
  return is_component_enabled();
}

void* HighPassFilterImpl::CreateHandle() const {
  return new FilterState;
}

int HighPassFilterImpl::DestroyHandle(void* handle) const {
  delete static_cast<Handle*>(handle);
  return apm_->kNoError;
}

int HighPassFilterImpl::InitializeHandle(void* handle) const {
  return InitializeFilter(static_cast<Handle*>(handle),
                          apm_->sample_rate_hz());
}

int HighPassFilterImpl::ConfigureHandle(void* /*handle*/) const {
  return apm_->kNoError; // Not configurable.
}

int HighPassFilterImpl::num_handles_required() const {
  return apm_->num_output_channels();
}

int HighPassFilterImpl::GetHandleError(void* handle) const {
  // The component has no detailed errors.
  assert(handle != NULL);
  return apm_->kUnspecifiedError;
}
}  // namespace webrtc
