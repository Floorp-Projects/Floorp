/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/source/thread_posix.h"

#include <algorithm>

#include <errno.h>
#include <unistd.h>
#ifdef WEBRTC_LINUX
#include <linux/unistd.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif

#if defined(WEBRTC_BSD) && !defined(__NetBSD__)
#include <pthread_np.h>
#endif

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace {
struct ThreadAttributes {
  ThreadAttributes() { pthread_attr_init(&attr); }
  ~ThreadAttributes() { pthread_attr_destroy(&attr); }
  pthread_attr_t* operator&() { return &attr; }
  pthread_attr_t attr;
};
}  // namespace

int ConvertToSystemPriority(ThreadPriority priority, int min_prio,
                            int max_prio) {
  DCHECK(max_prio - min_prio > 2);
  const int top_prio = max_prio - 1;
  const int low_prio = min_prio + 1;

  switch (priority) {
    case kLowPriority:
      return low_prio;
    case kNormalPriority:
      // The -1 ensures that the kHighPriority is always greater or equal to
      // kNormalPriority.
      return (low_prio + top_prio - 1) / 2;
    case kHighPriority:
      return std::max(top_prio - 2, low_prio);
    case kHighestPriority:
      return std::max(top_prio - 1, low_prio);
    case kRealtimePriority:
      return top_prio;
  }
  DCHECK(false);
  return low_prio;
}

// static
void* ThreadPosix::StartThread(void* param) {
  static_cast<ThreadPosix*>(param)->Run();
  return 0;
}

ThreadPosix::ThreadPosix(ThreadRunFunction func, void* obj,
                         const char* thread_name)
    : run_function_(func),
      obj_(obj),
      stop_event_(false, false),
      name_(thread_name ? thread_name : "webrtc"),
      thread_(0) {
  DCHECK(name_.length() < 64);
}

uint32_t ThreadWrapper::GetThreadId() {
  return rtc::CurrentThreadId();
}

ThreadPosix::~ThreadPosix() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// TODO(pbos): Make Start void, calling code really doesn't support failures
// here.
bool ThreadPosix::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!thread_) << "Thread already started?";

  ThreadAttributes attr;
  // Set the stack stack size to 1M.
  pthread_attr_setstacksize(&attr, 1024 * 1024);
  CHECK_EQ(0, pthread_create(&thread_, &attr, &StartThread, this));
  return true;
}

bool ThreadPosix::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_)
    return true;

  stop_event_.Set();
  CHECK_EQ(0, pthread_join(thread_, nullptr));
  thread_ = 0;

  return true;
}

bool ThreadPosix::SetPriority(ThreadPriority priority) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_)
    return false;
#if defined(WEBRTC_CHROMIUM_BUILD) && defined(WEBRTC_LINUX)
  // TODO(tommi): Switch to the same mechanism as Chromium uses for
  // changing thread priorities.
  return true;
#else
#ifdef WEBRTC_THREAD_RR
  const int policy = SCHED_RR;
#else
  const int policy = SCHED_FIFO;
#endif
  const int min_prio = sched_get_priority_min(policy);
  const int max_prio = sched_get_priority_max(policy);
  if (min_prio == -1 || max_prio == -1) {
    WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                 "unable to retreive min or max priority for threads");
    return false;
  }

  if (max_prio - min_prio <= 2)
    return false;

  sched_param param;
  param.sched_priority = ConvertToSystemPriority(priority, min_prio, max_prio);
  if (pthread_setschedparam(thread_, policy, &param) != 0) {
    WEBRTC_TRACE(
        kTraceError, kTraceUtility, -1, "unable to set thread priority");
    return false;
  }

  return true;
#endif  // defined(WEBRTC_CHROMIUM_BUILD) && defined(WEBRTC_LINUX)
}

void ThreadPosix::Run() {
  if (!name_.empty()) {
    // Setting the thread name may fail (harmlessly) if running inside a
    // sandbox. Ignore failures if they happen.
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_ANDROID) || defined(WEBRTC_GONK))
    prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name_.c_str()));
#elif defined(__NetBSD__)
    pthread_setname_np(pthread_self(), "%s", (void *)name_.c_str());
#elif defined(WEBRTC_BSD)
    pthread_set_name_np(pthread_self(), name_.c_str());
#elif defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
    pthread_setname_np(name_.substr(0, 63).c_str());
#endif
  }

  // It's a requirement that for successful thread creation that the run
  // function be called at least once (see RunFunctionIsCalled unit test),
  // so to fullfill that requirement, we use a |do| loop and not |while|.
  do {
    if (!run_function_(obj_))
      break;
  } while (!stop_event_.Wait(0));
}

}  // namespace webrtc
