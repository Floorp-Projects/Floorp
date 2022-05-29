/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "test/rtp_header_parser.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  RtpHeaderParser::IsRtcp(data, size);
  RtpHeaderParser::GetSsrc(data, size);
}

}  // namespace webrtc
