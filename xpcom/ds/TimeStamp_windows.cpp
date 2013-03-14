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

#include "mozilla/MathAlgorithms.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include <windows.h>

#include "prlog.h"
#include <stdio.h>

#include <intrin.h>

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
static PRLogModuleInfo*
GetTimeStampLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("TimeStampWindows");
  return sLog;
}
  #define LOG(x)  PR_LOG(GetTimeStampLog(), PR_LOG_DEBUG, x)
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

// If QPC is found faulty for two stamps in this interval, we disable it
// completely.
//
// Values is in [ms].
static const uint32_t kQPCHardFailureDetectionInterval = 2000;

// On every use of QPC values we check the overflow of skew difference of the
// two stamps doesn't go over this number of milliseconds.  Both timer
// functions jitter so we have to have some limit.  The value is based on tests.
//
// Changing kQPCHardFailureDetectionInterval influences this limit: prolonging
// just kQPCHardFailureDetectionInterval means to be more sensitive to threshold
// overflows.
//
// How this constant is used (see CheckQPC function):
// First, adjust the limit linearly to the check interval:
//   LIMIT = (GTC_now - GTC_epoch) / kQPCHardFailureDetectionInterval
// Then, check the skew difference overflow is in this adjusted limit:
//   ABS( (QPC_now - GTC_now) - (QPC_epoch - GTC_epoch) ) - THRESHOLD < LIMIT
//
// Thresholds are calculated dynamically, see sUnderrunThreshold and
// sOverrunThreshold below.
//
// Limit is in number of [ms].
static const ULONGLONG kOverflowLimit = 50;

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
#define mt2ms_f(x) (double(x) / sFrequencyPerSec)

// Result of QueryPerformanceFrequency
static LONGLONG sFrequencyPerSec = 0;

// Lower and upper bound that QueryPerformanceCounter - GetTickCount must not
// go under or over when compared to any older QPC - GTC difference (skew).
// Values are based on the GetTickCount update interval.
//
// Schematically, QPC works correctly if ((QPC_now - GTC_now) -
// (QPC_epoch - GTC_epoch)) is in  [sUnderrunThreshold, sOverrunThreshold]
// interval every time we compare two time stamps.
//
// Kept in [mt]
static LONGLONG sUnderrunThreshold;
static LONGLONG sOverrunThreshold;

// Interval to return duration using QPC.  When two time stamps
// are within this interval, perform QPC check first.
//
// Kept in [mt]
static LONGLONG sQPCHardFailureDetectionInterval;

// Flag for stable TSC that indicates platform where QPC is stable.
static bool sHasStableTSC = false;

// ----------------------------------------------------------------------------
// Global state variables, changing at runtime
// ----------------------------------------------------------------------------

// Initially true, set to false when QPC is found unstable and never
// returns back to true since that time.
static bool volatile sUseQPC = true;

// ----------------------------------------------------------------------------
// Global lock
// ----------------------------------------------------------------------------

// Thread spin count before entering the full wait state for sTimeStampLock.
// Inspired by Rob Arnold's work on PRMJ_Now().
static const DWORD kLockSpinCount = 4096;

// Common mutex (thanks the relative complexity of the logic, this is better
// then using CMPXCHG8B.)
// It is protecting the globals bellow.
static CRITICAL_SECTION sTimeStampLock;

// Used only when GetTickCount64 is not available on the platform.
// Last result of GetTickCount call.
//
// Kept in [ms]
static DWORD sLastGTCResult = 0;

// Higher part of the 64-bit value of MozGetTickCount64,
// incremented atomically.
static DWORD sLastGTCRollover = 0;

