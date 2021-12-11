/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_ONETIMEEVENT_H_
#define RTC_BASE_ONETIMEEVENT_H_

#include "rtc_base/criticalsection.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
// Provides a simple way to perform an operation (such as logging) one
// time in a certain scope.
// Example:
//   OneTimeEvent firstFrame;
//   ...
//   if (firstFrame()) {
//     RTC_LOG(LS_INFO) << "This is the first frame".
//   }
class OneTimeEvent {
 public:
  OneTimeEvent() {}
  bool operator()() {
    rtc::CritScope cs(&critsect_);
    if (happened_) {
      return false;
    }
    happened_ = true;
    return true;
  }

 private:
  bool happened_ = false;
  rtc::CriticalSection critsect_;
};

// A non-thread-safe, ligher-weight version of the OneTimeEvent class.
class ThreadUnsafeOneTimeEvent {
 public:
  ThreadUnsafeOneTimeEvent() {}
  bool operator()() {
    if (happened_) {
      return false;
    }
    happened_ = true;
    return true;
  }

 private:
  bool happened_ = false;
};

}  // namespace webrtc

#endif  // RTC_BASE_ONETIMEEVENT_H_
