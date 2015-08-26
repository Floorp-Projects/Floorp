/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This program provides processor power estimates. It does this by reading
// model-specific registers (MSRs) that are part Intel's Running Average Power
// Limit (RAPL) interface. These MSRs provide good quality estimates of the
// energy consumption of up to four system components:
// - PKG: the entire processor package;
// - PP0: the cores (a subset of the package);
// - PP1: the GPU (a subset of the package);
// - DRAM: main memory.
//
// For more details about RAPL, see section 14.9 of Volume 3 of the "Intel 64
// and IA-32 Architecture's Software Developer's Manual", Order Number 325384.
//
// This program exists because there are no existing tools on Mac that can
// obtain all four RAPL estimates. (|powermetrics| can obtain the package
// estimate, but not the others. Intel Power Gadget can obtain the package and
// cores estimates.)
//
// On Linux |perf| can obtain all four estimates (as Joules, which are easily
// converted to Watts), but this program is implemented for Linux because it's
// not too hard to do, and that gives us multi-platform consistency.
//
// This program does not support Windows, unfortunately. It's not obvious how
// to access the RAPL MSRs on Windows.
//
// This program deliberately uses only standard libraries and avoids
// Mozilla-specific code, to make it easy to compile and test on different
// machines.

#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <numeric>
#include <vector>

//---------------------------------------------------------------------------
// Utilities
//---------------------------------------------------------------------------

// The value of argv[0] passed to main(). Used in error messages.
static const char* gArgv0;

static void
Abort(const char* aFormat, ...)
{
  va_list vargs;
  va_start(vargs, aFormat);
  fprintf(stderr, "%s: ", gArgv0);
  vfprintf(stderr, aFormat, vargs);
  fprintf(stderr, "\n");
  va_end(vargs);

  exit(1);
}

static void
CmdLineAbort(const char* aMsg)
{
  if (aMsg) {
    fprintf(stderr, "%s: %s\n", gArgv0, aMsg);
  }
  fprintf(stderr, "Use --help for more information.\n");
  exit(1);
}

// A special value that represents an estimate from an unsupported RAPL domain.
static const double kUnsupported_j = -1.0;

// Print to stdout and flush it, so that the output appears immediately even if
// being redirected through |tee| or anything like that.
static void
PrintAndFlush(const char* aFormat, ...)
{
  va_list vargs;
  va_start(vargs, aFormat);
  vfprintf(stdout, aFormat, vargs);
  va_end(vargs);

  fflush(stdout);
}

//---------------------------------------------------------------------------
// Mac-specific code
//---------------------------------------------------------------------------

#if defined(__APPLE__)

// Because of the pkg_energy_statistics_t::pkes_version check below, the
// earliest OS X version this code will work with is 10.9.0 (xnu-2422.1.72).

#include <sys/types.h>
#include <sys/sysctl.h>

// OS X has four kinds of system calls:
//
//  1. Mach traps;
//  2. UNIX system calls;
//  3. machine-dependent calls;
//  4. diagnostic calls.
//
// (See "Mac OS X and iOS Internals" by Jonathan Levin for more details.)
//
// The last category has a single call named diagCall() or diagCall64(). Its
// mode is controlled by its first argument, and one of the modes allows access
// to the Intel RAPL MSRs.
//
// The interface to diagCall64() is not exported, so we have to import some
// definitions from the XNU kernel. All imported definitions are annotated with
// the XNU source file they come from, and information about what XNU versions
// they were introduced in and (if relevant) modified.

// The diagCall64() mode.
// From osfmk/i386/Diagnostics.h
// - In 10.8.4 (xnu-2050.24.15) this value was introduced. (In 10.8.3 the value
//   17 was used for dgGzallocTest.)
#define dgPowerStat 17

// From osfmk/i386/cpu_data.h
// - In 10.8.5 these values were introduced, along with core_energy_stat_t.
#define CPU_RTIME_BINS (12)
#define CPU_ITIME_BINS (CPU_RTIME_BINS)

