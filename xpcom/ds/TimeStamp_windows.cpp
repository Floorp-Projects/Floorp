/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implement TimeStamp::Now() with QueryPerformanceCounter() controlled with
// values of GetTickCount().

// XXX Forcing log to be able to catch issues in the field.  Should be removed
// before this reaches the Release or even Beta channel.
#define FORCE_PR_LOG

#include "mozilla/TimeStamp.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"
#include <pratom.h>
#include <windows.h>

#include "prlog.h"
#include <stdio.h>

#if defined(PR_LOGGING)
// Log module for mozilla::TimeStamp for Windows logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=TimeStampWindows:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
  PRLogModuleInfo* timeStampLog = PR_NewLogModule("TimeStampWindows");
  #define LOG(x)  PR_LOG(timeStampLog, PR_LOG_DEBUG, x)
#else
  #define LOG(x)
#endif /* PR_LOGGING */

// Estimate of the smallest duration of time we can measure.
static volatile ULONGLONG sResolution;
static volatile ULONGLONG sResolutionSigDigs;
static const double   kNsPerSecd  = 1000000000.0;
static const LONGLONG kNsPerSec   = 1000000000;
static const LONGLONG kNsPerMillisec = 1000000;


// ----------------------------------------------------------------------------
// Global constants
// ----------------------------------------------------------------------------

// After this time we always recalibrate the skew.
//
// On most platforms QPC and GTC have not quit the same slope, so after some
// time the two values will disperse.  The 4s calibration interval has been
// chosen mostly arbitrarily based on tests.
//
// Mostly, 4 seconds has been chosen based on the sleep/wake issue - timers
// shift after wakeup.  I wanted to make the time as reasonably short as
// possible to always recalibrate after even a very short standby time (quit
// reasonable test case).  So, there is a lot of space to prolong it
// to say 20 seconds or even more, needs testing in the field, though.
//
// Value is number of [ms].
static const ULONGLONG kCalibrationInterval = 4000;

// On every read of QPC we check the overflow of skew difference doesn't go
// over this number of milliseconds.  Both timer functions jitter so we have
// to have some limit.  The value is based on tests.
//
// Changing kCalibrationInterval influences this limit: prolonging
// just kCalibrationInterval means to be more sensitive to threshold overflows.
//
// How this constant is used (also see CheckCalibration function):
// First, adjust the limit linearly to the calibration interval:
//   LIMIT = (GTC_now - GTC_calib) / kCalibrationInterval
// Then, check the skew difference overflow is in this adjusted limit:
//   ABS((QPC_now - GTC_now) - (QPC_calib - GTC_calib)) - THRESHOLD < LIMIT
//
// Thresholds are calculated dynamically, see sUnderrunThreshold and
// sOverrunThreshold below.
//
// Value is number of [ms].
static const ULONGLONG kOverflowLimit = 100;

// If we are not able to get the value of GTC time increment, use this value
// which is the most usual increment.
static const DWORD kDefaultTimeIncrement = 156001;

// ----------------------------------------------------------------------------
// Global variables, not changing at runtime
// ----------------------------------------------------------------------------

/**
 * The [mt] unit:
 *
 * Many values are kept in ticks of the Performance Coutner x 1000,
 * further just referred as [mt], meaning milli-ticks.
 *
 * This is needed to preserve maximum precision of the performance frequency
 * representation.  GetTickCount values in milliseconds are multiplied with
 * frequency per second.  Therefor we need to multiply QPC value by 1000 to
 * have the same units to allow simple arithmentic with both QPC and GTC.
 */

#define ms2mt(x) ((x) * sFrequencyPerSec)
#define mt2ms(x) ((x) / sFrequencyPerSec)
#define mt2ms_d(x) (double(x) / sFrequencyPerSec)

// Result of QueryPerformanceFrequency
static LONGLONG sFrequencyPerSec = 0;

// Lower and upper bound that QueryPerformanceCounter - GetTickCount must not
// go under or over when compared to the calibrated QPC - GTC difference (skew)
// Values are based on the GetTickCount update interval.
//
// Schematically, QPC works correctly if ((QPC_now - GTC_now) -
// (QPC_calib - GTC_calib)) is in  [sUnderrunThreshold, sOverrunThreshold]
// interval every time we access them.
//
// Kept in [mt]
static LONGLONG sUnderrunThreshold;
static LONGLONG sOverrunThreshold;

// ----------------------------------------------------------------------------
// Global lock
// ----------------------------------------------------------------------------

// Thread spin count before entering the full wait state for sTimeStampLock.
// Inspired by Rob Arnold's work on PRMJ_Now().
static const DWORD kLockSpinCount = 4096;

