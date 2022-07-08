/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_queue.h"

#include <mmsystem.h>
#include <string.h>

#include <algorithm>
#include <queue>
#include <utility>

#include "absl/types/optional.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/refcount.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/timeutils.h"

namespace rtc {
namespace {
using Priority = TaskQueue::Priority;

DWORD g_queue_ptr_tls = 0;

BOOL CALLBACK InitializeTls(PINIT_ONCE init_once, void* param, void** context) {
  g_queue_ptr_tls = TlsAlloc();
  return TRUE;
}

DWORD GetQueuePtrTls() {
  static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
  ::InitOnceExecuteOnce(&init_once, InitializeTls, nullptr, nullptr);
  return g_queue_ptr_tls;
}

struct ThreadStartupData {
  Event* started;
  void* thread_context;
};

void CALLBACK InitializeQueueThread(ULONG_PTR param) {
  ThreadStartupData* data = reinterpret_cast<ThreadStartupData*>(param);
  ::TlsSetValue(GetQueuePtrTls(), data->thread_context);
  data->started->Set();
}

ThreadPriority TaskQueuePriorityToThreadPriority(Priority priority) {
  switch (priority) {
    case Priority::HIGH:
      return rtc::ThreadPriority::kRealtime;
    case Priority::LOW:
      return rtc::ThreadPriority::kLow;
    case Priority::NORMAL:
      return rtc::ThreadPriority::kNormal;
  }
}

int64_t GetTick() {
  static const UINT kPeriod = 1;
  bool high_res = (timeBeginPeriod(kPeriod) == TIMERR_NOERROR);
  int64_t ret = TimeMillis();
  if (high_res) {
    timeEndPeriod(kPeriod);
  }
  return ret;
}

class DelayedTaskInfo {
 public:
  // Default ctor needed to support priority_queue::pop().
  DelayedTaskInfo() {}
  DelayedTaskInfo(uint32_t milliseconds, std::unique_ptr<QueuedTask> task)
      : due_time_(GetTick() + milliseconds), task_(std::move(task)) {}
  DelayedTaskInfo(DelayedTaskInfo&&) = default;

  // Implement for priority_queue.
  bool operator>(const DelayedTaskInfo& other) const {
    return due_time_ > other.due_time_;
  }

  // Required by priority_queue::pop().
  DelayedTaskInfo& operator=(DelayedTaskInfo&& other) = default;

  // See below for why this method is const.
  std::unique_ptr<QueuedTask> take() const { return std::move(task_); }

  int64_t due_time() const { return due_time_; }

 private:
  int64_t due_time_ = 0;  // Absolute timestamp in milliseconds.

  // `task` needs to be mutable because std::priority_queue::top() returns
  // a const reference and a key in an ordered queue must not be changed.
  // There are two basic workarounds, one using const_cast, which would also
  // make the key (`due_time`), non-const and the other is to make the non-key
  // (`task`), mutable.
  // Because of this, the `task` variable is made private and can only be
  // mutated by calling the `take()` method.
  mutable std::unique_ptr<QueuedTask> task_;
};

}  // namespace

class TaskQueue::Impl : public RefCountInterface {
 public:
  // Every task queue has a shared_ptr<ReplyHandler>, and shared references
  // are handed out to other queues that name this queue as the "reply queue"
  // of a PostTaskAndReply() call.
  //
  // The impl_ member of this will be valid the entire period that this
  // queue is able to safely handle messages. If the queue is destroyed,
  // impl_ will be set to nullptr. Once this happens, any pending reply
  // messages from other queues will be dropped
  class ReplyHandler {
   public:
    ReplyHandler() : impl_(nullptr) {}

    void SetImpl(Impl* impl) {
      rtc::CritScope lock(&lock_);
      impl_ = impl;
    }

    void PostReplyTask(QueuedTask* reply_task) {
      rtc::CritScope lock(&lock_);

      if (!impl_) {
        delete reply_task;
        return;
      }

      impl_->PostTask([reply_task] {
        if (reply_task->Run()) {
          delete reply_task;
        }
      });
    }

    ReplyHandler(const ReplyHandler&) = delete;
    ReplyHandler& operator=(const ReplyHandler&) = delete;

   private:
    Impl* impl_;
    rtc::CriticalSection lock_;
  };

  Impl(const char* queue_name, TaskQueue* queue, Priority priority);
  ~Impl() override;

  static TaskQueue::Impl* Current();
  static TaskQueue* CurrentQueue();

  // Used for DCHECKing the current queue.
  bool IsCurrent() const;

