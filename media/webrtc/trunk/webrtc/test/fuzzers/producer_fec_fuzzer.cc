/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  ForwardErrorCorrection fec;
  ProducerFec producer(&fec);
  size_t i = 0;
  if (size < 4)
    return;
  FecProtectionParams params = {data[i++] % 128, data[i++] % 1,
                                static_cast<int>(data[i++] % 10),
                                kFecMaskBursty};
  producer.SetFecParameters(&params, 0);
  uint16_t seq_num = data[i++];

  while (i + 3 < size) {
    size_t rtp_header_length = data[i++] % 10 + 12;
    size_t payload_size = data[i++] % 10;
    if (i + payload_size + rtp_header_length + 2 > size)
      break;
    rtc::scoped_ptr<uint8_t[]> packet(
        new uint8_t[payload_size + rtp_header_length]);
    memcpy(packet.get(), &data[i], payload_size + rtp_header_length);
    ByteWriter<uint16_t>::WriteBigEndian(&packet[2], seq_num++);
    i += payload_size + rtp_header_length;
    // Make sure sequence numbers are increasing.
    const int kRedPayloadType = 98;
    rtc::scoped_ptr<RedPacket> red_packet(producer.BuildRedPacket(
        packet.get(), payload_size, rtp_header_length, kRedPayloadType));
    bool protect = static_cast<bool>(data[i++] % 2);
    if (protect) {
      producer.AddRtpPacketAndGenerateFec(packet.get(), payload_size,
                                          rtp_header_length);
    }
    uint16_t num_fec_packets = producer.NumAvailableFecPackets();
    std::vector<RedPacket*> fec_packets;
    if (num_fec_packets > 0) {
      fec_packets =
          producer.GetFecPackets(kRedPayloadType, 99, 100, rtp_header_length);
      RTC_CHECK_EQ(num_fec_packets, fec_packets.size());
    }
    for (RedPacket* fec_packet : fec_packets) {
      delete fec_packet;
    }
  }
}
}  // namespace webrtc
