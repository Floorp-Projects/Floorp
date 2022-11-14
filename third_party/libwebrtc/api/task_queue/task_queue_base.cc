/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_base.h"

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/functional/any_invocable.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"

#if defined(ABSL_HAVE_THREAD_LOCAL)

namespace webrtc {
namespace {

ABSL_CONST_INIT thread_local TaskQueueBase* current = nullptr;

}  // namespace

TaskQueueBase* TaskQueueBase::Current() {
  return current;
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(current) {
  current = task_queue;
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  current = previous_;
}
}  // namespace webrtc

#elif defined(WEBRTC_POSIX)

#include <pthread.h>

namespace webrtc {
namespace {

ABSL_CONST_INIT pthread_key_t g_queue_ptr_tls = 0;

void InitializeTls() {
  RTC_CHECK(pthread_key_create(&g_queue_ptr_tls, nullptr) == 0);
}

pthread_key_t GetQueuePtrTls() {
  static pthread_once_t init_once = PTHREAD_ONCE_INIT;
  RTC_CHECK(pthread_once(&init_once, &InitializeTls) == 0);
  return g_queue_ptr_tls;
}

}  // namespace

TaskQueueBase* TaskQueueBase::Current() {
  return static_cast<TaskQueueBase*>(pthread_getspecific(GetQueuePtrTls()));
}

TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(
    TaskQueueBase* task_queue)
    : previous_(TaskQueueBase::Current()) {
  pthread_setspecific(GetQueuePtrTls(), task_queue);
}

TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter() {
  pthread_setspecific(GetQueuePtrTls(), previous_);
}

}  // namespace webrtc

#else
#error Unsupported platform
#endif

// Functions to support transition from std::unique_ptr<QueuedTask>
// representation of a task to absl::AnyInvocable<void() &&> representation. In
// the base interface older and newer functions call each other. This way
// TaskQueue would work when classes derived from TaskQueueBase implements
// either old set of Post functions (before the transition) or new set of Post
// functions. Thus callers of the interface can be updated independently of all
// of the implementations of that TaskQueueBase interface.

// TODO(bugs.webrtc.org/14245): Delete these functions when transition is
// complete.
namespace webrtc {
namespace {
class Task : public QueuedTask {
 public:
  explicit Task(absl::AnyInvocable<void() &&> task) : task_(std::move(task)) {}
  ~Task() override = default;

  bool Run() override {
    std::move(task_)();
    return true;
  }

 private:
  absl::AnyInvocable<void() &&> task_;
};

std::unique_ptr<QueuedTask> ToLegacy(absl::AnyInvocable<void() &&> task) {
  return std::make_unique<Task>(std::move(task));
}

absl::AnyInvocable<void() &&> FromLegacy(std::unique_ptr<QueuedTask> task) {
  return [task = std::move(task)]() mutable {
    if (!task->Run()) {
      task.release();
    }
  };
}

}  // namespace

void TaskQueueBase::PostTask(std::unique_ptr<QueuedTask> task) {
  PostTask(FromLegacy(std::move(task)));
}

void TaskQueueBase::PostTask(absl::AnyInvocable<void() &&> task) {
  PostTask(ToLegacy(std::move(task)));
}

void TaskQueueBase::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                    uint32_t milliseconds) {
  PostDelayedTask(FromLegacy(std::move(task)), TimeDelta::Millis(milliseconds));
}

void TaskQueueBase::PostDelayedTask(absl::AnyInvocable<void() &&> task,
                                    TimeDelta delay) {
  PostDelayedTask(ToLegacy(std::move(task)), delay.ms());
}

void TaskQueueBase::PostDelayedHighPrecisionTask(
    absl::AnyInvocable<void() &&> task,
    TimeDelta delay) {
  PostDelayedHighPrecisionTask(ToLegacy(std::move(task)), delay.ms());
}

}  // namespace webrtc
