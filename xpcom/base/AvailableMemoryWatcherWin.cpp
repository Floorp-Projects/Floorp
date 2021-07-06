/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AvailableMemoryWatcher.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Unused.h"
#include "nsAppRunner.h"
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsMemoryPressure.h"
#include "nsWindowsHelpers.h"

#include <memoryapi.h>

namespace mozilla {

// This class is used to monitor low memory events delivered by Windows via
// memory resource notification objects. When we enter a low memory scenario
// the LowMemoryCallback() is invoked by Windows. This initial call triggers
// an nsITimer that polls to see when the low memory condition has been lifted.
// When it has, we'll stop polling and start waiting for the next
// LowMemoryCallback(). Meanwhile, the polling may be stopped and restarted by
// user-interaction events from the observer service.
class nsAvailableMemoryWatcher final : public nsIObserver,
                                       public nsITimerCallback,
                                       public nsAvailableMemoryWatcherBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  nsAvailableMemoryWatcher();
  nsresult Init(uint32_t aPollingInterval);

 private:
  // Observer topics we subscribe to, see below.
  static const char* const kObserverTopics[];

  static VOID CALLBACK LowMemoryCallback(PVOID aContext, BOOLEAN aIsTimer);
  static void RecordLowMemoryEvent();
  static bool IsCommitSpaceLow();

  ~nsAvailableMemoryWatcher();
  bool RegisterMemoryResourceHandler();
  void UnregisterMemoryResourceHandler();
  void MaybeSaveMemoryReport(const MutexAutoLock&);
  void Shutdown(const MutexAutoLock&);
  bool ListenForLowMemory();
  void OnLowMemory(const MutexAutoLock&);
  void OnHighMemory(const MutexAutoLock&);
  void StartPollingIfUserInteracting(const MutexAutoLock&);
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
  nsAutoHandle mLowMemoryHandle;
  HANDLE mWaitHandle;
  bool mPolling;
  bool mInteracting;
  // Indicates whether to start a timer when user interaction is notified
  bool mUnderMemoryPressure;
  bool mSavedReport;
  bool mIsShutdown;
  // These members are used only in the main thread.  No lock is needed.
  bool mInitialized;
  uint32_t mPollingInterval;
  nsCOMPtr<nsIObserverService> mObserverSvc;
};

const char* const nsAvailableMemoryWatcher::kObserverTopics[] = {
    // Use this shutdown phase to make sure the instance is destroyed in GTest
    "xpcom-shutdown",
    "user-interaction-active",
    "user-interaction-inactive",
};

NS_IMPL_ISUPPORTS_INHERITED(nsAvailableMemoryWatcher,
                            nsAvailableMemoryWatcherBase, nsIObserver,
                            nsITimerCallback)

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mMutex("low memory callback mutex"),
      mWaitHandle(nullptr),
      mPolling(false),
      mInteracting(false),
      mUnderMemoryPressure(false),
      mSavedReport(false),
      mIsShutdown(false),
      mInitialized(false),
      mPollingInterval(0) {}

