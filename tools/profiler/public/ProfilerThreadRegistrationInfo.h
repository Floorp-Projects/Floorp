/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerThreadRegistrationInfo_h
#define ProfilerThreadRegistrationInfo_h

#include "mozilla/ProfilerUtils.h"
#include "mozilla/TimeStamp.h"

#include <string>

namespace mozilla::profiler {

// This class contains immutable information about a thread which needs to be
// stored across restarts of the profiler and which can be useful even after the
// thread has stopped running.
class ThreadRegistrationInfo {
 public:
  // Construct on the thread.
  explicit ThreadRegistrationInfo(const char* aName) : mName(aName) {}

  // Construct for a foreign thread (e.g., Java).
  ThreadRegistrationInfo(const char* aName, ProfilerThreadId aThreadId,
                         bool aIsMainThread, const TimeStamp& aRegisterTime)
      : mName(aName),
        mRegisterTime(aRegisterTime),
        mThreadId(aThreadId),
        mIsMainThread(aIsMainThread) {}

  // Only allow move construction, for extraction when the thread ends.
  ThreadRegistrationInfo(ThreadRegistrationInfo&&) = default;

  // Other copies/moves disallowed.
  ThreadRegistrationInfo(const ThreadRegistrationInfo&) = delete;
  ThreadRegistrationInfo& operator=(const ThreadRegistrationInfo&) = delete;
  ThreadRegistrationInfo& operator=(ThreadRegistrationInfo&&) = delete;

  [[nodiscard]] const char* Name() const { return mName.c_str(); }
  [[nodiscard]] const TimeStamp& RegisterTime() const { return mRegisterTime; }
  [[nodiscard]] ProfilerThreadId ThreadId() const { return mThreadId; }
  [[nodiscard]] bool IsMainThread() const { return mIsMainThread; }

 private:
  const std::string mName;
  const TimeStamp mRegisterTime = TimeStamp::Now();
  const ProfilerThreadId mThreadId = profiler_current_thread_id();
  const bool mIsMainThread = profiler_is_main_thread();
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadRegistrationInfo_h
