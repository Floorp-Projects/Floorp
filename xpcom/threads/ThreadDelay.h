/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ChaosMode.h"

namespace mozilla {

// Sleep for a random number of microseconds less than aMicrosecondLimit
// if aFeature is enabled.  On Windows, the sleep will always be 1 millisecond
// due to platform limitations.
void DelayForChaosMode(ChaosFeature aFeature, const uint32_t aMicrosecondLimit);

}  // namespace mozilla