// core_energy_stat_t and pkg_energy_statistics_t are both from
// osfmk/i386/Diagnostics.c.
// - In 10.8.4 (xnu-2050.24.15) both structs were introduced, but with many
//   fewer fields.
// - In 10.8.5 (xnu-2050.48.11) both structs were substantially expanded, with
//   numerous new fields.
// - In 10.9.0 (xnu-2422.1.72) pkg_energy_statistics_t::pkes_version was added.
//   diagCall64(dgPowerStat) fills it with '1' in all versions since (up to
//   10.10.2 at time of writing).
// - in 10.10.2 (xnu-2782.10.72) core_energy_stat_t::gpmcs was conditionally
//   added, if DIAG_ALL_PMCS is true. (DIAG_ALL_PMCS is not even defined in the
//   source code, but it could be defined at compile-time via compiler flags.)
//   pkg_energy_statistics_t::pkes_version did not change, though.

typedef struct {
        uint64_t caperf;
        uint64_t cmperf;
        uint64_t ccres[6];
        uint64_t crtimes[CPU_RTIME_BINS];
        uint64_t citimes[CPU_ITIME_BINS];
        uint64_t crtime_total;
        uint64_t citime_total;
        uint64_t cpu_idle_exits;
        uint64_t cpu_insns;
        uint64_t cpu_ucc;
        uint64_t cpu_urc;
#if     DIAG_ALL_PMCS           // Added in 10.10.2 (xnu-2782.10.72).
        uint64_t gpmcs[4];      // Added in 10.10.2 (xnu-2782.10.72).
#endif /* DIAG_ALL_PMCS */      // Added in 10.10.2 (xnu-2782.10.72).
} core_energy_stat_t;

typedef struct {
        uint64_t pkes_version;  // Added in 10.9.0 (xnu-2422.1.72).
        uint64_t pkg_cres[2][7];

        // This is read from MSR 0x606, which Intel calls MSR_RAPL_POWER_UNIT
        // and XNU calls MSR_IA32_PKG_POWER_SKU_UNIT.
        uint64_t pkg_power_unit;

        // These are the four fields for the four RAPL domains. For each field
        // we list:
        //
        // - the corresponding MSR number;
        // - Intel's name for that MSR;
        // - XNU's name for that MSR;
        // - which Intel processors the MSR is supported on.
        //
        // The last of these is determined from chapter 35 of Volume 3 of the
        // "Intel 64 and IA-32 Architecture's Software Developer's Manual",
        // Order Number 325384. (Note that chapter 35 contradicts section 14.9
        // to some degree.)

        // 0x611 == MSR_PKG_ENERGY_STATUS == MSR_IA32_PKG_ENERGY_STATUS
        // Atom (various), Sandy Bridge, Next Gen Xeon Phi (model 0x57).
        uint64_t pkg_energy;

        // 0x639 == MSR_PP0_ENERGY_STATUS == MSR_IA32_PP0_ENERGY_STATUS
        // Atom (various), Sandy Bridge, Next Gen Xeon Phi (model 0x57).
        uint64_t pp0_energy;

        // 0x641 == MSR_PP1_ENERGY_STATUS == MSR_PP1_ENERGY_STATUS
        // Sandy Bridge, Haswell.
        uint64_t pp1_energy;

        // 0x619 == MSR_DRAM_ENERGY_STATUS == MSR_IA32_DDR_ENERGY_STATUS
        // Xeon E5, Xeon E5 v2, Haswell/Haswell-E, Next Gen Xeon Phi (model
        // 0x57)
        uint64_t ddr_energy;

        uint64_t llc_flushed_cycles;
        uint64_t ring_ratio_instantaneous;
        uint64_t IA_frequency_clipping_cause;
        uint64_t GT_frequency_clipping_cause;
        uint64_t pkg_idle_exits;
        uint64_t pkg_rtimes[CPU_RTIME_BINS];
        uint64_t pkg_itimes[CPU_ITIME_BINS];
        uint64_t mbus_delay_time;
        uint64_t mint_delay_time;
        uint32_t ncpus;
        core_energy_stat_t cest[];
} pkg_energy_statistics_t;

static int
diagCall64(uint64_t aMode, void* aBuf)
{
  // We cannot use syscall() here because it doesn't work with diagnostic
  // system calls -- it raises SIGSYS if you try. So we have to use asm.

#ifdef __x86_64__
  // The 0x40000 prefix indicates it's a diagnostic system call. The 0x01
  // suffix indicates the syscall number is 1, which also happens to be the
  // only diagnostic system call. See osfmk/mach/i386/syscall_sw.h for more
  // details.
  static const uint64_t diagCallNum = 0x4000001;
  uint64_t rv;

  __asm__ __volatile__(
    "syscall"

    // Return value goes in "a" (%rax).
    : /* outputs */ "=a"(rv)

    // The syscall number goes in "0", a synonym (from outputs) for "a" (%rax).
    // The syscall arguments go in "D" (%rdi) and "S" (%rsi).
    : /* inputs */ "0"(diagCallNum), "D"(aMode), "S"(aBuf)

    // The |syscall| instruction clobbers %rcx, %r11, and %rflags ("cc"). And
    // this particular syscall also writes memory (aBuf).
    : /* clobbers */ "rcx", "r11", "cc", "memory"
  );
  return rv;
#else
#error Sorry, only x86-64 is supported
#endif
}

