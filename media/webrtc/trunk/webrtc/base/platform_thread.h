/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_PLATFORM_THREAD_H_
#define WEBRTC_BASE_PLATFORM_THREAD_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/event.h"
#include "webrtc/base/platform_thread_types.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"

namespace rtc {

PlatformThreadId CurrentThreadId();
PlatformThreadRef CurrentThreadRef();

// Compares two thread identifiers for equality.
bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b);

// Sets the current thread name.
void SetCurrentThreadName(const char* name);

// Callback function that the spawned thread will enter once spawned.
// A return value of false is interpreted as that the function has no
// more work to do and that the thread can be released.
typedef bool (*ThreadRunFunction)(void*);

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

// Represents a simple worker thread.  The implementation must be assumed
// to be single threaded, meaning that all methods of the class, must be
// called from the same thread, including instantiation.
class PlatformThread {
 public:
  PlatformThread(ThreadRunFunction func, void* obj, const char* thread_name);
  virtual ~PlatformThread();

  // Spawns a thread and tries to set thread priority according to the priority
  // from when CreateThread was called.
  void Start();

  bool IsRunning() const;

  // Stops (joins) the spawned thread.
  void Stop();

  // Set the priority of the thread. Must be called when thread is running.
  bool SetPriority(ThreadPriority priority);

 private:
  void Run();

  ThreadRunFunction const run_function_;
  void* const obj_;
  // TODO(pbos): Make sure call sites use string literals and update to a const
  // char* instead of a std::string.
  const std::string name_;
  rtc::ThreadChecker thread_checker_;
#if defined(WEBRTC_WIN)
  static DWORD WINAPI StartThread(void* param);

  bool stop_;
  HANDLE thread_;
#else
  static void* StartThread(void* param);

  rtc::Event stop_event_;

  pthread_t thread_;
#endif  // defined(WEBRTC_WIN)
  RTC_DISALLOW_COPY_AND_ASSIGN(PlatformThread);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_PLATFORM_THREAD_H_
