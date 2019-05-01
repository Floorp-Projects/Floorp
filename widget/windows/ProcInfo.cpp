/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
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

void AppendThreads(ProcInfo* info) {
  THREADENTRY32 te32;
  auto getThreadDescription =
      reinterpret_cast<GETTHREADDESCRIPTION>(::GetProcAddress(
          ::GetModuleHandleW(L"Kernel32.dll"), "GetThreadDescription"));

  // Take a snapshot of all running threads, system-wide.
  nsAutoHandle hThreadSnap(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
  if (!hThreadSnap) {
    return;
  }
  te32.dwSize = sizeof(THREADENTRY32);

  // Retrieve information about the first thread,
  // and exit if unsuccessful
  if (!Thread32First(hThreadSnap.get(), &te32)) {
    return;
  }

  do {
    if (te32.th32OwnerProcessID == info->pid) {
      nsAutoHandle hThread(
          OpenThread(THREAD_QUERY_INFORMATION, TRUE, te32.th32ThreadID));
      if (!hThread) {
        continue;
      }

      FILETIME createTime, exitTime, kernelTime, userTime;
      if (!GetThreadTimes(hThread.get(), &createTime, &exitTime, &kernelTime,
                          &userTime)) {
        continue;
      }

      ThreadInfo thread;
      if (getThreadDescription) {
        PWSTR threadName = nullptr;
        if (getThreadDescription(hThread.get(), &threadName) && threadName) {
          thread.name = threadName;
        }
        if (threadName) {
          LocalFree(threadName);
        }
      }
      thread.tid = te32.th32ThreadID;
      thread.cpuKernel = ToNanoSeconds(kernelTime);
      thread.cpuUser = ToNanoSeconds(userTime);
      info->threads.AppendElement(thread);
    }
  } while (Thread32Next(hThreadSnap.get(), &te32));
}

RefPtr<ProcInfoPromise> GetProcInfo(base::ProcessId pid, int32_t childId,
                                    const ProcType& type) {
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
      __func__, [holder = std::move(holder), pid, type, childId]() -> void {
        nsAutoHandle handle(OpenProcess(
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));

        if (!handle) {
          holder->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }

        wchar_t filename[MAX_PATH];
        if (GetProcessImageFileNameW(handle.get(), filename, MAX_PATH) == 0) {
          holder->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        FILETIME createTime, exitTime, kernelTime, userTime;
        if (!GetProcessTimes(handle.get(), &createTime, &exitTime, &kernelTime,
                             &userTime)) {
          holder->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        PROCESS_MEMORY_COUNTERS memoryCounters;
        if (!GetProcessMemoryInfo(handle.get(),
                                  (PPROCESS_MEMORY_COUNTERS)&memoryCounters,
                                  sizeof(memoryCounters))) {
          holder->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        ProcInfo info;
        info.pid = pid;
        info.childId = childId;
        info.type = type;
        info.filename.Assign(filename);
        info.cpuKernel = ToNanoSeconds(kernelTime);
        info.cpuUser = ToNanoSeconds(userTime);
        info.residentSetSize = memoryCounters.WorkingSetSize;
        info.virtualMemorySize = memoryCounters.PagefileUsage;
        AppendThreads(&info);
        holder->Resolve(info, __func__);
      });

  rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }

  return promise;
}

}  // namespace mozilla
