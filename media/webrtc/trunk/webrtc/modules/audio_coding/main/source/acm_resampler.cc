/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_resampler.h"

#include <string.h>

#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

ACMResampler::ACMResampler() {
}

ACMResampler::~ACMResampler() {
}

int16_t ACMResampler::Resample10Msec(const int16_t* in_audio,
                                     int32_t in_freq_hz,
                                     int16_t* out_audio,
                                     int32_t out_freq_hz,
                                     uint8_t num_audio_channels) {
  if (in_freq_hz == out_freq_hz) {
    size_t length = static_cast<size_t>(in_freq_hz * num_audio_channels / 100);
    memcpy(out_audio, in_audio, length * sizeof(int16_t));
    return static_cast<int16_t>(in_freq_hz / 100);
  }

  // |max_length| is the maximum number of samples for 10ms at 48kHz.
  // TODO(turajs): is this actually the capacity of the |out_audio| buffer?
  int max_length = 480 * num_audio_channels;
  int in_length = in_freq_hz / 100 * num_audio_channels;

  if (resampler_.InitializeIfNeeded(in_freq_hz, out_freq_hz,
                                    num_audio_channels) != 0) {
    LOG_FERR3(LS_ERROR, InitializeIfNeeded, in_freq_hz, out_freq_hz,
              num_audio_channels);
    return -1;
  }

  int out_length = resampler_.Resample(in_audio, in_length, out_audio,
                                       max_length);
  if (out_length == -1) {
    LOG_FERR4(LS_ERROR, Resample, in_audio, in_length, out_audio, max_length);
    return -1;
  }

  return out_length / num_audio_channels;
}

}  // namespace webrtc
