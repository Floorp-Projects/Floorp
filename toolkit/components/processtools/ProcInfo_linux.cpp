/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/ProcInfo_linux.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Logging.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsMemoryReporterManager.h"
#include "nsWhitespaceTokenizer.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dirent.h>

#define NANOPERSEC 1000000000.

namespace mozilla {

int GetCycleTimeFrequencyMHz() { return 0; }

// StatReader can parse and tokenize a POSIX stat file.
// see http://man7.org/linux/man-pages/man5/proc.5.html
//
// Its usage is quite simple:
//
// StatReader reader(pid);
// ProcInfo info;
// rv = reader.ParseProc(info);
// if (NS_FAILED(rv)) {
//     // the reading of the file or its parsing failed.
// }
//
class StatReader {
 public:
  explicit StatReader(const base::ProcessId aPid)
      : mPid(aPid), mMaxIndex(15), mTicksPerSec(sysconf(_SC_CLK_TCK)) {}

  nsresult ParseProc(ProcInfo& aInfo) {
    nsAutoString fileContent;
    nsresult rv = ReadFile(fileContent);
    NS_ENSURE_SUCCESS(rv, rv);
    // We first extract the file or thread name
    int32_t startPos = fileContent.RFindChar('(');
    if (startPos == -1) {
      return NS_ERROR_FAILURE;
    }
    int32_t endPos = fileContent.RFindChar(')');
    if (endPos == -1) {
      return NS_ERROR_FAILURE;
    }
    int32_t len = endPos - (startPos + 1);
    mName.Assign(Substring(fileContent, startPos + 1, len));

    // now we can use the tokenizer for the rest of the file
    nsWhitespaceTokenizer tokenizer(Substring(fileContent, endPos + 2));
    int32_t index = 2;  // starting at third field
    while (tokenizer.hasMoreTokens() && index < mMaxIndex) {
      const nsAString& token = tokenizer.nextToken();
      rv = UseToken(index, token, aInfo);
      NS_ENSURE_SUCCESS(rv, rv);
      index++;
    }
    return NS_OK;
  }

 protected:
  // Called for each token found in the stat file.
  nsresult UseToken(int32_t aIndex, const nsAString& aToken, ProcInfo& aInfo) {
    // We're using a subset of what stat has to offer for now.
    nsresult rv = NS_OK;
    // see the proc documentation for fields index references.
    switch (aIndex) {
      case 13:
        // Amount of time that this process has been scheduled
        // in user mode, measured in clock ticks
        aInfo.cpuTime += GetCPUTime(aToken, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      case 14:
        // Amount of time that this process has been scheduled
        // in kernel mode, measured in clock ticks
        aInfo.cpuTime += GetCPUTime(aToken, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
    }
    return rv;
  }

  // Converts a token into a int64_t
  uint64_t Get64Value(const nsAString& aToken, nsresult* aRv) {
    // We can't use aToken.ToInteger64() since it returns a signed 64.
    // and that can result into an overflow.
    nsresult rv = NS_OK;
    uint64_t out = 0;
    if (sscanf(NS_ConvertUTF16toUTF8(aToken).get(), "%" PRIu64, &out) == 0) {
      rv = NS_ERROR_FAILURE;
    }
    *aRv = rv;
    return out;
  }

  // Converts a token into CPU time in nanoseconds.
  uint64_t GetCPUTime(const nsAString& aToken, nsresult* aRv) {
    nsresult rv;
    uint64_t value = Get64Value(aToken, &rv);
    *aRv = rv;
    if (NS_FAILED(rv)) {
      return 0;
    }
    if (value) {
      value = (value * NANOPERSEC) / mTicksPerSec;
    }
    return value;
  }

  base::ProcessId mPid;
  int32_t mMaxIndex;
  nsCString mFilepath;
  nsString mName;

 private:
  // Reads the stat file and puts its content in a nsString.
  nsresult ReadFile(nsAutoString& aFileContent) {
    if (mFilepath.IsEmpty()) {
      if (mPid == 0) {
        mFilepath.AssignLiteral("/proc/self/stat");
      } else {
        mFilepath.AppendPrintf("/proc/%u/stat", unsigned(mPid));
      }
    }
    FILE* fstat = fopen(mFilepath.get(), "r");
    if (!fstat) {
      return NS_ERROR_FAILURE;
    }
    // /proc is a virtual file system and all files are
    // of size 0, so GetFileSize() and related functions will
    // return 0 - so the way to read the file is to fill a buffer
    // of an arbitrary big size and look for the end of line char.
    char buffer[2048];
    char* end;
    char* start = fgets(buffer, 2048, fstat);
    fclose(fstat);
    if (start == nullptr) {
      return NS_ERROR_FAILURE;
    }
    // let's find the end
    end = strchr(buffer, '\n');
    if (!end) {
      return NS_ERROR_FAILURE;
    }
    aFileContent.AssignASCII(buffer, size_t(end - start));
    return NS_OK;
  }

  int64_t mTicksPerSec;
};

// Threads have the same stat file. The only difference is its path
// and we're getting less info in the ThreadInfo structure.
class ThreadInfoReader final : public StatReader {
 public:
  ThreadInfoReader(const base::ProcessId aPid, const base::ProcessId aTid)
      : StatReader(aPid) {
    mFilepath.AppendPrintf("/proc/%u/task/%u/stat", unsigned(aPid),
                           unsigned(aTid));
  }

  nsresult ParseThread(ThreadInfo& aInfo) {
    ProcInfo info;
    nsresult rv = StatReader::ParseProc(info);
    NS_ENSURE_SUCCESS(rv, rv);

    // Copying over the data we got from StatReader::ParseProc()
    aInfo.cpuTime = info.cpuTime;
    aInfo.name.Assign(mName);
    return NS_OK;
  }
};

nsresult GetCpuTimeSinceProcessStartInMs(uint64_t* aResult) {
  timespec t;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t) == 0) {
    uint64_t cpuTime =
        uint64_t(t.tv_sec) * 1'000'000'000u + uint64_t(t.tv_nsec);
    *aResult = cpuTime / PR_NSEC_PER_MSEC;
    return NS_OK;
  }

