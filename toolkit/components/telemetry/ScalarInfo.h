/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryScalarInfo_h__
#define TelemetryScalarInfo_h__

// This module is internal to Telemetry. It defines a structure that holds the
// scalar info. It should only be used by TelemetryScalarData.h automatically
// generated file and TelemetryScalar.cpp. This should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace {
struct ScalarInfo {
  uint32_t kind;
  uint32_t name_offset;
  uint32_t expiration_offset;
  uint32_t dataset;
  bool keyed;

  const char *name() const;
  const char *expiration() const;
};
} // namespace

#endif // TelemetryScalarInfo_h__
