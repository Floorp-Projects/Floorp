/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_IMPL_H
#define GECKO_TASK_TRACER_IMPL_H

#include "GeckoTaskTracer.h"
#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla {
namespace tasktracer {

struct LogRecDispatch {
  uint32_t mType;
  uint32_t mSourceEventType;
  uint64_t mTaskId;
  uint64_t mTime;
  uint64_t mSourceEventId;
  uint64_t mParentTaskId;
};

struct LogRecBegin {
  uint32_t mType;
  uint64_t mTaskId;
  uint64_t mTime;
  uint32_t mPid;
  uint32_t mTid;
};

struct LogRecEnd {
  uint32_t mType;
  uint64_t mTaskId;
  uint64_t mTime;
};

struct LogRecVPtr {
  uint32_t mType;
  uint64_t mTaskId;
  uintptr_t mVPtr;
};

struct LogRecLabel {
  uint32_t mType;
  uint32_t mStrIdx;
  uint64_t mTaskId;
  uint64_t mTime;
};

union TraceInfoLogType {
  uint32_t mType;
  LogRecDispatch mDispatch;
  LogRecBegin mBegin;
  LogRecEnd mEnd;
  LogRecVPtr mVPtr;
  LogRecLabel mLabel;
};

struct TraceInfoLogNode {
  TraceInfoLogType mLog;
  TraceInfoLogNode* mNext;
};

struct TraceInfo
{
  TraceInfo(uint32_t aThreadId)
    : mCurTraceSourceId(0)
    , mCurTaskId(0)
    , mCurTraceSourceType(Unknown)
    , mThreadId(aThreadId)
    , mLastUniqueTaskId(0)
    , mObsolete(false)
    , mLogsMutex("TraceInfoMutex")
    , mLogsHead(nullptr)
    , mLogsTail(nullptr)
    , mLogsSize(0)
  {
    MOZ_COUNT_CTOR(TraceInfo);
  }

  ~TraceInfo() {
    MOZ_COUNT_DTOR(TraceInfo);
    while (mLogsHead) {
      auto node = mLogsHead;
      mLogsHead = node->mNext;
      delete node;
    }
  }

  TraceInfoLogType* AppendLog();

  uint64_t mCurTraceSourceId;
  uint64_t mCurTaskId;
  SourceEventType mCurTraceSourceType;
  uint32_t mThreadId;
  uint32_t mLastUniqueTaskId;
  mozilla::Atomic<bool> mObsolete;

  // This mutex protects the following log
  mozilla::Mutex mLogsMutex;
  TraceInfoLogNode* mLogsHead;
  TraceInfoLogNode* mLogsTail;
  int mLogsSize;
  nsTArray<nsCString> mStrs;
};

// Return the TraceInfo of current thread, allocate a new one if not exit.
TraceInfo* GetOrCreateTraceInfo();

uint64_t GenNewUniqueTaskId();

void SetCurTraceInfo(uint64_t aSourceEventId, uint64_t aParentTaskId,
                     SourceEventType aSourceEventType);

/**
 * Logging functions of different trace actions.
 */
enum ActionType {
  ACTION_DISPATCH = 0,
  ACTION_BEGIN,
  ACTION_END,
  ACTION_ADD_LABEL,
  ACTION_GET_VTABLE
};

void LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId,
                 uint64_t aSourceEventId, SourceEventType aSourceEventType);

void LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId,
                 uint64_t aSourceEventId, SourceEventType aSourceEventType,
                 int aDelayTimeMs);

void LogBegin(uint64_t aTaskId, uint64_t aSourceEventId);

void LogEnd(uint64_t aTaskId, uint64_t aSourceEventId);

void LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, uintptr_t* aVptr);

} // namespace mozilla
} // namespace tasktracer

#endif
