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

#include "nsCRT.h"
#include "prlog.h"
#include "prenv.h"
#include "prprf.h"
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

// Tolerance to failures settings.
//
// What is the interval we want to have failure free.
// in [ms]
static const uint32_t kFailureFreeInterval = 5000;
// How many failures we are willing to tolerate in the interval.
static const uint32_t kMaxFailuresPerInterval = 4;
// What is the threshold to treat fluctuations as actual failures.
// in [ms]
static const uint32_t kFailureThreshold = 50;

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

// How much we are tolerant to GTC occasional loose of resoltion.
// This number says how many multiples of the minimal GTC resolution
// detected on the system are acceptable.  This number is empirical.
static const LONGLONG kGTCTickLeapTolerance = 4;

// Base tolerance (more: "inability of detection" range) threshold is calculated
// dynamically, and kept in sGTCResulutionThreshold.
//
// Schematically, QPC worked "100%" correctly if ((GTC_now - GTC_epoch) -
// (QPC_now - QPC_epoch)) was in  [-sGTCResulutionThreshold, sGTCResulutionThreshold]
// interval every time we'd compared two time stamps.
// If not, then we check the overflow behind this basic threshold
// is in kFailureThreshold.  If not, we condider it as a QPC failure.  If too many
// failures in short time are detected, QPC is considered faulty and disabled.
//
// Kept in [mt]
static LONGLONG sGTCResulutionThreshold;

// If QPC is found faulty for two stamps in this interval, we engage
// the fault detection algorithm.  For duration larger then this limit
// we bypass using durations calculated from QPC when jitter is detected,
// but don't touch the sUseQPC flag.
//
// Value is in [ms].
static const uint32_t kHardFailureLimit = 2000;
// Conversion to [mt]
static LONGLONG sHardFailureLimit;

// Conversion of kFailureFreeInterval and kFailureThreshold to [mt]
static LONGLONG sFailureFreeInterval;
static LONGLONG sFailureThreshold;

// ----------------------------------------------------------------------------
// Systemm status flags
// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------
// Global lock protected variables
// ----------------------------------------------------------------------------

// Timestamp in future until QPC must behave correctly.
// Set to now + kFailureFreeInterval on first QPC failure detection.
// Set to now + E * kFailureFreeInterval on following errors,
//   where E is number of errors detected during last kFailureFreeInterval
//   milliseconds, calculated simply as:
//   E = (sFaultIntoleranceCheckpoint - now) / kFailureFreeInterval + 1.
// When E > kMaxFailuresPerInterval -> disable QPC.
//
// Kept in [mt]
static ULONGLONG sFaultIntoleranceCheckpoint = 0;

// Used only when GetTickCount64 is not available on the platform.
// Last result of GetTickCount call.
//
// Kept in [ms]
static DWORD sLastGTCResult = 0;

// Higher part of the 64-bit value of MozGetTickCount64,
// incremented atomically.
static DWORD sLastGTCRollover = 0;

namespace mozilla {

TimeStamp TimeStamp::sFirstTimeStamp;
TimeStamp TimeStamp::sProcessCreation;

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

  // How many milli-ticks has the interval rounded up
  LONGLONG ticksPerGetTickCountResolutionCeiling =
    (int64_t(timeIncrementCeil) * sFrequencyPerSec) / 10000LL;

  // GTC may jump by 32 (2*16) ms in two steps, therefor use the ceiling value.
  sGTCResulutionThreshold =
    LONGLONG(kGTCTickLeapTolerance * ticksPerGetTickCountResolutionCeiling);

