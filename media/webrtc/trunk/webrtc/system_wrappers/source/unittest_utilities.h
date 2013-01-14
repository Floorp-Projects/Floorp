/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_UNITTEST_UTILITIES_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_UNITTEST_UTILITIES_H_

// This file contains utilities that make it simpler to write unittests
// that are appropriate for the system_wrappers classes.

#include <stdio.h>
#include <string.h>

#include "system_wrappers/interface/trace.h"

namespace webrtc {

class TestTraceCallback : public TraceCallback {
 public:
  virtual void Print(TraceLevel level, const char* msg, int length) {
    if (msg) {
      char* cmd_print = new char[length+1];
      memcpy(cmd_print, msg, length);
      cmd_print[length] = '\0';
      printf("%s\n", cmd_print);
      fflush(stdout);
      delete[] cmd_print;
    }
  }
};

// A class that turns on tracing to stdout at the beginning of the test,
// and turns it off once the test is finished.
// Intended usage:
// class SomeTest : public ::testing::Test {
//  protected:
//   SomeTest()
//     : trace_(false) {}  // Change to true to turn on tracing.
//  private:
//   ScopedTracing trace_;
// }
class ScopedTracing {
 public:
  explicit ScopedTracing(bool logOn) {
    logging_ = logOn;
    StartTrace();
  }

  ~ScopedTracing() {
    StopTrace();
  }

 private:
  void StartTrace() {
    if (logging_) {
      Trace::CreateTrace();
      Trace::SetLevelFilter(webrtc::kTraceAll);
      Trace::SetTraceCallback(&trace_);
    }
  }

  void StopTrace() {
    if (logging_) {
      Trace::ReturnTrace();
    }
  }

 private:
  bool logging_;
  TestTraceCallback trace_;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_SOURCE_UNITTEST_UTILITIES_H_
