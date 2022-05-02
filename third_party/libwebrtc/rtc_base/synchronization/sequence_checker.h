/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_
#define RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_

#include <type_traits>

#include "rtc_base/checks.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/synchronization/sequence_checker_internal.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// SequenceChecker is a helper class used to help verify that some methods
// of a class are called on the same task queue or thread. A
// SequenceChecker is bound to a a task queue if the object is
// created on a task queue, or a thread otherwise.
//
//
// Example:
// class MyClass {
//  public:
//   void Foo() {
//     RTC_DCHECK_RUN_ON(sequence_checker_);
//     ... (do stuff) ...
//   }
//
//  private:
//   SequenceChecker sequence_checker_;
// }
//
// In Release mode, IsCurrent will always return true.
#if RTC_DCHECK_IS_ON
class RTC_LOCKABLE SequenceChecker
    : public webrtc_sequence_checker_internal::SequenceCheckerImpl {};
#else
class RTC_LOCKABLE SequenceChecker
    : public webrtc_sequence_checker_internal::SequenceCheckerDoNothing {};
#endif  // RTC_ENABLE_THREAD_CHECKER

}  // namespace webrtc

// RTC_RUN_ON/RTC_GUARDED_BY/RTC_DCHECK_RUN_ON macros allows to annotate
// variables are accessed from same thread/task queue.
// Using tools designed to check mutexes, it checks at compile time everywhere
// variable is access, there is a run-time dcheck thread/task queue is correct.
//
// class ThreadExample {
//  public:
//   void NeedVar1() {
//     RTC_DCHECK_RUN_ON(network_thread_);
//     transport_->Send();
//   }
//
//  private:
//   rtc::Thread* network_thread_;
//   int transport_ RTC_GUARDED_BY(network_thread_);
// };
//
// class SequenceCheckerExample {
//  public:
//   int CalledFromPacer() RTC_RUN_ON(pacer_sequence_checker_) {
//     return var2_;
//   }
//
//   void CallMeFromPacer() {
//     RTC_DCHECK_RUN_ON(&pacer_sequence_checker_)
//        << "Should be called from pacer";
//     CalledFromPacer();
//   }
//
//  private:
//   int pacer_var_ RTC_GUARDED_BY(pacer_sequence_checker_);
//   SequenceChecker pacer_sequence_checker_;
// };
//
// class TaskQueueExample {
//  public:
//   class Encoder {
//    public:
//     rtc::TaskQueue* Queue() { return encoder_queue_; }
//     void Encode() {
//       RTC_DCHECK_RUN_ON(encoder_queue_);
//       DoSomething(var_);
//     }
//
//    private:
//     rtc::TaskQueue* const encoder_queue_;
//     Frame var_ RTC_GUARDED_BY(encoder_queue_);
//   };
//
//   void Encode() {
//     // Will fail at runtime when DCHECK is enabled:
//     // encoder_->Encode();
//     // Will work:
//     rtc::scoped_refptr<Encoder> encoder = encoder_;
//     encoder_->Queue()->PostTask([encoder] { encoder->Encode(); });
//   }
//
//  private:
//   rtc::scoped_refptr<Encoder> encoder_;
// }

// Document if a function expected to be called from same thread/task queue.
#define RTC_RUN_ON(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(x))

#define RTC_DCHECK_RUN_ON(x)                                               \
  webrtc::webrtc_sequence_checker_internal::SequenceCheckerScope scope(x); \
  RTC_DCHECK((x)->IsCurrent())                                             \
      << webrtc::webrtc_sequence_checker_internal::ExpectationToString(x)

#endif  // RTC_BASE_SYNCHRONIZATION_SEQUENCE_CHECKER_H_
