/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryHistogram_h__
#define TelemetryHistogram_h__

#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TelemetryProcessEnums.h"

#include "mozilla/TelemetryComms.h"
#include "nsXULAppAPI.h"

// This module is internal to Telemetry.  It encapsulates Telemetry's
// histogram accumulation and storage logic.  It should only be used by
// Telemetry.cpp.  These functions should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace TelemetryHistogram {

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
void SetHistogramRecordingEnabled(mozilla::Telemetry::HistogramID aID, bool aEnabled);

nsresult SetHistogramRecordingEnabled(const nsACString &id, bool aEnabled);

void Accumulate(mozilla::Telemetry::HistogramID aHistogram, uint32_t aSample);
void Accumulate(mozilla::Telemetry::HistogramID aID, const nsCString& aKey,
                                            uint32_t aSample);
void Accumulate(const char* name, uint32_t sample);
void Accumulate(const char* name, const nsCString& key, uint32_t sample);

void AccumulateCategorical(mozilla::Telemetry::HistogramID aId, const nsCString& aLabel);

void AccumulateChild(mozilla::Telemetry::ProcessID aProcessType,
                     const nsTArray<mozilla::Telemetry::Accumulation>& aAccumulations);
void AccumulateChildKeyed(mozilla::Telemetry::ProcessID aProcessType,
                          const nsTArray<mozilla::Telemetry::KeyedAccumulation>& aAccumulations);

nsresult
GetHistogramById(const nsACString &name, JSContext *cx,
                 JS::MutableHandle<JS::Value> ret);

nsresult
GetKeyedHistogramById(const nsACString &name, JSContext *cx,
                      JS::MutableHandle<JS::Value> ret);

const char*
GetHistogramName(mozilla::Telemetry::HistogramID id);

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

size_t
GetMapShallowSizesOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);

size_t
GetHistogramSizesofIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

} // namespace TelemetryHistogram

#endif // TelemetryHistogram_h__
