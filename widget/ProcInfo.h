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

namespace ipc {
class GeckoChildProcessHost;
}

// Process types. When updating this enum, please make sure to update
// WebIDLProcType and ProcTypeToWebIDL to mirror the changes.
enum class ProcType {
  // These must match the ones in ContentParent.h, and E10SUtils.jsm
  Web,
  File,
  Extension,
  PrivilegedAbout,
  WebLargeAllocation,
  // the rest matches GeckoProcessTypes.h
  Browser,  // Default is named Browser here
  Plugin,
  IPDLUnitTest,
  GMPlugin,
  GPU,
  VR,
  RDD,
  Socket,
  RemoteSandboxBroker,
  // Unknown type of process
  Unknown,
  Max = Unknown,
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

/*
 * GetProcInfo() uses a background thread to perform system calls.
 *
 * Depending on the platform, this call can be quite expensive and the
 * promise may return after several ms.
 */
#ifdef XP_MACOSX
RefPtr<ProcInfoPromise> GetProcInfo(base::ProcessId pid, int32_t childId,
                                    const ProcType& type,
                                    mach_port_t aChildTask = MACH_PORT_NULL);
#else
RefPtr<ProcInfoPromise> GetProcInfo(base::ProcessId pid, int32_t childId,
                                    const ProcType& type);
#endif

}  // namespace mozilla
#endif  // ProcInfo_h
