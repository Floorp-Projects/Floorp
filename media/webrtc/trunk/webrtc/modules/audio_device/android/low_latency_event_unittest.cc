/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/low_latency_event.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {

static const int kEventMsg = 1;

class LowLatencyEventTest : public testing::Test {
 public:
  LowLatencyEventTest()
      : process_thread_(ThreadWrapper::CreateThread(CbThread,
                                                    this,
                                                    kRealtimePriority,
                                                    "test_thread")),
        terminated_(false),
        iteration_count_(0),
        allowed_iterations_(0) {
    EXPECT_TRUE(event_.Start());
    Start();
  }
  ~LowLatencyEventTest() {
    EXPECT_GE(allowed_iterations_, 1);
    EXPECT_GE(iteration_count_, 1);
    Stop();
  }

  void AllowOneIteration() {
    ++allowed_iterations_;
    event_.SignalEvent(allowed_iterations_, kEventMsg);
  }

 private:
  void Start() {
    unsigned int thread_id = 0;
    EXPECT_TRUE(process_thread_->Start(thread_id));
  }
  void Stop() {
    terminated_ = true;
    event_.Stop();
    process_thread_->Stop();
  }

  static bool CbThread(void* context) {
    return reinterpret_cast<LowLatencyEventTest*>(context)->CbThreadImpl();
  }
  bool CbThreadImpl() {
    int allowed_iterations;
    int message;
    ++iteration_count_;
    event_.WaitOnEvent(&allowed_iterations, &message);
    EXPECT_EQ(iteration_count_, allowed_iterations);
    EXPECT_EQ(message, kEventMsg);
    return !terminated_;
  }

  LowLatencyEvent event_;

  scoped_ptr<ThreadWrapper> process_thread_;
  bool terminated_;
  int iteration_count_;
  int allowed_iterations_;
};


TEST_F(LowLatencyEventTest, TriggerEvent) {
  for (int i = 0; i < 3; ++i) {
    AllowOneIteration();
  }
}

// Events trigger in less than 3ms. Wait for 3 ms to ensure there are no
// spurious wakeups.
TEST_F(LowLatencyEventTest, NoTriggerEvent) {
  SleepMs(3);
  // If there were spurious wakeups either the wakeups would have triggered a
  // failure as we haven't allowed an iteration yet. Or the wakeup happened
  // to signal 0, 0 in which case the mismatch will be discovered when allowing
  // an iteration to happen.
  AllowOneIteration();
}

}  // namespace webrtc
