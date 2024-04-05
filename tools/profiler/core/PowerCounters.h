/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLS_POWERCOUNTERS_H_
#define TOOLS_POWERCOUNTERS_H_

#include "PlatformMacros.h"
#include "mozilla/ProfilerCounts.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#if defined(_MSC_VER)
class PowerMeterDevice;
#endif
#if defined(GP_PLAT_arm64_darwin)
class ProcessPower;
#endif
#if defined(GP_PLAT_amd64_darwin)
class RAPL;
#endif
#if defined(GP_PLAT_arm64_android)

/*
 * These declarations come from:
 * https://cs.android.com/android/platform/superproject/main/+/main:external/perfetto/src/android_internal/power_stats.h;l=34-52;drc=1777bdef274bcfbccd4e6f8b6d00a1bac48a8645
 */

struct RailDescriptor {
  // Index corresponding to the rail
  uint32_t index;
  // Name of the rail
  char rail_name[64];
  // Name of the subsystem to which this rail belongs
  char subsys_name[64];
  // Hardware sampling rate
  uint32_t sampling_rate;
};

struct RailEnergyData {
  // Index corresponding to RailDescriptor.index
  uint32_t index;
  // Time since device boot(CLOCK_BOOTTIME) in milli-seconds
  uint64_t timestamp;
  // Accumulated energy since device boot in microwatt-seconds (uWs)
  uint64_t energy;
};
bool GetRailEnergyData(RailEnergyData*, size_t* size_of_arr);
#endif

class PowerCounters {
 public:
#if defined(_MSC_VER) || defined(GP_OS_darwin) || \
    defined(GP_PLAT_amd64_linux) || defined(GP_PLAT_arm64_android)
  explicit PowerCounters();
#else
  explicit PowerCounters(){};
#endif
#if defined(_MSC_VER) || defined(GP_PLAT_amd64_darwin) || \
    defined(GP_PLAT_arm64_android)
  ~PowerCounters();
#else
  ~PowerCounters() = default;
#endif
#if defined(_MSC_VER) || defined(GP_PLAT_amd64_darwin) || \
    defined(GP_PLAT_arm64_android)
  void Sample();
#else
  void Sample(){};
#endif

  using CountVector = mozilla::Vector<mozilla::UniquePtr<BaseProfilerCount>, 4>;
  const CountVector& GetCounters() { return mCounters; }

 private:
  CountVector mCounters;

#if defined(_MSC_VER)
  mozilla::Vector<mozilla::UniquePtr<PowerMeterDevice>> mPowerMeterDevices;
#endif
#if defined(GP_PLAT_amd64_darwin)
  mozilla::UniquePtr<RAPL> mRapl;
#endif
#if defined(GP_PLAT_arm64_android)
  void* mLibperfettoModule = nullptr;
  decltype(&GetRailEnergyData) mGetRailEnergyData = nullptr;
  mozilla::Vector<RailDescriptor> mRailDescriptors;
  mozilla::Vector<RailEnergyData> mRailEnergyData;
#endif
};

#endif /* ndef TOOLS_POWERCOUNTERS_H_ */
