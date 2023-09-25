/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AvailableMemoryWatcher.h"
#include "AvailableMemoryWatcherUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/Unused.h"
#include "nsAppRunner.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsIThread.h"
#include "nsMemoryPressure.h"

namespace mozilla {

// Linux has no native low memory detection. This class creates a timer that
// polls for low memory and sends a low memory notification if it notices a
// memory pressure event.
class nsAvailableMemoryWatcher final : public nsITimerCallback,
                                       public nsINamed,
                                       public nsAvailableMemoryWatcherBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIOBSERVER
  NS_DECL_NSINAMED

  nsresult Init() override;
  nsAvailableMemoryWatcher();

  void HandleLowMemory();
  void MaybeHandleHighMemory();

 private:
  ~nsAvailableMemoryWatcher() = default;
  void StartPolling(const MutexAutoLock&);
  void StopPolling(const MutexAutoLock&);
  void ShutDown();
  void UpdateCrashAnnotation(const MutexAutoLock&);
  static bool IsMemoryLow();

  nsCOMPtr<nsITimer> mTimer MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsIThread> mThread MOZ_GUARDED_BY(mMutex);

  bool mPolling MOZ_GUARDED_BY(mMutex);
  bool mUnderMemoryPressure MOZ_GUARDED_BY(mMutex);

  // Polling interval to check for low memory. In high memory scenarios,
  // default to 5000 ms between each check.
  static const uint32_t kHighMemoryPollingIntervalMS = 5000;

  // Polling interval to check for low memory. Default to 1000 ms between each
  // check. Use this interval when memory is low,
  static const uint32_t kLowMemoryPollingIntervalMS = 1000;
};

// A modern version of linux should keep memory information in the
// /proc/meminfo path.
static const char* kMeminfoPath = "/proc/meminfo";

nsAvailableMemoryWatcher::nsAvailableMemoryWatcher()
    : mPolling(false), mUnderMemoryPressure(false) {}

nsresult nsAvailableMemoryWatcher::Init() {
  nsresult rv = nsAvailableMemoryWatcherBase::Init();
  if (NS_FAILED(rv)) {
    return rv;
  }
  MutexAutoLock lock(mMutex);
  mTimer = NS_NewTimer();
  nsCOMPtr<nsIThread> thread;
  // We have to make our own thread here instead of using the background pool,
  // because some low memory scenarios can cause the background pool to fill.
  rv = NS_NewNamedThread("MemoryPoller", getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Couldn't make a thread for nsAvailableMemoryWatcher.");
    // In this scenario we can't poll for low memory, since we can't dispatch
    // to our memory watcher thread.
    return rv;
  }
  mThread = thread;

  // Set the crash annotation to its initial state.
  UpdateCrashAnnotation(lock);

  StartPolling(lock);

  return NS_OK;
}

already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher() {
  RefPtr watcher(new nsAvailableMemoryWatcher);

  if (NS_FAILED(watcher->Init())) {
    return do_AddRef(new nsAvailableMemoryWatcherBase);
  }

  return watcher.forget();
}

NS_IMPL_ISUPPORTS_INHERITED(nsAvailableMemoryWatcher,
                            nsAvailableMemoryWatcherBase, nsITimerCallback,
                            nsIObserver, nsINamed);

void nsAvailableMemoryWatcher::StopPolling(const MutexAutoLock&)
    MOZ_REQUIRES(mMutex) {
  if (mPolling && mTimer) {
    // stop dispatching memory checks to the thread.
    mTimer->Cancel();
    mPolling = false;
  }
}

// Check /proc/meminfo for low memory. Largely C method for reading
// /proc/meminfo.
/* static */
bool nsAvailableMemoryWatcher::IsMemoryLow() {
  MemoryInfo memInfo{0, 0};
  bool aResult = false;

  nsresult rv = ReadMemoryFile(kMeminfoPath, memInfo);

  if (NS_FAILED(rv) || memInfo.memAvailable == 0) {
    // If memAvailable cannot be found, then we are using an older system.
    // We can't accurately poll on this.
    return aResult;
  }
  unsigned long memoryAsPercentage =
      (memInfo.memAvailable * 100) / memInfo.memTotal;

  if (memoryAsPercentage <=
          StaticPrefs::browser_low_commit_space_threshold_percent() ||
      memInfo.memAvailable <
          StaticPrefs::browser_low_commit_space_threshold_mb() * 1024) {
    aResult = true;
  }

  return aResult;
}

