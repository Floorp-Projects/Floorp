/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AvailableMemoryTracker.h"

#if defined(XP_WIN)
#  include <windows.h>
#  include "nsIMemoryReporter.h"
#endif

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/Mutex.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"

#if defined(MOZ_MEMORY)
#  include "mozmemory.h"
#endif  // MOZ_MEMORY

using namespace mozilla;

Atomic<uint32_t, MemoryOrdering::Relaxed> sNumLowPhysicalMemEvents;

namespace {

#if defined(XP_WIN)

#  if defined(__MINGW32__)
// Definitions for heap optimization that require the Windows SDK to target the
// Windows 8.1 Update
static const HEAP_INFORMATION_CLASS HeapOptimizeResources =
    static_cast<HEAP_INFORMATION_CLASS>(3);

static const DWORD HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION = 1;

typedef struct _HEAP_OPTIMIZE_RESOURCES_INFORMATION {
  DWORD Version;
  DWORD Flags;
} HEAP_OPTIMIZE_RESOURCES_INFORMATION, *PHEAP_OPTIMIZE_RESOURCES_INFORMATION;
#  endif

static int64_t LowMemoryEventsPhysicalDistinguishedAmount() {
  return sNumLowPhysicalMemEvents;
}

class LowEventsReporter final : public nsIMemoryReporter {
  ~LowEventsReporter() {}

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    // clang-format off
    MOZ_COLLECT_REPORT(
      "low-memory-events/physical", KIND_OTHER, UNITS_COUNT_CUMULATIVE,
      LowMemoryEventsPhysicalDistinguishedAmount(),
"Number of low-physical-memory events fired since startup. We fire such an "
"event when a windows low memory resource notification is signaled. The "
"machine will start to page if it runs out of physical memory.  This may "
"cause it to run slowly, but it shouldn't cause it to crash.");
    // clang-format on

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(LowEventsReporter, nsIMemoryReporter)

#endif  // defined(XP_WIN)

/**
 * This runnable is executed in response to a memory-pressure event; we spin
 * the event-loop when receiving the memory-pressure event in the hope that
 * other observers will synchronously free some memory that we'll be able to
 * purge here.
 */
class nsJemallocFreeDirtyPagesRunnable final : public Runnable {
  ~nsJemallocFreeDirtyPagesRunnable() = default;

#if defined(XP_WIN)
  void OptimizeSystemHeap();
#endif

 public:
  NS_DECL_NSIRUNNABLE

  nsJemallocFreeDirtyPagesRunnable()
      : Runnable("nsJemallocFreeDirtyPagesRunnable") {}
};

NS_IMETHODIMP
nsJemallocFreeDirtyPagesRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

#if defined(MOZ_MEMORY)
  jemalloc_free_dirty_pages();
#endif

#if defined(XP_WIN)
  OptimizeSystemHeap();
#endif

  return NS_OK;
}

#if defined(XP_WIN)
void nsJemallocFreeDirtyPagesRunnable::OptimizeSystemHeap() {
  HEAP_OPTIMIZE_RESOURCES_INFORMATION heapOptInfo = {
      HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION};

  ::HeapSetInformation(nullptr, HeapOptimizeResources, &heapOptInfo,
                       sizeof(heapOptInfo));
}
#endif  // defined(XP_WIN)

/**
 * The memory pressure watcher is used for listening to memory-pressure events
 * and reacting upon them. We use one instance per process currently only for
 * cleaning up dirty unused pages held by jemalloc.
 */
class nsMemoryPressureWatcher final : public nsIObserver {
  ~nsMemoryPressureWatcher() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init();
};

NS_IMPL_ISUPPORTS(nsMemoryPressureWatcher, nsIObserver)

/**
 * Initialize and subscribe to the memory-pressure events. We subscribe to the
 * observer service in this method and not in the constructor because we need
 * to hold a strong reference to 'this' before calling the observer service.
 */
void nsMemoryPressureWatcher::Init() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();

  if (os) {
    os->AddObserver(this, "memory-pressure", /* ownsWeak */ false);
  }
}

/**
 * Reacts to all types of memory-pressure events, launches a runnable to
 * free dirty pages held by jemalloc.
 */
NS_IMETHODIMP
nsMemoryPressureWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, "memory-pressure"), "Unknown topic");

  nsCOMPtr<nsIRunnable> runnable = new nsJemallocFreeDirtyPagesRunnable();

  NS_DispatchToMainThread(runnable);

  return NS_OK;
}

}  // namespace

namespace mozilla {
namespace AvailableMemoryTracker {

void Init() {
  // The watchers are held alive by the observer service.
  RefPtr<nsMemoryPressureWatcher> watcher = new nsMemoryPressureWatcher();
  watcher->Init();

#if defined(XP_WIN)
  RegisterLowMemoryEventsPhysicalDistinguishedAmount(
      LowMemoryEventsPhysicalDistinguishedAmount);
#endif  // defined(XP_WIN)
}

}  // namespace AvailableMemoryTracker
}  // namespace mozilla
