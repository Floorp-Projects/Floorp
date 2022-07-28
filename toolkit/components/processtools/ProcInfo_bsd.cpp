/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsMemoryReporterManager.h"
#include "nsWhitespaceTokenizer.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace mozilla {

int GetCycleTimeFrequencyMHz() { return 0; }

nsresult GetCpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  timespec t;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t) == 0) {
    uint64_t cpuTime =
        uint64_t(t.tv_sec) * 1'000'000'000u + uint64_t(t.tv_nsec);
    *aResult = cpuTime / PR_NSEC_PER_MSEC;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult GetGpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

ProcInfoPromise::ResolveOrRejectValue GetProcInfoSync(
    nsTArray<ProcInfoRequest>&& aRequests) {
  ProcInfoPromise::ResolveOrRejectValue result;

  HashMap<base::ProcessId, ProcInfo> gathered;
  if (!gathered.reserve(aRequests.Length())) {
    result.SetReject(NS_ERROR_OUT_OF_MEMORY);
    return result;
  }
  for (const auto& request : aRequests) {
    size_t size;
    int mib[6];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID | KERN_PROC_SHOW_THREADS;
    mib[3] = request.pid;
    mib[4] = sizeof(kinfo_proc);
    mib[5] = 0;
    if (sysctl(mib, 6, nullptr, &size, nullptr, 0) == -1) {
      // Can't get info for this process. Skip it.
      continue;
    }

    mib[5] = size / sizeof(kinfo_proc);
    auto procs = MakeUniqueFallible<kinfo_proc[]>(mib[5]);
    if (!procs) {
      result.SetReject(NS_ERROR_OUT_OF_MEMORY);
      return result;
    }
    if (sysctl(mib, 6, procs.get(), &size, nullptr, 0) == -1 &&
        errno != ENOMEM) {
      continue;
    }

    ProcInfo info;
    info.pid = request.pid;
    info.childId = request.childId;
    info.type = request.processType;
    info.origin = request.origin;
    info.windows = std::move(request.windowInfo);
    info.utilityActors = std::move(request.utilityInfo);

    bool found = false;
    for (size_t i = 0; i < size / sizeof(kinfo_proc); i++) {
      const auto& p = procs[i];
      if (p.p_tid == -1) {
        // This is the process.
        found = true;
        info.cpuTime = uint64_t(p.p_rtime_sec) * 1'000'000'000u +
                       uint64_t(p.p_rtime_usec) * 1'000u;
        info.memory =
            (p.p_vm_tsize + p.p_vm_dsize + p.p_vm_ssize) * getpagesize();
      } else {
        // This is one of its threads.
        ThreadInfo threadInfo;
        threadInfo.tid = p.p_tid;
        threadInfo.cpuTime = uint64_t(p.p_rtime_sec) * 1'000'000'000u +
                             uint64_t(p.p_rtime_usec) * 1'000u;
        info.threads.AppendElement(threadInfo);
      }
    }

    if (found && !gathered.put(request.pid, std::move(info))) {
      result.SetReject(NS_ERROR_OUT_OF_MEMORY);
      return result;
    }
  }

  // ... and we're done!
  result.SetResolve(std::move(gathered));
  return result;
}

}  // namespace mozilla
