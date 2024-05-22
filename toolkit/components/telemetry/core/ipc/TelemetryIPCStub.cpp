/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryIPC.h"

namespace mozilla {

void TelemetryIPC::AccumulateChildHistograms(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::HistogramAccumulation>& aAccumulations) {}

void TelemetryIPC::AccumulateChildKeyedHistograms(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::KeyedHistogramAccumulation>& aAccumulations) {}

void TelemetryIPC::UpdateChildScalars(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::ScalarAction>& aScalarActions) {}

void TelemetryIPC::UpdateChildKeyedScalars(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::KeyedScalarAction>& aScalarActions) {}

void TelemetryIPC::GetDynamicScalarDefinitions(
    nsTArray<mozilla::Telemetry::DynamicScalarDefinition>& aDefs) {}

void TelemetryIPC::AddDynamicScalarDefinitions(
    const nsTArray<mozilla::Telemetry::DynamicScalarDefinition>& aDefs) {}

void TelemetryIPC::RecordChildEvents(
    Telemetry::ProcessID aProcessType,
    const nsTArray<Telemetry::ChildEventData>& aEvents) {}

void TelemetryIPC::RecordDiscardedData(
    Telemetry::ProcessID aProcessType,
    const Telemetry::DiscardedData& aDiscardedData) {}
}  // namespace mozilla
