/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Callback.h"

#include "ipc/ChildIPC.h"
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

void
RegisterCallbackData(void* aData)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  if (!aData) {
    return;
  }

  RecordReplayAssert("RegisterCallbackData");

  AutoOrderedAtomicAccess at;
  StaticMutexAutoLock lock(gCallbackMutex);
  if (!gCallbackData) {
    gCallbackData = new ValueIndex();
  }
  gCallbackData->Insert(aData);
}

void
BeginCallback(size_t aCallbackId)
{
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());

  Thread* thread = Thread::Current();
  if (thread->IsMainThread()) {
    child::EndIdleTime();
  }
  thread->SetPassThrough(false);

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::ExecuteCallback);
  thread->Events().WriteScalar(aCallbackId);
}

void
EndCallback()
{
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());

  Thread* thread = Thread::Current();
  if (thread->IsMainThread()) {
    child::BeginIdleTime();
  }
  thread->SetPassThrough(true);
}

void
SaveOrRestoreCallbackData(void** aData)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());
  MOZ_RELEASE_ASSERT(gCallbackData);

  Thread* thread = Thread::Current();

  RecordReplayAssert("RestoreCallbackData");

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

void
RemoveCallbackData(void* aData)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  StaticMutexAutoLock lock(gCallbackMutex);
  gCallbackData->Remove(aData);
}

void
PassThroughThreadEventsAllowCallbacks(const std::function<void()>& aFn)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());

  Thread* thread = Thread::Current();

  if (IsRecording()) {
    if (thread->IsMainThread()) {
      child::BeginIdleTime();
    }
    thread->SetPassThrough(true);
    aFn();
    if (thread->IsMainThread()) {
      child::EndIdleTime();
    }
    thread->SetPassThrough(false);
    thread->Events().RecordOrReplayThreadEvent(ThreadEvent::CallbacksFinished);
  } else {
    while (true) {
      ThreadEvent ev = (ThreadEvent) thread->Events().ReadScalar();
      if (ev != ThreadEvent::ExecuteCallback) {
        if (ev != ThreadEvent::CallbacksFinished) {
          child::ReportFatalError("Unexpected event while replaying callback events");
        }
        break;
      }
      size_t id = thread->Events().ReadScalar();
      ReplayInvokeCallback(id);
    }
  }
}

} // namespace recordreplay
} // namespace mozilla