namespace mozilla {

typedef ULONGLONG (WINAPI* GetTickCount64_t)();
static GetTickCount64_t sGetTickCount64 = nullptr;

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

// Function protecting GetTickCount result from rolling over,
// result is in [ms]
static ULONGLONG WINAPI
MozGetTickCount64()
{
  DWORD GTC = ::GetTickCount();

  // Cheaper then CMPXCHG8B
  AutoCriticalSection lock(&sTimeStampLock);

  // Pull the rollover counter forward only if new value of GTC goes way
  // down under the last saved result
  if ((sLastGTCResult > GTC) && ((sLastGTCResult - GTC) > (1UL << 30)))
    ++sLastGTCRollover;

  sLastGTCResult = GTC;
  return ULONGLONG(sLastGTCRollover) << 32 | sLastGTCResult;
}

// Result is in [mt]
static inline ULONGLONG
PerformanceCounter()
{
  LARGE_INTEGER pc;
  ::QueryPerformanceCounter(&pc);
  return pc.QuadPart * 1000ULL;
}

static void
InitThresholds()
{
  DWORD timeAdjustment = 0, timeIncrement = 0;
  BOOL timeAdjustmentDisabled;
  GetSystemTimeAdjustment(&timeAdjustment,
                          &timeIncrement,
                          &timeAdjustmentDisabled);

  LOG(("TimeStamp: timeIncrement=%d [100ns]", timeIncrement));

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
    (int64_t(timeIncrement) * sFrequencyPerSec) / 10000LL;

  // How many milli-ticks has the interval rounded up
  LONGLONG ticksPerGetTickCountResolutionCeiling =
    (int64_t(timeIncrementCeil) * sFrequencyPerSec) / 10000LL;

  // I observed differences about 2 times of the GTC resolution.  GTC may
  // jump by 32 ms in two steps, therefor use the ceiling value.
  // Having 64 (15.6 or 16 * 4 exactly) is used to avoid false negatives
  // for very short times where QPC and GTC may jitter even more.
  sUnderrunThreshold =
    LONGLONG((-4) * ticksPerGetTickCountResolutionCeiling);

  // QPC should go no further than 2 * GTC resolution.
  sOverrunThreshold =
    LONGLONG((+4) * ticksPerGetTickCountResolution);

  sQPCHardFailureDetectionInterval =
    LONGLONG(kQPCHardFailureDetectionInterval) * sFrequencyPerSec;
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
    ULONGLONG start = PerformanceCounter();
    ULONGLONG end = PerformanceCounter();

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

// ----------------------------------------------------------------------------
// TimeStampValue implementation
// ----------------------------------------------------------------------------

TimeStampValue::TimeStampValue(_SomethingVeryRandomHere* nullValue)
  : mGTC(0)
  , mQPC(0)
  , mHasQPC(false)
  , mIsNull(true)
{
  MOZ_ASSERT(!nullValue);
}

TimeStampValue::TimeStampValue(ULONGLONG aGTC, ULONGLONG aQPC, bool aHasQPC)
  : mGTC(aGTC)
  , mQPC(aQPC)
  , mHasQPC(aHasQPC)
  , mIsNull(false)
{
}

TimeStampValue&
TimeStampValue::operator+=(const int64_t aOther)
{
  mGTC += aOther;
  mQPC += aOther;
  return *this;
}

TimeStampValue&
TimeStampValue::operator-=(const int64_t aOther)
{
  mGTC -= aOther;
  mQPC -= aOther;
  return *this;
}

// If the duration is less then one second, perform check of QPC stability
// by comparing both 'epoch' and 'now' skew (=GTC - QPC) values.
bool
TimeStampValue::CheckQPC(int64_t aDuration, const TimeStampValue &aOther) const
{
  if (!mHasQPC || !aOther.mHasQPC) // Not both holding QPC
    return false;

  if (sHasStableTSC) // For stable TSC there is no need to check
    return true;

  if (!sUseQPC) // QPC globally disabled
    return false;

  // Treat absolutely for calibration purposes
  aDuration = DeprecatedAbs(aDuration);

  // Check QPC is sane before using it.

  LONGLONG skew1 = mGTC - mQPC;
  LONGLONG skew2 = aOther.mGTC - aOther.mQPC;

  LONGLONG diff = skew1 - skew2;
  LONGLONG overflow;

  if (diff < sUnderrunThreshold)
    overflow = sUnderrunThreshold - diff;
  else if (diff > sOverrunThreshold)
    overflow = diff - sOverrunThreshold;
  else
    return true;

  ULONGLONG trend;
  if (aDuration)
    trend = LONGLONG(overflow * (double(sQPCHardFailureDetectionInterval) / aDuration));
  else
    trend = overflow;

  LOG(("TimeStamp: QPC check after %llums with overflow %1.4fms"
       ", adjusted trend per interval is %1.4fms",
       mt2ms(aDuration),
       mt2ms_f(overflow),
       mt2ms_f(trend)));

  if (trend <= ms2mt(kOverflowLimit)) {
    // We are in the limit, let go.
    return true;
  }

  // QPC deviates, don't use it.
  LOG(("TimeStamp: QPC found highly jittering"));

  if (aDuration < sQPCHardFailureDetectionInterval) {
    // Interval between the two time stamps is very short, consider
    // QPC as unstable and disable it completely.
    sUseQPC = false;
    LOG(("TimeStamp: QPC disabled"));
  }

  return false;
}

uint64_t
TimeStampValue::operator-(const TimeStampValue &aOther) const
{
  if (mIsNull && aOther.mIsNull)
    return uint64_t(0);

  if (CheckQPC(int64_t(mGTC - aOther.mGTC), aOther))
    return mQPC - aOther.mQPC;

  return mGTC - aOther.mGTC;
}

// ----------------------------------------------------------------------------
// TimeDuration and TimeStamp implementation
// ----------------------------------------------------------------------------

double
TimeDuration::ToSeconds() const
{
  // Converting before arithmetic avoids blocked store forward
  return double(mValue) / (double(sFrequencyPerSec) * 1000.0);
}

double
TimeDuration::ToSecondsSigDigits() const
{
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
  return TimeDuration::FromTicks(int64_t(ms2mt(aMilliseconds)));
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

static bool
HasStableTSC()
{
  union {
    int regs[4];
    struct {
      int nIds;
      char cpuString[12];
    };
  } cpuInfo;

  __cpuid(cpuInfo.regs, 0);
  // Only allow Intel CPUs for now
  // The order of the registers is reg[1], reg[3], reg[2].  We just adjust the
  // string so that we can compare in one go.
  if (_strnicmp(cpuInfo.cpuString, "GenuntelineI", sizeof(cpuInfo.cpuString)))
    return false;

  int regs[4];

  // detect if the Advanced Power Management feature is supported
  __cpuid(regs, 0x80000000);
  if (regs[0] < 0x80000007)
    return false;

  __cpuid(regs, 0x80000007);
  // if bit 8 is set than TSC will run at a constant rate
  // in all ACPI P-state, C-states and T-states
  return regs[3] & (1 << 8);
}

nsresult
TimeStamp::Startup()
{
  // Decide which implementation to use for the high-performance timer.

  HMODULE kernelDLL = GetModuleHandleW(L"kernel32.dll");
  sGetTickCount64 = reinterpret_cast<GetTickCount64_t>
    (GetProcAddress(kernelDLL, "GetTickCount64"));
  if (!sGetTickCount64) {
    // If the platform does not support the GetTickCount64 (Windows XP doesn't),
    // then use our fallback implementation based on GetTickCount.
    sGetTickCount64 = MozGetTickCount64;
  }

  InitializeCriticalSectionAndSpinCount(&sTimeStampLock, kLockSpinCount);

  sHasStableTSC = HasStableTSC();
  LOG(("TimeStamp: HasStableTSC=%d", sHasStableTSC));

  LARGE_INTEGER freq;
  sUseQPC = ::QueryPerformanceFrequency(&freq);
  if (!sUseQPC) {
    // No Performance Counter.  Fall back to use GetTickCount.
    InitResolution();

    LOG(("TimeStamp: using GetTickCount"));
    return NS_OK;
  }

  sFrequencyPerSec = freq.QuadPart;
  LOG(("TimeStamp: QPC frequency=%llu", sFrequencyPerSec));

  InitThresholds();
  InitResolution();

  return NS_OK;
}

void
TimeStamp::Shutdown()
{
  DeleteCriticalSection(&sTimeStampLock);
}

TimeStamp
TimeStamp::Now(bool aHighResolution)
{
  // sUseQPC is volatile
  bool useQPC = (aHighResolution && sUseQPC);

  // Both values are in [mt] units.
  ULONGLONG QPC = useQPC ? PerformanceCounter() : uint64_t(0);
  ULONGLONG GTC = ms2mt(sGetTickCount64());
  return TimeStamp(TimeStampValue(GTC, QPC, useQPC));
}

} // namespace mozilla
