/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryIPCAccumulator.h"

using mozilla::Telemetry::ScalarActionType;
using mozilla::Telemetry::ScalarVariant;

namespace TelemetryIPCAccumulator = mozilla::TelemetryIPCAccumulator;

void TelemetryIPCAccumulator::AccumulateChildHistogram(
    mozilla::Telemetry::HistogramID aId, uint32_t aSample) {}

void TelemetryIPCAccumulator::AccumulateChildKeyedHistogram(
    mozilla::Telemetry::HistogramID aId, const nsCString& aKey,
    uint32_t aSample) {}

void TelemetryIPCAccumulator::RecordChildScalarAction(
    uint32_t aId, bool aDynamic, ScalarActionType aAction,
    const ScalarVariant& aValue) {}

void TelemetryIPCAccumulator::RecordChildKeyedScalarAction(
    uint32_t aId, bool aDynamic, const nsAString& aKey,
    ScalarActionType aAction, const ScalarVariant& aValue) {}

void TelemetryIPCAccumulator::RecordChildEvent(
    const mozilla::TimeStamp& timestamp, const nsACString& category,
    const nsACString& method, const nsACString& object,
    const mozilla::Maybe<nsCString>& value,
    const nsTArray<mozilla::Telemetry::EventExtraEntry>& extra) {}

// To ensure we don't loop IPCTimerFired->AccumulateChild->arm timer, we don't
// unset gIPCTimerArmed until the IPC completes
//
// This function must be called on the main thread, otherwise IPC will fail.
void TelemetryIPCAccumulator::IPCTimerFired(nsITimer* aTimer, void* aClosure) {}

void TelemetryIPCAccumulator::DeInitializeGlobalState() {}

void TelemetryIPCAccumulator::DispatchToMainThread(
    already_AddRefed<nsIRunnable>&& aEvent) {}
