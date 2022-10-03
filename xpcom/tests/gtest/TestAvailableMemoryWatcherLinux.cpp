/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <sys/mman.h>  // For memory-locking.

#include "gtest/gtest.h"

#include "AvailableMemoryWatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsMemoryPressure.h"

using namespace mozilla;

namespace {

// Dummy tab unloader whose one job is to dispatch a low memory event.
class MockTabUnloader final : public nsITabUnloader {
  NS_DECL_THREADSAFE_ISUPPORTS
 public:
  MockTabUnloader() = default;

  NS_IMETHOD UnloadTabAsync() override {
    // We want to issue a memory pressure event for
    NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
    return NS_OK;
  }

 private:
  ~MockTabUnloader() = default;
};

NS_IMPL_ISUPPORTS(MockTabUnloader, nsITabUnloader)

// Class that gradually increases the percent memory threshold
// until it reaches 100%, which should guarantee a memory pressure
// notification.
class AvailableMemoryChecker final : public nsITimerCallback, public nsINamed {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  AvailableMemoryChecker();
  void Init();
  void Shutdown();

 private:
  ~AvailableMemoryChecker() = default;

  bool mResolved;
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<nsAvailableMemoryWatcherBase> mWatcher;
  RefPtr<MockTabUnloader> mTabUnloader;

  const uint32_t kPollingInterval = 50;
  const uint32_t kPrefIncrement = 5;
};

AvailableMemoryChecker::AvailableMemoryChecker() : mResolved(false) {}

NS_IMPL_ISUPPORTS(AvailableMemoryChecker, nsITimerCallback, nsINamed);

void AvailableMemoryChecker::Init() {
  mTabUnloader = new MockTabUnloader;

  mWatcher = nsAvailableMemoryWatcherBase::GetSingleton();
  mWatcher->RegisterTabUnloader(mTabUnloader);

  mTimer = NS_NewTimer();
  mTimer->InitWithCallback(this, kPollingInterval,
                           nsITimer::TYPE_REPEATING_SLACK);
}

void AvailableMemoryChecker::Shutdown() {
  if (mTimer) {
    mTimer->Cancel();
  }
  Preferences::ClearUser("browser.low_commit_space_threshold_percent");
}

// Timer callback to increase the pref threshold.
NS_IMETHODIMP
AvailableMemoryChecker::Notify(nsITimer* aTimer) {
  uint32_t threshold =
      StaticPrefs::browser_low_commit_space_threshold_percent();
  if (threshold >= 100) {
    mResolved = true;
    return NS_OK;
  }
  threshold += kPrefIncrement;
  Preferences::SetUint("browser.low_commit_space_threshold_percent", threshold);
  return NS_OK;
}

NS_IMETHODIMP AvailableMemoryChecker::GetName(nsACString& aName) {
  aName.AssignLiteral("AvailableMemoryChecker");
  return NS_OK;
}

// Class that listens for a given notification, then records
// if it was received.
class Spinner final : public nsIObserver {
  nsCOMPtr<nsIObserverService> mObserverSvc;
  nsDependentCString mTopic;
  bool mTopicObserved;

  ~Spinner() = default;

 public:
  NS_DECL_ISUPPORTS

  Spinner(nsIObserverService* aObserverSvc, const char* aTopic)
      : mObserverSvc(aObserverSvc), mTopic(aTopic), mTopicObserved(false) {}

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (mTopic == aTopic) {
      mTopicObserved = true;
      mObserverSvc->RemoveObserver(this, aTopic);

      // Force the loop to move in case there is no event in the queue.
      nsCOMPtr<nsIRunnable> dummyEvent = new Runnable(__func__);
      NS_DispatchToMainThread(dummyEvent);
    }
    return NS_OK;
  }
  void StartListening() {
    mObserverSvc->AddObserver(this, mTopic.get(), false);
  }
  bool TopicObserved() { return mTopicObserved; }
  bool WaitForNotification();
};
NS_IMPL_ISUPPORTS(Spinner, nsIObserver);

bool Spinner::WaitForNotification() {
  bool isTimeout = false;

  nsCOMPtr<nsITimer> timer;

  // This timer should time us out if we never observe our notification.
  // Set to 5000 since the memory checker should finish incrementing the
  // pref by then, and if it hasn't then it is probably stuck somehow.
  NS_NewTimerWithFuncCallback(
      getter_AddRefs(timer),
      [](nsITimer*, void* isTimeout) {
        *reinterpret_cast<bool*>(isTimeout) = true;
      },
      &isTimeout, 5000, nsITimer::TYPE_ONE_SHOT, __func__);

  SpinEventLoopUntil("Spinner:WaitForNotification"_ns, [&]() -> bool {
    if (isTimeout) {
      return true;
    }
    return mTopicObserved;
  });
  return !isTimeout;
}

void StartUserInteraction(const nsCOMPtr<nsIObserverService>& aObserverSvc) {
  aObserverSvc->NotifyObservers(nullptr, "user-interaction-active", nullptr);
}

TEST(AvailableMemoryWatcher, BasicTest)
{
  nsCOMPtr<nsIObserverService> observerSvc = services::GetObserverService();
  RefPtr<Spinner> aSpinner = new Spinner(observerSvc, "memory-pressure");
  aSpinner->StartListening();

  // Start polling for low memory.
  StartUserInteraction(observerSvc);

  RefPtr<AvailableMemoryChecker> checker = new AvailableMemoryChecker();
  checker->Init();

  aSpinner->WaitForNotification();

  // The checker should have dispatched a low memory event before reaching 100%
  // memory pressure threshold, so the topic should be observed by the spinner.
  EXPECT_TRUE(aSpinner->TopicObserved());
  checker->Shutdown();
}

TEST(AvailableMemoryWatcher, MemoryLowToHigh)
{
  // Setting this pref to 100 ensures we start in a low memory scenario.
  Preferences::SetUint("browser.low_commit_space_threshold_percent", 100);

  nsCOMPtr<nsIObserverService> observerSvc = services::GetObserverService();
  RefPtr<Spinner> lowMemorySpinner =
      new Spinner(observerSvc, "memory-pressure");
  lowMemorySpinner->StartListening();

  StartUserInteraction(observerSvc);

  // Start polling for low memory. We should start with low memory when we start
  // the checker.
  RefPtr<AvailableMemoryChecker> checker = new AvailableMemoryChecker();
  checker->Init();

  lowMemorySpinner->WaitForNotification();

  EXPECT_TRUE(lowMemorySpinner->TopicObserved());

  RefPtr<Spinner> highMemorySpinner =
      new Spinner(observerSvc, "memory-pressure-stop");
  highMemorySpinner->StartListening();

  // Now that we are definitely low on memory, let's reset the pref to 0 to
  // exit low memory.
  Preferences::SetUint("browser.low_commit_space_threshold_percent", 0);

  highMemorySpinner->WaitForNotification();

  EXPECT_TRUE(highMemorySpinner->TopicObserved());

  checker->Shutdown();
}
}  // namespace
