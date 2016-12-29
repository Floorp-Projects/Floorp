/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_FUZZERS_AUDIO_DECODER_FUZZER_H_
#define WEBRTC_TEST_FUZZERS_AUDIO_DECODER_FUZZER_H_

#include <stddef.h>

#include "webrtc/typedefs.h"

namespace webrtc {

class AudioDecoder;

void FuzzAudioDecoder(const uint8_t* data,
                      size_t size,
                      AudioDecoder* decoder,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded);

}  // namespace webrtc

#endif  // WEBRTC_TEST_FUZZERS_AUDIO_DECODER_FUZZER_H_
