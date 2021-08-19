/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerThreadPlatformData_h
#define ProfilerThreadPlatformData_h

#include "mozilla/ProfilerUtils.h"

#if defined(__APPLE__)
#  include <mach/mach_types.h>
#elif defined(__linux__) || defined(__ANDROID__) || defined(__FreeBSD__)
#  include "mozilla/Maybe.h"
#  include <time.h>
#endif

namespace mozilla::profiler {

class PlatformData {
#if defined(_MSC_VER) || defined(__MINGW32__)
 public:
  explicit PlatformData(ProfilerThreadId aThreadId);
  ~PlatformData();

  // Faking win32's HANDLE, because #including "windows.h" here causes trouble
  // (e.g., it #defines `Yield` as nothing!)
  // This type is static_check'ed against HANDLE in platform-win32.cpp.
  using WindowsHandle = void*;
  WindowsHandle ProfiledThread() const { return mProfiledThread; }

 private:
  WindowsHandle mProfiledThread;
#elif defined(__APPLE__)
 public:
  explicit PlatformData(ProfilerThreadId aThreadId);
  ~PlatformData();
  thread_act_t ProfiledThread() const { return mProfiledThread; }

 private:
  // Note: for mProfiledThread Mach primitives are used instead of pthread's
  // because the latter doesn't provide thread manipulation primitives
  // required. For details, consult "Mac OS X Internals" book, Section 7.3.
  thread_act_t mProfiledThread;
#elif defined(__linux__) || defined(__ANDROID__) || defined(__FreeBSD__)
 public:
  explicit PlatformData(ProfilerThreadId aThreadId);
  ~PlatformData();
  // Clock Id for this profiled thread. `Nothing` if `pthread_getcpuclockid`
  // failed (e.g., if the system doesn't support per-thread clocks).
  Maybe<clockid_t> GetClockId() const { return mClockId; }

 private:
  Maybe<clockid_t> mClockId;
#else
 public:
  explicit PlatformData(ProfilerThreadId aThreadId) {}
#endif
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadPlatformData_h
