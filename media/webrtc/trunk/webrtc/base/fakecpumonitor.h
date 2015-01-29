/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FAKECPUMONITOR_H_
#define WEBRTC_BASE_FAKECPUMONITOR_H_

#include "webrtc/base/cpumonitor.h"

namespace rtc {

class FakeCpuMonitor : public rtc::CpuMonitor {
 public:
  explicit FakeCpuMonitor(Thread* thread)
      : CpuMonitor(thread) {
  }
  ~FakeCpuMonitor() {
  }

  virtual void OnMessage(rtc::Message* msg) {
  }
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FAKECPUMONITOR_H_
