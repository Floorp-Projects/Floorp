/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <thread>
#include "gtest/gtest.h"

#include "mozilla/Atomics.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "nsMemoryPressure.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;

namespace {

enum class MemoryPressureEventType : int {
  LowMemory,
  LowMemoryOngoing,
  Stop,
};

class MemoryPressureObserver final : public nsIObserver {
  nsCOMPtr<nsIObserverService> mObserverSvc;
  Vector<MemoryPressureEventType> mEvents;

  ~MemoryPressureObserver() {
    EXPECT_TRUE(
        NS_SUCCEEDED(mObserverSvc->RemoveObserver(this, kTopicMemoryPressure)));
    EXPECT_TRUE(NS_SUCCEEDED(
        mObserverSvc->RemoveObserver(this, kTopicMemoryPressureStop)));
  }

 public:
  NS_DECL_ISUPPORTS

  MemoryPressureObserver()
      : mObserverSvc(do_GetService(NS_OBSERVERSERVICE_CONTRACTID)) {
    EXPECT_TRUE(NS_SUCCEEDED(mObserverSvc->AddObserver(
        this, kTopicMemoryPressure, /* ownsWeak */ false)));
    EXPECT_TRUE(NS_SUCCEEDED(mObserverSvc->AddObserver(
        this, kTopicMemoryPressureStop, /* ownsWeak */ false)));
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    Maybe<MemoryPressureEventType> event;
    if (strcmp(aTopic, kTopicMemoryPressure) == 0) {
      if (nsDependentString(aData) == kSubTopicLowMemoryNew) {
        event = Some(MemoryPressureEventType::LowMemory);
      } else if (nsDependentString(aData) == kSubTopicLowMemoryOngoing) {
        event = Some(MemoryPressureEventType::LowMemoryOngoing);
      } else {
        fprintf(stderr, "Unexpected subtopic: %S\n",
                reinterpret_cast<const wchar_t*>(aData));
        EXPECT_TRUE(false);
      }
    } else if (strcmp(aTopic, kTopicMemoryPressureStop) == 0) {
      event = Some(MemoryPressureEventType::Stop);
    } else {
      fprintf(stderr, "Unexpected topic: %s\n", aTopic);
      EXPECT_TRUE(false);
    }

    if (event) {
      Unused << mEvents.emplaceBack(event.value());
    }
    return NS_OK;
  }

  uint32_t GetCount() const { return mEvents.length(); }
  void Reset() { mEvents.clear(); }
  MemoryPressureEventType Top() const { return mEvents[0]; }

  bool ValidateTransitions() const {
    if (mEvents.length() == 0) {
      return true;
    }

    for (size_t i = 1; i < mEvents.length(); ++i) {
      MemoryPressureEventType eventFrom = mEvents[i - 1];
      MemoryPressureEventType eventTo = mEvents[i];
      if ((eventFrom == MemoryPressureEventType::LowMemory &&
           eventTo == MemoryPressureEventType::LowMemoryOngoing) ||
          (eventFrom == MemoryPressureEventType::LowMemoryOngoing &&
           eventTo == MemoryPressureEventType::LowMemoryOngoing) ||
          (eventFrom == MemoryPressureEventType::Stop &&
           eventTo == MemoryPressureEventType::LowMemory) ||
          (eventFrom == MemoryPressureEventType::LowMemoryOngoing &&
           eventTo == MemoryPressureEventType::Stop) ||
          (eventFrom == MemoryPressureEventType::LowMemory &&
           eventTo == MemoryPressureEventType::Stop)) {
        // Only these transitions are valid.
        continue;
      }

      fprintf(stderr, "Invalid transition: %d -> %d\n",
              static_cast<int>(eventFrom), static_cast<int>(eventTo));
      return false;
    }
    return true;
  }
};

NS_IMPL_ISUPPORTS(MemoryPressureObserver, nsIObserver)

template <MemoryPressureState State>
void PressureSender(Atomic<bool>& aContinue) {
  while (aContinue) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    NS_NotifyOfEventualMemoryPressure(State);
  }
}

template <MemoryPressureState State>
void PressureSenderQuick(Atomic<bool>& aContinue) {
  while (aContinue) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    NS_NotifyOfMemoryPressure(State);
  }
}

}  // anonymous namespace

TEST(MemoryPressure, Singlethread)
{
  RefPtr observer(new MemoryPressureObserver);
  NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
  SpinEventLoopUntil([&observer]() { return observer->GetCount() == 1; });
  EXPECT_EQ(observer->Top(), MemoryPressureEventType::LowMemory);

  observer->Reset();
  NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
  SpinEventLoopUntil([&observer]() { return observer->GetCount() == 1; });
  EXPECT_EQ(observer->Top(), MemoryPressureEventType::LowMemoryOngoing);

  observer->Reset();
  NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
  SpinEventLoopUntil([&observer]() { return observer->GetCount() == 1; });
  EXPECT_EQ(observer->Top(), MemoryPressureEventType::LowMemoryOngoing);

  observer->Reset();
  NS_NotifyOfEventualMemoryPressure(MemoryPressureState::NoPressure);
  SpinEventLoopUntil([&observer]() { return observer->GetCount() == 1; });
  EXPECT_EQ(observer->Top(), MemoryPressureEventType::Stop);
}

TEST(MemoryPressure, Multithread)
{
  // Start |kNumThreads| threads each for the following thread type:
  //  - LowMemory via NS_NotifyOfEventualMemoryPressure
  //  - LowMemory via NS_NotifyOfMemoryPressure
  //  - LowMemoryOngoing via NS_NotifyOfEventualMemoryPressure
  //  - LowMemoryOngoing via NS_NotifyOfMemoryPressure
  // and keep them running until |kNumEventsToValidate| memory-pressure events
  // are received.
  constexpr int kNumThreads = 5;
  constexpr int kNumEventsToValidate = 200;

  Atomic<bool> shouldContinue(true);
  Vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    Unused << threads.emplaceBack(
        PressureSender<MemoryPressureState::LowMemory>,
        std::ref(shouldContinue));
    Unused << threads.emplaceBack(
        PressureSender<MemoryPressureState::NoPressure>,
        std::ref(shouldContinue));
    Unused << threads.emplaceBack(
        PressureSenderQuick<MemoryPressureState::LowMemory>,
        std::ref(shouldContinue));
    Unused << threads.emplaceBack(
        PressureSenderQuick<MemoryPressureState::NoPressure>,
        std::ref(shouldContinue));
  }

  RefPtr observer(new MemoryPressureObserver);

  // We cannot sleep here because the main thread needs to keep running.
  SpinEventLoopUntil(
      [&observer]() { return observer->GetCount() >= kNumEventsToValidate; });

  shouldContinue = false;
  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_TRUE(observer->ValidateTransitions());
}
