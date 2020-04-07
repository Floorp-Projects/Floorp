/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fuzzyfox.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/TimeStamp.h"
#include "nsComponentManagerUtils.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "prrng.h"
#include "prtime.h"

// For usleep/Sleep & QueryPerformanceFrequency
#ifdef XP_WIN
#  include <windows.h>
#else
#  include <unistd.h>
#endif

using namespace mozilla;

static LazyLogModule sFuzzyfoxLog("Fuzzyfox");

#define US_TO_NS(x) ((x)*1000)
#define NS_TO_US(x) ((x) / 1000)

#ifdef LOG
#  undef LOG
#endif

#define LOG(level, args) MOZ_LOG(sFuzzyfoxLog, mozilla::LogLevel::level, args)

#define FUZZYFOX_ENABLED_PREF "privacy.fuzzyfox.enabled"
#define FUZZYFOX_ENABLED_PREF_DEFAULT false
#define FUZZYFOX_CLOCKGRAIN_PREF "privacy.fuzzyfox.clockgrainus"

static bool sFuzzyfoxInitializing;

NS_IMPL_ISUPPORTS_INHERITED(Fuzzyfox, Runnable, nsIObserver)

/* static */
void Fuzzyfox::Start() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Fuzzyfox> r = new Fuzzyfox();
  SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
}

Fuzzyfox::Fuzzyfox()
    : Runnable("Fuzzyfox"),
      mSanityCheck(false),
      mStartTime(0),
      mDuration(PickDuration()),
      mTickType(eUptick) {
  MOZ_ASSERT(NS_IsMainThread());

  // [[ I originally ran this after observing profile-after-change, but
  // it turned out that this contructor was getting called _after_ that
  // event had already fired. ]]
  bool fuzzyfoxEnabled = Preferences::GetBool(FUZZYFOX_ENABLED_PREF,
                                              FUZZYFOX_ENABLED_PREF_DEFAULT);

  LOG(Info, ("PT(%p) Created Fuzzyfox, FuzzyFox is now %s \n", this,
             (fuzzyfoxEnabled ? "initializing" : "disabled")));

  sFuzzyfoxInitializing = fuzzyfoxEnabled;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) {
    // Sometimes the call to the pref service fails. If that happens, we
    // don't add the observers. The user will have to restart to have
    // preference changes take effect.
    return;
  }

  prefs->AddObserver(FUZZYFOX_ENABLED_PREF, this, false);
  prefs->AddObserver(FUZZYFOX_CLOCKGRAIN_PREF, this, false);
}

Fuzzyfox::~Fuzzyfox() = default;

/*
 * Fuzzyfox::Run is the core of FuzzyFox. Every so often we pop into this
 * method, and pick a new point in the future to hold time constant until. If we
 * have not reached the _previous_ point in time we had picked, we sleep until
 * we do so. Then we round the current time downwards to a configurable grain
 * value, fix it in place so time does not advance, and let execution continue.
 */
NS_IMETHODIMP
Fuzzyfox::Observe(nsISupports* aObject, const char* aTopic,
                  const char16_t* aMessage) {
  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    NS_ConvertUTF16toUTF8 pref(aMessage);

    if (pref.EqualsLiteral(FUZZYFOX_ENABLED_PREF)) {
      bool fuzzyfoxEnabled = Preferences::GetBool(
          FUZZYFOX_ENABLED_PREF, FUZZYFOX_ENABLED_PREF_DEFAULT);

      LOG(Info, ("PT(%p) Observed a pref change, FuzzyFox is now %s \n", this,
                 (fuzzyfoxEnabled ? "initializing" : "disabled")));

      sFuzzyfoxInitializing = fuzzyfoxEnabled;

      if (sFuzzyfoxInitializing) {
        // Queue a runnable
        nsCOMPtr<nsIRunnable> r = this;
        SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
      } else {
        mStartTime = 0;
        mTickType = eUptick;
        mSanityCheck = false;
        TimeStamp::SetFuzzyfoxEnabled(false);
      }
    }
  }
  return NS_OK;
}

#define DISPATCH_AND_RETURN()                                \
  nsCOMPtr<nsIRunnable> r = this;                            \
  SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()); \
  return NS_OK

