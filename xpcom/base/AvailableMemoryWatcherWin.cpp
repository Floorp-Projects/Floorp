/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AvailableMemoryWatcher.h"
#include "mozilla/Atomics.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsAppRunner.h"
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsMemoryPressure.h"
#include "nsServiceManagerUtils.h"
#include "nsWindowsHelpers.h"

#include <memoryapi.h>

extern mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::Relaxed>
    sNumLowPhysicalMemEvents;

namespace mozilla {

// This class is used to monitor low memory events delivered by Windows via
// memory resource notification objects. When we enter a low memory scenario
// the LowMemoryCallback() is invoked by Windows. This initial call triggers
// an nsITimer that polls to see when the low memory condition has been lifted.
// When it has, we'll stop polling and start waiting for the next
// LowMemoryCallback(). Meanwhile, the polling may be stopped and restarted by
// user-interaction events from the observer service.
class nsAvailableMemoryWatcher final : public nsITimerCallback,
                                       public nsINamed,
                                       public nsAvailableMemoryWatcherBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  nsAvailableMemoryWatcher();
  nsresult Init() override;

 private:
  static VOID CALLBACK LowMemoryCallback(PVOID aContext, BOOLEAN aIsTimer);
  static void RecordLowMemoryEvent();
  static bool IsCommitSpaceLow();

  ~nsAvailableMemoryWatcher();
  bool RegisterMemoryResourceHandler(const MutexAutoLock& aLock)
      MOZ_REQUIRES(mMutex);
  void UnregisterMemoryResourceHandler(const MutexAutoLock&)
      MOZ_REQUIRES(mMutex);
  void MaybeSaveMemoryReport(const MutexAutoLock&) MOZ_REQUIRES(mMutex);
  void Shutdown(const MutexAutoLock& aLock) MOZ_REQUIRES(mMutex);
  bool ListenForLowMemory(const MutexAutoLock&) MOZ_REQUIRES(mMutex);
  void OnLowMemory(const MutexAutoLock&) MOZ_REQUIRES(mMutex);
  void OnHighMemory(const MutexAutoLock&) MOZ_REQUIRES(mMutex);
  void StartPollingIfUserInteracting(const MutexAutoLock& aLock)
      MOZ_REQUIRES(mMutex);
  void StopPolling(const MutexAutoLock&) MOZ_REQUIRES(mMutex);
  void StopPollingIfUserIdle(const MutexAutoLock&) MOZ_REQUIRES(mMutex);
  void OnUserInteracting(const MutexAutoLock&) MOZ_REQUIRES(mMutex);

  nsCOMPtr<nsITimer> mTimer MOZ_GUARDED_BY(mMutex);
  nsAutoHandle mLowMemoryHandle MOZ_GUARDED_BY(mMutex);
  HANDLE mWaitHandle MOZ_GUARDED_BY(mMutex);
  bool mPolling MOZ_GUARDED_BY(mMutex);

  // Indicates whether to start a timer when user interaction is notified.
  // This flag is needed because the low-memory callback may be triggered when
  // the user is inactive and we want to delay-start the timer.
  bool mNeedToRestartTimerOnUserInteracting MOZ_GUARDED_BY(mMutex);
  // Indicate that the available commit space is low.  The timer handler needs
  // this flag because it is triggered by the low physical memory regardless
  // of the available commit space.
  bool mUnderMemoryPressure MOZ_GUARDED_BY(mMutex);

  bool mSavedReport MOZ_GUARDED_BY(mMutex);
  bool mIsShutdown MOZ_GUARDED_BY(mMutex);

  // Members below this line are used only in the main thread.
  // No lock is needed.

  // Don't fire a low-memory notification more often than this interval.
  uint32_t mPollingInterval;
};

NS_IMPL_ISUPPORTS_INHERITED(nsAvailableMemoryWatcher,
                            nsAvailableMemoryWatcherBase, nsIObserver,
                            nsITimerCallback, nsINamed)

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mWaitHandle(nullptr),
      mPolling(false),
      mNeedToRestartTimerOnUserInteracting(false),
      mUnderMemoryPressure(false),
      mSavedReport(false),
      mIsShutdown(false),
      mPollingInterval(0) {}

