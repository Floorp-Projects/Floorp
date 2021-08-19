/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThreadInfo_h
#define ThreadInfo_h

#include "mozilla/ProfilerUtils.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

// This class contains information about a thread which needs to be stored
// across restarts of the profiler and which can be useful even after the
// thread has stopped running.
// It uses threadsafe refcounting and only contains immutable data.
class ThreadInfo final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ThreadInfo)

  ThreadInfo(
      const char* aName, ProfilerThreadId aThreadId, bool aIsMainThread,
      const mozilla::TimeStamp& aRegisterTime = mozilla::TimeStamp::Now())
      : mName(aName),
        mRegisterTime(aRegisterTime),
        mThreadId(aThreadId),
        mIsMainThread(aIsMainThread) {}

  const char* Name() const { return mName.get(); }
  mozilla::TimeStamp RegisterTime() const { return mRegisterTime; }
  ProfilerThreadId ThreadId() const { return mThreadId; }
  bool IsMainThread() const { return mIsMainThread; }

 private:
  ~ThreadInfo() {}

  const nsCString mName;
  const mozilla::TimeStamp mRegisterTime;
  const ProfilerThreadId mThreadId;
  const bool mIsMainThread;
};

#endif  // ThreadInfo_h
