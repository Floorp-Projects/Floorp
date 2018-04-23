/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryScalar_h__
#define TelemetryScalar_h__

#include "mozilla/TelemetryScalarEnums.h"
#include "mozilla/TelemetryProcessEnums.h"

// This module is internal to Telemetry. It encapsulates Telemetry's
// scalar accumulation and storage logic. It should only be used by
// Telemetry.cpp. These functions should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace mozilla {
#if defined(MOZ_TELEMETRY_GECKOVIEW)
// This is only used for the GeckoView persistence.
class JSONWriter;
#endif
namespace Telemetry {
  struct ScalarAction;
  struct KeyedScalarAction;
  struct DiscardedData;
  struct DynamicScalarDefinition;
} // namespace Telemetry
} // namespace mozilla

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

nsresult RegisterScalars(const nsACString& aCategoryName, JS::Handle<JS::Value> aScalarData,
                         bool aBuiltin, JSContext* cx);

// Event Summary
void SummarizeEvent(const nsCString& aUniqueEventName,
                    mozilla::Telemetry::ProcessID aProcessType, bool aDynamic);

// Only to be used for testing.
void ClearScalars();

size_t GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);
size_t GetScalarSizesOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

void UpdateChildData(mozilla::Telemetry::ProcessID aProcessType,
                     const nsTArray<mozilla::Telemetry::ScalarAction>& aScalarActions);

void UpdateChildKeyedData(mozilla::Telemetry::ProcessID aProcessType,
                          const nsTArray<mozilla::Telemetry::KeyedScalarAction>& aScalarActions);

void RecordDiscardedData(mozilla::Telemetry::ProcessID aProcessType,
                         const mozilla::Telemetry::DiscardedData& aDiscardedData);

void GetDynamicScalarDefinitions(nsTArray<mozilla::Telemetry::DynamicScalarDefinition>&);
void AddDynamicScalarDefinitions(const nsTArray<mozilla::Telemetry::DynamicScalarDefinition>&);

// They are responsible for updating in-memory probes with the data persisted
// on the disk and vice-versa.
#if defined(MOZ_TELEMETRY_GECKOVIEW)
nsresult SerializeScalars(mozilla::JSONWriter &aWriter);
nsresult SerializeKeyedScalars(mozilla::JSONWriter &aWriter);
nsresult DeserializePersistedScalars(JSContext* aCx, JS::HandleValue aData);
nsresult DeserializePersistedKeyedScalars(JSContext* aCx, JS::HandleValue aData);
#endif // MOZ_TELEMETRY_GECKOVIEW
} // namespace TelemetryScalar

#endif // TelemetryScalar_h__
