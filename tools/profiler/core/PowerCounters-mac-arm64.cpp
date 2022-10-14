/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PowerCounters.h"

#include <mach/mach.h>

class ProcessPower final : public BaseProfilerCount {
 public:
  ProcessPower()
      : BaseProfilerCount("Process Power", nullptr, nullptr, "power",
                          "Power utilization") {}

  CountSample Sample() override {
    CountSample result;
    result.count = GetTaskEnergy();
    result.number = 0;
    result.isSampleNew = true;
    return result;
  }

 private:
  int64_t GetTaskEnergy() {
    task_power_info_v2_data_t task_power_info;
    mach_msg_type_number_t count = TASK_POWER_INFO_V2_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_POWER_INFO_V2,
                                 (task_info_t)&task_power_info, &count);
    if (kr != KERN_SUCCESS) {
      return 0;
    }

    // task_energy is in nanojoules. To be consistent with the Windows EMI
    // API, return values in picowatt-hour.
    return task_power_info.task_energy / 3.6;
  }
};

PowerCounters::PowerCounters() : mProcessPower(new ProcessPower()) {
  if (mProcessPower) {
    (void)mCounters.append(mProcessPower.get());
  }
}

PowerCounters::~PowerCounters() { mCounters.clear(); }

void PowerCounters::Sample() {}
