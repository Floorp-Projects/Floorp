/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// System independant wrapper for spawning threads
// Note: the spawned thread will loop over the callback function until stopped.
// Note: The callback function is expected to return every 2 seconds or more
// often.

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_THREAD_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_THREAD_WRAPPER_H_

#if defined(WEBRTC_WIN)
#include <windows.h>
#endif

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Callback function that the spawned thread will enter once spawned.
// A return value of false is interpreted as that the function has no
// more work to do and that the thread can be released.
typedef bool(*ThreadRunFunction)(void*);

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
// TODO(tommi): There's no need for this to be a virtual interface since there's
// only ever a single implementation of it.
class ThreadWrapper {
 public:
  virtual ~ThreadWrapper() {}

  // Factory method. Constructor disabled.
  //
  // func        Pointer to a, by user, specified callback function.
  // obj         Object associated with the thread. Passed in the callback
  //             function.
  // prio        Thread priority. May require root/admin rights.
  // thread_name  NULL terminated thread name, will be visable in the Windows
  //             debugger.
  static rtc::scoped_ptr<ThreadWrapper> CreateThread(ThreadRunFunction func,
      void* obj, const char* thread_name);

  // Get the current thread's thread ID.
  // NOTE: This is a static method. It returns the id of the calling thread,
  // *not* the id of the worker thread that a ThreadWrapper instance represents.
  // TODO(tommi): Move outside of the ThreadWrapper class to avoid confusion.
  static uint32_t GetThreadId();

  // Tries to spawns a thread and returns true if that was successful.
  // Additionally, it tries to set thread priority according to the priority
  // from when CreateThread was called. However, failure to set priority will
  // not result in a false return value.
  virtual bool Start() = 0;

  // Stops the spawned thread and waits for it to be reclaimed with a timeout
  // of two seconds. Will return false if the thread was not reclaimed.
  // Multiple tries to Stop are allowed (e.g. to wait longer than 2 seconds).
  // It's ok to call Stop() even if the spawned thread has been reclaimed.
  virtual bool Stop() = 0;

  // Set the priority of the worker thread.  Must be called when thread
  // is running.
  virtual bool SetPriority(ThreadPriority priority) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_THREAD_WRAPPER_H_
