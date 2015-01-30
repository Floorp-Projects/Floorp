/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_MOCK_PROCESS_THREAD_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_MOCK_PROCESS_THREAD_H_

#include "webrtc/modules/utility/interface/process_thread.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace webrtc {

class MockProcessThread : public ProcessThread {
 public:
  MOCK_METHOD0(Start, int32_t());
  MOCK_METHOD0(Stop, int32_t());
  MOCK_METHOD1(RegisterModule, int32_t(Module* module));
  MOCK_METHOD1(DeRegisterModule, int32_t(const Module* module));
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_UTILITY_INTERFACE_MOCK_PROCESS_THREAD_H_
