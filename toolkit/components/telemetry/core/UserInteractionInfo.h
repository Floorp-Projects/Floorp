/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryUserInteractionInfo_h__
#define TelemetryUserInteractionInfo_h__

#include "TelemetryCommon.h"

// This module is internal to Telemetry. It defines a structure that holds the
// UserInteraction info. It should only be used by
// TelemetryUserInteractionData.h automatically generated file and
// TelemetryUserInteraction.cpp. This should not be used anywhere else. For the
// public interface to Telemetry functionality, see Telemetry.h.

namespace {

struct UserInteractionInfo {
  const uint32_t name_offset;

  explicit constexpr UserInteractionInfo(const uint32_t aNameOffset)
      : name_offset(aNameOffset) {}

  const char* name() const;
};

}  // namespace

#endif  // TelemetryUserInteractionInfo_h__