static void
diagCall64_dgPowerStat(pkg_energy_statistics_t* aPkes)
{
  static const uint64_t supported_version = 1;

  // Write an unsupported version number into pkes_version so that the check
  // below cannot succeed by dumb luck.
  aPkes->pkes_version = supported_version - 1;

  // diagCall64() returns 1 on success, and 0 on failure (which can only happen
  // if the mode is unrecognized, e.g. in 10.7.x or earlier versions).
  if (diagCall64(dgPowerStat, aPkes) != 1) {
    Abort("diagCall64() failed");
  }

  if (aPkes->pkes_version != 1) {
    Abort("unexpected pkes_version: %llu", aPkes->pkes_version);
  }
}

class RAPL
{
  bool mIsGpuSupported;   // Is the GPU domain supported by the processor?
  bool mIsRamSupported;   // Is the RAM domain supported by the processor?

  // The DRAM domain on Haswell servers has a fixed energy unit (1/65536 J ==
  // 15.3 microJoules) which is different to the power unit MSR. (See the
  // "Intel Xeon Processor E5-1600 and E5-2600 v3 Product Families, Volume 2 of
  // 2, Registers" datasheet, September 2014, Reference Number: 330784-001.)
  // This field records whether the quirk is present.
  bool mHasRamUnitsQuirk;

  // The abovementioned 15.3 microJoules value.
  static const double kQuirkyRamJoulesPerTick;

  // The previous sample's MSR values.
  uint64_t mPrevPkgTicks;
  uint64_t mPrevPp0Ticks;
  uint64_t mPrevPp1Ticks;
  uint64_t mPrevDdrTicks;

  // The struct passed to diagCall64().
  pkg_energy_statistics_t* mPkes;

public:
  RAPL()
    : mHasRamUnitsQuirk(false)
  {
    // Work out which RAPL MSRs this CPU model supports.
    int cpuModel;
    size_t size = sizeof(cpuModel);
    if (sysctlbyname("machdep.cpu.model", &cpuModel, &size, NULL, 0) != 0) {
      Abort("sysctlbyname(\"machdep.cpu.model\") failed");
    }

    // This is similar to arch/x86/kernel/cpu/perf_event_intel_rapl.c in
    // linux-4.1.5/.
    switch (cpuModel) {
      case 60:  // 0x3c: Haswell
      case 69:  // 0x45: Haswell-Celeron
      case 70:  // 0x46: Haswell
      case 61:  // 0x3d: Broadwell
        // Supports package, cores, GPU, RAM.
        mIsGpuSupported = true;
        mIsRamSupported = true;
        break;

      case 42:  // 0x2a: Sandy Bridge
      case 58:  // 0x3a: Ivy Bridge
        // Supports package, cores, GPU.
        mIsGpuSupported = true;
        mIsRamSupported = false;
        break;

      case 63:  // 0x3f: Haswell-Server
        mHasRamUnitsQuirk = true;
        // FALLTHROUGH
      case 45:  // 0x2d: Sandy Bridge-EP
      case 62:  // 0x3e: Ivy Bridge-E
        // Supports package, cores, RAM.
        mIsGpuSupported = false;
        mIsRamSupported = true;
        break;

      default:
        Abort("unknown CPU model: %d", cpuModel);
        break;
    }

    // Get the maximum number of logical CPUs so that we know how big to make
    // |mPkes|.
    int logicalcpu_max;
    size = sizeof(logicalcpu_max);
    if (sysctlbyname("hw.logicalcpu_max",
                     &logicalcpu_max, &size, NULL, 0) != 0) {
      Abort("sysctlbyname(\"hw.logicalcpu_max\") failed");
    }

    // Over-allocate by 1024 bytes per CPU to allow for the uncertainty around
    // core_energy_stat_t::gpmcs and for any other future extensions to that
    // struct. (The fields we read all come before the core_energy_stat_t
    // array, so it won't matter to us whether gpmcs is present or not.)
    size_t pkesSize = sizeof(pkg_energy_statistics_t) +
                      logicalcpu_max * sizeof(core_energy_stat_t) +
                      logicalcpu_max * 1024;
    mPkes = (pkg_energy_statistics_t*) malloc(pkesSize);
    if (!mPkes) {
      Abort("malloc() failed");
    }

    // Do an initial measurement so that the first sample's diffs are sensible.
    double dummy1, dummy2, dummy3, dummy4;
    EnergyEstimates(dummy1, dummy2, dummy3, dummy4);
  }

