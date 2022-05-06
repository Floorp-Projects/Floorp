/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TASK_UTILS_PENDING_TASK_SAFETY_FLAG_H_
#define RTC_BASE_TASK_UTILS_PENDING_TASK_SAFETY_FLAG_H_

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

// The PendingTaskSafetyFlag and the ScopedTaskSafety are designed to address
// the issue where you have a task to be executed later that has references,
// but cannot guarantee that the referenced object is alive when the task is
// executed.

// This mechanism can be used with tasks that are created and destroyed
// on a single thread / task queue, and with tasks posted to the same
// thread/task queue, but tasks can be posted from any thread/TQ.

// Typical usage:
// When posting a task, post a copy (capture by-value in a lambda) of the flag
// reference and before performing the work, check the |alive()| state. Abort if
// alive() returns |false|:
//
// class ExampleClass {
// ....
//    my_task_queue_->PostTask(ToQueuedTask(
//        [safety = pending_task_safety_flag_, this]() {
//          // Now running on the main thread.
//          if (!safety->alive())
//            return;
//          MyMethod();
//        }));
//   ....
//   ~ExampleClass() {
//     pending_task_safety_flag_->SetNotAlive();
//   }
//   scoped_refptr<PendingTaskSafetyFlag> pending_task_safety_flag_
//        = PendingTaskSafetyFlag::Create();
// }
//
// ToQueuedTask has an overload that makes this check automatic:
//
//    my_task_queue_->PostTask(ToQueuedTask(pending_task_safety_flag_,
//        [this]() { MyMethod(); }));
//
class PendingTaskSafetyFlag : public rtc::RefCountInterface {
 public:
  static rtc::scoped_refptr<PendingTaskSafetyFlag> Create();

  ~PendingTaskSafetyFlag() = default;

  void SetNotAlive();
  void SetAlive();
  bool alive() const;

 protected:
  PendingTaskSafetyFlag() = default;

 private:
  bool alive_ = true;
  RTC_NO_UNIQUE_ADDRESS SequenceChecker main_sequence_;
};

// The ScopedTaskSafety makes using PendingTaskSafetyFlag very simple.
// It does automatic PTSF creation and signalling of destruction when the
// ScopedTaskSafety instance goes out of scope.
//
// ToQueuedTask has an overload that takes a ScopedTaskSafety too, so there
// is no need to explicitly call the "flag" method.
//
// Example usage:
//
//     my_task_queue->PostTask(ToQueuedTask(scoped_task_safety,
//        [this]() {
//             // task goes here
//        }
//
// This should be used by the class that wants tasks dropped after destruction.
// The requirement is that the instance has to be constructed and destructed on
// the same thread as the potentially dropped tasks would be running on.
class ScopedTaskSafety {
 public:
  ScopedTaskSafety() = default;
  ~ScopedTaskSafety() { flag_->SetNotAlive(); }

  // Returns a new reference to the safety flag.
  rtc::scoped_refptr<PendingTaskSafetyFlag> flag() const { return flag_; }

 private:
  rtc::scoped_refptr<PendingTaskSafetyFlag> flag_ =
      PendingTaskSafetyFlag::Create();
};

}  // namespace webrtc

#endif  // RTC_BASE_TASK_UTILS_PENDING_TASK_SAFETY_FLAG_H_
