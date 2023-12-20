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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <procfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

// Maximal length on /proc filename
//  "/proc/999999/lwp/999999/lwpsinfo"
//  "/proc/999999/psinfo"
#define FNMAX_PROCFS "/proc/999999/lwp/999999/lwpsinfo"

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
    ProcInfo info;

    int fd;
    char filename[sizeof(FNMAX_PROCFS)];
    psinfo_t psinfo;
    snprintf(filename, sizeof(FNMAX_PROCFS), "/proc/%d/psinfo", request.pid);
    if ((fd = open(filename, O_RDONLY)) == -1) {
      result.SetReject(NS_ERROR_FAILURE);
      return result;
    }
    if (read(fd, &psinfo, sizeof(psinfo_t)) != sizeof(psinfo_t)) {
      close(fd);
      result.SetReject(NS_ERROR_FAILURE);
      return result;
    }
    close(fd);
    info.cpuTime =
        psinfo.pr_time.tv_sec * 1'000'000'000u + psinfo.pr_time.tv_nsec;
    info.memory = psinfo.pr_rssize * 1024;

    // Extra info
    info.pid = request.pid;
    info.childId = request.childId;
    info.type = request.processType;
    info.origin = request.origin;
    info.windows = std::move(request.windowInfo);
    info.utilityActors = std::move(request.utilityInfo);

    // Let's look at the threads
    for (int lwp = 1, lwp_max = psinfo.pr_nlwp; lwp <= lwp_max; lwp++) {
      lwpsinfo_t lwpsinfo;
      snprintf(filename, sizeof(FNMAX_PROCFS), "/proc/%d/lwp/%d/lwpsinfo",
               request.pid, lwp);
      if ((fd = open(filename, O_RDONLY)) == -1) {
        // Some LWPs might no longer exist. That just means that there are
        // probably others with bigger LWP id. But we need to limit the search
        // because of possible race condition.
        if (lwp_max < 2 * psinfo.pr_nlwp) lwp_max++;
        continue;
      }
      if (read(fd, &lwpsinfo, sizeof(lwpsinfo_t)) != sizeof(lwpsinfo_t)) {
        close(fd);
        continue;
      }
      close(fd);
      ThreadInfo threadInfo;
      threadInfo.tid = lwpsinfo.pr_lwpid;
      threadInfo.cpuTime =
          lwpsinfo.pr_time.tv_sec * 1'000'000'000u + lwpsinfo.pr_time.tv_nsec;
      threadInfo.name.AssignASCII(lwpsinfo.pr_name, strlen(lwpsinfo.pr_name));
      info.threads.AppendElement(threadInfo);
    }

    if (!gathered.put(request.pid, std::move(info))) {
      result.SetReject(NS_ERROR_OUT_OF_MEMORY);
      return result;
    }
  }

  // ... and we're done!
  result.SetResolve(std::move(gathered));
  return result;
}

}  // namespace mozilla
