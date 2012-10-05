/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MOCK_VOE_CONNECTION_OBSERVER_H_
#define MOCK_VOE_CONNECTION_OBSERVER_H_

#include "voice_engine/include/voe_network.h"

namespace webrtc {

class MockVoeConnectionObserver : public VoEConnectionObserver {
 public:
  MOCK_METHOD2(OnPeriodicDeadOrAlive, void(const int channel,
                                           const bool alive));
};

}

#endif  // MOCK_VOE_CONNECTION_OBSERVER_H_
