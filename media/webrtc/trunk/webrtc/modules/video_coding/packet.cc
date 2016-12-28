/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/packet.h"

#include <assert.h>

#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {

VCMPacket::VCMPacket()
    : payloadType(0),
      timestamp(0),
      ntp_time_ms_(0),
      seqNum(0),
      dataPtr(NULL),
      sizeBytes(0),
      markerBit(false),
      frameType(kEmptyFrame),
      codec(kVideoCodecUnknown),
      isFirstPacket(false),
      completeNALU(kNaluUnset),
      insertStartCode(false),
      width(0),
      height(0),
      codecSpecificHeader() {}

VCMPacket::VCMPacket(const uint8_t* ptr,
                     const size_t size,
                     const WebRtcRTPHeader& rtpHeader)
    : payloadType(rtpHeader.header.payloadType),
      timestamp(rtpHeader.header.timestamp),
      ntp_time_ms_(rtpHeader.ntp_time_ms),
      seqNum(rtpHeader.header.sequenceNumber),
      dataPtr(ptr),
      sizeBytes(size),
      markerBit(rtpHeader.header.markerBit),

      frameType(rtpHeader.frameType),
      codec(kVideoCodecUnknown),
      isFirstPacket(rtpHeader.type.Video.isFirstPacket),
      completeNALU(kNaluComplete),
      insertStartCode(false),
      width(rtpHeader.type.Video.width),
      height(rtpHeader.type.Video.height),
      codecSpecificHeader(rtpHeader.type.Video) {
  CopyCodecSpecifics(rtpHeader.type.Video);
}

VCMPacket::VCMPacket(const uint8_t* ptr,
                     size_t size,
                     uint16_t seq,
                     uint32_t ts,
                     bool mBit)
    : payloadType(0),
      timestamp(ts),
      ntp_time_ms_(0),
      seqNum(seq),
      dataPtr(ptr),
      sizeBytes(size),
      markerBit(mBit),

      frameType(kVideoFrameDelta),
      codec(kVideoCodecUnknown),
      isFirstPacket(false),
      completeNALU(kNaluComplete),
      insertStartCode(false),
      width(0),
      height(0),
      codecSpecificHeader() {}

void VCMPacket::Reset() {
  payloadType = 0;
  timestamp = 0;
  ntp_time_ms_ = 0;
  seqNum = 0;
  dataPtr = NULL;
  sizeBytes = 0;
  markerBit = false;
  frameType = kEmptyFrame;
  codec = kVideoCodecUnknown;
  isFirstPacket = false;
  completeNALU = kNaluUnset;
  insertStartCode = false;
  width = 0;
  height = 0;
  memset(&codecSpecificHeader, 0, sizeof(RTPVideoHeader));
}

void VCMPacket::CopyCodecSpecifics(const RTPVideoHeader& videoHeader) {
  if (markerBit) {
    codecSpecificHeader.rotation = videoHeader.rotation;
  }
  switch (videoHeader.codec) {
    case kRtpVideoVp8:
      // Handle all packets within a frame as depending on the previous packet
      // TODO(holmer): This should be changed to make fragments independent
      // when the VP8 RTP receiver supports fragments.
      if (isFirstPacket && markerBit)
        completeNALU = kNaluComplete;
      else if (isFirstPacket)
        completeNALU = kNaluStart;
      else if (markerBit)
        completeNALU = kNaluEnd;
      else
        completeNALU = kNaluIncomplete;

      codec = kVideoCodecVP8;
      return;
    case kRtpVideoVp9:
      if (isFirstPacket && markerBit)
        completeNALU = kNaluComplete;
      else if (isFirstPacket)
        completeNALU = kNaluStart;
      else if (markerBit)
        completeNALU = kNaluEnd;
      else
        completeNALU = kNaluIncomplete;

      codec = kVideoCodecVP9;
      return;
    case kRtpVideoH264:
      isFirstPacket = videoHeader.isFirstPacket;
      if (isFirstPacket)
        insertStartCode = true;

      if (isFirstPacket && markerBit) {
        completeNALU = kNaluComplete;
      } else if (isFirstPacket) {
        completeNALU = kNaluStart;
      } else if (markerBit) {
        completeNALU = kNaluEnd;
      } else {
        completeNALU = kNaluIncomplete;
      }
      codec = kVideoCodecH264;
      return;
    case kRtpVideoGeneric:
    case kRtpVideoNone:
      codec = kVideoCodecUnknown;
      return;
  }
}

}  // namespace webrtc