  template <class Closure,
            typename std::enable_if<!std::is_convertible<
                Closure, std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
  void PostTask(Closure&& closure) {
    PostTask(NewClosure(std::forward<Closure>(closure)));
  }

  void PostTask(std::unique_ptr<QueuedTask> task);
  void PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                        std::unique_ptr<QueuedTask> reply,
                        TaskQueue::Impl* reply_queue);

  void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

  void RunPendingTasks();

 private:
  void RunThreadMain();
  void RunDueTasks();
  void ScheduleNextTimer();

  // Since priority_queue<> by defult orders items in terms of
  // largest->smallest, using std::less<>, and we want smallest->largest,
  // we would like to use std::greater<> here. Alas it's only available in
  // C++14 and later, so we roll our own compare template that that relies on
  // operator<().
  template <typename T>
  struct greater {
    bool operator()(const T& l, const T& r) { return l > r; }
  };

  rtc::CriticalSection timer_lock_;
  std::priority_queue<DelayedTaskInfo, std::vector<DelayedTaskInfo>,
                      greater<DelayedTaskInfo>>
      timer_tasks_ RTC_GUARDED_BY(timer_lock_);

  TaskQueue* const queue_;
  rtc::PlatformThread thread_;
  rtc::CriticalSection pending_lock_;
  std::queue<std::unique_ptr<QueuedTask>> pending_
      RTC_GUARDED_BY(pending_lock_);
  // event handle for currently queued tasks
  HANDLE in_queue_;
  // event handle to stop execution
  HANDLE stop_queue_;
  // event handle for waitable timer events
  HANDLE task_timer_;

  std::shared_ptr<ReplyHandler> reply_handler_;
};

TaskQueue::Impl::Impl(const char* queue_name, TaskQueue* queue,
                      Priority priority)
    : queue_(queue),
      in_queue_(::CreateEvent(nullptr, true, false, nullptr)),
      stop_queue_(::CreateEvent(nullptr, TRUE, FALSE, nullptr)),
      task_timer_(::CreateWaitableTimer(nullptr, FALSE, nullptr)),
      reply_handler_(std::make_shared<ReplyHandler>()) {
  RTC_DCHECK(queue_name);
  RTC_DCHECK(in_queue_);
  RTC_DCHECK(stop_queue_);
  RTC_DCHECK(task_timer_);
  thread_ = rtc::PlatformThread::SpawnJoinable(
      [this] { RunThreadMain(); }, queue_name,
      rtc::ThreadAttributes().SetPriority(priority));

  Event event(false, false);
  ThreadStartupData startup = {&event, this};
  RTC_CHECK(thread_.QueueAPC(&InitializeQueueThread,
                             reinterpret_cast<ULONG_PTR>(&startup)));
  event.Wait(Event::kForever);

  // Once this is set, we can receive reply messages from other queues
  reply_handler_->SetImpl(this);
}

TaskQueue::Impl::~Impl() {
  RTC_DCHECK(!IsCurrent());

  // Once this is cleared, we will no longer be sent reply messages from
  // other queues
  reply_handler_->SetImpl(nullptr);

  ::SetEvent(stop_queue_);
  thread_.Stop();

  ::CloseHandle(stop_queue_);
  ::CloseHandle(task_timer_);
  ::CloseHandle(in_queue_);
}

// static
TaskQueue::Impl* TaskQueue::Impl::Current() {
  return static_cast<TaskQueue::Impl*>(::TlsGetValue(GetQueuePtrTls()));
}

// static
TaskQueue* TaskQueue::Impl::CurrentQueue() {
  TaskQueue::Impl* current = Current();
  return current ? current->queue_ : nullptr;
}

bool TaskQueue::Impl::IsCurrent() const {
  return IsThreadRefEqual(thread_.GetThreadRef(), CurrentThreadRef());
}

void TaskQueue::Impl::PostTask(std::unique_ptr<QueuedTask> task) {
  rtc::CritScope lock(&pending_lock_);
  pending_.push(std::move(task));
  ::SetEvent(in_queue_);
}

void TaskQueue::Impl::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                      uint32_t milliseconds) {
  if (!milliseconds) {
    PostTask(std::move(task));
    return;
  }

  {
    rtc::CritScope lock(&timer_lock_);
    // Record if we need to schedule our timer based on if either there are no
    // tasks currently being timed or the current task being timed would end
    // later than the task we're placing into the priority queue
    bool need_to_schedule_timer =
        timer_tasks_.empty() ||
        timer_tasks_.top().due_time() > GetTick() + milliseconds;

    timer_tasks_.emplace(milliseconds, std::move(task));

    if (need_to_schedule_timer) {
      ScheduleNextTimer();
    }
  }
}

