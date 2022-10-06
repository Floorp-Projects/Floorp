/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_ProcInfo_h
#define __mozilla_ProcInfo_h

#include <base/process.h>
#include <stdint.h>
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChromeUtilsBinding.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/HashTable.h"
#include "mozilla/MozPromise.h"

namespace mozilla {

namespace ipc {
class GeckoChildProcessHost;
}

/**
 * Return the number of milliseconds of CPU time used since process start.
 *
 * @return NS_OK on success.
 */
nsresult GetCpuTimeSinceProcessStartInMs(uint64_t* aResult);

/**
 * Return the number of milliseconds of GPU time used since process start.
 *
 * @return NS_OK on success.
 */
nsresult GetGpuTimeSinceProcessStartInMs(uint64_t* aResult);

// Process types. When updating this enum, please make sure to update
// WebIDLProcType, ChromeUtils::RequestProcInfo and ProcTypeToWebIDL to
// mirror the changes.
enum class ProcType {
  // These must match the ones in RemoteType.h, and E10SUtils.jsm
  Web,
  WebIsolated,
  File,
  Extension,
  PrivilegedAbout,
  PrivilegedMozilla,
  WebCOOPCOEP,
  WebServiceWorker,
// the rest matches GeckoProcessTypes.h
#define GECKO_PROCESS_TYPE(enum_value, enum_name, string_name, proc_typename, \
                           process_bin_type, procinfo_typename,               \
                           webidl_typename, allcaps_name)                     \
  procinfo_typename,
#define SKIP_PROCESS_TYPE_CONTENT
#ifndef MOZ_ENABLE_FORKSERVER
#  define SKIP_PROCESS_TYPE_FORKSERVER
#endif  // MOZ_ENABLE_FORKSERVER
#include "mozilla/GeckoProcessTypes.h"
#undef SKIP_PROCESS_TYPE_CONTENT
#ifndef MOZ_ENABLE_FORKSERVER
#  undef SKIP_PROCESS_TYPE_FORKSERVER
#endif  // MOZ_ENABLE_FORKSERVER
#undef GECKO_PROCESS_TYPE
  Preallocated,
  // Unknown type of process
  Unknown,
  Max = Unknown,
};

enum class UtilityActorName {
  Unknown,
  AudioDecoder_Generic,
  AudioDecoder_AppleMedia,
  AudioDecoder_WMF,
  MfMediaEngineCDM,
};

/* Get the CPU frequency to use to convert cycle time values to actual time.
 * @returns the TSC (Time Stamp Counter) frequency in MHz, or 0 if converting
 * cycle time values should not be attempted. */
int GetCycleTimeFrequencyMHz();

struct ThreadInfo {
  // Thread Id.
  base::ProcessId tid = 0;
  // Thread name, if any.
  nsString name;
  // CPU time in ns.
  uint64_t cpuTime = 0;
  // CPU time in cycles if available.
  uint64_t cpuCycleCount = 0;
};

// Info on a DOM window.
struct WindowInfo {
  explicit WindowInfo()
      : outerWindowId(0),
        documentURI(nullptr),
        documentTitle(u""_ns),
        isProcessRoot(false),
        isInProcess(false) {}
  WindowInfo(uint64_t aOuterWindowId, nsIURI* aDocumentURI,
             nsAString&& aDocumentTitle, bool aIsProcessRoot, bool aIsInProcess)
      : outerWindowId(aOuterWindowId),
        documentURI(aDocumentURI),
        documentTitle(std::move(aDocumentTitle)),
        isProcessRoot(aIsProcessRoot),
        isInProcess(aIsInProcess) {}

  // Internal window id.
  const uint64_t outerWindowId;

  // URI of the document.
  const nsCOMPtr<nsIURI> documentURI;

  // Title of the document.
  const nsString documentTitle;

  // True if this is the toplevel window of the process.
  // Note that this may be an iframe from another process.
  const bool isProcessRoot;

  const bool isInProcess;
};

// Info on a Utility process actor
struct UtilityInfo {
  explicit UtilityInfo() : actorName(UtilityActorName::Unknown) {}
  explicit UtilityInfo(UtilityActorName aActorName) : actorName(aActorName) {}
  const UtilityActorName actorName;
};

struct ProcInfo {
  // Process Id
  base::ProcessId pid = 0;
  // Child Id as defined by Firefox when a child process is created.
  dom::ContentParentId childId;
  // Process type
  ProcType type;
  // Origin, if any
  nsCString origin;
  // Memory size in bytes.
  uint64_t memory = 0;
  // CPU time in ns.
  uint64_t cpuTime = 0;
  uint64_t cpuCycleCount = 0;
  // Threads owned by this process.
  CopyableTArray<ThreadInfo> threads;
  // DOM windows represented by this process.
  CopyableTArray<WindowInfo> windows;
  // Utility process actors, empty for non Utility process
  CopyableTArray<UtilityInfo> utilityActors;
};

