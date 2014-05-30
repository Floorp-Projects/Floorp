/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/opensles_common.h"

#include <assert.h>

#include "webrtc/modules/audio_device/android/audio_common.h"

using webrtc::kNumChannels;

namespace webrtc_opensl {

SLDataFormat_PCM CreatePcmConfiguration(int sample_rate) {
  SLDataFormat_PCM configuration;
  configuration.formatType = SL_DATAFORMAT_PCM;
  configuration.numChannels = kNumChannels;
  // According to the opensles documentation in the ndk:
  // samplesPerSec is actually in units of milliHz, despite the misleading name.
  // It further recommends using constants. However, this would lead to a lot
  // of boilerplate code so it is not done here.
  configuration.samplesPerSec = sample_rate * 1000;
  configuration.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  configuration.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  configuration.channelMask = SL_SPEAKER_FRONT_CENTER;
  if (2 == configuration.numChannels) {
    configuration.channelMask =
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  }
  configuration.endianness = SL_BYTEORDER_LITTLEENDIAN;
  return configuration;
}

}  // namespace webrtc_opensl