  ~RAPL()
  {
    free(mPkes);
  }

  static double Joules(uint64_t aTicks, double aJoulesPerTick)
  {
    return double(aTicks) * aJoulesPerTick;
  }

  void EnergyEstimates(double& aPkg_J, double& aCores_J, double& aGpu_J,
                       double& aRam_J)
  {
    diagCall64_dgPowerStat(mPkes);

    // Bits 12:8 are the ESU.
    // Energy measurements come in multiples of 1/(2^ESU).
    uint32_t energyStatusUnits = (mPkes->pkg_power_unit >> 8) & 0x1f;
    double joulesPerTick = ((double)1 / (1 << energyStatusUnits));

    aPkg_J   = Joules(mPkes->pkg_energy - mPrevPkgTicks, joulesPerTick);
    aCores_J = Joules(mPkes->pp0_energy - mPrevPp0Ticks, joulesPerTick);
    aGpu_J   = mIsGpuSupported
             ? Joules(mPkes->pp1_energy - mPrevPp1Ticks, joulesPerTick)
             : -kUnsupported_j;
    aRam_J   = mIsRamSupported
             ? Joules(mPkes->ddr_energy - mPrevDdrTicks,
                      mHasRamUnitsQuirk ? kQuirkyRamJoulesPerTick
                                        : joulesPerTick)
             : -kUnsupported_j;

    mPrevPkgTicks = mPkes->pkg_energy;
    mPrevPp0Ticks = mPkes->pp0_energy;
    if (mIsGpuSupported) {
      mPrevPp1Ticks = mPkes->pp1_energy;
    }
    if (mIsRamSupported) {
      mPrevDdrTicks = mPkes->ddr_energy;
    }
  }
};

/* static */ const double RAPL::kQuirkyRamJoulesPerTick = (double)1 / 65536;

//---------------------------------------------------------------------------
// Linux-specific code
//---------------------------------------------------------------------------

#elif defined(__linux__)

#include <linux/perf_event.h>
#include <sys/syscall.h>

// There is no glibc wrapper for this system call so we provide our own.
static int
perf_event_open(struct perf_event_attr* aAttr, pid_t aPid, int aCpu,
                int aGroupFd, unsigned long aFlags)
{
  return syscall(__NR_perf_event_open, aAttr, aPid, aCpu, aGroupFd, aFlags);
}

// Returns false if the file cannot be opened.
template <typename T>
static bool
ReadValueFromPowerFile(const char* aStr1, const char* aStr2, const char* aStr3,
                       const char* aScanfString, T* aOut)
{
  // The filenames going into this buffer are under our control and the longest
  // one is "/sys/bus/event_source/devices/power/events/energy-cores.scale".
  // So 256 chars is plenty.
  char filename[256];

  sprintf(filename, "/sys/bus/event_source/devices/power/%s%s%s",
          aStr1, aStr2, aStr3);
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    return false;
  }
  if (fscanf(fp, aScanfString, aOut) != 1) {
    Abort("fscanf() failed");
  }
  fclose(fp);

  return true;
}

// This class encapsulates the reading of a single RAPL domain.
class Domain
{
  bool mIsSupported;      // Is the domain supported by the processor?

  // These three are only set if |mIsSupported| is true.
  double mJoulesPerTick;  // How many Joules each tick of the MSR represents.
  int mFd;                // The fd through which the MSR is read.
  double mPrevTicks;      // The previous sample's MSR value.

public:
  enum IsOptional { Optional, NonOptional };

