/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryEventInfo_h__
#define TelemetryEventInfo_h__

#include "TelemetryCommon.h"

// This module is internal to Telemetry. The structures here hold data that
// describe events.
// It should only be used by TelemetryEventData.h and TelemetryEvent.cpp.
//
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace {

struct CommonEventInfo {
  // Indices for the category and expiration strings.
  uint32_t category_offset;
  uint32_t expiration_version_offset;

  // The index and count for the extra key offsets in the extra table.
  uint32_t extra_index;
  uint32_t extra_count;

  // The dataset this event is recorded in.
  uint32_t dataset;

  // Which processes to record this event in.
  mozilla::Telemetry::Common::RecordedProcessType record_in_processes;

  // Which products to record this event on.
  mozilla::Telemetry::Common::SupportedProduct products;

  // Convenience functions for accessing event strings.
  const nsDependentCString expiration_version() const;
  const nsDependentCString category() const;
  const nsDependentCString extra_key(uint32_t index) const;
};

struct EventInfo {
  // The corresponding CommonEventInfo.
  const CommonEventInfo& common_info;

  // Indices for the method & object strings.
  uint32_t method_offset;
  uint32_t object_offset;

  const nsDependentCString method() const;
  const nsDependentCString object() const;
};

}  // namespace

#endif  // TelemetryEventInfo_h__
