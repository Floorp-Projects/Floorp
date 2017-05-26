/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadIdlePeriod.h"

#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "nsRefreshDriver.h"
#include "nsThreadUtils.h"

#define DEFAULT_LONG_IDLE_PERIOD 50.0f
#define DEFAULT_MIN_IDLE_PERIOD 3.0f
#define DEFAULT_MAX_TIMER_THREAD_BOUND 5

const uint32_t kMaxTimerThreadBoundClamp = 15;

namespace mozilla {

NS_IMETHODIMP
MainThreadIdlePeriod::GetIdlePeriodHint(TimeStamp* aIdleDeadline)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aIdleDeadline);

  TimeStamp now = TimeStamp::Now();
  TimeStamp currentGuess =
    now + TimeDuration::FromMilliseconds(GetLongIdlePeriod());

  currentGuess = nsRefreshDriver::GetIdleDeadlineHint(currentGuess);
  currentGuess = NS_GetTimerDeadlineHintOnCurrentThread(currentGuess, GetMaxTimerThreadBound());

  // If the idle period is too small, then just return a null time
  // to indicate we are busy. Otherwise return the actual deadline.
  TimeDuration minIdlePeriod =
    TimeDuration::FromMilliseconds(GetMinIdlePeriod());
  bool busySoon = currentGuess.IsNull() ||
                  (now >= (currentGuess - minIdlePeriod)) ||
                  currentGuess < mLastIdleDeadline;

  if (!busySoon) {
    *aIdleDeadline = mLastIdleDeadline = currentGuess;
  }

  return NS_OK;
}

/* static */ float
MainThreadIdlePeriod::GetLongIdlePeriod()
{
  MOZ_ASSERT(NS_IsMainThread());

  static float sLongIdlePeriod = DEFAULT_LONG_IDLE_PERIOD;
  static bool sInitialized = false;

  if (!sInitialized && Preferences::IsServiceAvailable()) {
    sInitialized = true;
    Preferences::AddFloatVarCache(&sLongIdlePeriod, "idle_queue.long_period",
                                  DEFAULT_LONG_IDLE_PERIOD);
  }

  return sLongIdlePeriod;
}

/* static */ float
MainThreadIdlePeriod::GetMinIdlePeriod()
{
  MOZ_ASSERT(NS_IsMainThread());

  static float sMinIdlePeriod = DEFAULT_MIN_IDLE_PERIOD;
  static bool sInitialized = false;

  if (!sInitialized && Preferences::IsServiceAvailable()) {
    sInitialized = true;
    Preferences::AddFloatVarCache(&sMinIdlePeriod, "idle_queue.min_period",
                                  DEFAULT_MIN_IDLE_PERIOD);
  }

  return sMinIdlePeriod;
}

/* static */ uint32_t
MainThreadIdlePeriod::GetMaxTimerThreadBound()
{
  MOZ_ASSERT(NS_IsMainThread());

  static uint32_t sMaxTimerThreadBound = DEFAULT_MAX_TIMER_THREAD_BOUND;
  static bool sInitialized = false;

  if (!sInitialized && Preferences::IsServiceAvailable()) {
    sInitialized = true;
    Preferences::AddUintVarCache(&sMaxTimerThreadBound, "idle_queue.max_timer_thread_bound",
                                 DEFAULT_MAX_TIMER_THREAD_BOUND);
  }

  return std::max(sMaxTimerThreadBound, kMaxTimerThreadBoundClamp);
}

} // namespace mozilla