typedef MozPromise<mozilla::HashMap<base::ProcessId, ProcInfo>, nsresult, true>
    ProcInfoPromise;

/**
 * Data we need to request process info (e.g. CPU usage, memory usage)
 * from the operating system and populate the resulting `ProcInfo`.
 *
 * Note that this structure contains a mix of:
 * - low-level handles that we need to request low-level process info
 *    (`aChildTask` on macOS, `aPid` on other platforms); and
 * - high-level data that we already acquired while looking for
 * `aPid`/`aChildTask` and that we will need further down the road.
 */
struct ProcInfoRequest {
  ProcInfoRequest(base::ProcessId aPid, ProcType aProcessType,
                  const nsACString& aOrigin, nsTArray<WindowInfo>&& aWindowInfo,
                  nsTArray<UtilityInfo>&& aUtilityInfo, uint32_t aChildId = 0
#ifdef XP_MACOSX
                  ,
                  mach_port_t aChildTask = 0
#endif  // XP_MACOSX
                  )
      : pid(aPid),
        processType(aProcessType),
        origin(aOrigin),
        windowInfo(std::move(aWindowInfo)),
        utilityInfo(std::move(aUtilityInfo)),
        childId(aChildId)
#ifdef XP_MACOSX
        ,
        childTask(aChildTask)
#endif  // XP_MACOSX
  {
  }
  const base::ProcessId pid;
  const ProcType processType;
  const nsCString origin;
  const nsTArray<WindowInfo> windowInfo;
  const nsTArray<UtilityInfo> utilityInfo;
  // If the process is a child, its child id, otherwise `0`.
  const int32_t childId;
#ifdef XP_MACOSX
  const mach_port_t childTask;
#endif  // XP_MACOSX
};

/**
 * Batch a request for low-level information on Gecko processes.
 *
 * # Request
 *
 * Argument `aRequests` is a list of processes, along with high-level data
 * we have already obtained on them and that we need to populate the
 * resulting array of `ProcInfo`.
 *
 * # Result
 *
 * This call succeeds (possibly with missing data, see below) unless we
 * cannot allocate memory.
 *
 * # Performance
 *
 * - This call is always executed on a background thread.
 * - This call does NOT wake up children processes.
 * - This function is sometimes observably slow to resolve, in particular
 *   under Windows.
 *
 * # Error-handling and race conditions
 *
 * Requesting low-level information on a process and its threads is inherently
 * subject to race conditions. Typically, if a process or a thread is killed
 * while we're preparing to fetch information, we can easily end up with
 * system/lib calls that return failures.
 *
 * For this reason, this API assumes that errors when placing a system/lib call
 * are likely and normal. When some information cannot be obtained, the API will
 * simply skip over said information.
 *
 * Note that due to different choices by OSes, the exact information we skip may
 * vary across platforms. For instance, under Unix, failing to access the
 * threads of a process will cause us to skip all data on the process, while
 * under Windows, process information will be returned without thread
 * information.
 */
RefPtr<ProcInfoPromise> GetProcInfo(nsTArray<ProcInfoRequest>&& aRequests);

/**
 * Synchronous version of GetProcInfo.
 */
ProcInfoPromise::ResolveOrRejectValue GetProcInfoSync(
    nsTArray<ProcInfoRequest>&& aRequests);

/**
 * Utility function: copy data from a `ProcInfo` and into either a
 * `ParentProcInfoDictionary` or a `ChildProcInfoDictionary`.
 */
template <typename T>
nsresult CopySysProcInfoToDOM(const ProcInfo& source, T* dest) {
  // Copy system info.
  dest->mPid = source.pid;
  dest->mMemory = source.memory;
  dest->mCpuTime = source.cpuTime;
  dest->mCpuCycleCount = source.cpuCycleCount;

  // Copy thread info.
  mozilla::dom::Sequence<mozilla::dom::ThreadInfoDictionary> threads;
  for (const ThreadInfo& entry : source.threads) {
    mozilla::dom::ThreadInfoDictionary* thread =
        threads.AppendElement(fallible);
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    thread->mCpuTime = entry.cpuTime;
    thread->mCpuCycleCount = entry.cpuCycleCount;
    thread->mTid = entry.tid;
    thread->mName.Assign(entry.name);
  }
  dest->mThreads = std::move(threads);
  return NS_OK;
}

}  // namespace mozilla
#endif  // ProcInfo_h
