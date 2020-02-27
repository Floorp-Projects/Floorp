/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThreadInfo_h
#define ThreadInfo_h

#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace baseprofiler {

// This class contains information about a thread which needs to be stored
// across restarts of the profiler and which can be useful even after the
// thread has stopped running.
// It uses threadsafe refcounting and only contains immutable data.
class ThreadInfo final {
 public:
  ThreadInfo(const char* aName, int aThreadId, bool aIsMainThread,
             const TimeStamp& aRegisterTime = TimeStamp::NowUnfuzzed())
      : mName(aName),
        mRegisterTime(aRegisterTime),
        mThreadId(aThreadId),
        mIsMainThread(aIsMainThread),
        mRefCnt(0) {
    // I don't know if we can assert this. But we should warn.
    MOZ_ASSERT(aThreadId >= 0, "native thread ID is < 0");
    MOZ_ASSERT(aThreadId <= INT32_MAX, "native thread ID is > INT32_MAX");
  }

  // Using hand-rolled ref-counting, because RefCounted.h macros don't produce
  // the same code between mozglue and libxul, see bug 1536656.
  MFBT_API void AddRef() const { ++mRefCnt; }
  MFBT_API void Release() const {
    MOZ_ASSERT(int32_t(mRefCnt) > 0);
    if (--mRefCnt == 0) {
      delete this;
    }
  }

  const char* Name() const { return mName.c_str(); }
  TimeStamp RegisterTime() const { return mRegisterTime; }
  int ThreadId() const { return mThreadId; }
  bool IsMainThread() const { return mIsMainThread; }

 private:
  const std::string mName;
  const TimeStamp mRegisterTime;
  const int mThreadId;
  const bool mIsMainThread;

  mutable Atomic<int32_t, MemoryOrdering::ReleaseAcquire,
                 recordreplay::Behavior::DontPreserve>
      mRefCnt;
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // ThreadInfo_h
