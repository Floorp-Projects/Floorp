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

class PowerCounters {
 public:
#if defined(_MSC_VER)
  explicit PowerCounters();
  ~PowerCounters();
  void Sample();
#else
  explicit PowerCounters(){};
  ~PowerCounters(){};
  void Sample(){};
#endif

  using CountVector = mozilla::Vector<BaseProfilerCount*, 4>;
  const CountVector& GetCounters() { return mCounters; }

 private:
  CountVector mCounters;

#if defined(_MSC_VER)
  mozilla::Vector<mozilla::UniquePtr<PowerMeterDevice>> mPowerMeterDevices;
#endif
};

#endif /* ndef TOOLS_POWERCOUNTERS_H_ */
