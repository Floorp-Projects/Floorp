/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AvailableMemoryTracker.h"

#if defined(XP_WIN)
#  include "mozilla/WindowsVersion.h"
#  include "nsExceptionHandler.h"
#  include "nsICrashReporter.h"
#  include "nsIMemoryReporter.h"
#  include "nsMemoryPressure.h"
#endif

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"

#if defined(MOZ_MEMORY)
#  include "mozmemory.h"
#endif  // MOZ_MEMORY

using namespace mozilla;

namespace {

#if defined(XP_WIN)

#  if (NTDDI_VERSION < NTDDI_WINBLUE) || \
      (NTDDI_VERSION == NTDDI_WINBLUE && !defined(WINBLUE_KBSPRING14))
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

Atomic<uint32_t, MemoryOrdering::Relaxed> sNumLowVirtualMemEvents;
Atomic<uint32_t, MemoryOrdering::Relaxed> sNumLowCommitSpaceEvents;

class nsAvailableMemoryWatcher final : public nsIObserver,
                                       public nsITimerCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  nsAvailableMemoryWatcher();
  nsresult Init();

 private:
  // Fire a low-memory notification if we have less than this many bytes of
  // virtual address space available.
#  if defined(HAVE_64BIT_BUILD)
  static const size_t kLowVirtualMemoryThreshold = 0;
#  else
  static const size_t kLowVirtualMemoryThreshold = 384 * 1024 * 1024;
#  endif

  // Fire a low-memory notification if we have less than this many bytes of
  // commit space (physical memory plus page file) left.
  static const size_t kLowCommitSpaceThreshold = 384 * 1024 * 1024;

  // Don't fire a low-memory notification more often than this interval.
  static const uint32_t kLowMemoryNotificationIntervalMS = 10000;

  // Poll the amount of free memory at this rate.
  static const uint32_t kPollingIntervalMS = 1000;

  // Observer topics we subscribe to, see below.
  static const char* const kObserverTopics[];

  static bool IsVirtualMemoryLow(const MEMORYSTATUSEX& aStat);
  static bool IsCommitSpaceLow(const MEMORYSTATUSEX& aStat);

  ~nsAvailableMemoryWatcher(){};
  bool OngoingMemoryPressure() { return mUnderMemoryPressure; }
  void AdjustPollingInterval(const bool aLowMemory);
  void SendMemoryPressureEvent();
  void MaybeSendMemoryPressureStopEvent();
  void MaybeSaveMemoryReport();
  void Shutdown();

  nsCOMPtr<nsITimer> mTimer;
  bool mUnderMemoryPressure;
  bool mSavedReport;
};

const char* const nsAvailableMemoryWatcher::kObserverTopics[] = {
    "quit-application",
    "user-interaction-active",
    "user-interaction-inactive",
};

NS_IMPL_ISUPPORTS(nsAvailableMemoryWatcher, nsIObserver, nsITimerCallback)

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mTimer(nullptr), mUnderMemoryPressure(false), mSavedReport(false) {}

nsresult nsAvailableMemoryWatcher::Init() {
  mTimer = NS_NewTimer();

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_ASSERT(observerService);

  for (auto topic : kObserverTopics) {
    nsresult rv = observerService->AddObserver(this, topic,
                                               /* ownsWeak */ false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  MOZ_TRY(mTimer->InitWithCallback(this, kPollingIntervalMS,
                                   nsITimer::TYPE_REPEATING_SLACK));
  return NS_OK;
}

void nsAvailableMemoryWatcher::Shutdown() {
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_ASSERT(observerService);

  for (auto topic : kObserverTopics) {
    Unused << observerService->RemoveObserver(this, topic);
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

/* static */
bool nsAvailableMemoryWatcher::IsVirtualMemoryLow(const MEMORYSTATUSEX& aStat) {
  if ((kLowVirtualMemoryThreshold != 0) &&
      (aStat.ullAvailVirtual < kLowVirtualMemoryThreshold)) {
    sNumLowVirtualMemEvents++;
    return true;
  }

  return false;
}

/* static */
bool nsAvailableMemoryWatcher::IsCommitSpaceLow(const MEMORYSTATUSEX& aStat) {
  if ((kLowCommitSpaceThreshold != 0) &&
      (aStat.ullAvailPageFile < kLowCommitSpaceThreshold)) {
    sNumLowCommitSpaceEvents++;
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::LowCommitSpaceEvents,
        uint32_t(sNumLowCommitSpaceEvents));
    return true;
  }

  return false;
}

void nsAvailableMemoryWatcher::SendMemoryPressureEvent() {
  MemoryPressureState state =
      OngoingMemoryPressure() ? MemPressure_Ongoing : MemPressure_New;
  NS_DispatchEventualMemoryPressure(state);
}

void nsAvailableMemoryWatcher::MaybeSendMemoryPressureStopEvent() {
  if (OngoingMemoryPressure()) {
    NS_DispatchEventualMemoryPressure(MemPressure_Stopping);
  }
}

void nsAvailableMemoryWatcher::MaybeSaveMemoryReport() {
  if (!mSavedReport && OngoingMemoryPressure()) {
    nsCOMPtr<nsICrashReporter> cr =
        do_GetService("@mozilla.org/toolkit/crash-reporter;1");
    if (cr) {
      if (NS_SUCCEEDED(cr->SaveMemoryReport())) {
        mSavedReport = true;
      }
    }
  }
}

void nsAvailableMemoryWatcher::AdjustPollingInterval(const bool aLowMemory) {
  if (aLowMemory) {
    // We entered a low-memory state, wait for a longer interval before polling
    // again as there's no point in rapidly sending further notifications.
    mTimer->SetDelay(kLowMemoryNotificationIntervalMS);
  } else if (OngoingMemoryPressure()) {
    // We were under memory pressure but we're not anymore, resume polling at
    // a faster pace.
    mTimer->SetDelay(kPollingIntervalMS);
  }
}

// Timer callback, polls memory stats to detect low-memory conditions. This
// will send memory-pressure events if memory is running low and adjust the
// polling interval accordingly.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Notify(nsITimer* aTimer) {
  MEMORYSTATUSEX stat;
  stat.dwLength = sizeof(stat);
  bool success = GlobalMemoryStatusEx(&stat);

  if (success) {
    bool lowMemory = IsVirtualMemoryLow(stat) || IsCommitSpaceLow(stat);

    if (lowMemory) {
      SendMemoryPressureEvent();
    } else {
      MaybeSendMemoryPressureStopEvent();
    }

    if (lowMemory) {
      MaybeSaveMemoryReport();
    } else {
      mSavedReport = false;  // Save a new report if memory gets low again
    }

    AdjustPollingInterval(lowMemory);
    mUnderMemoryPressure = lowMemory;
  }

  return NS_OK;
}

// Observer service callback, used to stop the polling timer when the user
// stops interacting with Firefox and resuming it when they interact again.
// Also used to shut down the service if the application is quitting.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  if (strcmp(aTopic, "quit-application") == 0) {
    Shutdown();
  } else if (strcmp(aTopic, "user-interaction-inactive") == 0) {
    mTimer->Cancel();
  } else if (strcmp(aTopic, "user-interaction-active") == 0) {
    mTimer->InitWithCallback(this, kPollingIntervalMS,
                             nsITimer::TYPE_REPEATING_SLACK);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unknown topic");
  }

  return NS_OK;
}

