/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryUserInteraction_h__
#define TelemetryUserInteraction_h__

#include "nsStringFwd.h"

namespace TelemetryUserInteraction {

void InitializeGlobalState(bool canRecordBase, bool canRecordExtended);
void DeInitializeGlobalState();

bool CanRecord(const nsAString& aName);

}  // namespace TelemetryUserInteraction

#endif  // TelemetryUserInteraction_h__
