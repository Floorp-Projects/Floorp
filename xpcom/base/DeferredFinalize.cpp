/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DeferredFinalize.h"

#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSRuntime.h"

void
mozilla::DeferredFinalize(nsISupports* aSupports)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    // RecordReplayRegisterDeferredFinalizeThing should have been called when
    // the reference on this object was added earlier. Cause the reference to
    // be released soon, at a consistent point in the recording and replay.
    mozilla::recordreplay::ActivateTrigger(aSupports);
  } else {
    rt->DeferredFinalize(aSupports);
  }
}

void
mozilla::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                          DeferredFinalizeFunction aFunc,
                          void* aThing)
{
  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(rt, "Should have a CycleCollectedJSRuntime by now");
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    // As above, cause the finalization action to occur soon, at a consistent
    // point in the recording and replay.
    mozilla::recordreplay::ActivateTrigger(aThing);
  } else {
    rt->DeferredFinalize(aAppendFunc, aFunc, aThing);
  }
}

static void
RecordReplayDeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                             DeferredFinalizeFunction aFunc,
                             void* aThing)
{
  mozilla::recordreplay::UnregisterTrigger(aThing);

  CycleCollectedJSRuntime* rt = CycleCollectedJSRuntime::Get();
  if (aAppendFunc) {
    rt->DeferredFinalize(aAppendFunc, aFunc, aThing);
  } else {
    rt->DeferredFinalize(reinterpret_cast<nsISupports*>(aThing));
  }
}

void
mozilla::RecordReplayRegisterDeferredFinalizeThing(DeferredFinalizeAppendFunction aAppendFunc,
                                                   DeferredFinalizeFunction aFunc,
                                                   void* aThing)
{
  if (mozilla::recordreplay::IsRecordingOrReplaying()) {
    mozilla::recordreplay::RegisterTrigger(aThing,
                                           [=]() {
                                             RecordReplayDeferredFinalize(aAppendFunc,
                                                                          aFunc, aThing);
                                           });
  }
}
