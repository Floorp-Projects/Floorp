/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryIPC_h__
#define TelemetryIPC_h__

#include "nsTArray.h"
#include "nsXULAppAPI.h"

// This module provides the interface to accumulate Telemetry from child processes.
// Top-level actors for different child processes types (ContentParent, GPUChild)
// will call this for messages from their respective processes.

namespace mozilla {
namespace Telemetry {

struct Accumulation;
struct KeyedAccumulation;
struct ScalarAction;
struct KeyedScalarAction;

}

namespace TelemetryIPC {

/**
 * Accumulate child process data into histograms for the given process type.
 *
 * @param aAccumulations - accumulation actions to perform
 */
void AccumulateChildHistograms(GeckoProcessType aProcessType, const nsTArray<Telemetry::Accumulation>& aAccumulations);

/**
 * Accumulate child process data into keyed histograms for the given process type.
 *
 * @param aAccumulations - accumulation actions to perform
 */
void AccumulateChildKeyedHistograms(GeckoProcessType aProcessType, const nsTArray<Telemetry::KeyedAccumulation>& aAccumulations);

/**
 * Update scalars for the given process type with the data coming from child process.
 *
 * @param aScalarActions - actions to update the scalar data
 */
void UpdateChildScalars(GeckoProcessType aProcessType, const nsTArray<Telemetry::ScalarAction>& aScalarActions);

/**
 * Update keyed  scalars for the given process type with the data coming from child process.
 *
 * @param aScalarActions - actions to update the keyed scalar data
 */
void UpdateChildKeyedScalars(GeckoProcessType aProcessType, const nsTArray<Telemetry::KeyedScalarAction>& aScalarActions);

}
}

#endif // TelemetryIPC_h__
