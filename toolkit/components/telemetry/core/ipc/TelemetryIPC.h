/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryIPC_h__
#define TelemetryIPC_h__

#include <stdint.h>
#include "mozilla/TelemetryProcessEnums.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

// This module provides the interface to accumulate Telemetry from child
// processes. Top-level actors for different child processes types
// (ContentParent, GPUChild) will call this for messages from their respective
// processes.

namespace mozilla {

namespace Telemetry {

struct HistogramAccumulation;
struct KeyedHistogramAccumulation;
struct ScalarAction;
struct KeyedScalarAction;
struct DynamicScalarDefinition;
struct ChildEventData;
struct DiscardedData;

}  // namespace Telemetry

namespace TelemetryIPC {

/**
 * Accumulate child process data into histograms for the given process type.
 *
 * @param aProcessType - the process type to accumulate the histograms for
 * @param aAccumulations - accumulation actions to perform
 */
void AccumulateChildHistograms(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::HistogramAccumulation>& aAccumulations);

/**
 * Accumulate child process data into keyed histograms for the given process
 * type.
 *
 * @param aProcessType - the process type to accumulate the keyed histograms for
 * @param aAccumulations - accumulation actions to perform
 */
void AccumulateChildKeyedHistograms(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::KeyedHistogramAccumulation>& aAccumulations);

/**
 * Update scalars for the given process type with the data coming from child
 * process.
 *
 * @param aProcessType - the process type to process the scalar actions for
 * @param aScalarActions - actions to update the scalar data
 */
void UpdateChildScalars(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::ScalarAction>& aScalarActions);

/**
 * Update keyed scalars for the given process type with the data coming from
 * child process.
 *
 * @param aProcessType - the process type to process the keyed scalar actions
 *                       for
 * @param aScalarActions - actions to update the keyed scalar data
 */
void UpdateChildKeyedScalars(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::KeyedScalarAction>& aScalarActions);

/**
 * Record events for the given process type with the data coming from child
 * process.
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

/**
 * Get the dynamic scalar definitions from the parent process.
 * @param aDefs - The array that will contain the scalar definitions.
 */
void GetDynamicScalarDefinitions(
    nsTArray<mozilla::Telemetry::DynamicScalarDefinition>& aDefs);

/**
 * Add the dynamic scalar definitions coming from the parent process
 * to the current child process.
 * @param aDefs - The array that contains the scalar definitions.
 */
void AddDynamicScalarDefinitions(
    const nsTArray<mozilla::Telemetry::DynamicScalarDefinition>& aDefs);

}  // namespace TelemetryIPC
}  // namespace mozilla

#endif  // TelemetryIPC_h__
