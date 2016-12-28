/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_LOGGING_FILE_HANDLING_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_LOGGING_FILE_HANDLING_

#include <stdio.h>

#include "webrtc/common_audio/wav_file.h"
#include "webrtc/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEBRTC_AEC_DEBUG_DUMP
// Opens a new Wav file for writing. If it was already open with a different
// sample frequency, it closes it first.
void WebRtcAec_ReopenWav(const char* name,
                         int instance_index,
                         int process_rate,
                         int sample_rate,
                         rtc_WavWriter** wav_file);

// Opens dumpfile with instance-specific filename.
void WebRtcAec_RawFileOpen(const char* name, int instance_index, FILE** file);

#endif  // WEBRTC_AEC_DEBUG_DUMP

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_LOGGING_FILE_HANDLING_