void TaskQueue::Impl::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                       std::unique_ptr<QueuedTask> reply,
                                       TaskQueue::Impl* reply_queue) {
  QueuedTask* task_ptr = task.release();
  QueuedTask* reply_task_ptr = reply.release();

  std::shared_ptr<ReplyHandler> reply_handler = reply_queue->reply_handler_;

  PostTask([task_ptr, reply_task_ptr, reply_handler] {
    if (task_ptr->Run()) {
      delete task_ptr;
    }

    reply_handler->PostReplyTask(reply_task_ptr);
  });
}

void TaskQueue::Impl::RunPendingTasks() {
  while (true) {
    std::unique_ptr<QueuedTask> task;
    {
      rtc::CritScope lock(&pending_lock_);
      if (pending_.empty()) {
        break;
      }
      task = std::move(pending_.front());
      pending_.pop();
    }

    if (!task->Run()) {
      task.release();
    }
  }
}

// mjf - this was removed upstream, so may need to be removed here.
// static
void TaskQueue::Impl::ThreadMain(void* context) {
  static_cast<TaskQueue::Impl*>(context)->RunThreadMain();
}

void TaskQueue::Impl::RunThreadMain() {
  // These handles correspond to the WAIT_OBJECT_0 + n expressions below where n
  // is the position in handles that WaitForMultipleObjectsEx() was alerted on.
  HANDLE handles[3] = {stop_queue_, task_timer_, in_queue_};
  while (true) {
    // Make sure we do an alertable wait as that's required to allow APCs to
    // run (e.g. required for InitializeQueueThread and stopping the thread in
    // PlatformThread).
    DWORD result = ::WaitForMultipleObjectsEx(arraysize(handles), handles,
                                              FALSE, INFINITE, TRUE);
    RTC_CHECK_NE(WAIT_FAILED, result);

    // run waitable timer tasks
    {
      rtc::CritScope lock(&timer_lock_);
      if (result == WAIT_OBJECT_0 + 1 ||
          (!timer_tasks_.empty() &&
           ::WaitForSingleObject(task_timer_, 0) == WAIT_OBJECT_0)) {
        RunDueTasks();
        ScheduleNextTimer();
      }
    }

    // reset in_queue_ event and run leftover tasks
    if (result == (WAIT_OBJECT_0 + 2)) {
      ::ResetEvent(in_queue_);
      TaskQueue::Impl::Current()->RunPendingTasks();
    }

    // empty queue and shut down thread
    if (result == (WAIT_OBJECT_0)) {
      break;
    }
  }
}

void TaskQueue::Impl::RunDueTasks() {
  RTC_DCHECK(!timer_tasks_.empty());
  auto now = GetTick();
  do {
    if (timer_tasks_.top().due_time() > now) {
      break;
    }
    std::unique_ptr<QueuedTask> task = timer_tasks_.top().take();
    timer_tasks_.pop();
    if(!task->Run()) {
      task.release();
    }
  } while (!timer_tasks_.empty());
}

void TaskQueue::Impl::ScheduleNextTimer() {
  RTC_DCHECK_NE(task_timer_, INVALID_HANDLE_VALUE);
  if (timer_tasks_.empty()) {
    return;
  }

  LARGE_INTEGER due_time;
  // We have to convert DelayedTaskInfo's due_time_ from milliseconds to
  // nanoseconds and make it a relative time instead of an absolute time so we
  // multiply it by -10,000 here
  due_time.QuadPart =
      -10000 * std::max(0ll, timer_tasks_.top().due_time() - GetTick());
  ::SetWaitableTimer(task_timer_, &due_time, 0, nullptr, nullptr, FALSE);
}

// Boilerplate for the PIMPL pattern.
TaskQueue::TaskQueue(const char* queue_name, Priority priority)
    : impl_(new RefCountedObject<TaskQueue::Impl>(queue_name, this, priority)) {
}

TaskQueue::~TaskQueue() {}

// static
TaskQueue* TaskQueue::Current() { return TaskQueue::Impl::CurrentQueue(); }

// Used for DCHECKing the current queue.
bool TaskQueue::IsCurrent() const { return impl_->IsCurrent(); }

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  return TaskQueue::impl_->PostTask(std::move(task));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            reply_queue->impl_.get());
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  return TaskQueue::impl_->PostTaskAndReply(std::move(task), std::move(reply),
                                            impl_.get());
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  return TaskQueue::impl_->PostDelayedTask(std::move(task), milliseconds);
}

}  // namespace rtc
