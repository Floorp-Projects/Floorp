/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/acm_resampler.h"

#include <string.h>

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

ACMResampler::ACMResampler()
    : resampler_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()) {
}

ACMResampler::~ACMResampler() {
  delete resampler_crit_sect_;
}

int ACMResampler::Resample10Msec(const int16_t* in_audio,
                                 int in_freq_hz,
                                 int out_freq_hz,
                                 int num_audio_channels,
                                 int16_t* out_audio) {
  CriticalSectionScoped cs(resampler_crit_sect_);

  if (in_freq_hz == out_freq_hz) {
    size_t length = static_cast<size_t>(in_freq_hz * num_audio_channels / 100);
    memcpy(out_audio, in_audio, length * sizeof(int16_t));
    return static_cast<int16_t>(in_freq_hz / 100);
  }

  // |maxLen| is maximum number of samples for 10ms at 48kHz.
  int max_len = 480 * num_audio_channels;
  int length_in = (in_freq_hz / 100) * num_audio_channels;
  int out_len;

  ResamplerType type = (num_audio_channels == 1) ? kResamplerSynchronous :
      kResamplerSynchronousStereo;

  if (resampler_.ResetIfNeeded(in_freq_hz, out_freq_hz, type) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Error in reset of resampler");
    return -1;
  }

  if (resampler_.Push(in_audio, length_in, out_audio, max_len, out_len) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Error in resampler: resampler.Push");
    return -1;
  }

  return out_len / num_audio_channels;
}

}  // namespace webrtc
