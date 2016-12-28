/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fuzzers/audio_decoder_fuzzer.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"

namespace webrtc {
namespace {
size_t PacketSizeFromTwoBytes(const uint8_t* data, size_t size) {
  if (size < 2)
    return 0;
  return static_cast<size_t>((data[0] << 8) + data[1]);
}
}  // namespace

// This function reads two bytes from the beginning of |data|, interprets them
// as the first packet length, and reads this many bytes if available. The
// payload is inserted into the decoder, and the process continues until no more
// data is available.
void FuzzAudioDecoder(const uint8_t* data,
                      size_t size,
                      AudioDecoder* decoder,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded) {
  const uint8_t* data_ptr = data;
  size_t remaining_size = size;
  size_t packet_len = PacketSizeFromTwoBytes(data_ptr, remaining_size);
  while (packet_len != 0 && packet_len <= remaining_size - 2) {
    data_ptr += 2;
    remaining_size -= 2;
    AudioDecoder::SpeechType speech_type;
    decoder->Decode(data_ptr, packet_len, sample_rate_hz, max_decoded_bytes,
                    decoded, &speech_type);
    data_ptr += packet_len;
    remaining_size -= packet_len;
    packet_len = PacketSizeFromTwoBytes(data_ptr, remaining_size);
  }
}
}  // namespace webrtc
