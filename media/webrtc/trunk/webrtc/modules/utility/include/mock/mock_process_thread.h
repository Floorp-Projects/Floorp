/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_INCLUDE_MOCK_MOCK_PROCESS_THREAD_H_
#define WEBRTC_MODULES_UTILITY_INCLUDE_MOCK_MOCK_PROCESS_THREAD_H_

#include "webrtc/modules/utility/include/process_thread.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace webrtc {

class MockProcessThread : public ProcessThread {
 public:
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(WakeUp, void(Module* module));
  MOCK_METHOD1(PostTask, void(ProcessTask* task));
  MOCK_METHOD1(RegisterModule, void(Module* module));
  MOCK_METHOD1(DeRegisterModule, void(Module* module));

  // MOCK_METHOD1 gets confused with mocking this method, so we work around it
  // by overriding the method from the interface and forwarding the call to a
  // mocked, simpler method.
  void PostTask(rtc::scoped_ptr<ProcessTask> task) override {
    PostTask(task.get());
  }
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_UTILITY_INCLUDE_MOCK_MOCK_PROCESS_THREAD_H_
