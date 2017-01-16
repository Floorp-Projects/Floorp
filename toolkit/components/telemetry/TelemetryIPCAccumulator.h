/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryIPCAccumulator_h__
#define TelemetryIPCAccumulator_h__

#include "mozilla/AlreadyAddRefed.h"

class nsIRunnable;
class nsITimer;
class nsCString;

namespace mozilla {
namespace Telemetry {

enum ID : uint32_t;

} // Telemetry
} // mozilla

namespace TelemetryIPCAccumulator {

// Histogram accumulation functions.
void AccumulateChildHistogram(mozilla::Telemetry::ID aId, uint32_t aSample);
void AccumulateChildKeyedHistogram(mozilla::Telemetry::ID aId, const nsCString& aKey,
                                   uint32_t aSample);

void IPCTimerFired(nsITimer* aTimer, void* aClosure);
void DeInitializeGlobalState();

void DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent);

}

#endif // TelemetryIPCAccumulator_h__