// Common mutex (thanks the relative complexity of the logic, this is better
// then using CMPXCHG8B.)
// It is protecting the globals bellow.
CRITICAL_SECTION sTimeStampLock;

// ----------------------------------------------------------------------------
// Globals heavily chaning at runtime, protected with sTimeStampLock mutex
// ----------------------------------------------------------------------------

// The calibrated difference between QPC and GTC.
//
// Kept in [mt]
static LONGLONG sSkew = 0;

// Keeps the last result we have returned from TickCount64 (bellow).  Protects
// from roll over and going backward.
//
// Kept in [ms]
static ULONGLONG sLastGTCResult = 0;

// Holder of the last result of our main hi-res function.  Protects from going
// backward.
//
// Kept in [mt]
static ULONGLONG sLastResult = 0;

// Time of the last performed calibration.
//
// Kept in [ms]
static ULONGLONG sLastCalibrated;

// After we have detected a run out of bounderies set this to true.  This
// then disallows use of QPC result for the hi-res timer.
static bool sFallBackToGTC = false;

// Set to true to force recalibration on QPC read.  This is generally set after
// system wake up, during which skew can change a lot.
static bool sForceRecalibrate = false;


namespace mozilla {


static ULONGLONG
CalibratedPerformanceCounter();


// ----------------------------------------------------------------------------
// Critical Section helper class
// ----------------------------------------------------------------------------

class AutoCriticalSection
{
public:
  AutoCriticalSection(LPCRITICAL_SECTION section)
    : mSection(section)
  {
    ::EnterCriticalSection(mSection);
  }
  ~AutoCriticalSection()
  {
    ::LeaveCriticalSection(mSection);
  }
private:
  LPCRITICAL_SECTION mSection;
};


// ----------------------------------------------------------------------------
// System standby and wakeup status observer.  Needed to ignore skew jump after
// the system has been woken up, happens mostly on XP.
// ----------------------------------------------------------------------------

class StandbyObserver : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

public:
  StandbyObserver()
  {
    LOG(("TimeStamp: StandByObserver::StandByObserver()"));
  }

  ~StandbyObserver()
  {
    LOG(("TimeStamp: StandByObserver::~StandByObserver()"));
  }

  static inline void Ensure()
  {
    if (sInitialized)
      return;

    // Not available to init on other then the main thread since using
    // the ObserverService.
    if (!NS_IsMainThread())
      return;

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs)
      return; // Too soon...

    sInitialized = true;

    nsRefPtr<StandbyObserver> observer = new StandbyObserver();
    obs->AddObserver(observer, "wake_notification", false);

    // There is no need to remove the observer, observer service is the only
    // referer and we don't hold reference back to the observer service.
  }

private:
  static bool sInitialized;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(StandbyObserver, nsIObserver)

bool
StandbyObserver::sInitialized = false;

NS_IMETHODIMP
StandbyObserver::Observe(nsISupports *subject,
                         const char *topic,
                         const PRUnichar *data)
{
  AutoCriticalSection lock(&sTimeStampLock);

  // Clear the potentiall fallback flag now and try using
  // QPC again after wake up.
  sFallBackToGTC = false;
  sForceRecalibrate = true;
  LOG(("TimeStamp: system has woken up, reset GTC fallback"));

  return NS_OK;
}


// ----------------------------------------------------------------------------
// The timer core implementation
// ----------------------------------------------------------------------------

static void
InitThresholds()
{
  DWORD timeAdjustment = 0, timeIncrement = 0;
  BOOL timeAdjustmentDisabled;
  GetSystemTimeAdjustment(&timeAdjustment,
                          &timeIncrement,
                          &timeAdjustmentDisabled);

  if (!timeIncrement)
    timeIncrement = kDefaultTimeIncrement;

  // Ceiling to a millisecond
  // Example values: 156001, 210000
  DWORD timeIncrementCeil = timeIncrement;
  // Don't want to round up if already rounded, values will be: 156000, 209999
  timeIncrementCeil -= 1;
  // Convert to ms, values will be: 15, 20
  timeIncrementCeil /= 10000;
  // Round up, values will be: 16, 21
  timeIncrementCeil += 1;
  // Convert back to 100ns, values will be: 160000, 210000
  timeIncrementCeil *= 10000;

  // How many milli-ticks has the interval
  LONGLONG ticksPerGetTickCountResolution =
    (PRInt64(timeIncrement) * sFrequencyPerSec) / 10000LL;

  // How many milli-ticks has the interval rounded up
  LONGLONG ticksPerGetTickCountResolutionCeiling =
    (PRInt64(timeIncrementCeil) * sFrequencyPerSec) / 10000LL;


  // I observed differences about 2 times of the GTC resolution.  GTC may
  // jump by 32 ms in two steps, therefor use the ceiling value.
  sUnderrunThreshold =
    LONGLONG((-2) * ticksPerGetTickCountResolutionCeiling);

  // QPC should go no further then 2 * GTC resolution
  sOverrunThreshold =
    LONGLONG((+2) * ticksPerGetTickCountResolution);
}

