/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/thread_wrapper.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"

namespace webrtc {

// Function that does nothing, and reports success.
bool NullRunFunction(void* obj) {
  SleepMs(0);  // Hand over timeslice, prevents busy looping.
  return true;
}

TEST(ThreadTest, StartStop) {
  rtc::scoped_ptr<ThreadWrapper> thread = ThreadWrapper::CreateThread(
      &NullRunFunction, nullptr, "ThreadTest");
  ASSERT_TRUE(thread->Start());
  EXPECT_TRUE(thread->Stop());
}

// Function that sets a boolean.
bool SetFlagRunFunction(void* obj) {
  bool* obj_as_bool = static_cast<bool*>(obj);
  *obj_as_bool = true;
  SleepMs(0);  // Hand over timeslice, prevents busy looping.
  return true;
}

TEST(ThreadTest, RunFunctionIsCalled) {
  bool flag = false;
  rtc::scoped_ptr<ThreadWrapper> thread = ThreadWrapper::CreateThread(
      &SetFlagRunFunction, &flag, "RunFunctionIsCalled");
  ASSERT_TRUE(thread->Start());

  // At this point, the flag may be either true or false.
  EXPECT_TRUE(thread->Stop());

  // We expect the thread to have run at least once.
  EXPECT_TRUE(flag);
}

}  // namespace webrtc
