/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadIdlePeriod.h"

#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "nsRefreshDriver.h"

#define DEFAULT_LONG_IDLE_PERIOD 50.0f

namespace mozilla {

NS_IMETHODIMP
MainThreadIdlePeriod::GetIdlePeriodHint(TimeStamp* aIdleDeadline)
{
  MOZ_ASSERT(aIdleDeadline);

  Maybe<TimeStamp> deadline = nsRefreshDriver::GetIdleDeadlineHint();

  if (deadline.isSome()) {
    *aIdleDeadline = deadline.value();
  } else {
    *aIdleDeadline =
      TimeStamp::Now() + TimeDuration::FromMilliseconds(GetLongIdlePeriod());
  }

  return NS_OK;
}

/* static */ float
MainThreadIdlePeriod::GetLongIdlePeriod()
{
  static float sLongIdlePeriod = DEFAULT_LONG_IDLE_PERIOD;
  static bool sInitialized = false;

  if (!sInitialized && Preferences::IsServiceAvailable()) {
    sInitialized = true;
    Preferences::AddFloatVarCache(&sLongIdlePeriod, "idle_queue.long_period",
                                  DEFAULT_LONG_IDLE_PERIOD);
  }

  return sLongIdlePeriod;
}

} // namespace mozilla
