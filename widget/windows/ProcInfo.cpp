/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsMemoryReporterManager.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

typedef HRESULT(WINAPI* GETTHREADDESCRIPTION)(HANDLE hThread,
                                              PWSTR* threadDescription);

namespace mozilla {

uint64_t ToNanoSeconds(const FILETIME& aFileTime) {
  // FILETIME values are 100-nanoseconds units, converting
  ULARGE_INTEGER usec = {{aFileTime.dwLowDateTime, aFileTime.dwHighDateTime}};
  return usec.QuadPart * 100;
}

RefPtr<ProcInfoPromise> GetProcInfo(nsTArray<ProcInfoRequest>&& aRequests) {
  auto holder = MakeUnique<MozPromiseHolder<ProcInfoPromise>>();
  RefPtr<ProcInfoPromise> promise = holder->Ensure(__func__);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get stream transport service");
    holder->Reject(rv, __func__);
    return promise;
  }

  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__,
      [holder = std::move(holder), requests = std::move(aRequests)]() -> void {
        HashMap<base::ProcessId, ProcInfo> gathered;
        if (!gathered.reserve(requests.Length())) {
          holder->Reject(NS_ERROR_OUT_OF_MEMORY, __func__);
          return;
        }

        // ---- Copying data on processes (minus threads).

        for (const auto& request : requests) {
          nsAutoHandle handle(OpenProcess(
              PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, request.pid));

          if (!handle) {
            // Ignore process, it may have died.
            continue;
          }

          wchar_t filename[MAX_PATH];
          if (GetProcessImageFileNameW(handle.get(), filename, MAX_PATH) == 0) {
            // Ignore process, it may have died.
            continue;
          }
          FILETIME createTime, exitTime, kernelTime, userTime;
          if (!GetProcessTimes(handle.get(), &createTime, &exitTime,
                               &kernelTime, &userTime)) {
            // Ignore process, it may have died.
            continue;
          }
          PROCESS_MEMORY_COUNTERS memoryCounters;
          if (!GetProcessMemoryInfo(handle.get(),
                                    (PPROCESS_MEMORY_COUNTERS)&memoryCounters,
                                    sizeof(memoryCounters))) {
            // Ignore process, it may have died.
            continue;
          }

          // Assumption: values of `pid` are distinct between processes,
          // regardless of any race condition we might have stumbled upon. Even
          // if it somehow could happen, in the worst case scenario, we might
          // end up overwriting one process info and we might end up with too
          // many threads attached to a process, as the data is not crucial, we
          // do not need to defend against that (unlikely) scenario.
          ProcInfo info;
          info.pid = request.pid;
          info.childId = request.childId;
          info.type = request.processType;
          info.origin = request.origin;
          info.filename.Assign(filename);
          info.cpuKernel = ToNanoSeconds(kernelTime);
          info.cpuUser = ToNanoSeconds(userTime);
          info.residentSetSize = memoryCounters.WorkingSetSize;
          info.virtualMemorySize = memoryCounters.PagefileUsage;

          // Computing the resident unique size is somewhat tricky,
          // so we use about:memory's implementation. This implementation
          // uses the `HANDLE` so, in theory, should be no additional
          // race condition. However, in case of error, the result is `0`.
          info.residentUniqueSize =
              nsMemoryReporterManager::ResidentUnique(handle.get());

          if (!gathered.put(request.pid, std::move(info))) {
            holder->Reject(NS_ERROR_OUT_OF_MEMORY, __func__);
            return;
          }
        }

        // ---- Add thread data to already-copied processes.

        // First, we need to capture a snapshot of all the threads on this
        // system.
        nsAutoHandle hThreadSnap(CreateToolhelp32Snapshot(
            /* dwFlags */ TH32CS_SNAPTHREAD, /* ignored */ 0));
        if (!hThreadSnap) {
          holder->Reject(NS_ERROR_UNEXPECTED, __func__);
          return;
        }

        // `GetThreadDescription` is available as of Windows 10.
        // We attempt to import it dynamically, knowing that it
        // may be `nullptr`.
        auto getThreadDescription =
            reinterpret_cast<GETTHREADDESCRIPTION>(::GetProcAddress(
                ::GetModuleHandleW(L"Kernel32.dll"), "GetThreadDescription"));

        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);

        // Now, walk through the threads.
        for (auto success = Thread32First(hThreadSnap.get(), &te32); success;
             success = Thread32Next(hThreadSnap.get(), &te32)) {
          auto processLookup = gathered.lookup(te32.th32OwnerProcessID);
          if (!processLookup) {
            // Not one of the processes we're interested in.
            continue;
          }
          ThreadInfo* threadInfo =
              processLookup->value().threads.AppendElement(fallible);
          if (!threadInfo) {
            holder->Reject(NS_ERROR_OUT_OF_MEMORY, __func__);
            return;
          }

          nsAutoHandle hThread(
              OpenThread(/* dwDesiredAccess = */ THREAD_QUERY_INFORMATION,
                         /* bInheritHandle = */ FALSE,
                         /* dwThreadId = */ te32.th32ThreadID));
          if (!hThread) {
            // Cannot open thread. Not sure why, but let's erase this thread
            // and attempt to find data on other threads.
            processLookup->value().threads.RemoveLastElement();
            continue;
          }

          threadInfo->tid = te32.th32ThreadID;

          // Attempt to get thread times.
          // If we fail, continue without this piece of information.
          FILETIME createTime, exitTime, kernelTime, userTime;
          if (GetThreadTimes(hThread.get(), &createTime, &exitTime, &kernelTime,
                             &userTime)) {
            threadInfo->cpuKernel = ToNanoSeconds(kernelTime);
            threadInfo->cpuUser = ToNanoSeconds(userTime);
          }

          // Attempt to get thread name.
          // If we fail, continue without this piece of information.
          if (getThreadDescription) {
            PWSTR threadName = nullptr;
            if (getThreadDescription(hThread.get(), &threadName) &&
                threadName) {
              threadInfo->name = threadName;
            }
            if (threadName) {
              LocalFree(threadName);
            }
          }
        }

        // ----- We're ready to return.
        holder->Resolve(std::move(gathered), __func__);
      });

  rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }

  return promise;
}

}  // namespace mozilla
