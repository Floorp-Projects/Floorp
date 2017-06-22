/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryIPC_h__
#define TelemetryIPC_h__

#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "mozilla/TelemetryProcessEnums.h"

// This module provides the interface to accumulate Telemetry from child processes.
// Top-level actors for different child processes types (ContentParent, GPUChild)
// will call this for messages from their respective processes.

namespace mozilla {
namespace Telemetry {

struct Accumulation;
struct KeyedAccumulation;
struct ScalarAction;
struct KeyedScalarAction;
struct ChildEventData;
struct DiscardedData;

}

namespace TelemetryIPC {

/**
 * Accumulate child process data into histograms for the given process type.
 *
 * @param aProcessType - the process type to accumulate the histograms for
 * @param aAccumulations - accumulation actions to perform
 */
void AccumulateChildHistograms(Telemetry::ProcessID aProcessType,
                               const nsTArray<Telemetry::Accumulation>& aAccumulations);

/**
 * Accumulate child process data into keyed histograms for the given process type.
 *
 * @param aProcessType - the process type to accumulate the keyed histograms for
 * @param aAccumulations - accumulation actions to perform
 */
void AccumulateChildKeyedHistograms(Telemetry::ProcessID aProcessType,
                                    const nsTArray<Telemetry::KeyedAccumulation>& aAccumulations);

/**
 * Update scalars for the given process type with the data coming from child process.
 *
 * @param aProcessType - the process type to process the scalar actions for
 * @param aScalarActions - actions to update the scalar data
 */
void UpdateChildScalars(Telemetry::ProcessID aProcessType,
                        const nsTArray<Telemetry::ScalarAction>& aScalarActions);

/**
 * Update keyed scalars for the given process type with the data coming from child process.
 *
 * @param aProcessType - the process type to process the keyed scalar actions for
 * @param aScalarActions - actions to update the keyed scalar data
 */
void UpdateChildKeyedScalars(Telemetry::ProcessID aProcessType,
                             const nsTArray<Telemetry::KeyedScalarAction>& aScalarActions);

/**
 * Record events for the given process type with the data coming from child process.
 *
 * @param aProcessType - the process type to record the events for
 * @param aEvents - events to record
 */
void RecordChildEvents(Telemetry::ProcessID aProcessType,
                       const nsTArray<Telemetry::ChildEventData>& aEvents);

/**
 * Record the counts of data the child process had to discard
 *
 * @param aProcessType - the process reporting the discarded data
 * @param aDiscardedData - stats about the discarded data
 */
void RecordDiscardedData(Telemetry::ProcessID aProcessType,
                         const Telemetry::DiscardedData& aDiscardedData);

}
}

#endif // TelemetryIPC_h__