static void
InitResolution()
{
  // 10 total trials is arbitrary: what we're trying to avoid by
  // looping is getting unlucky and being interrupted by a context
  // switch or signal, or being bitten by paging/cache effects

  ULONGLONG minres = ~0ULL;
  int loops = 10;
  do {
    ULONGLONG start = CalibratedPerformanceCounter();
    ULONGLONG end = CalibratedPerformanceCounter();

    ULONGLONG candidate = (end - start);
    if (candidate < minres)
      minres = candidate;
  } while (--loops && minres);

  if (0 == minres) {
    minres = 1;
  }

  // Converting minres that is in [mt] to nanosecods, multiplicating
  // the argument to preserve resolution.
  ULONGLONG result = mt2ms(minres * kNsPerMillisec);
  if (0 == result) {
    result = 1;
  }

  sResolution = result;

  // find the number of significant digits in mResolution, for the
  // sake of ToSecondsSigDigits()
  ULONGLONG sigDigs;
  for (sigDigs = 1;
       !(sigDigs == result
         || 10*sigDigs > result);
       sigDigs *= 10);

  sResolutionSigDigs = sigDigs;
}

// Function protecting GetTickCount result from rolling over, result is in [ms]
// @param gtc
// Result of GetTickCount().  Passing it as an arg lets us call it out
// of the common mutex.
static inline ULONGLONG
TickCount64(DWORD now)
{
  ULONGLONG lastResultHiPart = sLastGTCResult & (~0ULL << 32);
  ULONGLONG result = lastResultHiPart | ULONGLONG(now);

  // It may happen that when accessing GTC on multiple threads the results
  // may differ (GTC value may be lower due to running before the others
  // right around the overflow moment).  That falsely shifts the high part.
  // Easiest solution is to check for a significant difference.

  if (sLastGTCResult > result) {
    if ((sLastGTCResult - result) > (1ULL << 31))
      result += 1ULL << 32;
    else
      result = sLastGTCResult;
  }

  sLastGTCResult = result;
  return result;
}

// Result is in [mt]
static inline ULONGLONG
PerformanceCounter()
{
  LARGE_INTEGER pc;
  ::QueryPerformanceCounter(&pc);
  return pc.QuadPart * 1000ULL;
}

// Called when we detect a larger deviation of QPC to disable it.
static inline void
RecordFlaw()
{
  sFallBackToGTC = true;

  LOG(("TimeStamp: falling back to GTC :("));

#if 0
  // This code has been disabled, because we:
  // 0. InitResolution must not be called under the lock (would reenter) while
  //    we shouldn't release it here just to allow it
  // 1. may return back to using QPC after system wake up
  // 2. InitResolution for GTC will probably return 0 anyway (increments
  //    only every 15 or 16 ms.)
  //
  // There is no need to drop sFrequencyPerSec to 1, result of TickCount64
  // is multiplied and later divided with sFrequencyPerSec.  Changing it
  // here may introduce sync problems.  Syncing access to sFrequencyPerSec
  // is overkill.  Drawback is we loose some bits from the upper bound of
  // the 64 bits timer value, usualy up to 7, it means the app cannot run
  // more then some 4'000'000 years :)
  InitResolution();
#endif
}

// Check the current skew is in bounderies and occasionally recalculate it.
// Return true if QPC is OK to use, return false to use GTC only.
//
// Arguments:
// overflow - the calculated overflow out of the bounderies for skew difference
// qpc - current value of QueryPerformanceCounter
// gtc - current value of GetTickCount, more actual according possible system
//       sleep between read of QPC and GTC
static inline bool
CheckCalibration(LONGLONG overflow, ULONGLONG qpc, ULONGLONG gtc)
{
  if (sFallBackToGTC) {
    // We are forbidden to use QPC
    return false;
  }

  ULONGLONG sinceLastCalibration = gtc - sLastCalibrated;

  if (overflow && !sForceRecalibrate) {
    // Calculate trend of the overflow to correspond to the calibration
    // interval, we may get here long after the last calibration because we
    // either didn't read the hi-res function or the system was suspended.
    ULONGLONG trend = LONGLONG(overflow *
      (double(kCalibrationInterval) / sinceLastCalibration));

    LOG(("TimeStamp: calibration after %llus with overflow %1.4fms"
         ", adjusted trend per calibration interval is %1.4fms",
         sinceLastCalibration / 1000,
         mt2ms_d(overflow),
         mt2ms_d(trend)));

    if (trend > ms2mt(kOverflowLimit)) {
      // This sets sFallBackToGTC, we have detected
      // an unreliability of QPC, stop using it.
      RecordFlaw();
      return false;
    }
  }

  if (sinceLastCalibration > kCalibrationInterval || sForceRecalibrate) {
    // Recalculate the skew now
    sSkew = qpc - ms2mt(gtc);
    sLastCalibrated = gtc;
    LOG(("TimeStamp: new skew is %1.2fms (force:%d)",
      mt2ms_d(sSkew), sForceRecalibrate));

    sForceRecalibrate = false;
  }

  return true;
}