  StatReader reader(0);
  ProcInfo info;
  nsresult rv = reader.ParseProc(info);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aResult = info.cpuTime / PR_NSEC_PER_MSEC;
  return NS_OK;
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

    timespec t;
    clockid_t clockid = MAKE_PROCESS_CPUCLOCK(request.pid, CPUCLOCK_SCHED);
    if (clock_gettime(clockid, &t) == 0) {
      info.cpuTime = uint64_t(t.tv_sec) * 1'000'000'000u + uint64_t(t.tv_nsec);
    } else {
      // Fallback to parsing /proc/<pid>/stat
      StatReader reader(request.pid);
      nsresult rv = reader.ParseProc(info);
      if (NS_FAILED(rv)) {
        // Can't read data for this proc.
        // Probably either a sandboxing issue or a race condition, e.g.
        // the process has been just been killed. Regardless, skip process.
        continue;
      }
    }

    // The 'Memory' value displayed in the system monitor is resident -
    // shared. statm contains more fields, but we're only interested in
    // the first three.
    static const int MAX_FIELD = 3;
    size_t VmSize, resident, shared;
    info.memory = 0;
    FILE* f = fopen(nsPrintfCString("/proc/%u/statm", request.pid).get(), "r");
    if (f) {
      int nread = fscanf(f, "%zu %zu %zu", &VmSize, &resident, &shared);
      fclose(f);
      if (nread == MAX_FIELD) {
        info.memory = (resident - shared) * getpagesize();
      }
    }

    // Extra info
    info.pid = request.pid;
    info.childId = request.childId;
    info.type = request.processType;
    info.origin = request.origin;
    info.windows = std::move(request.windowInfo);
    info.utilityActors = std::move(request.utilityInfo);

    // Let's look at the threads
    nsCString taskPath;
    taskPath.AppendPrintf("/proc/%u/task", unsigned(request.pid));
    DIR* dirHandle = opendir(taskPath.get());
    if (!dirHandle) {
      // For some reason, we have no data on the threads for this process.
      // Most likely reason is that we have just lost a race condition and
      // the process is dead.
      // Let's stop here and ignore the entire process.
      continue;
    }
    auto cleanup = mozilla::MakeScopeExit([&] { closedir(dirHandle); });

    // If we can't read some thread info, we ignore that thread.
    dirent* entry;
    while ((entry = readdir(dirHandle)) != nullptr) {
      if (entry->d_name[0] == '.') {
        continue;
      }
      nsAutoCString entryName(entry->d_name);
      nsresult rv;
      int32_t tid = entryName.ToInteger(&rv);
      if (NS_FAILED(rv)) {
        continue;
      }

      ThreadInfo threadInfo;
      threadInfo.tid = tid;

      timespec ts;
      if (clock_gettime(MAKE_THREAD_CPUCLOCK(tid, CPUCLOCK_SCHED), &ts) == 0) {
        threadInfo.cpuTime =
            uint64_t(ts.tv_sec) * 1'000'000'000u + uint64_t(ts.tv_nsec);

        nsCString path;
        path.AppendPrintf("/proc/%u/task/%u/comm", unsigned(request.pid),
                          unsigned(tid));
        FILE* fstat = fopen(path.get(), "r");
        if (fstat) {
          // /proc is a virtual file system and all files are
          // of size 0, so GetFileSize() and related functions will
          // return 0 - so the way to read the file is to fill a buffer
          // of an arbitrary big size and look for the end of line char.
          // The size of the buffer needs to be as least 16, which is the
          // value of TASK_COMM_LEN in the Linux kernel.
          char buffer[32];
          char* start = fgets(buffer, sizeof(buffer), fstat);
          fclose(fstat);
          if (start) {
            // The thread name should always be smaller than our buffer,
            // so we should find a newline character.
            char* end = strchr(buffer, '\n');
            if (end) {
              threadInfo.name.AssignASCII(buffer, size_t(end - start));
              info.threads.AppendElement(threadInfo);
              continue;
            }
          }
        }
      }

      // Fallback to parsing /proc/<pid>/task/<tid>/stat
      // This is needed for child processes, as access to the per-thread
      // CPU clock is restricted to the process owning the thread.
      ThreadInfoReader reader(request.pid, tid);
      rv = reader.ParseThread(threadInfo);
      if (NS_FAILED(rv)) {
        continue;
      }
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
