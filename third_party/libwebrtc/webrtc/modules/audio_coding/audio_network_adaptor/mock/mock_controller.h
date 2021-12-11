/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_CONTROLLER_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_CONTROLLER_H_

#include "modules/audio_coding/audio_network_adaptor/controller.h"
#include "test/gmock.h"

namespace webrtc {

class MockController : public Controller {
 public:
  virtual ~MockController() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_METHOD1(UpdateNetworkMetrics,
               void(const NetworkMetrics& network_metrics));
  MOCK_METHOD1(MakeDecision, void(AudioEncoderRuntimeConfig* config));
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_MOCK_MOCK_CONTROLLER_H_
