/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryHistogram_h__
#define TelemetryHistogram_h__

#include "mozilla/TelemetryHistogramEnums.h"

#include "mozilla/TelemetryComms.h"

// This module is internal to Telemetry.  It encapsulates Telemetry's
// histogram accumulation and storage logic.  It should only be used by
// Telemetry.cpp.  These functions should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace TelemetryHistogram {

void CreateStatisticsRecorder();
void DestroyStatisticsRecorder();

void InitializeGlobalState(bool canRecordBase, bool canRecordExtended);
void DeInitializeGlobalState();
#ifdef DEBUG
bool GlobalStateHasBeenInitialized();
#endif

bool CanRecordBase();
void SetCanRecordBase(bool b);
bool CanRecordExtended();
void SetCanRecordExtended(bool b);

void InitHistogramRecordingEnabled();
void SetHistogramRecordingEnabled(mozilla::Telemetry::ID aID, bool aEnabled);

nsresult SetHistogramRecordingEnabled(const nsACString &id, bool aEnabled);

void Accumulate(mozilla::Telemetry::ID aHistogram, uint32_t aSample);
void Accumulate(mozilla::Telemetry::ID aID, const nsCString& aKey,
                                            uint32_t aSample);
void Accumulate(const char* name, uint32_t sample);
void Accumulate(const char* name, const nsCString& key, uint32_t sample);

void AccumulateCategorical(mozilla::Telemetry::ID aId, const nsCString& aLabel);

void AccumulateChild(const nsTArray<mozilla::Telemetry::Accumulation>& aAccumulations);
void AccumulateChildKeyed(const nsTArray<mozilla::Telemetry::KeyedAccumulation>& aAccumulations);

void
ClearHistogram(mozilla::Telemetry::ID aId);

nsresult
GetHistogramById(const nsACString &name, JSContext *cx,
                 JS::MutableHandle<JS::Value> ret);

nsresult
GetKeyedHistogramById(const nsACString &name, JSContext *cx,
                      JS::MutableHandle<JS::Value> ret);

const char*
GetHistogramName(mozilla::Telemetry::ID id);

nsresult
HistogramFrom(const nsACString &name, const nsACString &existing_name,
              JSContext *cx, JS::MutableHandle<JS::Value> ret);

nsresult
CreateHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret,
                         bool subsession, bool clearSubsession);

nsresult
RegisteredHistograms(uint32_t aDataset, uint32_t *aCount,
                     char*** aHistograms);

nsresult
RegisteredKeyedHistograms(uint32_t aDataset, uint32_t *aCount,
                          char*** aHistograms);

nsresult
GetKeyedHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret);

nsresult
RegisterAddonHistogram(const nsACString &id, const nsACString &name,
                       uint32_t histogramType, uint32_t min, uint32_t max,
                       uint32_t bucketCount, uint8_t optArgCount);

nsresult
GetAddonHistogram(const nsACString &id, const nsACString &name,
                  JSContext *cx, JS::MutableHandle<JS::Value> ret);

nsresult
UnregisterAddonHistograms(const nsACString &id);

nsresult
GetAddonHistogramSnapshots(JSContext *cx, JS::MutableHandle<JS::Value> ret);

size_t
GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

size_t
GetHistogramSizesofIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

void
IPCTimerFired(nsITimer* aTimer, void* aClosure);
} // namespace TelemetryHistogram

#endif // TelemetryHistogram_h__
