/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryOrigin_h__
#define TelemetryOrigin_h__

#include "TelemetryOriginEnums.h"
#include "jsapi.h"
#include "nsError.h"
#include "nsString.h"

// This module is internal to Telemetry. It encapsulates Telemetry's
// origin recording and storage logic. It should only be used by
// Telemetry.cpp. These functions should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace TelemetryOrigin {

void InitializeGlobalState();
void DeInitializeGlobalState();

// C++ Recording Endpoint
nsresult RecordOrigin(mozilla::Telemetry::OriginMetricID aId,
                      const nsACString& aOrigin);

// JS API Endpoints.
nsresult GetOriginSnapshot(bool aClear, JSContext* aCx,
                           JS::MutableHandleValue aResult);

nsresult GetEncodedOriginSnapshot(bool aClear, JSContext* aCx,
                                  JS::MutableHandleValue aSnapshot);

// Only to be used for testing.
void ClearOrigins();

size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

// Only to be used for testing.
size_t SizeOfPrioDatasPerMetric();

}  // namespace TelemetryOrigin

#endif  // TelemetryOrigin_h__
