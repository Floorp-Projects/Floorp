/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TEST_SUPPORT_TRACE_TO_STDERR_H_
#define WEBRTC_TEST_TEST_SUPPORT_TRACE_TO_STDERR_H_

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace test {

// Upon constructing an instance of this class, all traces will be redirected
// to stderr. At destruction, redirection is halted.
class TraceToStderr : public TraceCallback {
 public:
  TraceToStderr();
  virtual ~TraceToStderr();

  virtual void Print(TraceLevel level, const char* msg_array, int length);
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TEST_SUPPORT_TRACE_TO_STDERR_H_