  Domain(const char* aName, uint32_t aType, IsOptional aOptional = NonOptional)
  {
    uint64_t config;
    if (!ReadValueFromPowerFile("events/energy-", aName, "", "event=%llx",
         &config)) {
      // Failure is allowed for optional domains.
      if (aOptional == NonOptional) {
        Abort("failed to open file for non-optional domain '%s'", aName);
      }
      mIsSupported = false;
      return;
    }

    mIsSupported = true;

    ReadValueFromPowerFile("events/energy-", aName, ".scale", "%lf",
                           &mJoulesPerTick);

    // The unit should be "Joules", so 128 chars should be plenty.
    char unit[128];
    ReadValueFromPowerFile("events/energy-", aName, ".unit", "%127s", unit);
    if (strcmp(unit, "Joules") != 0) {
      Abort("unexpected unit '%s' in .unit file", unit);
    }

    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = aType;
    attr.size = uint32_t(sizeof(attr));
    attr.config = config;

    // Measure all processes/threads. The specified CPU doesn't matter.
    mFd = perf_event_open(&attr, /* pid = */ -1, /* cpu = */ 0,
                          /* group_fd = */ -1, /* flags = */ 0);
    if (mFd < 0) {
      Abort("perf_event_open() failed\n"
            "Did you run as root or "
            "set /proc/sys/kernel/perf_event_paranoid to 0?");
    }

    mPrevTicks = 0;
  }

  ~Domain()
  {
    if (mIsSupported) {
      close(mFd);
    }
  }

  double EnergyEstimate()
  {
    if (!mIsSupported) {
      return -kUnsupported_j;
    }

    uint64_t thisTicks;
    if (read(mFd, &thisTicks, sizeof(uint64_t)) != sizeof(uint64_t)) {
      Abort("read() failed");
    }

    uint64_t ticks = thisTicks - mPrevTicks;
    mPrevTicks = thisTicks;
    double joules = ticks * mJoulesPerTick;
    return joules;
  }
};

class RAPL
{
  Domain* mPkg;
  Domain* mCores;
  Domain* mGpu;
  Domain* mRam;

public:
  RAPL()
  {
    uint32_t type;
    ReadValueFromPowerFile("type", "", "", "%u", &type);

    mPkg   = new Domain("pkg",   type);
    mCores = new Domain("cores", type);
    mGpu   = new Domain("gpu",   type, Domain::Optional);
    mRam   = new Domain("ram",   type, Domain::Optional);
    if (!mPkg || !mCores || !mGpu || !mRam) {
      Abort("new Domain() failed");
    }
  }

  ~RAPL()
  {
    delete mPkg;
    delete mCores;
    delete mGpu;
    delete mRam;
  }

  void EnergyEstimates(double& aPkg_J, double& aCores_J, double& aGpu_J,
                       double& aRam_J)
  {
    aPkg_J   = mPkg->EnergyEstimate();
    aCores_J = mCores->EnergyEstimate();
    aGpu_J   = mGpu->EnergyEstimate();
    aRam_J   = mRam->EnergyEstimate();
  }
};

#else

//---------------------------------------------------------------------------
// Unsupported platforms
//---------------------------------------------------------------------------

#error Sorry, this platform is not supported

#endif // platform

//---------------------------------------------------------------------------
// The main loop
//---------------------------------------------------------------------------

// The sample interval, measured in seconds.
static double gSampleInterval_sec;

// The platform-specific RAPL-reading machinery.
static RAPL* gRapl;

// All the sampled "total" values, in Watts.
static std::vector<double> gTotals_W;

// Power = Energy / Time, where power is measured in Watts, Energy is measured
// in Joules, and Time is measured in seconds.
static double
JoulesToWatts(double aJoules)
{
  return aJoules / gSampleInterval_sec;
}

// "Normalize" here means convert kUnsupported_j to zero so it can be used in
// additive expressions. All printed values are 5 or maybe 6 chars (though 6
// chars would require a value > 100 W, which is unlikely).
static void
NormalizeAndPrintAsWatts(char* aBuf, double& aValue_J)
{
  if (aValue_J == kUnsupported_j) {
    aValue_J = 0;
    sprintf(aBuf, "%s", " n/a ");
  } else {
    sprintf(aBuf, "%5.2f", JoulesToWatts(aValue_J));
  }
}

