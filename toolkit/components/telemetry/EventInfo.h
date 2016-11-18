/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryEventInfo_h__
#define TelemetryEventInfo_h__

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

  // The day since UNIX epoch that this probe expires on.
  uint32_t expiration_day;

  // The dataset this event is recorded in.
  uint32_t dataset;

  // Convenience functions for accessing event strings.
  const char* expiration_version() const;
  const char* category() const;
  const char* extra_key(uint32_t index) const;
};

struct EventInfo {
  // The corresponding CommonEventInfo.
  const CommonEventInfo& common_info;

  // Indices for the method & object strings.
  uint32_t method_offset;
  uint32_t object_offset;

  const char* method() const;
  const char* object() const;
};

} // namespace

#endif // TelemetryEventInfo_h__
