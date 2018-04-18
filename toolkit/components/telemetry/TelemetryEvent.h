/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryEvent_h__
#define TelemetryEvent_h__

#include "mozilla/TelemetryEventEnums.h"
#include "mozilla/TelemetryProcessEnums.h"

namespace mozilla {
namespace Telemetry {
  struct ChildEventData;
}
}

// This module is internal to Telemetry. It encapsulates Telemetry's
// event recording and storage logic. It should only be used by
// Telemetry.cpp. These functions should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace TelemetryEvent {

void InitializeGlobalState(bool canRecordBase, bool canRecordExtended);
void DeInitializeGlobalState();

void SetCanRecordBase(bool b);
void SetCanRecordExtended(bool b);

// JS API Endpoints.
nsresult RecordEvent(const nsACString& aCategory, const nsACString& aMethod,
                     const nsACString& aObject, JS::HandleValue aValue,
                     JS::HandleValue aExtra, JSContext* aCx,
                     uint8_t optional_argc);

void SetEventRecordingEnabled(const nsACString& aCategory, bool aEnabled);
nsresult RegisterEvents(const nsACString& aCategory, JS::Handle<JS::Value> aEventData,
                        bool aBuiltin, JSContext* cx);

nsresult CreateSnapshots(uint32_t aDataset, bool aClear, JSContext* aCx,
                         uint8_t optional_argc, JS::MutableHandleValue aResult);

// Record events from child processes.
nsresult RecordChildEvents(mozilla::Telemetry::ProcessID aProcessType,
                           const nsTArray<mozilla::Telemetry::ChildEventData>& aEvents);

// Only to be used for testing.
void ClearEvents();

size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

} // namespace TelemetryEvent

#endif // TelemetryEvent_h__