static void
SigAlrmHandler(int aSigNum, siginfo_t* aInfo, void* aContext)
{
  static int sampleNumber = 1;

  double pkg_J, cores_J, gpu_J, ram_J;
  gRapl->EnergyEstimates(pkg_J, cores_J, gpu_J, ram_J);

  // We should have pkg and cores estimates, but might not have gpu and ram
  // estimates.
  assert(pkg_J   != kUnsupported_j);
  assert(cores_J != kUnsupported_j);

  // This needs to be big enough to print watt values to two decimal places. 16
  // should be plenty.
  static const size_t kNumStrLen = 16;

  static char pkgStr[kNumStrLen], coresStr[kNumStrLen], gpuStr[kNumStrLen],
              ramStr[kNumStrLen];
  NormalizeAndPrintAsWatts(pkgStr,   pkg_J);
  NormalizeAndPrintAsWatts(coresStr, cores_J);
  NormalizeAndPrintAsWatts(gpuStr,   gpu_J);
  NormalizeAndPrintAsWatts(ramStr,   ram_J);

  // Core and GPU power are a subset of the package power.
  assert(pkg_J >= cores_J + gpu_J);

  // Compute "other" (i.e. rest of the package) and "total" only after the
  // other values have been normalized.

  char otherStr[kNumStrLen];
  double other_J = pkg_J - cores_J - gpu_J;
  NormalizeAndPrintAsWatts(otherStr, other_J);

  char totalStr[kNumStrLen];
  double total_J = pkg_J + ram_J;
  NormalizeAndPrintAsWatts(totalStr, total_J);

  gTotals_W.push_back(JoulesToWatts(total_J));

  // Print and flush so that the output appears immediately even if being
  // redirected through |tee| or anything like that.
  PrintAndFlush("#%02d %s W = %s (%s + %s + %s) + %s W\n",
                sampleNumber++, totalStr, pkgStr, coresStr, gpuStr, otherStr,
                ramStr);
}

static void
Finish()
{
  size_t n = gTotals_W.size();

  // This time calculation assumes that the timers are perfectly accurate which
  // is not true but the inaccuracy should be small in practice.
  double time = n * gSampleInterval_sec;

  printf("\n");
  printf("%d sample%s taken over a period of %.3f second%s\n",
    int(n), n == 1 ? "" : "s",
    n * gSampleInterval_sec, time == 1.0 ? "" : "s");

  if (n == 0) {
    exit(0);
  }

  // Compute the mean.
  double sum = std::accumulate(gTotals_W.begin(), gTotals_W.end(), 0);
  double mean = sum / n;

  // Compute the *population* standard deviation:
  //
  //   popStdDev = sqrt(Sigma(x - m)^2 / n)
  //
  // where |x| is the sum variable, |m| is the mean, and |n| is the
  // population size.
  //
  // This is different from the *sample* standard deviation, which divides by
  // |n - 1|, and would be appropriate if we were using a random sample of a
  // larger population.
  double sumOfSquaredDeviations = 0;
  for (auto iter = gTotals_W.begin(); iter != gTotals_W.end(); ++iter) {
    double deviation = (*iter - mean);
    sumOfSquaredDeviations += deviation * deviation;
  }
  double popStdDev = sqrt(sumOfSquaredDeviations / n);

  // Sort so that percentiles can be determined. We use the "Nearest Rank"
  // method of determining percentiles, which is simplest to compute and which
  // chooses values from those that appear in the input set.
  std::sort(gTotals_W.begin(), gTotals_W.end());

  printf("\n");
  printf("Distribution of 'total' values:\n");
  printf("            mean = %5.2f W\n", mean);
  printf("         std dev = %5.2f W\n", popStdDev);
  printf("  0th percentile = %5.2f W (min)\n", gTotals_W[0]);
  printf("  5th percentile = %5.2f W\n", gTotals_W[ceil(0.05 * n) - 1]);
  printf(" 25th percentile = %5.2f W\n", gTotals_W[ceil(0.25 * n) - 1]);
  printf(" 50th percentile = %5.2f W\n", gTotals_W[ceil(0.50 * n) - 1]);
  printf(" 75th percentile = %5.2f W\n", gTotals_W[ceil(0.75 * n) - 1]);
  printf(" 95th percentile = %5.2f W\n", gTotals_W[ceil(0.95 * n) - 1]);
  printf("100th percentile = %5.2f W (max)\n", gTotals_W[n - 1]);

  exit(0);
}

static void
SigIntHandler(int aSigNum, siginfo_t* aInfo, void *aContext)
{
  Finish();
}

