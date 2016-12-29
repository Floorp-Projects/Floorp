/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmbr.h"

#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

using webrtc::RTCPUtility::PT_RTPFB;
using webrtc::RTCPUtility::RTCPPacketRTPFBTMMBR;
using webrtc::RTCPUtility::RTCPPacketRTPFBTMMBRItem;

namespace webrtc {
namespace rtcp {
namespace {
const uint32_t kUnusedMediaSourceSsrc0 = 0;

void AssignUWord8(uint8_t* buffer, size_t* offset, uint8_t value) {
  buffer[(*offset)++] = value;
}

void AssignUWord32(uint8_t* buffer, size_t* offset, uint32_t value) {
  ByteWriter<uint32_t>::WriteBigEndian(buffer + *offset, value);
  *offset += 4;
}

void ComputeMantissaAnd6bitBase2Exponent(uint32_t input_base10,
                                         uint8_t bits_mantissa,
                                         uint32_t* mantissa,
                                         uint8_t* exp) {
  // input_base10 = mantissa * 2^exp
  assert(bits_mantissa <= 32);
  uint32_t mantissa_max = (1 << bits_mantissa) - 1;
  uint8_t exponent = 0;
  for (uint32_t i = 0; i < 64; ++i) {
    if (input_base10 <= (mantissa_max << i)) {
      exponent = i;
      break;
    }
  }
  *exp = exponent;
  *mantissa = (input_base10 >> exponent);
}

void CreateTmmbrItem(const RTCPPacketRTPFBTMMBRItem& tmmbr_item,
                     uint8_t* buffer,
                     size_t* pos) {
  uint32_t bitrate_bps = tmmbr_item.MaxTotalMediaBitRate * 1000;
  uint32_t mantissa = 0;
  uint8_t exp = 0;
  ComputeMantissaAnd6bitBase2Exponent(bitrate_bps, 17, &mantissa, &exp);

  AssignUWord32(buffer, pos, tmmbr_item.SSRC);
  AssignUWord8(buffer, pos, (exp << 2) + ((mantissa >> 15) & 0x03));
  AssignUWord8(buffer, pos, mantissa >> 7);
  AssignUWord8(buffer, pos, (mantissa << 1) +
                            ((tmmbr_item.MeasuredOverhead >> 8) & 0x01));
  AssignUWord8(buffer, pos, tmmbr_item.MeasuredOverhead);
}

// Temporary Maximum Media Stream Bit Rate Request (TMMBR) (RFC 5104).
//
// FCI:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void CreateTmmbr(const RTCPPacketRTPFBTMMBR& tmmbr,
                 const RTCPPacketRTPFBTMMBRItem& tmmbr_item,
                 uint8_t* buffer,
                 size_t* pos) {
  AssignUWord32(buffer, pos, tmmbr.SenderSSRC);
  AssignUWord32(buffer, pos, kUnusedMediaSourceSsrc0);
  CreateTmmbrItem(tmmbr_item, buffer, pos);
}
}  // namespace

bool Tmmbr::Create(uint8_t* packet,
                   size_t* index,
                   size_t max_length,
                   RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  const uint8_t kFmt = 3;
  CreateHeader(kFmt, PT_RTPFB, HeaderLength(), packet, index);
  CreateTmmbr(tmmbr_, tmmbr_item_, packet, index);
  return true;
}

}  // namespace rtcp
}  // namespace webrtc
