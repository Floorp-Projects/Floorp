/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/interface/module_common_types.h"

static const int kChunkSizeMs = 10;
static const webrtc::AudioProcessing::Error kNoErr =
    webrtc::AudioProcessing::kNoError;

static void SetFrameSampleRate(webrtc::AudioFrame* frame, int sample_rate_hz) {
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = kChunkSizeMs * sample_rate_hz / 1000;
}