NS_IMETHODIMP
Fuzzyfox::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sFuzzyfoxInitializing && !TimeStamp::GetFuzzyfoxEnabled()) {
    LOG(Info, ("[FuzzyfoxEvent] PT(%p) Fuzzyfox is shut down, doing nothing \n",
               this));
    return NS_OK;
  }

  if (sFuzzyfoxInitializing) {
    // This is the first time we are running afer enabling FuzzyFox. We need
    // to prevent time from going backwards, so for the first run we round the
    // time up to the next grain.
    mStartTime = CeilToGrain(ActualTime());
    MOZ_ASSERT(mStartTime != 0);
    TimeStamp newTimeStamp = CeilToGrain(TimeStamp::NowUnfuzzed());
    Fuzzyfox::UpdateClocks(mStartTime, newTimeStamp);

    mSanityCheck = true;
    LOG(Info, ("[FuzzyfoxEvent] PT(%p) Going to start Fuzzyfox, queuing up the "
               "job \n",
               this));

    // Now that we've updated the timestamps, we can let everyone know Fuzzyfox
    // is ready to use
    TimeStamp::SetFuzzyfoxEnabled(true);
    sFuzzyfoxInitializing = false;

    DISPATCH_AND_RETURN();
  }

  // We need to check how long its been since we ran
  uint64_t endTime = ActualTime();

  uint64_t remaining = 0;

  // Pick the amount to sleep
  if (endTime < mStartTime) {
    // This can only happen if we just enabled FuzzyFox, rounded up, and then
    // re-ran the runnable before we advanced to the next grain. If that
    // happens, then repeat the current time. We use mSanityCheck just to be
    // sure (and will eventually remove it.)
    MOZ_ASSERT(mSanityCheck);
    LOG(Warning, ("[FuzzyfoxEvent] Unusual!! PT(%p) endTime < mStartTime "
                  "mStartTime %" PRIu64 " endTime %" PRIu64 " \n",
                  this, mStartTime, endTime));

    mSanityCheck = true;
    DISPATCH_AND_RETURN();
  }

  uint64_t actualRunDuration = endTime - mStartTime;
  LOG(Verbose,
      ("[FuzzyfoxEvent] PT(%p) mDuration: %" PRIu32 " endTime: %" PRIu64
       " mStartTime: %" PRIu64 " actualRunDuration: %" PRIu64 " \n",
       this, mDuration, endTime, mStartTime, actualRunDuration));
  if (actualRunDuration > mDuration) {
    // We ran over our budget!
    uint64_t over = actualRunDuration - mDuration;
    LOG(Debug, ("[FuzzyfoxEvent] PT(%p) Overran budget of %" PRIu32
                " by %" PRIu64 " \n",
                this, mDuration, over));

    uint64_t nextDuration = PickDuration();
    while (over > nextDuration) {
      over -= nextDuration;
      nextDuration = PickDuration();
      mTickType = mTickType == eUptick ? eDowntick : eUptick;
    }

    remaining = nextDuration - over;
  } else {
    // Didn't go over budget
    remaining = mDuration - actualRunDuration;
    LOG(Debug, ("[FuzzyfoxEvent] PT(%p) Finishing budget of %" PRIu32
                " with %" PRIu64 " \n",
                this, mDuration, remaining));
  }
  mSanityCheck = false;

  // Sleep for now
#ifdef XP_WIN
  Sleep(remaining);
#else
  usleep(remaining);
#endif

  /*
   * Update clocks (and fire pending events etc)
   *
   * Note: Anytime we round the current time to the grain, and then round the
   * 'real' time to the grain, we are introducing the risk that we split the
   * grain. That is, the time advances enough after the first rounding that the
   * second rounding causes us to move to a different grain.
   *
   * In theory, such an occurance breaks the security of FuzzyFox, and if an
   * attacker can influence the event to occur reliably, and then measure
   * against it they can attack FuzzyFox. But such an attack is so difficult
   * that it will never be acheived until you read this comment in a future
   * Academic Publication that demonstrates it. And at that point the paper
   * would surely never be accepted into any _respectable_ journal unless the
   * authors had also presented a solution for the issue that was usable and
   * incorporated into Firefox!
   */
  uint64_t newTime = FloorToGrain(ActualTime());
  TimeStamp newTimeStamp = FloorToGrain(TimeStamp::NowUnfuzzed());
  UpdateClocks(newTime, newTimeStamp);

  // Reset values
  mTickType = mTickType == eUptick ? eDowntick : eUptick;
  mStartTime = ActualTime();
  mDuration = PickDuration();

  LOG(Verbose, ("[FuzzyfoxEvent] PT(%p) For next time mDuration: %" PRIu32
                " mStartTime: %" PRIu64 " \n",
                this, mDuration, mStartTime));

  DISPATCH_AND_RETURN();
}

/*
 * ActualTime returns the unfuzzed/unrounded time in microseconds since the
 * epoch
 */
uint64_t Fuzzyfox::ActualTime() { return PR_Now(); }

/*
 * Calculate a duration we will wait until we allow time to advance again
 */
uint64_t Fuzzyfox::PickDuration() {
  // TODO: Bug 1484298 - use a real RNG
  long int rval = rand();

  // Avoid divide by zero errors and overflow errors
  uint32_t duration =
      std::max((uint32_t)1, StaticPrefs::privacy_fuzzyfox_clockgrainus());
  duration = duration >= (UINT32_MAX / 2) ? (UINT32_MAX / 2) : duration;

  // We want uniform distribution from 1->duration*2
  // so that the mean is duration
  return 1 + (rval % (duration * 2));
}

