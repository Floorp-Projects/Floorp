/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryIPCAccumulator.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "TelemetryHistogram.h"
#include "TelemetryScalar.h"

using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::StaticAutoPtr;
using mozilla::Telemetry::Accumulation;
using mozilla::Telemetry::KeyedAccumulation;
using mozilla::Telemetry::ScalarActionType;
using mozilla::Telemetry::ScalarAction;
using mozilla::Telemetry::KeyedScalarAction;
using mozilla::Telemetry::ScalarVariant;
using mozilla::Telemetry::ChildEventData;

namespace TelemetryIPCAccumulator = mozilla::TelemetryIPCAccumulator;

// Sending each remote accumulation immediately places undue strain on the
// IPC subsystem. Batch the remote accumulations for a period of time before
// sending them all at once. This value was chosen as a balance between data
// timeliness and performance (see bug 1218576)
const uint32_t kBatchTimeoutMs = 2000;

// To stop growing unbounded in memory while waiting for kBatchTimeoutMs to
// drain the probe accumulation arrays, we request an immediate flush if the
// arrays manage to reach certain high water mark of elements.
const size_t kHistogramAccumulationsArrayHighWaterMark = 5 * 1024;
const size_t kScalarActionsArrayHighWaterMark = 10000;
// With the current limits, events cost us about 1100 bytes each.
// This limits memory use to about 10MB.
const size_t kEventsArrayHighWaterMark = 10000;

// This timer is used for batching and sending child process accumulations to the parent.
nsITimer* gIPCTimer = nullptr;
mozilla::Atomic<bool, mozilla::Relaxed> gIPCTimerArmed(false);
mozilla::Atomic<bool, mozilla::Relaxed> gIPCTimerArming(false);

// This batches child process accumulations that should be sent to the parent.
StaticAutoPtr<nsTArray<Accumulation>> gHistogramAccumulations;
StaticAutoPtr<nsTArray<KeyedAccumulation>> gKeyedHistogramAccumulations;
StaticAutoPtr<nsTArray<ScalarAction>> gChildScalarsActions;
StaticAutoPtr<nsTArray<KeyedScalarAction>> gChildKeyedScalarsActions;
StaticAutoPtr<nsTArray<ChildEventData>> gChildEvents;

// This is a StaticMutex rather than a plain Mutex so that (1)
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
static StaticMutex gTelemetryIPCAccumulatorMutex;

namespace {

void
DoArmIPCTimerMainThread(const StaticMutexAutoLock& lock)
{
  MOZ_ASSERT(NS_IsMainThread());
  gIPCTimerArming = false;
  if (gIPCTimerArmed) {
    return;
  }
  if (!gIPCTimer) {
    CallCreateInstance(NS_TIMER_CONTRACTID, &gIPCTimer);
  }
  if (gIPCTimer) {
    gIPCTimer->InitWithNamedFuncCallback(TelemetryIPCAccumulator::IPCTimerFired,
                                         nullptr, kBatchTimeoutMs,
                                         nsITimer::TYPE_ONE_SHOT,
                                         "TelemetryIPCAccumulator::IPCTimerFired");
    gIPCTimerArmed = true;
  }
}

void
ArmIPCTimer(const StaticMutexAutoLock& lock)
{
  if (gIPCTimerArmed || gIPCTimerArming) {
    return;
  }
  gIPCTimerArming = true;
  if (NS_IsMainThread()) {
    DoArmIPCTimerMainThread(lock);
  } else {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction(
                                                    "TelemetryIPCAccumulator::ArmIPCTimer",
                                                    []() -> void {
      StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
      DoArmIPCTimerMainThread(locker);
    }));
  }
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryIPCAccumulator::

void
TelemetryIPCAccumulator::AccumulateChildHistogram(mozilla::Telemetry::HistogramID aId,
                                                  uint32_t aSample)
{
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  if (!gHistogramAccumulations) {
    gHistogramAccumulations = new nsTArray<Accumulation>();
  }
  if (gHistogramAccumulations->Length() == kHistogramAccumulationsArrayHighWaterMark) {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
    }));
  }
  gHistogramAccumulations->AppendElement(Accumulation{aId, aSample});
  ArmIPCTimer(locker);
}

void
TelemetryIPCAccumulator::AccumulateChildKeyedHistogram(mozilla::Telemetry::HistogramID aId,
                                                       const nsCString& aKey, uint32_t aSample)
{
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  if (!gKeyedHistogramAccumulations) {
    gKeyedHistogramAccumulations = new nsTArray<KeyedAccumulation>();
  }
  if (gKeyedHistogramAccumulations->Length() == kHistogramAccumulationsArrayHighWaterMark) {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
    }));
  }
  gKeyedHistogramAccumulations->AppendElement(KeyedAccumulation{aId, aSample, aKey});
  ArmIPCTimer(locker);
}

void
TelemetryIPCAccumulator::RecordChildScalarAction(mozilla::Telemetry::ScalarID aId,
                                                 ScalarActionType aAction, const ScalarVariant& aValue)
{
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  // Make sure to have the storage.
  if (!gChildScalarsActions) {
    gChildScalarsActions = new nsTArray<ScalarAction>();
  }
  if (gChildScalarsActions->Length() == kScalarActionsArrayHighWaterMark) {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
    }));
  }
  // Store the action.
  gChildScalarsActions->AppendElement(ScalarAction{aId, aAction, Some(aValue)});
  ArmIPCTimer(locker);
}

