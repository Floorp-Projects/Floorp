/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Implement TimeStamp::Now() with mach_absolute_time
//
// The "tick" unit for mach_absolute_time is defined using mach_timebase_info() which
// gives a conversion ratio to nanoseconds. For more information see Apple's QA1398.
//
// This code is inspired by Chromium's time_mac.cc. The biggest
// differences are that we explicitly initialize using
// TimeStamp::Initialize() instead of lazily in Now() and that
// we store the time value in ticks and convert when needed instead
// of storing the time value in nanoseconds.

#include <mach/mach_time.h>
#include <time.h>

#include "mozilla/TimeStamp.h"

// Estimate of the smallest duration of time we can measure.
static PRUint64 sResolution;
static PRUint64 sResolutionSigDigs;

static const PRUint16 kNsPerUs   =       1000;
static const PRUint64 kNsPerMs   =    1000000;
static const PRUint64 kNsPerSec  = 1000000000;
static const double kNsPerMsd    =    1000000.0;
static const double kNsPerSecd   = 1000000000.0;

static bool gInitialized = false;
static double sNsPerTick;

static PRUint64
ClockTime()
{
  // mach_absolute_time is it when it comes to ticks on the Mac.  Other calls
  // with less precision (such as TickCount) just call through to
  // mach_absolute_time.
  //
  // At the time of writing mach_absolute_time returns the number of nanoseconds
  // since boot. This won't overflow 64bits for 500+ years so we aren't going
  // to worry about that possiblity
  return mach_absolute_time();
}

static PRUint64
ClockResolutionNs()
{
  PRUint64 start = ClockTime();
  PRUint64 end = ClockTime();
  PRUint64 minres = (end - start);

  // 10 total trials is arbitrary: what we're trying to avoid by
  // looping is getting unlucky and being interrupted by a context
  // switch or signal, or being bitten by paging/cache effects
  for (int i = 0; i < 9; ++i) {
    start = ClockTime();
    end = ClockTime();

    PRUint64 candidate = (start - end);
    if (candidate < minres)
      minres = candidate;
  }

  if (0 == minres) {
    // measurable resolution is either incredibly low, ~1ns, or very
    // high.  fall back on NSPR's resolution assumption
    minres = 1 * kNsPerMs;
  }

  return minres;
}

namespace mozilla {

double
TimeDuration::ToSeconds() const
{
  NS_ABORT_IF_FALSE(gInitialized, "calling TimeDuration too early");
  return (mValue * sNsPerTick) / kNsPerSecd;
}

double
TimeDuration::ToSecondsSigDigits() const
{
  NS_ABORT_IF_FALSE(gInitialized, "calling TimeDuration too early");
  // don't report a value < mResolution ...
  PRInt64 valueSigDigs = sResolution * (mValue / sResolution);
  // and chop off insignificant digits
  valueSigDigs = sResolutionSigDigs * (valueSigDigs / sResolutionSigDigs);
  return (valueSigDigs * sNsPerTick) / kNsPerSecd;
}

TimeDuration
TimeDuration::FromMilliseconds(double aMilliseconds)
{
  NS_ABORT_IF_FALSE(gInitialized, "calling TimeDuration too early");
  return TimeDuration::FromTicks(PRInt64((aMilliseconds * kNsPerMsd) / sNsPerTick));
}

TimeDuration
TimeDuration::Resolution()
{
  NS_ABORT_IF_FALSE(gInitialized, "calling TimeDuration too early");
  return TimeDuration::FromTicks(PRInt64(sResolution));
}

struct TimeStampInitialization
{
  TimeStampInitialization() {
    TimeStamp::Startup();
  }
  ~TimeStampInitialization() {
    TimeStamp::Shutdown();
  }
};

static TimeStampInitialization initOnce;

nsresult
TimeStamp::Startup()
{
  if (gInitialized)
    return NS_OK;

  mach_timebase_info_data_t timebaseInfo;
  // Apple's QA1398 suggests that the output from mach_timebase_info
  // will not change while a program is running, so it should be safe
  // to cache the result.
  kern_return_t kr = mach_timebase_info(&timebaseInfo);
  if (kr != KERN_SUCCESS)
    NS_RUNTIMEABORT("mach_timebase_info failed");

  sNsPerTick = double(timebaseInfo.numer) / timebaseInfo.denom;

  sResolution = ClockResolutionNs();

  // find the number of significant digits in sResolution, for the
  // sake of ToSecondsSigDigits()
  for (sResolutionSigDigs = 1;
       !(sResolutionSigDigs == sResolution
         || 10*sResolutionSigDigs > sResolution);
       sResolutionSigDigs *= 10);

  gInitialized = true;
  return NS_OK;
}

void
TimeStamp::Shutdown()
{
}

TimeStamp
TimeStamp::Now()
{
  return TimeStamp(ClockTime());
}

}
