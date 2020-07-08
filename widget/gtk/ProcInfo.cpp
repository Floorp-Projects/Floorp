/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProcInfo.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Logging.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsAutoRef.h"
#include "nsLocalFile.h"
#include "nsNetCID.h"
#include "nsWhitespaceTokenizer.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <dirent.h>

#define NANOPERSEC 1000000000.

template <>
class nsAutoRefTraits<DIR> : public nsPointerRefTraits<DIR> {
 public:
  static void Release(DIR* dirHandle) { closedir(dirHandle); }
};

namespace mozilla {

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
      : mPid(aPid), mMaxIndex(53), mTicksPerSec(sysconf(_SC_CLK_TCK)) {
    mFilepath.AppendPrintf("/proc/%u/stat", mPid);
  }

  nsresult ParseProc(ProcInfo& aInfo) {
    nsAutoString fileContent;
    nsresult rv = ReadFile(fileContent);
    NS_ENSURE_SUCCESS(rv, rv);
    // We first extract the filename
    int32_t startPos = fileContent.RFindChar('(');
    if (startPos == -1) {
      return NS_ERROR_FAILURE;
    }
    int32_t endPos = fileContent.RFindChar(')');
    if (endPos == -1) {
      return NS_ERROR_FAILURE;
    }
    int32_t len = endPos - (startPos + 1);
    aInfo.filename.Assign(Substring(fileContent, startPos + 1, len));

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
        aInfo.cpuUser = GetCPUTime(aToken, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      case 14:
        // Amount of time that this process has been scheduled
        // in kernel mode, measured in clock ticks
        aInfo.cpuKernel = GetCPUTime(aToken, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      case 22:
        // Virtual memory size in bytes.
        aInfo.virtualMemorySize = Get64Value(aToken, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      case 23:
        // Resident Set Size: number of pages the process has
        // in real memory.
        uint64_t pageCount = Get64Value(aToken, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        uint64_t pageSize = sysconf(_SC_PAGESIZE);
        aInfo.residentSetSize = pageCount * pageSize;
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
  ProcInfo mProcInfo;

 private:
  // Reads the stat file and puts its content in a nsString.
  nsresult ReadFile(nsAutoString& aFileContent) {
    RefPtr<nsLocalFile> file = new nsLocalFile(mFilepath);
    bool exists;
    nsresult rv = file->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!exists) {
      return NS_ERROR_FAILURE;
    }
    // /proc is a virtual file system and all files are
    // of size 0, so GetFileSize() and related functions will
    // return 0 - so the way to read the file is to fill a buffer
    // of an arbitrary big size and look for the end of line char.
    FILE* fstat;
    if (NS_FAILED(file->OpenANSIFileDesc("r", &fstat)) || !fstat) {
      return NS_ERROR_FAILURE;
    }
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
      : StatReader(aPid), mTid(aTid) {
    // Adding the thread path
    mFilepath.Truncate();
    mFilepath.AppendPrintf("/proc/%u/task/%u/stat", aPid, mTid);
    mMaxIndex = 17;
  }

  nsresult ParseThread(ThreadInfo& aInfo) {
    ProcInfo info;
    nsresult rv = StatReader::ParseProc(info);
    NS_ENSURE_SUCCESS(rv, rv);

    aInfo.tid = mTid;
    // Copying over the data we got from StatReader::ParseProc()
    aInfo.cpuKernel = info.cpuKernel;
    aInfo.cpuUser = info.cpuUser;
    aInfo.name.Assign(info.filename);
    return NS_OK;
  }

 private:
  base::ProcessId mTid;
};

RefPtr<ProcInfoPromise> GetProcInfo(base::ProcessId pid, int32_t childId,
                                    const ProcType& type,
                                    const nsAString& origin) {
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

  // Ensure that the string is still alive when the runnable is called.
  nsString originCopy(origin);
  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [holder = std::move(holder), pid, type,
                 originCopy = std::move(originCopy), childId]() {
        // opening the stat file and reading its content
        StatReader reader(pid);
        ProcInfo info;
        nsresult rv = reader.ParseProc(info);
        if (NS_FAILED(rv)) {
          holder->Reject(rv, __func__);
          return;
        }
        // Extra info
        info.pid = pid;
        info.childId = childId;
        info.type = type;
        info.origin = originCopy;

        // Let's look at the threads
        nsCString taskPath;
        taskPath.AppendPrintf("/proc/%u/task", pid);
        nsAutoRef<DIR> dirHandle(opendir(taskPath.get()));
        if (!dirHandle) {
          // No threads ? Let's stop here and ignore the problem.
          holder->Resolve(info, __func__);
          return;
        }

        // If we can't read some thread info, we ignore that thread.
        dirent* entry;
        while ((entry = readdir(dirHandle)) != nullptr) {
          if (entry->d_name[0] == '.') {
            continue;
          }
          // Threads have a stat file, like processes.
          nsAutoCString entryName(entry->d_name);
          int32_t tid = entryName.ToInteger(&rv);
          if (NS_FAILED(rv)) {
            continue;
          }
          ThreadInfoReader reader(pid, tid);
          ThreadInfo threadInfo;
          rv = reader.ParseThread(threadInfo);
          if (NS_FAILED(rv)) {
            continue;
          }
          info.threads.AppendElement(threadInfo);
        }
        holder->Resolve(info, __func__);
      });

  rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }
  return promise;
}

}  // namespace mozilla
