/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Implement TimeStamp::Now() with POSIX clocks.
//
// The "tick" unit for POSIX clocks is simply a nanosecond, as this is
// the smallest unit of time representable by struct timespec.  That
// doesn't mean that a nanosecond is the resolution of TimeDurations
// obtained with this API; see TimeDuration::Resolution;
//

#include <time.h>

#include "mozilla/TimeStamp.h"

// Estimate of the smallest duration of time we can measure.
static uint64_t sResolution;
static uint64_t sResolutionSigDigs;

static const uint16_t kNsPerUs   =       1000;
static const uint64_t kNsPerMs   =    1000000;
static const uint64_t kNsPerSec  = 1000000000; 
static const double kNsPerMsd    =    1000000.0;
static const double kNsPerSecd   = 1000000000.0;

static uint64_t
TimespecToNs(const struct timespec& ts)
{
  uint64_t baseNs = uint64_t(ts.tv_sec) * kNsPerSec;
  return baseNs + uint64_t(ts.tv_nsec);
}

static uint64_t
ClockTimeNs()
{
  struct timespec ts;
  // this can't fail: we know &ts is valid, and TimeStamp::Startup()
  // checks that CLOCK_MONOTONIC is supported (and aborts if not)
  clock_gettime(CLOCK_MONOTONIC, &ts);

  // tv_sec is defined to be relative to an arbitrary point in time,
  // but it would be madness for that point in time to be earlier than
  // the Epoch.  So we can safely assume that even if time_t is 32
  // bits, tv_sec won't overflow while the browser is open.  Revisit
  // this argument if we're still building with 32-bit time_t around
  // the year 2037.
  return TimespecToNs(ts);
}

static uint64_t
ClockResolutionNs()
{
  // NB: why not rely on clock_getres()?  Two reasons: (i) it might
  // lie, and (ii) it might return an "ideal" resolution that while
  // theoretically true, could never be measured in practice.  Since
  // clock_gettime() likely involves a system call on your platform,
  // the "actual" timing resolution shouldn't be lower than syscall
  // overhead.

  uint64_t start = ClockTimeNs();
  uint64_t end = ClockTimeNs();
  uint64_t minres = (end - start);

  // 10 total trials is arbitrary: what we're trying to avoid by
  // looping is getting unlucky and being interrupted by a context
  // switch or signal, or being bitten by paging/cache effects
  for (int i = 0; i < 9; ++i) {
    start = ClockTimeNs();
    end = ClockTimeNs();

    uint64_t candidate = (start - end);
    if (candidate < minres)
      minres = candidate;
  }

  if (0 == minres) {
    // measurable resolution is either incredibly low, ~1ns, or very
    // high.  fall back on clock_getres()
    struct timespec ts;
    if (0 == clock_getres(CLOCK_MONOTONIC, &ts)) {
      minres = TimespecToNs(ts);
    }
  }

  if (0 == minres) {
    // clock_getres probably failed.  fall back on NSPR's resolution
    // assumption
    minres = 1 * kNsPerMs;
  }

  return minres;
}


namespace mozilla {

double
TimeDuration::ToSeconds() const
{
  return double(mValue) / kNsPerSecd;
}

double
TimeDuration::ToSecondsSigDigits() const
{
  // don't report a value < mResolution ...
  int64_t valueSigDigs = sResolution * (mValue / sResolution);
  // and chop off insignificant digits
  valueSigDigs = sResolutionSigDigs * (valueSigDigs / sResolutionSigDigs);
  return double(valueSigDigs) / kNsPerSecd;
}

TimeDuration
TimeDuration::FromMilliseconds(double aMilliseconds)
{
  return TimeDuration::FromTicks(aMilliseconds * kNsPerMsd);
}

TimeDuration
TimeDuration::Resolution()
{
  return TimeDuration::FromTicks(int64_t(sResolution));
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
static bool gInitialized = false;

nsresult
TimeStamp::Startup()
{
  if (gInitialized)
    return NS_OK;

  struct timespec dummy;
  if (0 != clock_gettime(CLOCK_MONOTONIC, &dummy))
      NS_RUNTIMEABORT("CLOCK_MONOTONIC is absent!");

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
TimeStamp::Now(bool aHighResolution)
{
  return TimeStamp(ClockTimeNs());
}

}
