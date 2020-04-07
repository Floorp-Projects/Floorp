/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryIPCAccumulator.h"

#include "core/TelemetryHistogram.h"
#include "core/TelemetryScalar.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"

using mozilla::StaticAutoPtr;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::TaskCategory;
using mozilla::Telemetry::ChildEventData;
using mozilla::Telemetry::DiscardedData;
using mozilla::Telemetry::HistogramAccumulation;
using mozilla::Telemetry::KeyedHistogramAccumulation;
using mozilla::Telemetry::KeyedScalarAction;
using mozilla::Telemetry::ScalarAction;
using mozilla::Telemetry::ScalarActionType;
using mozilla::Telemetry::ScalarVariant;

namespace TelemetryIPCAccumulator = mozilla::TelemetryIPCAccumulator;

// To stop growing unbounded in memory while waiting for
// StaticPrefs::toolkit_telemetry_ipcBatchTimeout() milliseconds to drain the
// probe accumulation arrays, we request an immediate flush if the arrays
// manage to reach certain high water mark of elements.
const size_t kHistogramAccumulationsArrayHighWaterMark = 5 * 1024;
const size_t kScalarActionsArrayHighWaterMark = 10000;
// With the current limits, events cost us about 1100 bytes each.
// This limits memory use to about 10MB.
const size_t kEventsArrayHighWaterMark = 10000;
// If we are starved we can overshoot the watermark.
// This is the multiplier over which we will discard data.
const size_t kWaterMarkDiscardFactor = 5;

// Counts of how many pieces of data we have discarded.
DiscardedData gDiscardedData = {0};

// This timer is used for batching and sending child process accumulations to
// the parent.
nsITimer* gIPCTimer = nullptr;
mozilla::Atomic<bool, mozilla::Relaxed> gIPCTimerArmed(false);
mozilla::Atomic<bool, mozilla::Relaxed> gIPCTimerArming(false);

// This batches child process accumulations that should be sent to the parent.
StaticAutoPtr<nsTArray<HistogramAccumulation>> gHistogramAccumulations;
StaticAutoPtr<nsTArray<KeyedHistogramAccumulation>>
    gKeyedHistogramAccumulations;
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

void DoArmIPCTimerMainThread(const StaticMutexAutoLock& lock) {
  MOZ_ASSERT(NS_IsMainThread());
  gIPCTimerArming = false;
  if (gIPCTimerArmed) {
    return;
  }
  if (!gIPCTimer) {
    gIPCTimer = NS_NewTimer().take();
  }
  if (gIPCTimer) {
    gIPCTimer->InitWithNamedFuncCallback(
        TelemetryIPCAccumulator::IPCTimerFired, nullptr,
        mozilla::StaticPrefs::toolkit_telemetry_ipcBatchTimeout(),
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
        "TelemetryIPCAccumulator::IPCTimerFired");
    gIPCTimerArmed = true;
  }
}

void ArmIPCTimer(const StaticMutexAutoLock& lock) {
  if (gIPCTimerArmed || gIPCTimerArming) {
    return;
  }
  gIPCTimerArming = true;
  if (NS_IsMainThread()) {
    DoArmIPCTimerMainThread(lock);
  } else {
    TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction(
        "TelemetryIPCAccumulator::ArmIPCTimer", []() -> void {
          StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
          DoArmIPCTimerMainThread(locker);
        }));
  }
}

