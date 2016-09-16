/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryScalar_h__
#define TelemetryScalar_h__

#include "mozilla/TelemetryScalarEnums.h"

// This module is internal to Telemetry. It encapsulates Telemetry's
// scalar accumulation and storage logic. It should only be used by
// Telemetry.cpp. These functions should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace TelemetryScalar {

void InitializeGlobalState(bool canRecordBase, bool canRecordExtended);
void DeInitializeGlobalState();

void SetCanRecordBase(bool b);
void SetCanRecordExtended(bool b);

// JS API Endpoints.
nsresult Add(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx);
nsresult Set(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx);
nsresult SetMaximum(const nsACString& aName, JS::HandleValue aVal, JSContext* aCx);
nsresult CreateSnapshots(unsigned int aDataset, bool aClearScalars,
                         JSContext* aCx, uint8_t optional_argc,
                         JS::MutableHandle<JS::Value> aResult);

// Keyed JS API Endpoints.
nsresult Add(const nsACString& aName, const nsAString& aKey, JS::HandleValue aVal,
             JSContext* aCx);
nsresult Set(const nsACString& aName, const nsAString& aKey, JS::HandleValue aVal,
             JSContext* aCx);
nsresult SetMaximum(const nsACString& aName, const nsAString& aKey, JS::HandleValue aVal,
                    JSContext* aCx);
nsresult CreateKeyedSnapshots(unsigned int aDataset, bool aClearScalars,
                              JSContext* aCx, uint8_t optional_argc,
                              JS::MutableHandle<JS::Value> aResult);

// C++ API Endpoints.
void Add(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, const nsAString& aValue);
void Set(mozilla::Telemetry::ScalarID aId, bool aValue);
void SetMaximum(mozilla::Telemetry::ScalarID aId, uint32_t aValue);

// Keyed C++ API Endpoints.
void Add(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);
void Set(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, bool aValue);
void SetMaximum(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);

// Only to be used for testing.
void ClearScalars();

size_t GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);
size_t GetScalarSizesOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

} // namespace TelemetryScalar

#endif // TelemetryScalar_h__