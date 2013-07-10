/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_

#include "webrtc/typedefs.h"

namespace webrtc {
namespace RtpFormatVideoGeneric {
  static const uint8_t kKeyFrameBit = 0x01;
  static const uint8_t kFirstPacketBit = 0x02;
}  // namespace RtpFormatVideoGeneric
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