void DispatchIPCTimerFired() {
  TelemetryIPCAccumulator::DispatchToMainThread(NS_NewRunnableFunction(
      "TelemetryIPCAccumulator::IPCTimerFired", []() -> void {
        TelemetryIPCAccumulator::IPCTimerFired(nullptr, nullptr);
      }));
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryIPCAccumulator::

void TelemetryIPCAccumulator::AccumulateChildHistogram(
    mozilla::Telemetry::HistogramID aId, uint32_t aSample) {
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  if (!gHistogramAccumulations) {
    gHistogramAccumulations = new nsTArray<HistogramAccumulation>();
  }
  if (gHistogramAccumulations->Length() >=
      kWaterMarkDiscardFactor * kHistogramAccumulationsArrayHighWaterMark) {
    gDiscardedData.mDiscardedHistogramAccumulations++;
    return;
  }
  if (gHistogramAccumulations->Length() ==
      kHistogramAccumulationsArrayHighWaterMark) {
    DispatchIPCTimerFired();
  }
  gHistogramAccumulations->AppendElement(HistogramAccumulation{aId, aSample});
  ArmIPCTimer(locker);
}

void TelemetryIPCAccumulator::AccumulateChildKeyedHistogram(
    mozilla::Telemetry::HistogramID aId, const nsCString& aKey,
    uint32_t aSample) {
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  if (!gKeyedHistogramAccumulations) {
    gKeyedHistogramAccumulations = new nsTArray<KeyedHistogramAccumulation>();
  }
  if (gKeyedHistogramAccumulations->Length() >=
      kWaterMarkDiscardFactor * kHistogramAccumulationsArrayHighWaterMark) {
    gDiscardedData.mDiscardedKeyedHistogramAccumulations++;
    return;
  }
  if (gKeyedHistogramAccumulations->Length() ==
      kHistogramAccumulationsArrayHighWaterMark) {
    DispatchIPCTimerFired();
  }
  gKeyedHistogramAccumulations->AppendElement(
      KeyedHistogramAccumulation{aId, aSample, aKey});
  ArmIPCTimer(locker);
}

void TelemetryIPCAccumulator::RecordChildScalarAction(
    uint32_t aId, bool aDynamic, ScalarActionType aAction,
    const ScalarVariant& aValue) {
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  // Make sure to have the storage.
  if (!gChildScalarsActions) {
    gChildScalarsActions = new nsTArray<ScalarAction>();
  }
  if (gChildScalarsActions->Length() >=
      kWaterMarkDiscardFactor * kScalarActionsArrayHighWaterMark) {
    gDiscardedData.mDiscardedScalarActions++;
    return;
  }
  if (gChildScalarsActions->Length() == kScalarActionsArrayHighWaterMark) {
    DispatchIPCTimerFired();
  }
  // Store the action. The ProcessID will be determined by the receiver.
  gChildScalarsActions->AppendElement(ScalarAction{
      aId, aDynamic, aAction, Some(aValue), Telemetry::ProcessID::Count});
  ArmIPCTimer(locker);
}

void TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
    uint32_t aId, bool aDynamic, const nsAString& aKey,
    ScalarActionType aAction, const ScalarVariant& aValue) {
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
  // Make sure to have the storage.
  if (!gChildKeyedScalarsActions) {
    gChildKeyedScalarsActions = new nsTArray<KeyedScalarAction>();
  }
  if (gChildKeyedScalarsActions->Length() >=
      kWaterMarkDiscardFactor * kScalarActionsArrayHighWaterMark) {
    gDiscardedData.mDiscardedKeyedScalarActions++;
    return;
  }
  if (gChildKeyedScalarsActions->Length() == kScalarActionsArrayHighWaterMark) {
    DispatchIPCTimerFired();
  }
  // Store the action. The ProcessID will be determined by the receiver.
  gChildKeyedScalarsActions->AppendElement(
      KeyedScalarAction{aId, aDynamic, aAction, NS_ConvertUTF16toUTF8(aKey),
                        Some(aValue), Telemetry::ProcessID::Count});
  ArmIPCTimer(locker);
}

void TelemetryIPCAccumulator::RecordChildEvent(
    const mozilla::TimeStamp& timestamp, const nsACString& category,
    const nsACString& method, const nsACString& object,
    const mozilla::Maybe<nsCString>& value,
    const nsTArray<mozilla::Telemetry::EventExtraEntry>& extra) {
  StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);

  if (!gChildEvents) {
    gChildEvents = new nsTArray<ChildEventData>();
  }

  if (gChildEvents->Length() >=
      kWaterMarkDiscardFactor * kEventsArrayHighWaterMark) {
    gDiscardedData.mDiscardedChildEvents++;
    return;
  }

  if (gChildEvents->Length() == kEventsArrayHighWaterMark) {
    DispatchIPCTimerFired();
  }

  // Store the event.
  gChildEvents->AppendElement(ChildEventData{
      timestamp, nsCString(category), nsCString(method), nsCString(object),
      value, nsTArray<mozilla::Telemetry::EventExtraEntry>(extra)});
  ArmIPCTimer(locker);
}

