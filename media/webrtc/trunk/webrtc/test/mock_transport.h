/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_MOCK_TRANSPORT_H_
#define WEBRTC_TEST_MOCK_TRANSPORT_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/transport.h"

namespace webrtc {

class MockTransport : public webrtc::Transport {
 public:
  MOCK_METHOD3(SendPacket,
      int(int channel, const void* data, int len));
  MOCK_METHOD3(SendRTCPPacket,
      int(int channel, const void* data, int len));
};
}  // namespace webrtc
#endif  // WEBRTC_TEST_MOCK_TRANSPORT_H_
