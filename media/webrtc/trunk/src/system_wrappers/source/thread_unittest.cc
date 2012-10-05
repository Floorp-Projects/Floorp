/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/thread_wrapper.h"

#include "gtest/gtest.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

const int kLogTrace = 0;

class TestTraceCallback : public TraceCallback {
 public:
  virtual void Print(const TraceLevel level,
                     const char* traceString,
                     const int length) {
    if (traceString) {
      char* cmd_print = new char[length+1];
      memcpy(cmd_print, traceString, length);
      cmd_print[length] = '\0';
      printf("%s\n", cmd_print);
      fflush(stdout);
      delete[] cmd_print;
    }
  }
};

class ThreadTest : public ::testing::Test {
 public:
  ThreadTest() {
    StartTrace();
  }
  ~ThreadTest() {
    StopTrace();
  }

 private:
  void StartTrace() {
    if (kLogTrace) {
      Trace::CreateTrace();
      Trace::SetLevelFilter(webrtc::kTraceAll);
      Trace::SetTraceCallback(&trace_);
    }
  }

  void StopTrace() {
    if (kLogTrace) {
      Trace::ReturnTrace();
    }
  }

  TestTraceCallback trace_;
};

// Function that does nothing, and reports success.
bool NullRunFunction(void* /* obj */) {
  return true;
}

TEST_F(ThreadTest, StartStop) {
  ThreadWrapper* thread = ThreadWrapper::CreateThread(&NullRunFunction);
  unsigned int id = 42;
  ASSERT_TRUE(thread->Start(id));
  EXPECT_TRUE(thread->Stop());
  delete thread;
}

// Function that sets a boolean.
bool SetFlagRunFunction(void* obj) {
  bool* obj_as_bool = static_cast<bool*> (obj);
  *obj_as_bool = true;
  return true;
}

TEST_F(ThreadTest, RunFunctionIsCalled) {
  bool flag = false;
  ThreadWrapper* thread = ThreadWrapper::CreateThread(&SetFlagRunFunction,
                                                      &flag);
  unsigned int id = 42;
  ASSERT_TRUE(thread->Start(id));
  // At this point, the flag may be either true or false.
  EXPECT_TRUE(thread->Stop());
  // We expect the thread to have run at least once.
  EXPECT_TRUE(flag);
  delete thread;
}

}  // namespace webrtc
