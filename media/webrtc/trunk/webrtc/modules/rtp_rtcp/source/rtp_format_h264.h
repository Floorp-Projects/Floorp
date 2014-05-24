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
  enum {
    kH264NALU_SLICE             = 1,
    kH264NALU_IDR               = 5,
    kH264NALU_SEI               = 6,
    kH264NALU_SPS               = 7,
    kH264NALU_PPS               = 8,
    kH264NALU_STAPA             = 24,
    kH264NALU_FUA               = 28
  };

  static const int kH264NALHeaderLengthInBytes = 1;
  static const int kH264FUAHeaderLengthInBytes = 2;

// bits for FU (A and B) indicators
  enum H264NalDefs {
    kH264NAL_FBit = 0x80,
    kH264NAL_NRIMask = 0x60,
    kH264NAL_TypeMask = 0x1F
  };

  enum H264FUDefs {
    // bits for FU (A and B) headers
    kH264FU_SBit = 0x80,
    kH264FU_EBit = 0x40,
    kH264FU_RBit = 0x20
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
