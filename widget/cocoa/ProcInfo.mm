/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "nsMemoryReporterManager.h"
#include "nsNetCID.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <libproc.h>
#include <sys/sysctl.h>
#include <mach/mach.h>

namespace mozilla {

RefPtr<ProcInfoPromise> GetProcInfo(nsTArray<ProcInfoRequest>&& aRequests) {
  auto holder = MakeUnique<MozPromiseHolder<ProcInfoPromise>>();
  RefPtr<ProcInfoPromise> promise = holder->Ensure(__func__);
  nsresult rv = NS_OK;
  nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get stream transport service");
    holder->Reject(rv, __func__);
    return promise;
  }

  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [holder = std::move(holder), requests = std::move(aRequests)]() {
        HashMap<base::ProcessId, ProcInfo> gathered;
        if (!gathered.reserve(requests.Length())) {
          holder->Reject(NS_ERROR_OUT_OF_MEMORY, __func__);
          return;
        }
        for (const auto& request : requests) {
          ProcInfo info;
          info.pid = request.pid;
          info.childId = request.childId;
          info.type = request.processType;
          info.origin = std::move(request.origin);
          struct proc_bsdinfo proc;
          if ((unsigned long)proc_pidinfo(request.pid, PROC_PIDTBSDINFO, 0, &proc,
                                          PROC_PIDTBSDINFO_SIZE) < PROC_PIDTBSDINFO_SIZE) {
            // Can't read data for this proc.
            // Probably either a sandboxing issue or a race condition, e.g.
            // the process has been just been killed. Regardless, skip process.
            continue;
          }

          struct proc_taskinfo pti;
          if ((unsigned long)proc_pidinfo(request.pid, PROC_PIDTASKINFO, 0, &pti,
                                          PROC_PIDTASKINFO_SIZE) < PROC_PIDTASKINFO_SIZE) {
            continue;
          }

          // copying all the info to the ProcInfo struct
          info.filename.AssignASCII(proc.pbi_name);
          info.virtualMemorySize = pti.pti_virtual_size;
          info.residentSetSize = pti.pti_resident_size;
          info.cpuUser = pti.pti_total_user;
          info.cpuKernel = pti.pti_total_system;

          mach_port_t selectedTask;
          // If we did not get a task from a child process, we use mach_task_self()
          if (request.childTask == MACH_PORT_NULL) {
            selectedTask = mach_task_self();
          } else {
            selectedTask = request.childTask;
          }
          // Computing the resident unique size is somewhat tricky,
          // so we use about:memory's implementation. This implementation
          // uses the `task` so, in theory, should be no additional
          // race condition. However, in case of error, the result is `0`.
          info.residentUniqueSize = nsMemoryReporterManager::ResidentUnique(selectedTask);

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
          // Note that this deallocation is entirely undocumented, so the following code is based
          // on guesswork and random examples found on the web.
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

          mach_msg_type_number_t count;

          for (mach_msg_type_number_t i = 0; i < threadCount; i++) {
            // Basic thread info.
            thread_extended_info_data_t threadInfoData;
            count = THREAD_EXTENDED_INFO_COUNT;
            kret = thread_info(threadList[i], THREAD_EXTENDED_INFO, (thread_info_t)&threadInfoData,
                               &count);
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
              holder->Reject(NS_ERROR_OUT_OF_MEMORY, __func__);
              return;
            }
            thread->cpuUser = threadInfoData.pth_user_time;
            thread->cpuKernel = threadInfoData.pth_system_time;
            thread->name.AssignASCII(threadInfoData.pth_name);
            thread->tid = identifierInfo.thread_id;
          }

          if (!gathered.put(request.pid, std::move(info))) {
            holder->Reject(NS_ERROR_OUT_OF_MEMORY, __func__);
            return;
          }
        }
        // ... and we're done!
        holder->Resolve(std::move(gathered), __func__);
      });

  rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }
  return promise;
}

}  // namespace mozilla
