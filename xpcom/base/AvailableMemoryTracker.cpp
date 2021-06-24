/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AvailableMemoryTracker.h"

#if defined(XP_WIN)
#  include "mozilla/StaticPrefs_browser.h"
#  include "mozilla/WindowsVersion.h"
#  include "nsExceptionHandler.h"
#  include "nsICrashReporter.h"
#  include "nsIMemoryReporter.h"
#  include "nsMemoryPressure.h"
#  include "memoryapi.h"
#endif

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/Mutex.h"
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

Atomic<uint32_t, MemoryOrdering::Relaxed> sNumLowPhysicalMemEvents;

// This class is used to monitor low memory events delivered by Windows via
// memory resource notification objects. When we enter a low memory scenario
// the LowMemoryCallback() is invoked by Windows. This initial call triggers
// an nsITimer that polls to see when the low memory condition has been lifted.
// When it has, we'll stop polling and start waiting for the next
// LowMemoryCallback(). Meanwhile, the polling may be stopped and restarted by
// user-interaction events from the observer service.
class nsAvailableMemoryWatcher final : public nsIObserver,
                                       public nsITimerCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  nsAvailableMemoryWatcher();
  nsresult Init();

 private:
  // Don't fire a low-memory notification more often than this interval.
  static const uint32_t kLowMemoryNotificationIntervalMS = 10000;

  // Observer topics we subscribe to, see below.
  static const char* const kObserverTopics[];

  static VOID CALLBACK LowMemoryCallback(PVOID aContext, BOOLEAN aIsTimer);
  static void RecordLowMemoryEvent();

  ~nsAvailableMemoryWatcher(){};
  bool RegisterMemoryResourceHandler();
  void UnregisterMemoryResourceHandler();
  void Shutdown(const MutexAutoLock&);
  bool ListenForLowMemory();
  void OnLowMemory(const MutexAutoLock&);
  void OnHighMemory(const MutexAutoLock&);
  bool IsMemoryLow() const;
  bool IsCommitSpaceLow() const;
  void StartPollingIfUserInteracting();
  void StopPolling();
  void StopPollingIfUserIdle(const MutexAutoLock&);
  void OnUserInteracting(const MutexAutoLock&);
  void OnUserIdle(const MutexAutoLock&);

  // The publicly available methods (::Observe() and ::Notify()) are called on
  // the main thread while the ::LowMemoryCallback() method is called by an
  // external thread. All functions called from those must acquire a lock on
  // this mutex before accessing the object's fields to prevent races.
  Mutex mMutex;
  nsCOMPtr<nsITimer> mTimer;
  HANDLE mLowMemoryHandle;
  HANDLE mWaitHandle;
  bool mPolling;
  bool mInteracting;
  bool mUnderMemoryPressure;
  bool mSavedReport;
  bool mIsShutdown;
};

const char* const nsAvailableMemoryWatcher::kObserverTopics[] = {
    "quit-application",
    "user-interaction-active",
    "user-interaction-inactive",
};

NS_IMPL_ISUPPORTS(nsAvailableMemoryWatcher, nsIObserver, nsITimerCallback)

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mMutex("low memory callback mutex"),
      mTimer(nullptr),
      mLowMemoryHandle(nullptr),
      mWaitHandle(nullptr),
      mPolling(false),
      mInteracting(false),
      mUnderMemoryPressure(false),
      mSavedReport(false),
      mIsShutdown(false) {}

