/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_MOCK_MOCK_BITRATE_CONTROLLER_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_MOCK_MOCK_BITRATE_CONTROLLER_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {
namespace test {

class MockBitrateObserver : public BitrateObserver {
 public:
  MOCK_METHOD3(OnNetworkChanged,
               void(uint32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int64_t rtt_ms));
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_MOCK_MOCK_BITRATE_CONTROLLER_H_