nsresult nsAvailableMemoryWatcher::Init(uint32_t aPollingInterval) {
  MOZ_ASSERT(
      NS_IsMainThread(),
      "nsAvailableMemoryWatcher needs to be initialized in the main thread.");
  if (mInitialized) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mTimer = NS_NewTimer();
  if (!mTimer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mObserverSvc = services::GetObserverService();
  MOZ_ASSERT(mObserverSvc);
  mPollingInterval = aPollingInterval;

  if (!RegisterMemoryResourceHandler()) {
    return NS_ERROR_FAILURE;
  }

  for (auto topic : kObserverTopics) {
    nsresult rv = mObserverSvc->AddObserver(this, topic,
                                            /* ownsWeak */ false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mInitialized = true;
  return NS_OK;
}

nsAvailableMemoryWatcher::~nsAvailableMemoryWatcher() {
  // These handles should have been released during the shutdown phase.
  MOZ_ASSERT(!mLowMemoryHandle);
  MOZ_ASSERT(!mWaitHandle);
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

    // On Windows, memory allocations fails when the available commit space is
    // not sufficient.  It's possible that this callback function is invoked
    // but there is still commit space enough for the application to continue
    // to run.  In such a case, there is no strong need to trigger the memory
    // pressure event.  So we trigger the event only when the available commit
    // space is low.
    if (IsCommitSpaceLow()) {
      watcher->OnLowMemory(lock);
    }
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
  mLowMemoryHandle.own(
      ::CreateMemoryResourceNotification(LowMemoryResourceNotification));

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

  mLowMemoryHandle.reset();
}

void nsAvailableMemoryWatcher::Shutdown(const MutexAutoLock&) {
  mIsShutdown = true;

  for (auto topic : kObserverTopics) {
    Unused << mObserverSvc->RemoveObserver(this, topic);
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

void nsAvailableMemoryWatcher::MaybeSaveMemoryReport(const MutexAutoLock&) {
  if (mSavedReport) {
    return;
  }

  if (nsCOMPtr<nsICrashReporter> cr =
          do_GetService("@mozilla.org/toolkit/crash-reporter;1")) {
    mSavedReport = NS_SUCCEEDED(cr->SaveMemoryReport());
  }
}

void nsAvailableMemoryWatcher::OnLowMemory(const MutexAutoLock& aLock) {
  mUnderMemoryPressure = true;
  RecordLowMemoryEvent();

  if (NS_IsMainThread()) {
    MaybeSaveMemoryReport(aLock);
    {
      // Don't invoke UnloadTabAsync() with the lock to avoid deadlock
      // because nsAvailableMemoryWatcher::Notify may be invoked while
      // running the method.
      MutexAutoUnlock unlock(mMutex);
      mTabUnloader->UnloadTabAsync();
    }
  } else {
    // SaveMemoryReport and mTabUnloader needs to be run in the main thread
    // (See nsMemoryReporterManager::GetReportsForThisProcessExtended)
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "nsAvailableMemoryWatcher::OnLowMemory", [self = RefPtr{this}]() {
          {
            MutexAutoLock lock(self->mMutex);
            self->MaybeSaveMemoryReport(lock);
          }
          self->mTabUnloader->UnloadTabAsync();
        }));
  }

  StartPollingIfUserInteracting(aLock);
}

void nsAvailableMemoryWatcher::OnHighMemory(const MutexAutoLock&) {
  MOZ_ASSERT(NS_IsMainThread());

  mUnderMemoryPressure = false;
  mSavedReport = false;  // Will save a new report if memory gets low again
  NS_NotifyOfEventualMemoryPressure(MemoryPressureState::NoPressure);
  StopPolling();
  ListenForLowMemory();
}

// static
bool nsAvailableMemoryWatcher::IsCommitSpaceLow() {
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

void nsAvailableMemoryWatcher::StartPollingIfUserInteracting(
    const MutexAutoLock&) {
  if (mInteracting && !mPolling) {
    if (NS_SUCCEEDED(mTimer->InitWithCallback(
            this, mPollingInterval, nsITimer::TYPE_REPEATING_SLACK))) {
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

void nsAvailableMemoryWatcher::OnUserInteracting(const MutexAutoLock& aLock) {
  mInteracting = true;
  if (mUnderMemoryPressure) {
    StartPollingIfUserInteracting(aLock);
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

  if (IsCommitSpaceLow()) {
    OnLowMemory(lock);
  } else {
    OnHighMemory(lock);
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

  if (strcmp(aTopic, "xpcom-shutdown") == 0) {
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

already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher() {
  // Don't fire a low-memory notification more often than this interval.
  // (Use a very short interval for GTest to verify the timer's behavior)
  const uint32_t kLowMemoryNotificationIntervalMS = gIsGtest ? 10 : 10000;

  RefPtr watcher(new nsAvailableMemoryWatcher);
  if (NS_FAILED(watcher->Init(kLowMemoryNotificationIntervalMS))) {
    return do_AddRef(new nsAvailableMemoryWatcherBase);  // fallback
  }
  return watcher.forget();
}

}  // namespace mozilla
