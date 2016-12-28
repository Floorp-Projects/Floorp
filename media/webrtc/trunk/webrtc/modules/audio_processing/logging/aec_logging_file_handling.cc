/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/logging/aec_logging_file_handling.h"

#include <stdint.h>
#include <stdio.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/typedefs.h"

#ifdef WEBRTC_AEC_DEBUG_DUMP
void WebRtcAec_ReopenWav(const char* name,
                         int instance_index,
                         int process_rate,
                         int sample_rate,
                         rtc_WavWriter** wav_file) {
  if (*wav_file) {
    if (rtc_WavSampleRate(*wav_file) == sample_rate)
      return;
    rtc_WavClose(*wav_file);
  }
  char filename[64];
  int written = rtc::sprintfn(filename, sizeof(filename), "%s%d-%d.wav", name,
                              instance_index, process_rate);

  // Ensure there was no buffer output error.
  RTC_DCHECK_GE(written, 0);
  // Ensure that the buffer size was sufficient.
  RTC_DCHECK_LT(static_cast<size_t>(written), sizeof(filename));

  *wav_file = rtc_WavOpen(filename, sample_rate, 1);
}

void WebRtcAec_RawFileOpen(const char* name, int instance_index, FILE** file) {
  char filename[64];
  int written = rtc::sprintfn(filename, sizeof(filename), "%s_%d.dat", name,
                              instance_index);

  // Ensure there was no buffer output error.
  RTC_DCHECK_GE(written, 0);
  // Ensure that the buffer size was sufficient.
  RTC_DCHECK_LT(static_cast<size_t>(written), sizeof(filename));

  *file = fopen(filename, "wb");
}

#endif  // WEBRTC_AEC_DEBUG_DUMP
