/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>  // memcpy

#include "webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

RtpFormatH264::RtpFormatH264(const uint8_t* payload_data,
                             uint32_t payload_size,
                             int max_payload_len)
    : payload_data_(payload_data),
      payload_size_(static_cast<int>(payload_size)),
      max_payload_len_(static_cast<int>(max_payload_len)),
      fragments_(0),
      fragment_size_(0),
      next_fragment_(-1) {
  if (payload_size_ <= max_payload_len_) {
    fragments_ = 0;
  } else {
    fragment_size_ = max_payload_len_ - kH264FUAHeaderLengthInBytes;
    fragments_ = (payload_size_ - kH264NALHeaderLengthInBytes) / fragment_size_;
    if (fragments_ * fragment_size_ !=
        (payload_size_ - kH264NALHeaderLengthInBytes)) {
      ++fragments_;
    }
    next_fragment_ = 0;
  }
}

RtpFormatH264::~RtpFormatH264() {
}

int RtpFormatH264::NextPacket(uint8_t* buffer,
                              int* bytes_to_send,
                              bool* last_packet) {
  if (next_fragment_ == fragments_) {
    *bytes_to_send = 0;
    *last_packet   = true;
    return -1;
  }

  if (payload_size_ <= max_payload_len_) {
    // single NAL_UNIT
    *bytes_to_send = payload_size_;
    *last_packet   = false;
    memcpy(buffer, payload_data_, payload_size_);
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
                 "RtpFormatH264(single NALU with type:%d, payload_size:%d",
                 payload_data_[0] & 0x1F, payload_size_);
    return 0;
  } else {
    unsigned char header = payload_data_[0];
    unsigned char type   = header & 0x1F;
    unsigned char fu_indicator = (header & 0xE0) | kH264FUANALUType;
    unsigned char fu_header = 0;
    bool first_fragment = (next_fragment_ == 0);
    bool last_fragment = (next_fragment_ == (fragments_ -1));

    // S | E | R | 5 bit type.
    fu_header |= (first_fragment ? 128 : 0);  // S bit
    fu_header |= (last_fragment ? 64 :0);  // E bit
    fu_header |= type;
    buffer[0] = fu_indicator;
    buffer[1] = fu_header;

    if (last_fragment) {
      // last fragment
      *bytes_to_send = payload_size_ -
                       kH264NALHeaderLengthInBytes -
                       next_fragment_ * fragment_size_ +
                       kH264FUAHeaderLengthInBytes;
      *last_packet   = true;
      memcpy(buffer + kH264FUAHeaderLengthInBytes,
             payload_data_ + kH264NALHeaderLengthInBytes +
                next_fragment_ * fragment_size_,
             *bytes_to_send - kH264FUAHeaderLengthInBytes);
      // We do not send original NALU header
    } else {
      *bytes_to_send = fragment_size_ + kH264FUAHeaderLengthInBytes;
      *last_packet   = false;
      memcpy(buffer + kH264FUAHeaderLengthInBytes,
             payload_data_ + kH264NALHeaderLengthInBytes +
                 next_fragment_ * fragment_size_,
             fragment_size_);  // We do not send original NALU header
    }
    next_fragment_++;
    return 1;
  }
}

}  // namespace webrtc
