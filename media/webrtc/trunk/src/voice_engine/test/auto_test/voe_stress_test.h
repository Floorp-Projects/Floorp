/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_STRESS_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_STRESS_TEST_H

namespace webrtc {
class ThreadWrapper;
}

namespace voetest {
// TODO(andrew): using directives are not permitted.
using namespace webrtc;

class VoETestManager;

class VoEStressTest {
 public:
  VoEStressTest(VoETestManager& mgr) :
    _mgr(mgr), _ptrExtraApiThread(NULL) {
  }
  ~VoEStressTest() {
  }
  int DoTest();

 private:
  int MenuSelection();
  int StartStopTest();
  int CreateDeleteChannelsTest();
  int MultipleThreadsTest();

  static bool RunExtraApi(void* ptr);
  bool ProcessExtraApi();

  VoETestManager& _mgr;
  static const char* _key;

  ThreadWrapper* _ptrExtraApiThread;
};

} //  namespace voetest

#endif // WEBRTC_VOICE_ENGINE_VOE_STRESS_TEST_H
