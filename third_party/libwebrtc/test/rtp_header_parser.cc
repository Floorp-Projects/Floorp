/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/rtp_header_parser.h"

#include "modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

absl::optional<uint32_t> RtpHeaderParser::GetSsrc(const uint8_t* packet,
                                                  size_t length) {
  RtpUtility::RtpHeaderParser rtp_parser(packet, length);
  RTPHeader header;
  if (rtp_parser.Parse(&header, nullptr)) {
    return header.ssrc;
  }
  return absl::nullopt;
}

}  // namespace webrtc
