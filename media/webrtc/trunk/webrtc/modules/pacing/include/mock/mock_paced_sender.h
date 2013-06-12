/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_INCLUDE_MOCK_MOCK_PACED_SENDER_H_
#define WEBRTC_MODULES_PACING_INCLUDE_MOCK_MOCK_PACED_SENDER_H_

#include <gmock/gmock.h>

#include <vector>

#include "webrtc/modules/pacing/include/paced_sender.h"

namespace webrtc {

class MockPacedSender : public PacedSender {
 public:
  MockPacedSender() : PacedSender(NULL, 0, 0) {}
  MOCK_METHOD5(SendPacket, bool(Priority priority,
                                uint32_t ssrc,
                                uint16_t sequence_number,
                                int64_t capture_time_ms,
                                int bytes));
  MOCK_CONST_METHOD0(QueueInMs, int());
  MOCK_CONST_METHOD0(QueueInPackets, int());
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_PACING_INCLUDE_MOCK_MOCK_PACED_SENDER_H_
