 /*
  *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
  *
  *  Use of this source code is governed by a BSD-style license
  *  that can be found in the LICENSE file in the root of the source
  *  tree. An additional intellectual property rights grant can be found
  *  in the file PATENTS.  All contributing project authors may
  *  be found in the AUTHORS file in the root of the source tree.
  */

 /*
  * This file contains the declaration of the H264 packetizer class.
  * A packetizer object is created for each encoded video frame. The
  * constructor is called with the payload data and size,
  * together with the fragmentation information and a packetizer mode
  * of choice. Alternatively, if no fragmentation info is available, the
  * second constructor can be used with only payload data and size; in that
  * case the mode kEqualSize is used.
  *
  * After creating the packetizer, the method NextPacket is called
  * repeatedly to get all packets for the frame. The method returns
  * false as long as there are more packets left to fetch.
  */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Packetizer for H264.
class RtpFormatH264 {
 public:

  // This supports H.264 RTP packetization modes 0/1 from RFC 6184
  // Single NALU: NAL Header (1 byte), Data...
  // FU-A   NALU: NAL Header, FU Header (1 byte), Data...
  // STAP-A NALU: NAL Header, Length1 (2 bytes), Data1, Length2, Data2...

  enum NalHeader { // Network Abstraction Layer Unit Header
    kNalHeaderOffset = 0, // start of every RTP payload
    kNalHeaderSize = 1, // 1 byte:
    kTypeMask = 0x1f, // bits 0-4: NAL Type
    kNriMask = 0x60, // bits 5-6: Non-Ref Indicator
    kFBit = 0x80, // bit 7: Forbidden (always 0)
  };

  enum NalType { // 0-23 from H.264, 24-31 from RFC 6184
    kIpb = 1, // I/P/B slice
    kIdr = 5, // IDR slice
    kSei = 6, // Supplementary Enhancement Info
    kSeiRecPt = 6, // Recovery Point SEI Payload
    kSps = 7, // Sequence Parameter Set
    kPps = 8, // Picture Parameter Set
    kPrefix = 14, // Prefix
    kStapA = 24, // Single-Time Aggregation Packet Type A
    kFuA = 28, // Fragmentation Unit Type A
  };

  enum FuAHeader {
    kFuAHeaderOffset = 1, // follows NAL Header
    kFuAHeaderSize = 1, // 1 byte: bits 0-4: Original NAL Type
    kFragStartBit = 0x80, // bit 7: Start of Fragment
    kFragEndBit = 0x40, // bit 6: End of Fragment
    kReservedBit = 0x20 // bit 5: Reserved
  };
  enum StapAHeader {
    kStapAHeaderOffset = 1, // follows NAL Header
    kAggUnitLengthSize = 2 // 2-byte length of next NALU including NAL header
  };
  enum StartCodePrefix { // H.264 Annex B format {0,0,0,1}
    kStartCodeSize = 4 // 4 byte prefix before each NAL header
  };

  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded H264 frame.
  RtpFormatH264(const uint8_t* payload_data,
                uint32_t payload_size,
                int max_payload_len);

  ~RtpFormatH264();

  // Get the next payload with H264 payload header.
  // max_payload_len limits the sum length of payload and H264 payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. Parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns 0 on success for single NAL_UNIT
  // Returns 1 on success for fragmentation
  // return -1 on error.
  int NextPacket(uint8_t* buffer,
                 int* bytes_to_send,
                 bool* last_packet);

 private:
  const uint8_t* payload_data_;
  const int payload_size_;
  const int max_payload_len_;
  int   fragments_;
  int   fragment_size_;
  int   next_fragment_;

  DISALLOW_COPY_AND_ASSIGN(RtpFormatH264);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
