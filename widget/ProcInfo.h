/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_ProcInfo_h
#define __mozilla_ProcInfo_h

#include <base/process.h>
#include <stdint.h>
#include "mozilla/dom/ipc/IdType.h"

namespace mozilla {

// Process types
enum class ProcType {
  // These must match the ones in ContentParent.h and E10SUtils.jsm
  Web,
  File,
  Extension,
  Privileged,
  WebLargeAllocation,
  // GPU process (only on Windows)
  Gpu,
  // RDD process (Windows and macOS)
  Rdd,
  // Socket process
  Socket,
  // Main process
  Browser,
  // Unknown type of process
  Unknown
};

struct ThreadInfo {
  // Thread Id.
  base::ProcessId tid = 0;
  // Thread name, if any.
  nsString name;
  // User time in ns.
  uint64_t cpuUser = 0;
  // System time in ns.
  uint64_t cpuKernel = 0;
};

struct ProcInfo {
  // Process Id
  base::ProcessId pid = 0;
  // Child Id as defined by Firefox when a child process is created.
  dom::ContentParentId childId;
  // Process type
  ProcType type;
  // Process filename (without the path name).
  nsString filename;
  // VMS in bytes.
  uint64_t virtualMemorySize = 0;
  // RSS in bytes.
  int64_t residentSetSize = 0;
  // User time in ns.
  uint64_t cpuUser = 0;
  // System time in ns.
  uint64_t cpuKernel = 0;
  // Threads owned by this process.
  nsTArray<ThreadInfo> threads;
};

typedef MozPromise<ProcInfo, nsresult, true> ProcInfoPromise;

RefPtr<ProcInfoPromise> GetProcInfo(base::ProcessId pid, int32_t childId,
                                    const ProcType& type);

}  // namespace mozilla
#endif  // ProcInfo_h
