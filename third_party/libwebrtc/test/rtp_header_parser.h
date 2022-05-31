/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RTP_HEADER_PARSER_H_
#define TEST_RTP_HEADER_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"

namespace webrtc {

class RtpHeaderParser {
 public:
  static absl::optional<uint32_t> GetSsrc(const uint8_t* packet, size_t length);
};
}  // namespace webrtc
#endif  // TEST_RTP_HEADER_PARSER_H_
