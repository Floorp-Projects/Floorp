/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/app.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

using webrtc::RTCPUtility::RtcpCommonHeader;

namespace webrtc {
namespace rtcp {

// Application-Defined packet (APP) (RFC 3550).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| subtype |   PT=APP=204  |             length            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                           SSRC/CSRC                           |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                          name (ASCII)                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |                   application-dependent data                ...
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
bool App::Parse(const RtcpCommonHeader& header, const uint8_t* payload) {
  RTC_DCHECK(header.packet_type == kPacketType);

  sub_type_ = header.count_or_format;
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&payload[0]);
  name_ = ByteReader<uint32_t>::ReadBigEndian(&payload[4]);
  data_.SetData(&payload[8], header.payload_size_bytes - 8);
  return true;
}

void App::WithSubType(uint8_t subtype) {
  RTC_DCHECK_LE(subtype, 0x1f);
  sub_type_ = subtype;
}

void App::WithData(const uint8_t* data, size_t data_length) {
  RTC_DCHECK(data);
  RTC_DCHECK_EQ(0u, data_length % 4) << "Data must be 32 bits aligned.";
  RTC_DCHECK(data_length <= kMaxDataSize) << "App data size << " << data_length
                                          << "exceed maximum of "
                                          << kMaxDataSize << " bytes.";
  data_.SetData(data, data_length);
}

bool App::Create(uint8_t* packet,
                 size_t* index,
                 size_t max_length,
                 RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  const size_t index_end = *index + BlockLength();
  CreateHeader(sub_type_, kPacketType, HeaderLength(), packet, index);

  ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 0], ssrc_);
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 4], name_);
  memcpy(&packet[*index + 8], data_.data(), data_.size());
  *index += (8 + data_.size());
  RTC_DCHECK_EQ(index_end, *index);
  return true;
}

}  // namespace rtcp
}  // namespace webrtc
