/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/vad/include/vad.h"

#include "webrtc/base/checks.h"

namespace webrtc {

Vad::Vad(enum Aggressiveness mode) {
  CHECK_EQ(WebRtcVad_Create(&handle_), 0);
  CHECK_EQ(WebRtcVad_Init(handle_), 0);
  CHECK_EQ(WebRtcVad_set_mode(handle_, mode), 0);
}

Vad::~Vad() {
  WebRtcVad_Free(handle_);
}

enum Vad::Activity Vad::VoiceActivity(const int16_t* audio,
                                      size_t num_samples,
                                      int sample_rate_hz) {
  int ret = WebRtcVad_Process(
      handle_, sample_rate_hz, audio, static_cast<int>(num_samples));
  switch (ret) {
    case 0:
      return kPassive;
    case 1:
      return kActive;
    default:
      DCHECK(false) << "WebRtcVad_Process returned an error.";
      return kError;
  }
}

}  // namespace webrtc
