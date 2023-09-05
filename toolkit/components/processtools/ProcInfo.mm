/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "nsMemoryReporterManager.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <libproc.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

static void GetTimeBase(mach_timebase_info_data_t* timebase) {
  // Expected results are 125/3 on aarch64, and 1/1 on Intel CPUs.
  if (mach_timebase_info(timebase) != KERN_SUCCESS) {
    timebase->numer = 1;
    timebase->denom = 1;
  }
}

namespace mozilla {

nsresult GetCpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  struct proc_taskinfo pti;
  if ((unsigned long)proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &pti,
                                  PROC_PIDTASKINFO_SIZE) <
      PROC_PIDTASKINFO_SIZE) {
    return NS_ERROR_FAILURE;
  }

  mach_timebase_info_data_t timebase;
  GetTimeBase(&timebase);

  *aResult = (pti.pti_total_user + pti.pti_total_system) * timebase.numer /
             timebase.denom / PR_NSEC_PER_MSEC;
  return NS_OK;
}

nsresult GetGpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  task_power_info_v2_data_t task_power_info;
  mach_msg_type_number_t count = TASK_POWER_INFO_V2_COUNT;
  kern_return_t kr = task_info(mach_task_self(), TASK_POWER_INFO_V2,
                               (task_info_t)&task_power_info, &count);
  if (kr != KERN_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  *aResult = task_power_info.gpu_energy.task_gpu_utilisation / PR_NSEC_PER_MSEC;
  return NS_OK;
}

int GetCycleTimeFrequencyMHz() { return 0; }

ProcInfoPromise::ResolveOrRejectValue GetProcInfoSync(
    nsTArray<ProcInfoRequest>&& aRequests) {
  ProcInfoPromise::ResolveOrRejectValue result;

  HashMap<base::ProcessId, ProcInfo> gathered;
  if (!gathered.reserve(aRequests.Length())) {
    result.SetReject(NS_ERROR_OUT_OF_MEMORY);
    return result;
  }

  mach_timebase_info_data_t timebase;
  GetTimeBase(&timebase);

  for (const auto& request : aRequests) {
    ProcInfo info;
    info.pid = request.pid;
    info.childId = request.childId;
    info.type = request.processType;
    info.origin = std::move(request.origin);
    info.windows = std::move(request.windowInfo);
    info.utilityActors = std::move(request.utilityInfo);

    struct proc_taskinfo pti;
    if ((unsigned long)proc_pidinfo(request.pid, PROC_PIDTASKINFO, 0, &pti,
                                    PROC_PIDTASKINFO_SIZE) <
        PROC_PIDTASKINFO_SIZE) {
      // Can't read data for this process.
      // Probably either a sandboxing issue or a race condition, e.g.
      // the process has been just been killed. Regardless, skip process.
      continue;
    }
    info.cpuTime = (pti.pti_total_user + pti.pti_total_system) *
                   timebase.numer / timebase.denom;

    mach_port_t selectedTask;
    // If we did not get a task from a child process, we use mach_task_self()
    if (request.childTask == MACH_PORT_NULL) {
      selectedTask = mach_task_self();
    } else {
      selectedTask = request.childTask;
    }

    // The phys_footprint value (introduced in 10.11) of the TASK_VM_INFO data
    // matches the value in the 'Memory' column of the Activity Monitor.
    task_vm_info_data_t task_vm_info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t kr = task_info(selectedTask, TASK_VM_INFO,
                                 (task_info_t)&task_vm_info, &count);
    info.memory = kr == KERN_SUCCESS ? task_vm_info.phys_footprint : 0;

    // Now getting threads info

    // task_threads() gives us a snapshot of the process threads
    // but those threads can go away. All the code below makes
    // the assumption that thread_info() calls may fail, and
    // these errors will be ignored.
    thread_act_port_array_t threadList;
    mach_msg_type_number_t threadCount;
    kern_return_t kret = task_threads(selectedTask, &threadList, &threadCount);
    if (kret != KERN_SUCCESS) {
      // For some reason, we have no data on the threads for this process.
      // Most likely reason is that we have just lost a race condition and
      // the process is dead.
      // Let's stop here and ignore the entire process.
      continue;
    }

    // Deallocate the thread list.
    // Note that this deallocation is entirely undocumented, so the following
    // code is based on guesswork and random examples found on the web.
    auto guardThreadCount = MakeScopeExit([&] {
      if (threadList == nullptr) {
        return;
      }
      // Free each thread to avoid leaks.
      for (mach_msg_type_number_t i = 0; i < threadCount; i++) {
        mach_port_deallocate(mach_task_self(), threadList[i]);
      }
      vm_deallocate(mach_task_self(), /* address */ (vm_address_t)threadList,
                    /* size */ sizeof(thread_t) * threadCount);
    });

    for (mach_msg_type_number_t i = 0; i < threadCount; i++) {
      // Basic thread info.
      thread_extended_info_data_t threadInfoData;
      count = THREAD_EXTENDED_INFO_COUNT;
      kret = thread_info(threadList[i], THREAD_EXTENDED_INFO,
                         (thread_info_t)&threadInfoData, &count);
      if (kret != KERN_SUCCESS) {
        continue;
      }

      // Getting the thread id.
      thread_identifier_info identifierInfo;
      count = THREAD_IDENTIFIER_INFO_COUNT;
      kret = thread_info(threadList[i], THREAD_IDENTIFIER_INFO,
                         (thread_info_t)&identifierInfo, &count);
      if (kret != KERN_SUCCESS) {
        continue;
      }

      // The two system calls were successful, let's add that thread
      ThreadInfo* thread = info.threads.AppendElement(fallible);
      if (!thread) {
        result.SetReject(NS_ERROR_OUT_OF_MEMORY);
        return result;
      }
      thread->cpuTime =
          threadInfoData.pth_user_time + threadInfoData.pth_system_time;
      thread->name.AssignASCII(threadInfoData.pth_name);
      thread->tid = identifierInfo.thread_id;
    }

    if (!gathered.put(request.pid, std::move(info))) {
      result.SetReject(NS_ERROR_OUT_OF_MEMORY);
      return result;
    }
  }

  result.SetResolve(std::move(gathered));
  return result;
}

}  // namespace mozilla
