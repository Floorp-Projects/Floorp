/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_thread.h"

#include "rtc_base/event.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"

namespace rtc {
namespace {

void NullRunFunction(void* obj) {}

// Function that sets a boolean.
void SetFlagRunFunction(void* obj) {
  bool* obj_as_bool = static_cast<bool*>(obj);
  *obj_as_bool = true;
}

void StdFunctionRunFunction(void* obj) {
  std::function<void()>* fun = static_cast<std::function<void()>*>(obj);
  (*fun)();
}

}  // namespace

TEST(PlatformThreadTest, StartStop) {
  PlatformThread thread(&NullRunFunction, nullptr, "PlatformThreadTest");
  EXPECT_TRUE(thread.name() == "PlatformThreadTest");
  EXPECT_TRUE(thread.GetThreadRef() == 0);
  thread.Start();
  EXPECT_TRUE(thread.GetThreadRef() != 0);
  thread.Stop();
  EXPECT_TRUE(thread.GetThreadRef() == 0);
}

TEST(PlatformThreadTest, StartStop2) {
  PlatformThread thread1(&NullRunFunction, nullptr, "PlatformThreadTest1");
  PlatformThread thread2(&NullRunFunction, nullptr, "PlatformThreadTest2");
  EXPECT_TRUE(thread1.GetThreadRef() == thread2.GetThreadRef());
  thread1.Start();
  thread2.Start();
  EXPECT_TRUE(thread1.GetThreadRef() != thread2.GetThreadRef());
  thread2.Stop();
  thread1.Stop();
}

TEST(PlatformThreadTest, RunFunctionIsCalled) {
  bool flag = false;
  PlatformThread thread(&SetFlagRunFunction, &flag, "RunFunctionIsCalled");
  thread.Start();

  // At this point, the flag may be either true or false.
  thread.Stop();

  // We expect the thread to have run at least once.
  EXPECT_TRUE(flag);
}

TEST(PlatformThreadTest, JoinsThread) {
  // This test flakes if there are problems with the join implementation.
  EXPECT_TRUE(ThreadAttributes().joinable);
  rtc::Event event;
  std::function<void()> thread_function = [&] { event.Set(); };
  PlatformThread thread(&StdFunctionRunFunction, &thread_function, "T");
  thread.Start();
  thread.Stop();
  EXPECT_TRUE(event.Wait(/*give_up_after_ms=*/0));
}

TEST(PlatformThreadTest, StopsBeforeDetachedThreadExits) {
  // This test flakes if there are problems with the detached thread
  // implementation.
  bool flag = false;
  rtc::Event thread_started;
  rtc::Event thread_continue;
  rtc::Event thread_exiting;
  std::function<void()> thread_function = [&] {
    thread_started.Set();
    thread_continue.Wait(Event::kForever);
    flag = true;
    thread_exiting.Set();
  };
  {
    PlatformThread thread(&StdFunctionRunFunction, &thread_function, "T",
                          ThreadAttributes().SetDetached());
    thread.Start();
    thread.Stop();
  }
  thread_started.Wait(Event::kForever);
  EXPECT_FALSE(flag);
  thread_continue.Set();
  thread_exiting.Wait(Event::kForever);
  EXPECT_TRUE(flag);
}

}  // namespace rtc
