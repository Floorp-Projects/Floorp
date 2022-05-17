/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_thread.h"

#include <memory>

#if !defined(WEBRTC_WIN)
#include <sched.h>
#endif
#include <stdint.h>
#include <time.h>

#include <algorithm>

#include "absl/memory/memory.h"
#include "rtc_base/checks.h"

#include "MicroGeckoProfiler.h"

namespace rtc {

namespace {
struct ThreadStartData {
  ThreadRunFunction run_function;
  void* obj;
  std::string thread_name;
  ThreadPriority priority;
};

bool SetPriority(ThreadPriority priority) {
#if defined(WEBRTC_WIN)
  return SetThreadPriority(GetCurrentThread(), priority) != FALSE;
#elif defined(__native_client__) || defined(WEBRTC_FUCHSIA)
  // Setting thread priorities is not supported in NaCl or Fuchsia.
  return true;
#elif defined(WEBRTC_CHROMIUM_BUILD) && defined(WEBRTC_LINUX)
  // TODO(tommi): Switch to the same mechanism as Chromium uses for changing
  // thread priorities.
  return true;
#else
  const int policy = SCHED_FIFO;
  const int min_prio = sched_get_priority_min(policy);
  const int max_prio = sched_get_priority_max(policy);
  if (min_prio == -1 || max_prio == -1) {
    return false;
  }

  if (max_prio - min_prio <= 2)
    return false;

  // Convert webrtc priority to system priorities:
  sched_param param;
  const int top_prio = max_prio - 1;
  const int low_prio = min_prio + 1;
  switch (priority) {
    case kLowPriority:
      param.sched_priority = low_prio;
      break;
    case kNormalPriority:
      // The -1 ensures that the kHighPriority is always greater or equal to
      // kNormalPriority.
      param.sched_priority = (low_prio + top_prio - 1) / 2;
      break;
    case kHighPriority:
      param.sched_priority = std::max(top_prio - 2, low_prio);
      break;
    case kHighestPriority:
      param.sched_priority = std::max(top_prio - 1, low_prio);
      break;
    case kRealtimePriority:
      param.sched_priority = top_prio;
      break;
  }
  return pthread_setschedparam(pthread_self(), policy, &param) == 0;
#endif  // defined(WEBRTC_WIN)
}

void RunPlatformThread(std::unique_ptr<ThreadStartData> data) {
  rtc::SetCurrentThreadName(data->thread_name.c_str());

  char stacktop;
  AutoRegisterProfiler profiler(data->thread_name.c_str(), &stacktop);

  data->thread_name.clear();
  SetPriority(data->priority);
  data->run_function(data->obj);
}

#if defined(WEBRTC_WIN)
DWORD WINAPI StartThread(void* param) {
  // The GetLastError() function only returns valid results when it is called
  // after a Win32 API function that returns a "failed" result. A crash dump
  // contains the result from GetLastError() and to make sure it does not
  // falsely report a Windows error we call SetLastError here.
  ::SetLastError(ERROR_SUCCESS);
  RunPlatformThread(absl::WrapUnique(static_cast<ThreadStartData*>(param)));
  return 0;
}
#else
void* StartThread(void* param) {
  RunPlatformThread(absl::WrapUnique(static_cast<ThreadStartData*>(param)));
  return 0;
}
#endif  // defined(WEBRTC_WIN)

}  // namespace

PlatformThread::PlatformThread(ThreadRunFunction func,
                               void* obj,
                               absl::string_view thread_name,
                               ThreadAttributes attributes)
    : run_function_(func),
      attributes_(attributes),
      obj_(obj),
      name_(thread_name) {
  RTC_DCHECK(func);
  RTC_DCHECK(!name_.empty());
  // TODO(tommi): Consider lowering the limit to 15 (limit on Linux).
  RTC_DCHECK(name_.length() < 64);
}

PlatformThread::~PlatformThread() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!thread_);
#if defined(WEBRTC_WIN)
  RTC_DCHECK(!thread_id_);
#endif  // defined(WEBRTC_WIN)
}

void PlatformThread::Start() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!thread_) << "Thread already started?";
  ThreadStartData* data =
      new ThreadStartData{run_function_, obj_, name_, attributes_.priority};
#if defined(WEBRTC_WIN)
  // See bug 2902 for background on STACK_SIZE_PARAM_IS_A_RESERVATION.
  // Set the reserved stack stack size to 1M, which is the default on Windows
  // and Linux.
  // Mozilla: Set to 256kb for consistency with nsIThreadManager.idl
  thread_ = ::CreateThread(nullptr, 256 * 1024, &StartThread, data,
                           STACK_SIZE_PARAM_IS_A_RESERVATION, &thread_id_);
  RTC_CHECK(thread_) << "CreateThread failed";
  RTC_DCHECK(thread_id_);
#else
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  // Set the stack stack size to 1M.
  // Mozilla: Set to 256kb for consistency with nsIThreadManager.idl
  pthread_attr_setstacksize(&attr, 256 * 1024);
  pthread_attr_setdetachstate(&attr, attributes_.joinable
                                         ? PTHREAD_CREATE_JOINABLE
                                         : PTHREAD_CREATE_DETACHED);
  RTC_CHECK_EQ(0, pthread_create(&thread_, &attr, &StartThread, data));
  pthread_attr_destroy(&attr);
#endif  // defined(WEBRTC_WIN)
}

bool PlatformThread::IsRunning() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
#if defined(WEBRTC_WIN)
  return thread_ != nullptr;
#else
  return thread_ != 0;
#endif  // defined(WEBRTC_WIN)
}

PlatformThreadRef PlatformThread::GetThreadRef() const {
#if defined(WEBRTC_WIN)
  return thread_id_;
#else
  return thread_;
#endif  // defined(WEBRTC_WIN)
}

void PlatformThread::Stop() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!IsRunning())
    return;

#if defined(WEBRTC_WIN)
  if (attributes_.joinable) {
    WaitForSingleObject(thread_, INFINITE);
  }
  CloseHandle(thread_);
  thread_ = nullptr;
  thread_id_ = 0;
#else
  if (attributes_.joinable) {
    RTC_CHECK_EQ(0, pthread_join(thread_, nullptr));
  }
  thread_ = 0;
#endif  // defined(WEBRTC_WIN)
}

#if defined(WEBRTC_WIN)
bool PlatformThread::QueueAPC(PAPCFUNC function, ULONG_PTR data) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(IsRunning());

  return QueueUserAPC(function, thread_, data) != FALSE;
}
#endif

}  // namespace rtc