static int64_t LowMemoryEventsVirtualDistinguishedAmount() {
  return sNumLowVirtualMemEvents;
}

static int64_t LowMemoryEventsCommitSpaceDistinguishedAmount() {
  return sNumLowCommitSpaceEvents;
}

class LowEventsReporter final : public nsIMemoryReporter {
  ~LowEventsReporter() {}

 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    // clang-format off
    MOZ_COLLECT_REPORT(
      "low-memory-events/virtual", KIND_OTHER, UNITS_COUNT_CUMULATIVE,
      LowMemoryEventsVirtualDistinguishedAmount(),
"Number of low-virtual-memory events fired since startup. We fire such an "
"event if we notice there is less than predefined amount of virtual address "
"space available (if zero, this behavior is disabled, see "
"xpcom/base/AvailableMemoryTracker.cpp). The process will probably crash if "
"it runs out of virtual address space, so this event is dire.");

    MOZ_COLLECT_REPORT(
      "low-memory-events/commit-space", KIND_OTHER, UNITS_COUNT_CUMULATIVE,
      LowMemoryEventsCommitSpaceDistinguishedAmount(),
"Number of low-commit-space events fired since startup. We fire such an "
"event if we notice there is less than a predefined amount of commit space "
"available (if zero, this behavior is disabled, see "
"xpcom/base/AvailableMemoryTracker.cpp). Windows will likely kill the process "
"if it runs out of commit space, so this event is dire.");
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
class nsJemallocFreeDirtyPagesRunnable final : public nsIRunnable {
  ~nsJemallocFreeDirtyPagesRunnable() {}

#if defined(XP_WIN)
  void OptimizeSystemHeap();
#endif

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

NS_IMPL_ISUPPORTS(nsJemallocFreeDirtyPagesRunnable, nsIRunnable)

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
  // HeapSetInformation exists prior to Windows 8.1, but the
  // HeapOptimizeResources information class does not.
  if (IsWin8Point1OrLater()) {
    HEAP_OPTIMIZE_RESOURCES_INFORMATION heapOptInfo = {
        HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION};

    ::HeapSetInformation(nullptr, HeapOptimizeResources, &heapOptInfo,
                         sizeof(heapOptInfo));
  }
}
#endif  // defined(XP_WIN)

/**
 * The memory pressure watcher is used for listening to memory-pressure events
 * and reacting upon them. We use one instance per process currently only for
 * cleaning up dirty unused pages held by jemalloc.
 */
class nsMemoryPressureWatcher final : public nsIObserver {
  ~nsMemoryPressureWatcher() {}

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
  RegisterStrongMemoryReporter(new LowEventsReporter());
  RegisterLowMemoryEventsVirtualDistinguishedAmount(
      LowMemoryEventsVirtualDistinguishedAmount);
  RegisterLowMemoryEventsCommitSpaceDistinguishedAmount(
      LowMemoryEventsCommitSpaceDistinguishedAmount);

  if (XRE_IsParentProcess()) {
    RefPtr<nsAvailableMemoryWatcher> poller = new nsAvailableMemoryWatcher();

    if (NS_FAILED(poller->Init())) {
      NS_WARNING("Could not start the available memory watcher");
    }
  }
#endif  // defined(XP_WIN)
}

}  // namespace AvailableMemoryTracker
}  // namespace mozilla
