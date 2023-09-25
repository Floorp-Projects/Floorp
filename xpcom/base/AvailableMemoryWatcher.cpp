/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AvailableMemoryWatcher.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "nsExceptionHandler.h"
#include "nsMemoryPressure.h"
#include "nsXULAppAPI.h"

namespace mozilla {

// Use this class as the initial value of
// nsAvailableMemoryWatcherBase::mCallback until RegisterCallback() is called
// so that nsAvailableMemoryWatcherBase does not have to check if its callback
// object is valid or not.
class NullTabUnloader final : public nsITabUnloader {
  ~NullTabUnloader() = default;

 public:
  NullTabUnloader() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSITABUNLOADER
};

NS_IMPL_ISUPPORTS(NullTabUnloader, nsITabUnloader)

NS_IMETHODIMP NullTabUnloader::UnloadTabAsync() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

StaticRefPtr<nsAvailableMemoryWatcherBase>
    nsAvailableMemoryWatcherBase::sSingleton;

/*static*/
already_AddRefed<nsAvailableMemoryWatcherBase>
nsAvailableMemoryWatcherBase::GetSingleton() {
  if (!sSingleton) {
    sSingleton = CreateAvailableMemoryWatcher();
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

NS_IMPL_ISUPPORTS(nsAvailableMemoryWatcherBase, nsIAvailableMemoryWatcherBase);

nsAvailableMemoryWatcherBase::nsAvailableMemoryWatcherBase()
    : mMutex("nsAvailableMemoryWatcher mutex"),
      mNumOfTabUnloading(0),
      mNumOfMemoryPressure(0),
      mTabUnloader(new NullTabUnloader),
      mInteracting(false) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Watching memory only in the main process.");
}

const char* const nsAvailableMemoryWatcherBase::kObserverTopics[] = {
    // Use this shutdown phase to make sure the instance is destroyed in GTest
    "xpcom-shutdown",
    "user-interaction-active",
    "user-interaction-inactive",
};

nsresult nsAvailableMemoryWatcherBase::Init() {
  MOZ_ASSERT(NS_IsMainThread(),
             "nsAvailableMemoryWatcherBase needs to be initialized "
             "in the main thread.");

  if (mObserverSvc) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mObserverSvc = services::GetObserverService();
  MOZ_ASSERT(mObserverSvc);

  for (auto topic : kObserverTopics) {
    nsresult rv = mObserverSvc->AddObserver(this, topic,
                                            /* ownsWeak */ false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

void nsAvailableMemoryWatcherBase::Shutdown() {
  for (auto topic : kObserverTopics) {
    mObserverSvc->RemoveObserver(this, topic);
  }
}

NS_IMETHODIMP
nsAvailableMemoryWatcherBase::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "xpcom-shutdown") == 0) {
    Shutdown();
  } else if (strcmp(aTopic, "user-interaction-inactive") == 0) {
    mInteracting = false;
#ifdef MOZ_CRASHREPORTER
    CrashReporter::SetInactiveStateStart();
#endif
  } else if (strcmp(aTopic, "user-interaction-active") == 0) {
    mInteracting = true;
#ifdef MOZ_CRASHREPORTER
    CrashReporter::ClearInactiveStateStart();
#endif
  }
  return NS_OK;
}

nsresult nsAvailableMemoryWatcherBase::RegisterTabUnloader(
    nsITabUnloader* aTabUnloader) {
  mTabUnloader = aTabUnloader;
  return NS_OK;
}

// This method is called as a part of UnloadTabAsync(). Like Notify(), if we
// call this method synchronously without releasing the lock first we can lead
// to deadlock.
nsresult nsAvailableMemoryWatcherBase::OnUnloadAttemptCompleted(
    nsresult aResult) {
  MutexAutoLock lock(mMutex);
  switch (aResult) {
    // A tab was unloaded successfully.
    case NS_OK:
      ++mNumOfTabUnloading;
      break;

    // There was no unloadable tab.
    case NS_ERROR_NOT_AVAILABLE:
      ++mNumOfMemoryPressure;
      NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
      break;

    // There was a pending task to unload a tab.
    case NS_ERROR_ABORT:
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected aResult");
      break;
  }
  return NS_OK;
}

void nsAvailableMemoryWatcherBase::UpdateLowMemoryTimeStamp() {
  if (mLowMemoryStart.IsNull()) {
    mLowMemoryStart = TimeStamp::NowLoRes();
  }
}

void nsAvailableMemoryWatcherBase::RecordTelemetryEventOnHighMemory(
    const MutexAutoLock&) {
  Telemetry::SetEventRecordingEnabled("memory_watcher"_ns, true);
  Telemetry::RecordEvent(
      Telemetry::EventID::Memory_watcher_OnHighMemory_Stats,
      Some(nsPrintfCString(
          "%u,%u,%f", mNumOfTabUnloading, mNumOfMemoryPressure,
          (TimeStamp::NowLoRes() - mLowMemoryStart).ToSeconds())),
      Nothing());
  mNumOfTabUnloading = mNumOfMemoryPressure = 0;
  mLowMemoryStart = TimeStamp();
}

// Define the fallback method for a platform for which a platform-specific
// CreateAvailableMemoryWatcher() is not defined.
#if defined(ANDROID) || \
    !defined(XP_WIN) && !defined(XP_DARWIN) && !defined(XP_LINUX)
already_AddRefed<nsAvailableMemoryWatcherBase> CreateAvailableMemoryWatcher() {
  RefPtr instance(new nsAvailableMemoryWatcherBase);
  return do_AddRef(instance);
}
#endif

}  // namespace mozilla
