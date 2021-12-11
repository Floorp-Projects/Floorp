/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryIPCAccumulator_h__
#define TelemetryIPCAccumulator_h__

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Maybe.h"
#include "nsStringFwd.h"
#include "TelemetryComms.h"

class nsIRunnable;
class nsITimer;

namespace mozilla {

class TimeStamp;

namespace TelemetryIPCAccumulator {

// Histogram accumulation functions.
void AccumulateChildHistogram(mozilla::Telemetry::HistogramID aId,
                              uint32_t aSample);
void AccumulateChildKeyedHistogram(mozilla::Telemetry::HistogramID aId,
                                   const nsCString& aKey, uint32_t aSample);

// Scalar accumulation functions.
void RecordChildScalarAction(uint32_t aId, bool aDynamic,
                             mozilla::Telemetry::ScalarActionType aAction,
                             const mozilla::Telemetry::ScalarVariant& aValue);

void RecordChildKeyedScalarAction(
    uint32_t aId, bool aDynamic, const nsAString& aKey,
    mozilla::Telemetry::ScalarActionType aAction,
    const mozilla::Telemetry::ScalarVariant& aValue);

void RecordChildEvent(
    const mozilla::TimeStamp& timestamp, const nsACString& category,
    const nsACString& method, const nsACString& object,
    const mozilla::Maybe<nsCString>& value,
    const nsTArray<mozilla::Telemetry::EventExtraEntry>& extra);

void IPCTimerFired(nsITimer* aTimer, void* aClosure);

void DeInitializeGlobalState();

void DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent);

}  // namespace TelemetryIPCAccumulator
}  // namespace mozilla

#endif  // TelemetryIPCAccumulator_h__
