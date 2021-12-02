/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/SSE.h"
#include "gfxWindowsPlatform.h"
#include "nsMemoryReporterManager.h"
#include "nsWindowsHelpers.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#define PR_USEC_PER_NSEC 1000L

typedef HRESULT(WINAPI* GETTHREADDESCRIPTION)(HANDLE hThread,
                                              PWSTR* threadDescription);

namespace mozilla {

static uint64_t ToNanoSeconds(const FILETIME& aFileTime) {
  // FILETIME values are 100-nanoseconds units, converting
  ULARGE_INTEGER usec = {{aFileTime.dwLowDateTime, aFileTime.dwHighDateTime}};
  return usec.QuadPart * 100;
}

/* Get the CPU frequency to use to convert cycle time values to actual time.
 * @returns the TSC frequency in MHz, or 0 if converting cycle time values
 * should not be attempted. */
static int GetCycleTimeFrequency() {
  static int result = -1;
  if (result != -1) {
    return result;
  }

  result = 0;

  // Having a constant TSC is required to convert cycle time to actual time.
  if (!mozilla::has_constant_tsc()) {
    return result;
  }

  // Now get the nominal CPU frequency.
  HKEY key;
  static const WCHAR keyName[] =
      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyName, 0, KEY_QUERY_VALUE, &key) ==
      ERROR_SUCCESS) {
    DWORD data, len;
    len = sizeof(data);

    if (RegQueryValueEx(key, L"~Mhz", 0, 0, reinterpret_cast<LPBYTE>(&data),
                        &len) == ERROR_SUCCESS) {
      result = static_cast<int>(data);
    }
  }

  return result;
}

nsresult GetCpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  FILETIME createTime, exitTime, kernelTime, userTime;
  int frequencyInMHz = GetCycleTimeFrequency();
  if (frequencyInMHz) {
    uint64_t cpuCycleCount;
    if (!QueryProcessCycleTime(::GetCurrentProcess(), &cpuCycleCount)) {
      return NS_ERROR_FAILURE;
    }
    constexpr int HZ_PER_MHZ = 1000000;
    *aResult =
        cpuCycleCount / (frequencyInMHz * (HZ_PER_MHZ / PR_MSEC_PER_SEC));
    return NS_OK;
  }

  if (!GetProcessTimes(::GetCurrentProcess(), &createTime, &exitTime,
                       &kernelTime, &userTime)) {
    return NS_ERROR_FAILURE;
  }
  *aResult =
      (ToNanoSeconds(kernelTime) + ToNanoSeconds(userTime)) / PR_NSEC_PER_MSEC;
  return NS_OK;
}

nsresult GetGpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  return gfxWindowsPlatform::GetGpuTimeSinceProcessStartInMs(aResult);
}

ProcInfoPromise::ResolveOrRejectValue GetProcInfoSync(
    nsTArray<ProcInfoRequest>&& aRequests) {
  ProcInfoPromise::ResolveOrRejectValue result;

  HashMap<base::ProcessId, ProcInfo> gathered;
  if (!gathered.reserve(aRequests.Length())) {
    result.SetReject(NS_ERROR_OUT_OF_MEMORY);
    return result;
  }

  int frequencyInMHz = GetCycleTimeFrequency();

  // ---- Copying data on processes (minus threads).

  for (const auto& request : aRequests) {
    nsAutoHandle handle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE, request.pid));

    if (!handle) {
      // Ignore process, it may have died.
      continue;
    }

    uint64_t cpuCycleTime;
    if (!QueryProcessCycleTime(handle.get(), &cpuCycleTime)) {
      // Ignore process, it may have died.
      continue;
    }

    uint64_t cpuTime;
    if (frequencyInMHz) {
      cpuTime = cpuCycleTime * PR_USEC_PER_NSEC / frequencyInMHz;
    } else {
      FILETIME createTime, exitTime, kernelTime, userTime;
      if (!GetProcessTimes(handle.get(), &createTime, &exitTime, &kernelTime,
                           &userTime)) {
        // Ignore process, it may have died.
        continue;
      }
      cpuTime = ToNanoSeconds(kernelTime) + ToNanoSeconds(userTime);
    }

    PROCESS_MEMORY_COUNTERS_EX memoryCounters;
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
    info.windows = std::move(request.windowInfo);
    info.cpuTime = cpuTime;
    info.cpuCycleCount = cpuCycleTime;
    info.memory = memoryCounters.PrivateUsage;

    if (!gathered.put(request.pid, std::move(info))) {
      result.SetReject(NS_ERROR_OUT_OF_MEMORY);
      return result;
    }
  }

  // ---- Add thread data to already-copied processes.

  // First, we need to capture a snapshot of all the threads on this
  // system.
  nsAutoHandle hThreadSnap(CreateToolhelp32Snapshot(
      /* dwFlags */ TH32CS_SNAPTHREAD, /* ignored */ 0));
  if (!hThreadSnap) {
    result.SetReject(NS_ERROR_UNEXPECTED);
    return result;
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
      result.SetReject(NS_ERROR_OUT_OF_MEMORY);
      return result;
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
    if (QueryThreadCycleTime(hThread.get(), &threadInfo->cpuCycleCount) &&
        frequencyInMHz) {
      threadInfo->cpuTime =
          threadInfo->cpuCycleCount * PR_USEC_PER_NSEC / frequencyInMHz;
    } else {
      FILETIME createTime, exitTime, kernelTime, userTime;
      if (GetThreadTimes(hThread.get(), &createTime, &exitTime, &kernelTime,
                         &userTime)) {
        threadInfo->cpuTime =
            ToNanoSeconds(kernelTime) + ToNanoSeconds(userTime);
      }
    }

    // Attempt to get thread name.
    // If we fail, continue without this piece of information.
    if (getThreadDescription) {
      PWSTR threadName = nullptr;
      if (getThreadDescription(hThread.get(), &threadName) && threadName) {
        threadInfo->name = threadName;
      }
      if (threadName) {
        LocalFree(threadName);
      }
    }
  }

  // ----- We're ready to return.
  result.SetResolve(std::move(gathered));
  return result;
}

}  // namespace mozilla