void
TelemetryIPCAccumulator::RecordChildKeyedScalarAction(mozilla::Telemetry::ScalarID aId,
                                                      const nsAString& aKey,
                                                      ScalarActionType aAction,
                                                      const ScalarVariant& aValue)
{
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  // Make sure to have the storage.
  if (!gChildKeyedScalarsActions) {
    gChildKeyedScalarsActions = new nsTArray<KeyedScalarAction>();
  }
  if (gChildKeyedScalarsActions->Length() == kScalarActionsArrayHighWaterMark) {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
    }));
  }
  // Store the action.
  gChildKeyedScalarsActions->AppendElement(
    KeyedScalarAction{aId, aAction, NS_ConvertUTF16toUTF8(aKey), Some(aValue)});
  ArmIPCTimer(locker);
}

void
TelemetryIPCAccumulator::RecordChildEvent(const mozilla::TimeStamp& timestamp,
                                          const nsACString& category,
                                          const nsACString& method,
                                          const nsACString& object,
                                          const mozilla::Maybe<nsCString>& value,
                                          const nsTArray<mozilla::Telemetry::EventExtraEntry>& extra)
{
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);

  if (!gChildEvents) {
    gChildEvents = new nsTArray<ChildEventData>();
  }

  if (gChildEvents->Length() == kEventsArrayHighWaterMark) {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction([]() -> void {
      TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
    }));
  }

  // Store the event.
  gChildEvents->AppendElement(ChildEventData{timestamp, nsCString(category),
                                             nsCString(method), nsCString(object),
                                             value,
                                             nsTArray<mozilla::Telemetry::EventExtraEntry>(extra)});
  ArmIPCTimer(locker);
}

// This method takes the lock only to double-buffer the batched telemetry.
// It releases the lock before calling out to IPC code which can (and does)
// Accumulate (which would deadlock)
template<class TActor>
static void
SendAccumulatedData(TActor* ipcActor)
{
  // Get the accumulated data and free the storage buffers.
  nsTArray<Accumulation> accumulationsToSend;
  nsTArray<KeyedAccumulation> keyedAccumulationsToSend;
  nsTArray<ScalarAction> scalarsToSend;
  nsTArray<KeyedScalarAction> keyedScalarsToSend;
  nsTArray<ChildEventData> eventsToSend;

  {
    StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
    if (gHistogramAccumulations) {
      accumulationsToSend.SwapElements(*gHistogramAccumulations);
    }
    if (gKeyedHistogramAccumulations) {
      keyedAccumulationsToSend.SwapElements(*gKeyedHistogramAccumulations);
    }
    // Copy the scalar actions.
    if (gChildScalarsActions) {
      scalarsToSend.SwapElements(*gChildScalarsActions);
    }
    if (gChildKeyedScalarsActions) {
      keyedScalarsToSend.SwapElements(*gChildKeyedScalarsActions);
    }
    if (gChildEvents) {
      eventsToSend.SwapElements(*gChildEvents);
    }
  }

  // Send the accumulated data to the parent process.
  mozilla::Unused << NS_WARN_IF(!ipcActor);
  if (accumulationsToSend.Length()) {
    mozilla::Unused <<
      NS_WARN_IF(!ipcActor->SendAccumulateChildHistograms(accumulationsToSend));
  }
  if (keyedAccumulationsToSend.Length()) {
    mozilla::Unused <<
      NS_WARN_IF(!ipcActor->SendAccumulateChildKeyedHistograms(keyedAccumulationsToSend));
  }
  if (scalarsToSend.Length()) {
    mozilla::Unused <<
      NS_WARN_IF(!ipcActor->SendUpdateChildScalars(scalarsToSend));
  }
  if (keyedScalarsToSend.Length()) {
    mozilla::Unused <<
      NS_WARN_IF(!ipcActor->SendUpdateChildKeyedScalars(keyedScalarsToSend));
  }
  if (eventsToSend.Length()) {
    mozilla::Unused <<
      NS_WARN_IF(!ipcActor->SendRecordChildEvents(eventsToSend));
  }
}


// To ensure we don't loop IPCTimerFired->AccumulateChild->arm timer, we don't
// unset gIPCTimerArmed until the IPC completes
//
// This function must be called on the main thread, otherwise IPC will fail.
void
TelemetryIPCAccumulator::IPCTimerFired(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Send accumulated data to the correct parent process.
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content:
      SendAccumulatedData(mozilla::dom::ContentChild::GetSingleton());
      break;
    case GeckoProcessType_GPU:
      SendAccumulatedData(mozilla::gfx::GPUParent::GetSingleton());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported process type");
      break;
  }

  gIPCTimerArmed = false;
}

void
TelemetryIPCAccumulator::DeInitializeGlobalState()
{
  MOZ_ASSERT(NS_IsMainThread());

  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  if (gIPCTimer) {
    NS_RELEASE(gIPCTimer);
  }

  gHistogramAccumulations = nullptr;
  gKeyedHistogramAccumulations = nullptr;
  gChildScalarsActions = nullptr;
  gChildKeyedScalarsActions = nullptr;
  gChildEvents = nullptr;
}

void
TelemetryIPCAccumulator::DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    NS_WARNING("NS_FAILED DispatchToMainThread. Maybe we're shutting down?");
    return;
  }
  thread->Dispatch(event, 0);
}