static void
PrintUsage()
{
  printf(
"usage: rapl [options]\n"
"\n"
"Options:\n"
"\n"
"  -h --help                 show this message\n"
"  -i --sample-interval <N>  sample every N ms [default=1000]\n"
"  -n --sample-count <N>     get N samples (0 means unlimited) [default=0]\n"
"\n"
#if defined(__APPLE__)
"On Mac this program can be run by any user.\n"
#elif defined(__linux__)
"On Linux this program can only be run by the super-user unless the contents\n"
"of /proc/sys/kernel/perf_event_paranoid is set to 0 or lower.\n"
#else
#error Sorry, this platform is not supported
#endif
"\n"
  );
}

int
main(int argc, char** argv)
{
  // Process command line options.

  gArgv0 = argv[0];

  // Default values.
  int sampleInterval_msec = 1000;
  int sampleCount = 0;

  struct option longOptions[] = {
    { "help",            no_argument,       NULL, 'h' },
    { "sample-interval", required_argument, NULL, 'i' },
    { "sample-count",    required_argument, NULL, 'n' },
    { NULL,              0,                 NULL, 0   }
  };
  const char* shortOptions = "hi:n:";

  int c;
  char* endPtr;
  while ((c = getopt_long(argc, argv, shortOptions, longOptions, NULL)) != -1) {
    switch (c) {
      case 'h':
        PrintUsage();
        exit(0);

      case 'i':
        sampleInterval_msec = strtol(optarg, &endPtr, /* base = */ 10);
        if (*endPtr) {
          CmdLineAbort("sample interval is not an integer");
        }
        if (sampleInterval_msec < 1 || sampleInterval_msec > 3600000) {
          CmdLineAbort("sample interval must be in the range 1..3600000 ms");
        }
        break;

      case 'n':
        sampleCount = strtol(optarg, &endPtr, /* base = */ 10);
        if (*endPtr) {
          CmdLineAbort("sample count is not an integer");
        }
        if (sampleCount < 0 || sampleCount > 1000000) {
          CmdLineAbort("sample count must be in the range 0..1000000");
        }
        break;

      default:
        CmdLineAbort(NULL);
    }
  }

  // The RAPL MSRs update every ~1 ms, but the measurement period isn't exactly
  // 1 ms, which means the sample periods are not exact. "Power Measurement
  // Techniques on Standard Compute Nodes: A Quantitative Comparison" by
  // Hackenberg et al. suggests the following.
  //
  //   "RAPL provides energy (and not power) consumption data without
  //   timestamps associated to each counter update. This makes sampling rates
  //   above 20 Samples/s unfeasible if the systematic error should be below
  //   5%... Constantly polling the RAPL registers will both occupy a processor
  //   core and distort the measurement itself."
  //
  // So warn about this case.
  if (sampleInterval_msec < 50) {
    fprintf(stderr,
            "\nWARNING: sample intervals < 50 ms are likely to produce "
            "inaccurate estimates\n\n");
  }
  gSampleInterval_sec = double(sampleInterval_msec) / 1000;

  // Initialize the platform-specific RAPL reading machinery.
  gRapl = new RAPL();
  if (!gRapl) {
    Abort("new RAPL() failed");
  }

  // Install the signal handlers.

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigemptyset(&sa.sa_mask) < 0) {
    Abort("sigemptyset() failed");
  }
  sa.sa_sigaction = SigAlrmHandler;
  if (sigaction(SIGALRM, &sa, NULL) < 0) {
    Abort("sigaction(SIGALRM) failed");
  }
  sa.sa_sigaction = SigIntHandler;
  if (sigaction(SIGINT, &sa, NULL) < 0) {
    Abort("sigaction(SIGINT) failed");
  }

  // Set up the timer.
  struct itimerval timer;
  timer.it_interval.tv_sec = sampleInterval_msec / 1000;
  timer.it_interval.tv_usec = (sampleInterval_msec % 1000) * 1000;
  timer.it_value = timer.it_interval;
  if (setitimer(ITIMER_REAL, &timer, NULL) < 0) {
    Abort("setitimer() failed");
  }

  // Print header.
  PrintAndFlush("    total W = _pkg_ (cores + _gpu_ + other) + _ram_ W\n");

  // Take samples.
  if (sampleCount == 0) {
    while (true) {
      pause();
    }
  } else {
    for (int i = 0; i < sampleCount; i++) {
      pause();
    }
  }

  Finish();

  return 0;
}

