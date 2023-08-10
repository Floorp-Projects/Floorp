/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerCPUFreq.h"
#include "nsThreadUtils.h"
#include <fcntl.h>
#include <unistd.h>

ProfilerCPUFreq::ProfilerCPUFreq() {
  if (!mCPUCounters.resize(mozilla::GetNumberOfProcessors())) {
    NS_WARNING("failing to resize the mCPUCounters vector");
    return;
  }

  for (unsigned cpuId = 0; cpuId < mCPUCounters.length(); ++cpuId) {
    const size_t buf_sz = 64;
    char buf[buf_sz];
    int rv = sprintf(
        buf, "/sys/devices/system/cpu/cpu%u/cpufreq/scaling_cur_freq", cpuId);
    if (NS_WARN_IF(rv < 0)) {
      continue;
    }

    int fd = open(buf, O_RDONLY);
    if (NS_WARN_IF(!fd)) {
      continue;
    }

    mCPUCounters[cpuId].fd = fd;
  }
}

ProfilerCPUFreq::~ProfilerCPUFreq() {
  for (CPUCounterInfo& CPUCounter : mCPUCounters) {
    int fd = CPUCounter.fd;
    if (NS_WARN_IF(!fd)) {
      continue;
    }
    close(fd);
    CPUCounter.fd = 0;
  }
}

uint32_t ProfilerCPUFreq::GetCPUSpeedMHz(unsigned cpuId) {
  MOZ_ASSERT(cpuId < mCPUCounters.length());
  int fd = mCPUCounters[cpuId].fd;
  if (NS_WARN_IF(!fd)) {
    return 0;
  }

  long rv = lseek(fd, 0, SEEK_SET);
  if (NS_WARN_IF(rv < 0)) {
    return 0;
  }

  const size_t buf_sz = 64;
  char buf[buf_sz];
  rv = read(fd, buf, buf_sz);
  if (NS_WARN_IF(rv < 0)) {
    return 0;
  }

  int cpufreq = 0;
  rv = sscanf(buf, "%u", &cpufreq);
  if (NS_WARN_IF(rv != 1)) {
    return 0;
  }

  // Convert kHz to MHz, rounding to the nearst 10Mhz, to ignore tiny
  // variations that are likely due to rounding errors.
  return uint32_t(cpufreq / 10000) * 10;
}
