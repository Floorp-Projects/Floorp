/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsITelemetry.h"
#include "nsVersionComparator.h"

#include "TelemetryCommon.h"

#include <cstring>

namespace mozilla {
namespace Telemetry {
namespace Common {

bool
IsExpiredVersion(const char* aExpiration)
{
  static mozilla::Version current_version = mozilla::Version(MOZ_APP_VERSION);
  MOZ_ASSERT(aExpiration);
  return strcmp(aExpiration, "never") && strcmp(aExpiration, "default") &&
    (mozilla::Version(aExpiration) <= current_version);
}

bool
IsInDataset(uint32_t aDataset, uint32_t aContainingDataset)
{
  if (aDataset == aContainingDataset) {
    return true;
  }

  // The "optin on release channel" dataset is a superset of the
  // "optout on release channel one".
  if (aContainingDataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN &&
      aDataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT) {
    return true;
  }

  return false;
}

bool
CanRecordDataset(uint32_t aDataset, bool aCanRecordBase, bool aCanRecordExtended)
{
  // If we are extended telemetry is enabled, we are allowed to record
  // regardless of the dataset.
  if (aCanRecordExtended) {
    return true;
  }

  // If base telemetry data is enabled and we're trying to record base
  // telemetry, allow it.
  if (aCanRecordBase &&
      IsInDataset(aDataset, nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT)) {
      return true;
  }

  // We're not recording extended telemetry or this is not the base
  // dataset. Bail out.
  return false;
}

} // namespace Common
} // namespace Telemetry
} // namespace mozilla
