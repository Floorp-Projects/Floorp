/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"

#include <cstdlib>

namespace webrtc {

RTPReceiverStrategy::RTPReceiverStrategy(RtpData* data_callback)
    : data_callback_(data_callback) {
  memset(&last_payload_, 0, sizeof(last_payload_));
}

void RTPReceiverStrategy::GetLastMediaSpecificPayload(
  ModuleRTPUtility::PayloadUnion* payload) const {
  memcpy(payload, &last_payload_, sizeof(*payload));
}

void RTPReceiverStrategy::SetLastMediaSpecificPayload(
  const ModuleRTPUtility::PayloadUnion& payload) {
  memcpy(&last_payload_, &payload, sizeof(last_payload_));
}

}  // namespace webrtc