/*
 * Update the TimeStamp's class value for the current (constant) time and
 * dispatch the new (constant) timestamp so observers can register to receive it
 * to update their own time code.
 */
void Fuzzyfox::UpdateClocks(uint64_t aNewTime, TimeStamp aNewTimeStamp) {
// newTime is the new canonical time for this scope!
#ifndef XP_WIN
  LOG(Debug, ("[Time] New time is %" PRIu64 " (compare to %" PRIu64
              ") and timestamp is %" PRIu64 " (compare to %" PRIu64 ")\n",
              aNewTime, ActualTime(), aNewTimeStamp.mValue.mTimeStamp,
              TimeStamp::NowUnfuzzed().mValue.mTimeStamp));
#else
  LOG(Debug, ("[Time] New time is %" PRIu64 " (compare to %" PRIu64 ") \n",
              aNewTime, ActualTime()));
#endif

  // Fire notifications
  if (MOZ_UNLIKELY(!mObs)) {
    mObs = services::GetObserverService();
    if (NS_WARN_IF(!mObs)) {
      return;
    }
  }

  // Event firings on occur on downticks and have no data
  if (mTickType == eDowntick) {
    mObs->NotifyObservers(nullptr, FUZZYFOX_FIREOUTBOUND_OBSERVER_TOPIC,
                          nullptr);
  }

  if (!mTimeUpdateWrapper) {
    mTimeUpdateWrapper = do_CreateInstance(NS_SUPPORTS_PRINT64_CONTRACTID);
    if (NS_WARN_IF(!mTimeUpdateWrapper)) {
      return;
    }
  }

  mTimeUpdateWrapper->SetData(aNewTime);

  // Clocks get the new official (frozen) time. This happens on all ticks
  mObs->NotifyObservers(mTimeUpdateWrapper, FUZZYFOX_UPDATECLOCK_OBSERVER_TOPIC,
                        nullptr);

  // Update the timestamp's canonicaltimes
  TimeStamp::UpdateFuzzyTime(aNewTime);
  TimeStamp::UpdateFuzzyTimeStamp(aNewTimeStamp);
}

/*
 * FloorToGrain accepts a timestamp in microsecond precision
 * and returns it in microseconds, rounded down to the nearest
 * ClockGrain value.
 */
uint64_t Fuzzyfox::FloorToGrain(uint64_t aValue) {
  return aValue - (aValue % StaticPrefs::privacy_fuzzyfox_clockgrainus());
}

/*
 * FloorToGrain accepts a timestamp and returns it, rounded down
 * to the nearest ClockGrain value.
 */
TimeStamp Fuzzyfox::FloorToGrain(TimeStamp aValue) {
#ifdef XP_WIN
  // grain is in us
  uint64_t grain = StaticPrefs::privacy_fuzzyfox_clockgrainus();
  // GTC and QPS are stored in |mt| and need to be converted to
  uint64_t GTC = mt2ms(aValue.mValue.mGTC) * 1000;
  uint64_t QPC = mt2ms(aValue.mValue.mQPC) * 1000;

  return TimeStamp(TimeStampValue(ms2mt((GTC - (GTC % grain)) / 1000),
                                  ms2mt((QPC - (QPC % grain)) / 1000),
                                  aValue.mValue.mHasQPC, true));
#else
  return TimeStamp(TimeStampValue(
      true, US_TO_NS(FloorToGrain(NS_TO_US(aValue.mValue.mTimeStamp)))));
#endif
}

/*
 * CeilToGrain accepts a timestamp in microsecond precision
 * and returns it in microseconds, rounded up to the nearest
 * ClockGrain value.
 */
uint64_t Fuzzyfox::CeilToGrain(uint64_t aValue) {
  return (aValue / StaticPrefs::privacy_fuzzyfox_clockgrainus()) *
         StaticPrefs::privacy_fuzzyfox_clockgrainus();
}

/*
 * CeilToGrain accepts a timestamp and returns it, rounded up
 * to the nearest ClockGrain value.
 */
TimeStamp Fuzzyfox::CeilToGrain(TimeStamp aValue) {
#ifdef XP_WIN
  // grain is in us
  uint64_t grain = StaticPrefs::privacy_fuzzyfox_clockgrainus();
  // GTC and QPS are stored in |mt| and need to be converted
  uint64_t GTC = mt2ms(aValue.mValue.mGTC) * 1000;
  uint64_t QPC = mt2ms(aValue.mValue.mQPC) * 1000;

  return TimeStamp(TimeStampValue(ms2mt(((GTC / grain) * grain) / 1000),
                                  ms2mt(((QPC / grain) * grain) / 1000),
                                  aValue.mValue.mHasQPC, true));
#else
  return TimeStamp(TimeStampValue(
      true, US_TO_NS(CeilToGrain(NS_TO_US(aValue.mValue.mTimeStamp)))));
#endif
}
