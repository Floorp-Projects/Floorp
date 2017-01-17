/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryScalarInfo_h__
#define TelemetryScalarInfo_h__

#include "nsXULAppAPI.h"

// This module is internal to Telemetry. It defines a structure that holds the
// scalar info. It should only be used by TelemetryScalarData.h automatically
// generated file and TelemetryScalar.cpp. This should not be used anywhere else.
// For the public interface to Telemetry functionality, see Telemetry.h.

namespace {

enum class RecordedProcessType : uint32_t {
  Main       = (1 << GeckoProcessType_Default),  // Also known as "parent process"
  Content    = (1 << GeckoProcessType_Content),
  Gpu        = (1 << GeckoProcessType_GPU),
  AllChilds  = 0xFFFFFFFF - 1,  // All the children processes (i.e. content, gpu, ...)
  All        = 0xFFFFFFFF       // All the processes
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RecordedProcessType)

struct ScalarInfo {
  uint32_t kind;
  uint32_t name_offset;
  uint32_t expiration_offset;
  uint32_t dataset;
  RecordedProcessType record_in_processes;
  bool keyed;

  const char *name() const;
  const char *expiration() const;
};
} // namespace

#endif // TelemetryScalarInfo_h__
