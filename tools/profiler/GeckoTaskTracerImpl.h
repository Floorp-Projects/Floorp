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

typedef nsTArray<nsCString> TraceInfoLogsType;

struct TraceInfo
{
  TraceInfo(uint32_t aThreadId, bool aStartLogging)
    : mCurTraceSourceId(0)
    , mCurTaskId(0)
    , mSavedCurTraceSourceId(0)
    , mSavedCurTaskId(0)
    , mCurTraceSourceType(UNKNOWN)
    , mSavedCurTraceSourceType(UNKNOWN)
    , mThreadId(aThreadId)
    , mLastUniqueTaskId(0)
    , mStartLogging(aStartLogging)
    , mLogsMutex("TraceInfoMutex")
  {
    MOZ_COUNT_CTOR(TraceInfo);
  }

  ~TraceInfo() { MOZ_COUNT_DTOR(TraceInfo); }

  nsCString* AppendLog();
  void MoveLogsInto(TraceInfoLogsType& aResult);

  uint64_t mCurTraceSourceId;
  uint64_t mCurTaskId;
  uint64_t mSavedCurTraceSourceId;
  uint64_t mSavedCurTaskId;
  SourceEventType mCurTraceSourceType;
  SourceEventType mSavedCurTraceSourceType;
  uint32_t mThreadId;
  uint32_t mLastUniqueTaskId;
  bool mStartLogging;

  // This mutex protects the following log array because MoveLogsInto() might
  // be called on another thread.
  mozilla::Mutex mLogsMutex;
  TraceInfoLogsType mLogs;
};

// Return the TraceInfo of current thread, allocate a new one if not exit.
TraceInfo* GetOrCreateTraceInfo();

uint64_t GenNewUniqueTaskId();

class AutoSaveCurTraceInfo
{
public:
  AutoSaveCurTraceInfo();
  ~AutoSaveCurTraceInfo();
};

void SetCurTraceInfo(uint64_t aSourceEventId, uint64_t aParentTaskId,
                     SourceEventType aSourceEventType);

void GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                     SourceEventType* aOutSourceEventType);

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

void LogBegin(uint64_t aTaskId, uint64_t aSourceEventId);

void LogEnd(uint64_t aTaskId, uint64_t aSourceEventId);

void LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr);

} // namespace mozilla
} // namespace tasktracer

#endif
