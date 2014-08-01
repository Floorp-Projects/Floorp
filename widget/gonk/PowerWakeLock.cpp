/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PowerWakeLock.h"
#include <hardware_legacy/power.h>

const char* kPower_WAKE_LOCK_ID = "PowerKeyEvent";

namespace mozilla {
namespace hal_impl {
StaticRefPtr <PowerWakelock> gPowerWakelock;
PowerWakelock::PowerWakelock()
{
  acquire_wake_lock(PARTIAL_WAKE_LOCK, kPower_WAKE_LOCK_ID);
}

PowerWakelock::~PowerWakelock()
{
  release_wake_lock(kPower_WAKE_LOCK_ID);
}
} // hal_impl
} // mozilla