void nsAvailableMemoryWatcher::ShutDown() {
  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock lock(mMutex);
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
    thread = mThread.forget();
  }
  // thread->Shutdown() spins a nested event loop while waiting for the thread
  // to end. But the thread might execute some previously dispatched event that
  // wants to lock our mutex, too, before arriving at the shutdown event.
  if (thread) {
    thread->Shutdown();
  }
}

// We will use this to poll for low memory.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Notify(nsITimer* aTimer) {
  MutexAutoLock lock(mMutex);
  if (!mThread) {
    // If we've made it this far and there's no  |mThread|,
    // we might have failed to dispatch it for some reason.
    MOZ_ASSERT(mThread);
    return NS_ERROR_FAILURE;
  }
  nsresult rv = mThread->Dispatch(
      NS_NewRunnableFunction("MemoryPoller", [self = RefPtr{this}]() {
        if (self->IsMemoryLow()) {
          self->HandleLowMemory();
        } else {
          self->MaybeHandleHighMemory();
        }
      }));

  if NS_FAILED (rv) {
    NS_WARNING("Cannot dispatch memory polling event.");
  }
  return NS_OK;
}

void nsAvailableMemoryWatcher::HandleLowMemory() {
  MutexAutoLock lock(mMutex);
  if (!mTimer) {
    // We have been shut down from outside while in flight.
    return;
  }
  if (!mUnderMemoryPressure) {
    mUnderMemoryPressure = true;
    UpdateCrashAnnotation(lock);
    // Poll more frequently under memory pressure.
    StartPolling(lock);
  }
  UpdateLowMemoryTimeStamp();
  // We handle low memory offthread, but we want to unload
  // tabs only from the main thread, so we will dispatch this
  // back to the main thread.
  // Since we are doing this async, we don't need to unlock the mutex first;
  // the AutoLock will unlock the mutex when we finish the dispatch.
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "nsAvailableMemoryWatcher::OnLowMemory",
      [self = RefPtr{this}]() { self->mTabUnloader->UnloadTabAsync(); }));
}

void nsAvailableMemoryWatcher::UpdateCrashAnnotation(const MutexAutoLock&)
    MOZ_REQUIRES(mMutex) {
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::LinuxUnderMemoryPressure,
      mUnderMemoryPressure);
}

// If memory is not low, we may need to dispatch an
// event for it if we have been under memory pressure.
// We can also adjust our polling interval.
void nsAvailableMemoryWatcher::MaybeHandleHighMemory() {
  MutexAutoLock lock(mMutex);
  if (!mTimer) {
    // We have been shut down from outside while in flight.
    return;
  }
  if (mUnderMemoryPressure) {
    RecordTelemetryEventOnHighMemory(lock);
    NS_NotifyOfEventualMemoryPressure(MemoryPressureState::NoPressure);
    mUnderMemoryPressure = false;
    UpdateCrashAnnotation(lock);
  }
  StartPolling(lock);
}

// When we change the polling interval, we will need to restart the timer
// on the new interval.
void nsAvailableMemoryWatcher::StartPolling(const MutexAutoLock& aLock)
    MOZ_REQUIRES(mMutex) {
  uint32_t pollingInterval = mUnderMemoryPressure
                                 ? kLowMemoryPollingIntervalMS
                                 : kHighMemoryPollingIntervalMS;
  if (!mPolling) {
    // Restart the timer with the new interval if it has stopped.
    // For testing, use a small polling interval.
    if (NS_SUCCEEDED(
            mTimer->InitWithCallback(this, gIsGtest ? 10 : pollingInterval,
                                     nsITimer::TYPE_REPEATING_SLACK))) {
      mPolling = true;
    }
  } else {
    mTimer->SetDelay(gIsGtest ? 10 : pollingInterval);
  }
}

// Observe events for shutting down and starting/stopping the timer.
NS_IMETHODIMP
nsAvailableMemoryWatcher::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  nsresult rv = nsAvailableMemoryWatcherBase::Observe(aSubject, aTopic, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (strcmp(aTopic, "xpcom-shutdown") == 0) {
    ShutDown();
  } else {
    MutexAutoLock lock(mMutex);
    if (mTimer) {
      if (strcmp(aTopic, "user-interaction-active") == 0) {
        StartPolling(lock);
      } else if (strcmp(aTopic, "user-interaction-inactive") == 0) {
        StopPolling(lock);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsAvailableMemoryWatcher::GetName(nsACString& aName) {
  aName.AssignLiteral("nsAvailableMemoryWatcher");
  return NS_OK;
}

}  // namespace mozilla
