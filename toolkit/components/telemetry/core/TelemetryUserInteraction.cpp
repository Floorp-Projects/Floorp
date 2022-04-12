/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "MainThreadUtils.h"
#include "TelemetryUserInteraction.h"
#include "TelemetryUserInteractionData.h"
#include "TelemetryUserInteractionNameMap.h"
#include "UserInteractionInfo.h"

using mozilla::Telemetry::UserInteractionIDByNameLookup;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE TYPES
namespace {

struct UserInteractionKey {
  uint32_t id;
};

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {
// Set to true once this global state has been initialized.
bool gInitDone = false;

bool gCanRecordBase;
bool gCanRecordExtended;
}  // namespace

namespace {
// Implements the methods for UserInteractionInfo.
const char* UserInteractionInfo::name() const {
  return &gUserInteractionsStringTable[this->name_offset];
}

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryUserInteraction::

void TelemetryUserInteraction::InitializeGlobalState(bool aCanRecordBase,
                                                     bool aCanRecordExtended) {
  if (!XRE_IsParentProcess()) {
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gInitDone,
             "TelemetryUserInteraction::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = aCanRecordBase;
  gCanRecordExtended = aCanRecordExtended;
  gInitDone = true;
}

void TelemetryUserInteraction::DeInitializeGlobalState() {
  if (!XRE_IsParentProcess()) {
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(gInitDone);
  if (!gInitDone) {
    return;
  }

  gInitDone = false;
}

bool TelemetryUserInteraction::CanRecord(const nsAString& aName) {
  if (!gCanRecordBase) {
    return false;
  }

  nsCString name = NS_ConvertUTF16toUTF8(aName);
  const uint32_t idx = UserInteractionIDByNameLookup(name);

  MOZ_DIAGNOSTIC_ASSERT(
      idx < mozilla::Telemetry::UserInteractionID::UserInteractionCount,
      "Intermediate lookup should always give a valid index.");

  if (name.Equals(gUserInteractions[idx].name())) {
    return true;
  }

  return false;
}