// The main function.  Result is in [mt] ensuring to not go back and be mostly
// reliable with highest possible resolution.
static ULONGLONG
CalibratedPerformanceCounter()
{
  // XXX This is using ObserverService, cannot instantiate in the static
  // startup, really needs a better initation code here.
  StandbyObserver::Ensure();

  // Don't hold the lock over call to QueryPerformanceCounter, since it is
  // the largest bottleneck, let threads read the value concurently to have
  // possibly a better performance.

  ULONGLONG qpc = PerformanceCounter();
  DWORD gtcw = GetTickCount();

  AutoCriticalSection lock(&sTimeStampLock);

  // Rollover protection
  ULONGLONG gtc = TickCount64(gtcw);

  LONGLONG diff = qpc - ms2mt(gtc) - sSkew;
  LONGLONG overflow = 0;

  if (diff < sUnderrunThreshold) {
    overflow = sUnderrunThreshold - diff;
  }
  else if (diff > sOverrunThreshold) {
    overflow = diff - sOverrunThreshold;
  }

  ULONGLONG result = qpc;
  if (!CheckCalibration(overflow, qpc, gtc)) {
    // We are back on GTC, QPC has been observed unreliable
    result = ms2mt(gtc) + sSkew;
  }

#if 0
  LOG(("TimeStamp: result = %1.2fms, diff = %1.4fms",
      mt2ms_d(result), mt2ms_d(diff)));
#endif

  if (result > sLastResult)
    sLastResult = result;

  return sLastResult;
}

// ----------------------------------------------------------------------------
// TimeDuration and TimeStamp implementation
// ----------------------------------------------------------------------------

double
TimeDuration::ToSeconds() const
{
  return double(mValue) / (sFrequencyPerSec * 1000ULL);
}

double
TimeDuration::ToSecondsSigDigits() const
{
  AutoCriticalSection lock(&sTimeStampLock);

  // don't report a value < mResolution ...
  LONGLONG resolution = sResolution;
  LONGLONG resolutionSigDigs = sResolutionSigDigs;
  LONGLONG valueSigDigs = resolution * (mValue / resolution);
  // and chop off insignificant digits
  valueSigDigs = resolutionSigDigs * (valueSigDigs / resolutionSigDigs);
  return double(valueSigDigs) / kNsPerSecd;
}

TimeDuration
TimeDuration::FromMilliseconds(double aMilliseconds)
{
  return TimeDuration::FromTicks(PRInt64(ms2mt(aMilliseconds)));
}

TimeDuration
TimeDuration::Resolution()
{
  AutoCriticalSection lock(&sTimeStampLock);

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
  // Decide which implementation to use for the high-performance timer.

  InitializeCriticalSectionAndSpinCount(&sTimeStampLock, kLockSpinCount);

  LARGE_INTEGER freq;
  BOOL QPCAvailable = ::QueryPerformanceFrequency(&freq);
  if (!QPCAvailable) {
    // No Performance Counter.  Fall back to use GetTickCount.
    sFrequencyPerSec = 1;
    sFallBackToGTC = true;
    InitResolution();

    LOG(("TimeStamp: using GetTickCount"));
    return NS_OK;
  }

  sFrequencyPerSec = freq.QuadPart;

  ULONGLONG qpc = PerformanceCounter();
  sLastCalibrated = TickCount64(::GetTickCount());
  sSkew = qpc - ms2mt(sLastCalibrated);

  InitThresholds();
  InitResolution();

  LOG(("TimeStamp: initial skew is %1.2fms", mt2ms_d(sSkew)));

  return NS_OK;
}

void
TimeStamp::Shutdown()
{
  DeleteCriticalSection(&sTimeStampLock);
}

TimeStamp
TimeStamp::Now()
{
  return TimeStamp(PRUint64(CalibratedPerformanceCounter()));
}

} // namespace mozilla
