/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PLATFORM_THREAD_H_
#define RTC_BASE_PLATFORM_THREAD_H_

#ifndef WEBRTC_WIN
#include <pthread.h>
#endif
#include <string>

#include "absl/strings/string_view.h"
#include "api/sequence_checker.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/platform_thread_types.h"

#include "rtc_base/deprecated/recursive_critical_section.h"

namespace rtc {

// Bug 1691641
class PlatformUIThread;

// Callback function that the spawned thread will enter once spawned.
typedef void (*ThreadRunFunction)(void*);

enum ThreadPriority {
#ifdef WEBRTC_WIN
  kLowPriority = THREAD_PRIORITY_BELOW_NORMAL,
  kNormalPriority = THREAD_PRIORITY_NORMAL,
  kHighPriority = THREAD_PRIORITY_ABOVE_NORMAL,
  kHighestPriority = THREAD_PRIORITY_HIGHEST,
  kRealtimePriority = THREAD_PRIORITY_TIME_CRITICAL
#else
  kLowPriority = 1,
  kNormalPriority = 2,
  kHighPriority = 3,
  kHighestPriority = 4,
  kRealtimePriority = 5
#endif
};

struct ThreadAttributes {
  ThreadPriority priority = kNormalPriority;
  bool joinable = true;

  ThreadAttributes& SetPriority(ThreadPriority priority_param) {
    priority = priority_param;
    return *this;
  }
  ThreadAttributes& SetDetached() {
    joinable = false;
    return *this;
  }
};

// Represents a simple worker thread.  The implementation must be assumed
// to be single threaded, meaning that all methods of the class, must be
// called from the same thread, including instantiation.
class PlatformThread {
 public:
  PlatformThread(ThreadRunFunction func,
                 void* obj,
                 absl::string_view thread_name,
                 ThreadAttributes attributes = ThreadAttributes());
  virtual ~PlatformThread();

  const std::string& name() const { return name_; }

  // Spawns a thread and tries to set thread priority according to the priority
  // from when CreateThread was called.
  // Start can only be called after the constructor or after a call to Stop().
  virtual void Start();

  bool IsRunning() const;

  // Returns an identifier for the worker thread that can be used to do
  // thread checks.
  PlatformThreadRef GetThreadRef() const;

  // Stop() prepares the PlatformThread for destruction or another call to
  // Start(). For a PlatformThread that's been created with
  // ThreadAttributes::joinable true (the default), Stop() suspends the calling
  // thread until the created thread exits unless the thread has already exited.
  // Stop() can only be called after calling Start().
  virtual void Stop();

 protected:
#if defined(WEBRTC_WIN)
  // Exposed to derived classes to allow for special cases specific to Windows.
  bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data);
#endif

 private:
  ThreadRunFunction const run_function_ = nullptr;
  const ThreadAttributes attributes_;
  void* const obj_;
  // TODO(pbos): Make sure call sites use string literals and update to a const
  // char* instead of a std::string.
  const std::string name_;
  webrtc::SequenceChecker thread_checker_;
#if defined(WEBRTC_WIN)
  HANDLE thread_ = nullptr;
  DWORD thread_id_ = 0;
  RecursiveCriticalSection cs_;
#else
  pthread_t thread_ = 0;
#endif  // defined(WEBRTC_WIN)
  // Bug 1691641
  friend PlatformUIThread;
  RTC_DISALLOW_COPY_AND_ASSIGN(PlatformThread);
};

}  // namespace rtc

#endif  // RTC_BASE_PLATFORM_THREAD_H_