  sHardFailureLimit = ms2mt(kHardFailureLimit);
  sFailureFreeInterval = ms2mt(kFailureFreeInterval);
  sFailureThreshold = ms2mt(kFailureThreshold);
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

// If the duration is less then two seconds, perform check of QPC stability
// by comparing both GTC and QPC calculated durations of this and aOther.
uint64_t
TimeStampValue::CheckQPC(const TimeStampValue &aOther) const
{
  uint64_t deltaGTC = mGTC - aOther.mGTC;

  if (!mHasQPC || !aOther.mHasQPC) // Both not holding QPC
    return deltaGTC;

  uint64_t deltaQPC = mQPC - aOther.mQPC;

  if (sHasStableTSC) // For stable TSC there is no need to check
    return deltaQPC;

  if (!sUseQPC) // QPC globally disabled
    return deltaGTC;

  // Check QPC is sane before using it.
  int64_t diff = DeprecatedAbs(int64_t(deltaQPC) - int64_t(deltaGTC));
  if (diff <= sGTCResulutionThreshold)
    return deltaQPC;

  // Treat absolutely for calibration purposes
  int64_t duration = DeprecatedAbs(int64_t(deltaGTC));
  int64_t overflow = diff - sGTCResulutionThreshold;

  LOG(("TimeStamp: QPC check after %llums with overflow %1.4fms",
       mt2ms(duration), mt2ms_f(overflow)));

  if (overflow <= sFailureThreshold) // We are in the limit, let go.
    return deltaQPC; // XXX Should we return GTC here?

  // QPC deviates, don't use it, since now this method may only return deltaGTC.
  LOG(("TimeStamp: QPC jittered over failure threshold"));

  if (duration < sHardFailureLimit) {
    // Interval between the two time stamps is very short, consider
    // QPC as unstable and record a failure.
    uint64_t now = ms2mt(sGetTickCount64());

    AutoCriticalSection lock(&sTimeStampLock);

    if (sFaultIntoleranceCheckpoint && sFaultIntoleranceCheckpoint > now) {
      // There's already been an error in the last fault intollerant interval.
      // Time since now to the checkpoint actually holds information on how many
      // failures there were in the failure free interval we have defined.
      uint64_t failureCount = (sFaultIntoleranceCheckpoint - now + sFailureFreeInterval - 1) /
                               sFailureFreeInterval;
      if (failureCount > kMaxFailuresPerInterval) {
        sUseQPC = false;
        LOG(("TimeStamp: QPC disabled"));
      }
      else {
        // Move the fault intolerance checkpoint more to the future, prolong it
        // to reflect the number of detected failures.
        ++failureCount;
        sFaultIntoleranceCheckpoint = now + failureCount * sFailureFreeInterval;
        LOG(("TimeStamp: recording %dth QPC failure", failureCount));
      }
    }
    else {
      // Setup fault intolerance checkpoint in the future for first detected error.
      sFaultIntoleranceCheckpoint = now + sFailureFreeInterval;
      LOG(("TimeStamp: recording 1st QPC failure"));
    }
  }

  return deltaGTC;
}

uint64_t
TimeStampValue::operator-(const TimeStampValue &aOther) const
{
  if (mIsNull && aOther.mIsNull)
    return uint64_t(0);

  return CheckQPC(aOther);
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
  sFirstTimeStamp = TimeStamp::Now();
  sProcessCreation = TimeStamp();

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

// Computes and returns the current process uptime in microseconds.
// Returns 0 if an error was encountered while computing the uptime.

static uint64_t
ComputeProcessUptime()
{
  SYSTEMTIME nowSys;
  GetSystemTime(&nowSys);

  FILETIME now;
  bool success = SystemTimeToFileTime(&nowSys, &now);

  if (!success)
    return 0;

  FILETIME start, foo, bar, baz;
  success = GetProcessTimes(GetCurrentProcess(), &start, &foo, &bar, &baz);

  if (!success)
    return 0;

  ULARGE_INTEGER startUsec = {
    start.dwLowDateTime,
    start.dwHighDateTime
  };
  ULARGE_INTEGER nowUsec = {
    now.dwLowDateTime,
    now.dwHighDateTime
  };

  return (nowUsec.QuadPart - startUsec.QuadPart) / 10ULL;
}

TimeStamp
TimeStamp::ProcessCreation(bool& aIsInconsistent)
{
  aIsInconsistent = false;

  if (sProcessCreation.IsNull()) {
    char *mozAppRestart = PR_GetEnv("MOZ_APP_RESTART");
    TimeStamp ts;

    if (mozAppRestart) {
      ts = TimeStamp(TimeStampValue(nsCRT::atoll(mozAppRestart), 0, false));
    } else {
      TimeStamp now = TimeStamp::Now();
      uint64_t uptime = ComputeProcessUptime();
      ts = now - TimeDuration::FromMicroseconds(static_cast<double>(uptime));

      if ((ts > sFirstTimeStamp) || (uptime == 0)) {
        // If the process creation timestamp was inconsistent replace it with the
        // first one instead and notify that a telemetry error was detected.
        aIsInconsistent = true;
        ts = sFirstTimeStamp;
      }
    }

    sProcessCreation = ts;
  }

  return sProcessCreation;
}

void
TimeStamp::RecordProcessRestart()
{
  PR_SetEnv(PR_smprintf("MOZ_APP_RESTART=%lld", ms2mt(sGetTickCount64())));
  sProcessCreation = TimeStamp();
}

} // namespace mozilla