nsresult nsAvailableMemoryWatcher::Init() {
  nsresult rv = nsAvailableMemoryWatcherBase::Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  MutexAutoLock lock(mMutex);
  mTimer = NS_NewTimer();
  if (!mTimer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Use a very short interval for GTest to verify the timer's behavior.
  mPollingInterval = gIsGtest ? 10 : 10000;

  if (!RegisterMemoryResourceHandler(lock)) {
    return NS_ERROR_FAILURE;
  }

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
  if (aIsTimer) {
    return;
  }

  // The |watcher| was addref'ed when we registered the wait handle in
  // ListenForLowMemory().  It is decremented when exiting the function,
  // so please make sure we unregister the wait handle after this line.
  RefPtr<nsAvailableMemoryWatcher> watcher =
      already_AddRefed<nsAvailableMemoryWatcher>(
          static_cast<nsAvailableMemoryWatcher*>(aContext));

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
  } else {
    // Since we have unregistered the callback, we need to start a timer to
    // continue watching memory.
    watcher->StartPollingIfUserInteracting(lock);
  }
}

// static
void nsAvailableMemoryWatcher::RecordLowMemoryEvent() {
  sNumLowPhysicalMemEvents++;
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::LowPhysicalMemoryEvents,
      sNumLowPhysicalMemEvents);
}

bool nsAvailableMemoryWatcher::RegisterMemoryResourceHandler(
    const MutexAutoLock& aLock) {
  mLowMemoryHandle.own(
      ::CreateMemoryResourceNotification(LowMemoryResourceNotification));

  if (!mLowMemoryHandle) {
    return false;
  }

  return ListenForLowMemory(aLock);
}

void nsAvailableMemoryWatcher::UnregisterMemoryResourceHandler(
    const MutexAutoLock&) {
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

void nsAvailableMemoryWatcher::Shutdown(const MutexAutoLock& aLock) {
  mIsShutdown = true;
  mNeedToRestartTimerOnUserInteracting = false;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  UnregisterMemoryResourceHandler(aLock);
}

bool nsAvailableMemoryWatcher::ListenForLowMemory(const MutexAutoLock&) {
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
    // Once we register the wait handle, we no longer need to start
    // the timer because we can continue watching memory via callback.
    mNeedToRestartTimerOnUserInteracting = false;
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
    UpdateLowMemoryTimeStamp();
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
            self->UpdateLowMemoryTimeStamp();
          }
          self->mTabUnloader->UnloadTabAsync();
        }));
  }

  StartPollingIfUserInteracting(aLock);
}

void nsAvailableMemoryWatcher::OnHighMemory(const MutexAutoLock& aLock) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mUnderMemoryPressure) {
    RecordTelemetryEventOnHighMemory(aLock);
    NS_NotifyOfEventualMemoryPressure(MemoryPressureState::NoPressure);
  }

  mUnderMemoryPressure = false;
  mSavedReport = false;  // Will save a new report if memory gets low again
  StopPolling(aLock);
  ListenForLowMemory(aLock);
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
  // When the user is inactive, we mark the flag to delay-start
  // the timer when the user becomes active later.
  mNeedToRestartTimerOnUserInteracting = true;

  if (mInteracting && !mPolling) {
    if (NS_SUCCEEDED(mTimer->InitWithCallback(
            this, mPollingInterval, nsITimer::TYPE_REPEATING_SLACK))) {
      mPolling = true;
    }
  }
}

void nsAvailableMemoryWatcher::StopPolling(const MutexAutoLock&) {
  mTimer->Cancel();
  mPolling = false;
}

void nsAvailableMemoryWatcher::StopPollingIfUserIdle(
    const MutexAutoLock& aLock) {
  if (!mInteracting) {
    StopPolling(aLock);
  }
}

void nsAvailableMemoryWatcher::OnUserInteracting(const MutexAutoLock& aLock) {
  if (mNeedToRestartTimerOnUserInteracting) {
    StartPollingIfUserInteracting(aLock);
  }
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

NS_IMETHODIMP
nsAvailableMemoryWatcher::GetName(nsACString& aName) {
  aName.AssignLiteral("nsAvailableMemoryWatcher");
  return NS_OK;
}

// Observer service callback, used to stop the polling timer when the user
// stops interacting with Firefox and resuming it when they interact again.
// Also used to shut down the service if the application is quitting.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  nsresult rv = nsAvailableMemoryWatcherBase::Observe(aSubject, aTopic, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MutexAutoLock lock(mMutex);

  if (strcmp(aTopic, "xpcom-shutdown") == 0) {
    Shutdown(lock);
  } else if (strcmp(aTopic, "user-interaction-active") == 0) {
    OnUserInteracting(lock);
  }

  return NS_OK;
}

already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher() {
  RefPtr watcher(new nsAvailableMemoryWatcher);
  if (NS_FAILED(watcher->Init())) {
    return do_AddRef(new nsAvailableMemoryWatcherBase);  // fallback
  }
  return watcher.forget();
}

}  // namespace mozilla
