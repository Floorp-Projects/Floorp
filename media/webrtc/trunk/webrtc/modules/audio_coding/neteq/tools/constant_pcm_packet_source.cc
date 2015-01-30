/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/tools/constant_pcm_packet_source.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/pcm16b.h"
#include "webrtc/modules/audio_coding/neteq/tools/packet.h"

namespace webrtc {
namespace test {

ConstantPcmPacketSource::ConstantPcmPacketSource(size_t payload_len_samples,
                                                 int16_t sample_value,
                                                 int sample_rate_hz,
                                                 int payload_type)
    : payload_len_samples_(payload_len_samples),
      packet_len_bytes_(2 * payload_len_samples_ + kHeaderLenBytes),
      samples_per_ms_(sample_rate_hz / 1000),
      next_arrival_time_ms_(0.0),
      payload_type_(payload_type),
      seq_number_(0),
      timestamp_(0),
      payload_ssrc_(0xABCD1234) {
  int encoded_len = WebRtcPcm16b_EncodeW16(&sample_value, 1, &encoded_sample_);
  CHECK_EQ(encoded_len, 2);
}

Packet* ConstantPcmPacketSource::NextPacket() {
  CHECK_GT(packet_len_bytes_, kHeaderLenBytes);
  uint8_t* packet_memory = new uint8_t[packet_len_bytes_];
  // Fill the payload part of the packet memory with the pre-encoded value.
  std::fill_n(reinterpret_cast<int16_t*>(packet_memory + kHeaderLenBytes),
              payload_len_samples_,
              encoded_sample_);
  WriteHeader(packet_memory);
  // |packet| assumes ownership of |packet_memory|.
  Packet* packet =
      new Packet(packet_memory, packet_len_bytes_, next_arrival_time_ms_);
  next_arrival_time_ms_ += payload_len_samples_ / samples_per_ms_;
  return packet;
}

void ConstantPcmPacketSource::WriteHeader(uint8_t* packet_memory) {
  packet_memory[0] = 0x80;
  packet_memory[1] = payload_type_ & 0xFF;
  packet_memory[2] = (seq_number_ >> 8) & 0xFF;
  packet_memory[3] = seq_number_ & 0xFF;
  packet_memory[4] = (timestamp_ >> 24) & 0xFF;
  packet_memory[5] = (timestamp_ >> 16) & 0xFF;
  packet_memory[6] = (timestamp_ >> 8) & 0xFF;
  packet_memory[7] = timestamp_ & 0xFF;
  packet_memory[8] = (payload_ssrc_ >> 24) & 0xFF;
  packet_memory[9] = (payload_ssrc_ >> 16) & 0xFF;
  packet_memory[10] = (payload_ssrc_ >> 8) & 0xFF;
  packet_memory[11] = payload_ssrc_ & 0xFF;
  ++seq_number_;
  timestamp_ += static_cast<uint32_t>(payload_len_samples_);
}

}  // namespace test
}  // namespace webrtc