// This method takes the lock only to double-buffer the batched telemetry.
// It releases the lock before calling out to IPC code which can (and does)
// Accumulate (which would deadlock)
template <class TActor>
static void SendAccumulatedData(TActor* ipcActor) {
  // Get the accumulated data and free the storage buffers.
  nsTArray<HistogramAccumulation> histogramsToSend;
  nsTArray<KeyedHistogramAccumulation> keyedHistogramsToSend;
  nsTArray<ScalarAction> scalarsToSend;
  nsTArray<KeyedScalarAction> keyedScalarsToSend;
  nsTArray<ChildEventData> eventsToSend;
  DiscardedData discardedData;

  {
    StaticMutexAutoLock locker(gTelemetryIPCAccumulatorMutex);
    if (gHistogramAccumulations) {
      histogramsToSend.SwapElements(*gHistogramAccumulations);
    }
    if (gKeyedHistogramAccumulations) {
      keyedHistogramsToSend.SwapElements(*gKeyedHistogramAccumulations);
    }
    if (gChildScalarsActions) {
      scalarsToSend.SwapElements(*gChildScalarsActions);
    }
    if (gChildKeyedScalarsActions) {
      keyedScalarsToSend.SwapElements(*gChildKeyedScalarsActions);
    }
    if (gChildEvents) {
      eventsToSend.SwapElements(*gChildEvents);
    }
    discardedData = gDiscardedData;
    gDiscardedData = {0};
  }

  // Send the accumulated data to the parent process.
  MOZ_ASSERT(ipcActor);
  if (histogramsToSend.Length()) {
    mozilla::Unused << NS_WARN_IF(
        !ipcActor->SendAccumulateChildHistograms(histogramsToSend));
  }
  if (keyedHistogramsToSend.Length()) {
    mozilla::Unused << NS_WARN_IF(
        !ipcActor->SendAccumulateChildKeyedHistograms(keyedHistogramsToSend));
  }
  if (scalarsToSend.Length()) {
    mozilla::Unused << NS_WARN_IF(
        !ipcActor->SendUpdateChildScalars(scalarsToSend));
  }
  if (keyedScalarsToSend.Length()) {
    mozilla::Unused << NS_WARN_IF(
        !ipcActor->SendUpdateChildKeyedScalars(keyedScalarsToSend));
  }
  if (eventsToSend.Length()) {
    mozilla::Unused << NS_WARN_IF(
        !ipcActor->SendRecordChildEvents(eventsToSend));
  }
  mozilla::Unused << NS_WARN_IF(
      !ipcActor->SendRecordDiscardedData(discardedData));
}

// To ensure we don't loop IPCTimerFired->AccumulateChild->arm timer, we don't
// unset gIPCTimerArmed until the IPC completes
//
// This function must be called on the main thread, otherwise IPC will fail.
void TelemetryIPCAccumulator::IPCTimerFired(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());

  // Send accumulated data to the correct parent process.
  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Content:
      SendAccumulatedData(mozilla::dom::ContentChild::GetSingleton());
      break;
    case GeckoProcessType_GPU:
      SendAccumulatedData(mozilla::gfx::GPUParent::GetSingleton());
      break;
    case GeckoProcessType_Socket:
      SendAccumulatedData(mozilla::net::SocketProcessChild::GetSingleton());
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported process type");
      break;
  }

  gIPCTimerArmed = false;
}

void TelemetryIPCAccumulator::DeInitializeGlobalState() {
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

void TelemetryIPCAccumulator::DispatchToMainThread(
    already_AddRefed<nsIRunnable>&& aEvent) {
  SchedulerGroup::Dispatch(TaskCategory::Other,
      std::move(aEvent));
}
