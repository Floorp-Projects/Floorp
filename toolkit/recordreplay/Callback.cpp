/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Callback.h"

#include "ipc/ChildInternal.h"
#include "mozilla/Assertions.h"
#include "mozilla/RecordReplay.h"
#include "mozilla/StaticMutex.h"
#include "ProcessRewind.h"
#include "Thread.h"
#include "ValueIndex.h"

namespace mozilla {
namespace recordreplay {

static ValueIndex* gCallbackData;
static StaticMutexNotRecorded gCallbackMutex;

void RegisterCallbackData(void* aData) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  if (!aData) {
    return;
  }

  AutoOrderedAtomicAccess at(&gCallbackData);
  StaticMutexAutoLock lock(gCallbackMutex);
  if (!gCallbackData) {
    gCallbackData = new ValueIndex();
  }
  if (!gCallbackData->Contains(aData)) {
    gCallbackData->Insert(aData);
  }
}

void BeginCallback(size_t aCallbackId) {
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());

  Thread* thread = Thread::Current();
  if (thread->IsMainThread()) {
    js::EndIdleTime();
  }
  thread->SetPassThrough(false);

  RecordingEventSection res(thread);
  MOZ_RELEASE_ASSERT(res.CanAccessEvents());

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::ExecuteCallback);
  thread->Events().WriteScalar(aCallbackId);
}

void EndCallback() {
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());

  Thread* thread = Thread::Current();
  if (thread->IsMainThread()) {
    js::BeginIdleTime();
  }
  thread->SetPassThrough(true);
}

void SaveOrRestoreCallbackData(void** aData) {
  MOZ_RELEASE_ASSERT(gCallbackData);

  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  MOZ_RELEASE_ASSERT(res.CanAccessEvents());

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::RestoreCallbackData);

  size_t index = 0;
  if (IsRecording() && *aData) {
    StaticMutexAutoLock lock(gCallbackMutex);
    index = gCallbackData->GetIndex(*aData);
  }
  thread->Events().RecordOrReplayScalar(&index);

  if (IsReplaying()) {
    *aData = const_cast<void*>(gCallbackData->GetValue(index));
  }
}

void RemoveCallbackData(void* aData) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  StaticMutexAutoLock lock(gCallbackMutex);
  gCallbackData->Remove(aData);
}

void PassThroughThreadEventsAllowCallbacks(const std::function<void()>& aFn) {
  Thread* thread = Thread::Current();
  Maybe<RecordingEventSection> res;

  if (IsRecording()) {
    if (thread->IsMainThread()) {
      js::BeginIdleTime();
    }
    thread->SetPassThrough(true);
    aFn();
    if (thread->IsMainThread()) {
      js::EndIdleTime();
    }
    thread->SetPassThrough(false);

    res.emplace(thread);
    MOZ_RELEASE_ASSERT(res->CanAccessEvents());
    thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CallbacksFinished);
  } else {
    while (true) {
      res.emplace(thread);
      MOZ_RELEASE_ASSERT(res->CanAccessEvents());
      ThreadEvent ev = thread->Events().ReplayThreadEvent();
      if (ev != ThreadEvent::ExecuteCallback) {
        MOZ_RELEASE_ASSERT(ev == ThreadEvent::CallbacksFinished);
        break;
      }
      size_t id = thread->Events().ReadScalar();
      res.reset();
      ReplayInvokeCallback(id);
    }
  }
}

}  // namespace recordreplay
}  // namespace mozilla
