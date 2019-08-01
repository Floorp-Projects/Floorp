/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewStreamingTelemetry_h__
#define GeckoViewStreamingTelemetry_h__

#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"

#include <cstdint>

namespace GeckoViewStreamingTelemetry {

void HistogramAccumulate(const nsCString& aName, uint32_t aValue);

// Classes wishing to receive Streaming Telemetry must implement this interface
// and register themselves via RegisterDelegate.
class StreamingTelemetryDelegate
    : public mozilla::RefCounted<StreamingTelemetryDelegate> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(StreamingTelemetryDelegate);
  // Will be called from time to time on the main thread.
  virtual void ReceiveHistogramSamples(const nsCString& aName,
                                       const nsTArray<uint32_t>& aSamples) = 0;
  virtual ~StreamingTelemetryDelegate() = default;
};

// Registers the provided StreamingTelemetryDelegate to receive Streaming
// Telemetry, overwriting any previous delegate registration.
// Call on any thread.
void RegisterDelegate(const RefPtr<StreamingTelemetryDelegate>& aDelegate);

}  // namespace GeckoViewStreamingTelemetry

#endif  // GeckoViewStreamingTelemetry_h__
