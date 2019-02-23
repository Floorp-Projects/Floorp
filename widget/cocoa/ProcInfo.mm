/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Logging.h"
#include "nsNetCID.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <libproc.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

namespace mozilla {

RefPtr<ProcInfoPromise> GetProcInfo(base::ProcessId pid, int32_t childId, const ProcType& type) {
  auto holder = MakeUnique<MozPromiseHolder<ProcInfoPromise>>();
  RefPtr<ProcInfoPromise> promise = holder->Ensure(__func__);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get stream transport service");
    holder->Reject(rv, __func__);
    return promise;
  }

  auto ResolveGetProcinfo = [holder = std::move(holder), pid, type, childId]() {
    ProcInfo info;
    info.pid = pid;
    info.childId = childId;
    info.type = type;
    struct proc_bsdinfo proc;
    if ((unsigned long)proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE) <
        PROC_PIDTBSDINFO_SIZE) {
      holder->Reject(NS_ERROR_FAILURE, __func__);
      return;
    }

    struct proc_taskinfo pti;
    if ((unsigned long)proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &pti, PROC_PIDTASKINFO_SIZE) <
        PROC_PIDTASKINFO_SIZE) {
      holder->Reject(NS_ERROR_FAILURE, __func__);
      return;
    }

    // copying all the info to the ProcInfo struct
    info.filename.AssignASCII(proc.pbi_name);
    info.virtualMemorySize = pti.pti_virtual_size;
    info.residentSetSize = pti.pti_resident_size;
    info.cpuUser = pti.pti_total_user;
    info.cpuKernel = pti.pti_total_system;

    // Now getting threads info
    mach_port_t task;
    kern_return_t kret = task_for_pid(mach_task_self(), pid, &task);
    if (kret != KERN_SUCCESS) {
      // If we can't get threads detail, we just ignore that.
      holder->Resolve(info, __func__);
      return;
    }

    // task_threads() gives us a snapshot of the process threads
    // but those threads can go away. All the code below makes
    // the assumption that thread_info() calls may fail, and
    // these errors will be ignored.
    thread_act_port_array_t threadList;
    mach_msg_type_number_t threadCount;
    kret = task_threads(task, &threadList, &threadCount);
    if (kret != KERN_SUCCESS) {
      holder->Resolve(info, __func__);
      return;
    }

    for (mach_msg_type_number_t i = 0; i < threadCount; i++) {
      // Basic thread info.
      mach_msg_type_number_t threadInfoCount = THREAD_EXTENDED_INFO_COUNT;
      thread_extended_info_data_t threadInfoData;
      kret = thread_info(threadList[i], THREAD_EXTENDED_INFO, (thread_info_t)&threadInfoData,
                         &threadInfoCount);
      if (kret != KERN_SUCCESS) {
        continue;
      }

      // Getting the thread id.
      thread_identifier_info identifierInfo;
      kret = thread_info(threadList[i], THREAD_IDENTIFIER_INFO, (thread_info_t)&identifierInfo,
                         &threadInfoCount);
      if (kret != KERN_SUCCESS) {
        continue;
      }

      // The two system calls were successful, let's add that thread
      ThreadInfo thread;
      thread.cpuUser = threadInfoData.pth_user_time;
      thread.cpuKernel = threadInfoData.pth_system_time;
      thread.name.AssignASCII(threadInfoData.pth_name);
      thread.tid = identifierInfo.thread_id;
      info.threads.AppendElement(thread);
    }
    holder->Resolve(info, __func__);
  };

  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__, std::move(ResolveGetProcinfo));
  rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }
  return promise;
}

}