nsresult nsAvailableMemoryWatcher::Init() {
  mTimer = NS_NewTimer();
  if (!mTimer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_ASSERT(observerService);

  if (!RegisterMemoryResourceHandler()) {
    return NS_ERROR_FAILURE;
  }

  for (auto topic : kObserverTopics) {
    nsresult rv = observerService->AddObserver(this, topic,
                                               /* ownsWeak */ false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// static
VOID CALLBACK nsAvailableMemoryWatcher::LowMemoryCallback(PVOID aContext,
                                                          BOOLEAN aIsTimer) {
  RefPtr<nsAvailableMemoryWatcher> watcher =
      already_AddRefed<nsAvailableMemoryWatcher>(
          static_cast<nsAvailableMemoryWatcher*>(aContext));
  if (!aIsTimer) {
    MutexAutoLock lock(watcher->mMutex);
    if (watcher->mIsShutdown) {
      // mWaitHandle should have been unregistered during shutdown
      MOZ_ASSERT(!watcher->mWaitHandle);
      return;
    }

    ::UnregisterWait(watcher->mWaitHandle);
    watcher->mWaitHandle = nullptr;
    watcher->OnLowMemory(lock);
  }
}

// static
void nsAvailableMemoryWatcher::RecordLowMemoryEvent() {
  sNumLowPhysicalMemEvents++;
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::LowPhysicalMemoryEvents,
      sNumLowPhysicalMemEvents);
}

bool nsAvailableMemoryWatcher::RegisterMemoryResourceHandler() {
  mLowMemoryHandle =
      ::CreateMemoryResourceNotification(LowMemoryResourceNotification);

  if (!mLowMemoryHandle) {
    return false;
  }

  return ListenForLowMemory();
}

void nsAvailableMemoryWatcher::UnregisterMemoryResourceHandler() {
  if (mWaitHandle) {
    bool res = ::UnregisterWait(mWaitHandle);
    if (res || ::GetLastError() != ERROR_IO_PENDING) {
      // We decrement the refcount only when we're sure the LowMemoryCallback()
      // callback won't be invoked, otherwise the callback will do it
      this->Release();
    }
    mWaitHandle = nullptr;
  }

  if (mLowMemoryHandle) {
    Unused << ::CloseHandle(mLowMemoryHandle);
    mLowMemoryHandle = nullptr;
  }
}

void nsAvailableMemoryWatcher::Shutdown(const MutexAutoLock&) {
  mIsShutdown = true;

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  MOZ_ASSERT(observerService);

  for (auto topic : kObserverTopics) {
    Unused << observerService->RemoveObserver(this, topic);
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  UnregisterMemoryResourceHandler();
}

bool nsAvailableMemoryWatcher::ListenForLowMemory() {
  if (mLowMemoryHandle && !mWaitHandle) {
    // We're giving ownership of this object to the LowMemoryCallback(). We
    // increment the count here so that the object is kept alive until the
    // callback decrements it.
    this->AddRef();
    bool res = ::RegisterWaitForSingleObject(
        &mWaitHandle, mLowMemoryHandle, LowMemoryCallback, this, INFINITE,
        WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE);
    if (!res) {
      // We couldn't register the callback, decrement the count
      this->Release();
    }
    return res;
  }

  return false;
}

void nsAvailableMemoryWatcher::OnLowMemory(const MutexAutoLock&) {
  mUnderMemoryPressure = true;

  // On Windows, memory allocations fails when the available commit space is
  // not sufficient.  It's possible that this callback function is invoked
  // but there is still commit space enough for the application to continue
  // to run.  In such a case, there is no strong need to trigger the memory
  // pressure event.  So we trigger the event only when the available commit
  // space is low.
  if (IsCommitSpaceLow()) {
    if (!mSavedReport) {
      // SaveMemoryReport needs to be run in the main thread
      // (See nsMemoryReporterManager::GetReportsForThisProcessExtended)
      NS_DispatchToMainThread(NS_NewRunnableFunction(
          "nsAvailableMemoryWatcher::SaveMemoryReport",
          [self = RefPtr{this}]() {
            if (nsCOMPtr<nsICrashReporter> cr =
                    do_GetService("@mozilla.org/toolkit/crash-reporter;1")) {
              MutexAutoLock lock(self->mMutex);
              self->mSavedReport = NS_SUCCEEDED(cr->SaveMemoryReport());
            }
          }));
    }

    RecordLowMemoryEvent();
    NS_DispatchEventualMemoryPressure(MemPressure_New);
  }

  StartPollingIfUserInteracting();
}

void nsAvailableMemoryWatcher::OnHighMemory(const MutexAutoLock&) {
  mUnderMemoryPressure = false;
  mSavedReport = false;  // Will save a new report if memory gets low again
  NS_DispatchEventualMemoryPressure(MemPressure_Stopping);
  StopPolling();
  ListenForLowMemory();
}

bool nsAvailableMemoryWatcher::IsMemoryLow() const {
  BOOL lowMemory = FALSE;
  if (::QueryMemoryResourceNotification(mLowMemoryHandle, &lowMemory)) {
    return lowMemory;
  }
  return false;
}

bool nsAvailableMemoryWatcher::IsCommitSpaceLow() const {
  // Other options to get the available page file size:
  //   - GetPerformanceInfo
  //     Too slow, don't use it.
  //   - PdhCollectQueryData and PdhGetRawCounterValue
  //     Faster than GetPerformanceInfo, but slower than GlobalMemoryStatusEx.
  //   - NtQuerySystemInformation(SystemMemoryUsageInformation)
  //     Faster than GlobalMemoryStatusEx, but undocumented.
  MEMORYSTATUSEX memStatus = {sizeof(memStatus)};
  if (!::GlobalMemoryStatusEx(&memStatus)) {
    return false;
  }

  constexpr size_t kBytesPerMB = 1024 * 1024;
  return (memStatus.ullAvailPageFile / kBytesPerMB) <
         StaticPrefs::browser_low_commit_space_threshold_mb();
}

void nsAvailableMemoryWatcher::StartPollingIfUserInteracting() {
  if (mInteracting && !mPolling) {
    if (NS_SUCCEEDED(
            mTimer->InitWithCallback(this, kLowMemoryNotificationIntervalMS,
                                     nsITimer::TYPE_REPEATING_SLACK))) {
      mPolling = true;
    }
  }
}

void nsAvailableMemoryWatcher::StopPolling() {
  mTimer->Cancel();
  mPolling = false;
}

void nsAvailableMemoryWatcher::StopPollingIfUserIdle(const MutexAutoLock&) {
  if (!mInteracting) {
    StopPolling();
  }
}

void nsAvailableMemoryWatcher::OnUserInteracting(const MutexAutoLock&) {
  mInteracting = true;
  if (mUnderMemoryPressure) {
    StartPollingIfUserInteracting();
  }
}

void nsAvailableMemoryWatcher::OnUserIdle(const MutexAutoLock&) {
  mInteracting = false;
}

// Timer callback, polls the low memory resource notification to detect when
// we've freed enough memory or if we have to send more memory pressure events.
// Polling stops automatically when the user is not interacting with the UI.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Notify(nsITimer* aTimer) {
  MutexAutoLock lock(mMutex);
  StopPollingIfUserIdle(lock);

  if (!IsMemoryLow()) {
    OnHighMemory(lock);
    return NS_OK;
  }

  if (IsCommitSpaceLow()) {
    NS_DispatchEventualMemoryPressure(MemPressure_Ongoing);
  }

  return NS_OK;
}

// Observer service callback, used to stop the polling timer when the user
// stops interacting with Firefox and resuming it when they interact again.
// Also used to shut down the service if the application is quitting.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  MutexAutoLock lock(mMutex);

  if (strcmp(aTopic, "quit-application") == 0) {
    Shutdown(lock);
  } else if (strcmp(aTopic, "user-interaction-inactive") == 0) {
    OnUserIdle(lock);
  } else if (strcmp(aTopic, "user-interaction-active") == 0) {
    OnUserInteracting(lock);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unknown topic");
  }

  return NS_OK;
}

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
