/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryIPCAccumulator_h__
#define TelemetryIPCAccumulator_h__

#include "mozilla/AlreadyAddRefed.h"

class nsIRunnable;
class nsITimer;
class nsAString;
class nsCString;
class nsIVariant;

namespace mozilla {
namespace Telemetry {

enum ID : uint32_t;
enum class ScalarID : uint32_t;
enum class ScalarActionType : uint32_t;

} // Telemetry
} // mozilla

namespace TelemetryIPCAccumulator {

// Histogram accumulation functions.
void AccumulateChildHistogram(mozilla::Telemetry::ID aId, uint32_t aSample);
void AccumulateChildKeyedHistogram(mozilla::Telemetry::ID aId, const nsCString& aKey,
                                   uint32_t aSample);

// Scalar accumulation functions.
void RecordChildScalarAction(mozilla::Telemetry::ScalarID aId, uint32_t aKind,
                             mozilla::Telemetry::ScalarActionType aAction, nsIVariant* aValue);

void RecordChildKeyedScalarAction(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
                                  uint32_t aKind, mozilla::Telemetry::ScalarActionType aAction,
                                  nsIVariant* aValue);

void IPCTimerFired(nsITimer* aTimer, void* aClosure);
void DeInitializeGlobalState();

void DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent);

}

#endif // TelemetryIPCAccumulator_h__
