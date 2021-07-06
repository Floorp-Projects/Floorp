/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <windows.h>
#include <memoryapi.h>
#include "gtest/gtest.h"

#include "AvailableMemoryWatcher.h"
#include "mozilla/Atomics.h"
#include "mozilla/Preferences.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsITimer.h"
#include "nsMemoryPressure.h"
#include "nsWindowsHelpers.h"
#include "nsIWindowsRegKey.h"
#include "nsXULAppAPI.h"

using namespace mozilla;

namespace {

static constexpr size_t kBytesInMB = 1024 * 1024;

template <typename ConditionT>
bool WaitUntil(const ConditionT& aCondition, uint32_t aTimeoutMs) {
  const uint64_t t0 = ::GetTickCount64();
  bool isTimeout = false;

  // The message queue can be empty and the loop stops
  // waiting for a new event before detecting timeout.
  // Creating a timer to fire a timeout event.
  nsCOMPtr<nsITimer> timer;
  NS_NewTimerWithFuncCallback(
      getter_AddRefs(timer),
      [](nsITimer*, void* isTimeout) {
        *reinterpret_cast<bool*>(isTimeout) = true;
      },
      &isTimeout, aTimeoutMs, nsITimer::TYPE_ONE_SHOT, __func__);

  SpinEventLoopUntil([&]() -> bool {
    if (isTimeout) {
      return true;
    }

    bool done = aCondition();
    if (done) {
      fprintf(stderr, "Done in %llu msec\n", ::GetTickCount64() - t0);
    }
    return done;
  });

  return !isTimeout;
}

class Spinner final : public nsIObserver {
  nsCOMPtr<nsIObserverService> mObserverSvc;
  nsDependentCString mTopicToWatch;
  Maybe<nsDependentString> mSubTopicToWatch;
  bool mTopicObserved;

  ~Spinner() = default;

 public:
  NS_DECL_ISUPPORTS

  Spinner(nsIObserverService* aObserverSvc, const char* const aTopic,
          const char16_t* const aSubTopic)
      : mObserverSvc(aObserverSvc),
        mTopicToWatch(aTopic),
        mSubTopicToWatch(aSubTopic ? Some(nsDependentString(aSubTopic))
                                   : Nothing()),
        mTopicObserved(false) {}

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (mTopicToWatch == aTopic) {
      if ((mSubTopicToWatch.isNothing() && !aData) ||
          mSubTopicToWatch.ref() == aData) {
        mTopicObserved = true;
        mObserverSvc->RemoveObserver(this, aTopic);

        // Force the loop to move in case that there is no event in the queue.
        nsCOMPtr<nsIRunnable> dummyEvent = new Runnable(__func__);
        NS_DispatchToMainThread(dummyEvent);
      }
    } else {
      fprintf(stderr, "Unexpected topic: %s\n", aTopic);
    }

    return NS_OK;
  }

  void StartListening() {
    mTopicObserved = false;
    mObserverSvc->AddObserver(this, mTopicToWatch.get(), false);
  }

  bool Wait(uint32_t aTimeoutMs) {
    return WaitUntil([this]() { return this->mTopicObserved; }, aTimeoutMs);
  }
};

NS_IMPL_ISUPPORTS(Spinner, nsIObserver)

/**
 * Starts a new thread with a message queue to process
 * memory allocation/free requests
 */
class MemoryEater {
  using PageT = UniquePtr<void, VirtualFreeDeleter>;

  static DWORD WINAPI ThreadStart(LPVOID aParam) {
    return reinterpret_cast<MemoryEater*>(aParam)->ThreadProc();
  }

  static void TouchMemory(void* aAddr, size_t aSize) {
    constexpr uint32_t kPageSize = 4096;
    volatile uint8_t x = 0;
    auto base = reinterpret_cast<uint8_t*>(aAddr);
    for (int64_t i = 0, pages = aSize / kPageSize; i < pages; ++i) {
      // Pick a random place in every allocated page
      // and dereference it.
      x ^= *(base + i * kPageSize + rand() % kPageSize);
    }
  }

  static uint32_t GetAvailablePhysicalMemoryInMb() {
    MEMORYSTATUSEX statex = {sizeof(statex)};
    if (!::GlobalMemoryStatusEx(&statex)) {
      return 0;
    }

    return static_cast<uint32_t>(statex.ullAvailPhys / kBytesInMB);
  }

  static bool AddWorkingSet(size_t aSize, Vector<PageT>& aOutput) {
    constexpr size_t kMinGranularity = 64 * 1024;

    size_t currentSize = aSize;
    size_t consumed = 0;
    while (aSize >= kMinGranularity) {
      if (!GetAvailablePhysicalMemoryInMb()) {
        // If the available physical memory is less than 1MB, we finish
        // allocation though there may be still the available commit space.
        fprintf(stderr, "No enough physical memory.\n");
        return false;
      }

      PageT page(::VirtualAlloc(nullptr, currentSize, MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE));
      if (!page) {
        DWORD gle = ::GetLastError();
        if (gle != ERROR_COMMITMENT_LIMIT) {
          return false;
        }

        // Try again with a smaller allocation size.
        currentSize /= 2;
        continue;
      }

      aSize -= currentSize;
      consumed += currentSize;

      // VirtualAlloc consumes the commit space, but we need to *touch* memory
      // to consume physical memory
      TouchMemory(page.get(), currentSize);
      Unused << aOutput.emplaceBack(std::move(page));
    }
    return true;
  }

  DWORD mThreadId;
  nsAutoHandle mThread;
  nsAutoHandle mMessageQueueReady;
  Atomic<bool> mTaskStatus;

  enum class TaskType : UINT {
    Alloc = WM_USER,  // WPARAM = Allocation size
    Free,

    Last,
  };

  DWORD ThreadProc() {
    Vector<PageT> stock;
    MSG msg;

    // Force the system to create a message queue
    ::PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    // Ready to get a message.  Unblock the main thread.
    ::SetEvent(mMessageQueueReady.get());

    for (;;) {
      BOOL result = ::GetMessage(&msg, reinterpret_cast<HWND>(-1), WM_QUIT,
                                 static_cast<UINT>(TaskType::Last));
      if (result == -1) {
        return ::GetLastError();
      }
      if (!result) {
        // Got WM_QUIT
        break;
      }

      switch (static_cast<TaskType>(msg.message)) {
        case TaskType::Alloc:
          mTaskStatus = AddWorkingSet(msg.wParam, stock);
          break;
        case TaskType::Free:
          stock = Vector<PageT>();
          mTaskStatus = true;
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected message in the queue");
          break;
      }
    }

    return static_cast<DWORD>(msg.wParam);
  }

  bool PostTask(TaskType aTask, WPARAM aW = 0, LPARAM aL = 0) const {
    return !!::PostThreadMessageW(mThreadId, static_cast<UINT>(aTask), aW, aL);
  }

 public:
  MemoryEater()
      : mThread(::CreateThread(nullptr, 0, ThreadStart, this, 0, &mThreadId)),
        mMessageQueueReady(::CreateEventW(nullptr, /*bManualReset*/ TRUE,
                                          /*bInitialState*/ FALSE, nullptr)) {
    ::WaitForSingleObject(mMessageQueueReady.get(), INFINITE);
  }

  ~MemoryEater() {
    ::PostThreadMessageW(mThreadId, WM_QUIT, 0, 0);
    if (::WaitForSingleObject(mThread.get(), 30000) != WAIT_OBJECT_0) {
      ::TerminateThread(mThread.get(), 0);
    }
  }

  bool GetTaskStatus() const { return mTaskStatus; }
  void RequestAlloc(size_t aSize) { PostTask(TaskType::Alloc, aSize); }
  void RequestFree() { PostTask(TaskType::Free); }
};

class MockTabUnloader final : public nsITabUnloader {
  ~MockTabUnloader() = default;

  uint32_t mCounter;

 public:
  MockTabUnloader() : mCounter(0) {}

  NS_DECL_THREADSAFE_ISUPPORTS

  void ResetCounter() { mCounter = 0; }
  uint32_t GetCounter() const { return mCounter; }

  NS_IMETHOD UnloadTabAsync() override {
    ++mCounter;
    // Issue a memory-pressure to verify OnHighMemory issues
    // a memory-pressure-stop event.
    NS_NotifyOfEventualMemoryPressure(MemoryPressureState::LowMemory);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(MockTabUnloader, nsITabUnloader)

}  // namespace

class AvailableMemoryWatcherFixture : public ::testing::Test {
  static const char kPrefLowCommitSpaceThreshold[];

  RefPtr<nsAvailableMemoryWatcherBase> mWatcher;
  nsCOMPtr<nsIObserverService> mObserverSvc;

 protected:
  static bool IsPageFileExpandable() {
    const auto kMemMgmtKey =
        u"SYSTEM\\CurrentControlSet\\Control\\"
        u"Session Manager\\Memory Management"_ns;

    nsresult rv;
    nsCOMPtr<nsIWindowsRegKey> regKey =
        do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv);
    if (NS_FAILED(rv)) {
      return false;
    }

    rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE, kMemMgmtKey,
                      nsIWindowsRegKey::ACCESS_READ);
    if (NS_FAILED(rv)) {
      return false;
    }

    nsAutoString pagingFiles;
    rv = regKey->ReadStringValue(u"PagingFiles"_ns, pagingFiles);
    if (NS_FAILED(rv)) {
      return false;
    }

    // The value data is REG_MULTI_SZ and each element is "<path> <min> <max>".
    // If the page file size is automatically managed for all drives, the <path>
    // is set to "?:\pagefile.sys".
    // If the page file size is configured per drive, for a drive whose page
    // file is set to "system managed size", both <min> and <max> are set to 0.
    return !pagingFiles.IsEmpty() &&
           (pagingFiles[0] == u'?' || FindInReadable(u" 0 0"_ns, pagingFiles));
  }

  static size_t GetAllocationSizeToTriggerMemoryNotification() {
    // The percentage of the used physical memory to the total physical memory
    // size which is big enough to trigger a memory resource notification.
    constexpr uint32_t kThresholdPercentage = 98;
    // If the page file is not expandable, leave a little commit space.
    const uint32_t kMinimumSafeCommitSpaceMb =
        IsPageFileExpandable() ? 0 : 1024;

    MEMORYSTATUSEX statex = {sizeof(statex)};
    EXPECT_TRUE(::GlobalMemoryStatusEx(&statex));

    // How much memory needs to be used to trigger the notification
    const size_t targetUsedTotalMb =
        (statex.ullTotalPhys / kBytesInMB) * kThresholdPercentage / 100;

    // How much memory is currently consumed
    const size_t currentConsumedMb =
        (statex.ullTotalPhys - statex.ullAvailPhys) / kBytesInMB;

    if (currentConsumedMb >= targetUsedTotalMb) {
      fprintf(stderr, "The available physical memory is already low.\n");
      return 0;
    }

    // How much memory we need to allocate to trigger the notification
    const uint32_t allocMb = targetUsedTotalMb - currentConsumedMb;

    // If we allocate the target amount, how much commit space will be
    // left available.
    const uint32_t estimtedAvailCommitSpace = std::max(
        0,
        static_cast<int32_t>((statex.ullAvailPageFile / kBytesInMB) - allocMb));

    // If the available commit space will be too low, we should not continue
    if (estimtedAvailCommitSpace < kMinimumSafeCommitSpaceMb) {
      fprintf(stderr, "The available commit space will be short - %d\n",
              estimtedAvailCommitSpace);
      return 0;
    }

    fprintf(stderr,
            "Total physical memory  = %ul\n"
            "Available commit space = %ul\n"
            "Amount to allocate     = %ul\n"
            "Future available commit space after allocation = %d\n",
            static_cast<uint32_t>(statex.ullTotalPhys / kBytesInMB),
            static_cast<uint32_t>(statex.ullAvailPageFile / kBytesInMB),
            allocMb, estimtedAvailCommitSpace);
    return allocMb * kBytesInMB;
  }

  static void SetThresholdAsPercentageOfCommitSpace(uint32_t aPercentage) {
    aPercentage = std::min(100u, aPercentage);

    MEMORYSTATUSEX statex = {sizeof(statex)};
    EXPECT_TRUE(::GlobalMemoryStatusEx(&statex));

    const uint32_t newVal = static_cast<uint32_t>(
        (statex.ullAvailPageFile / kBytesInMB) * aPercentage / 100);
    fprintf(stderr, "Setting %s to %u\n", kPrefLowCommitSpaceThreshold, newVal);

    Preferences::SetUint(kPrefLowCommitSpaceThreshold, newVal);
  }

  static constexpr uint32_t kStateChangeTimeoutMs = 10000;
  static constexpr uint32_t kNotificationTimeoutMs = 5000;

  RefPtr<Spinner> mHighMemoryObserver;
  RefPtr<MockTabUnloader> mTabUnloader;
  MemoryEater mMemEater;
  nsAutoHandle mLowMemoryHandle;

  void SetUp() override {
    mObserverSvc = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    ASSERT_TRUE(mObserverSvc);

    mHighMemoryObserver =
        new Spinner(mObserverSvc, "memory-pressure-stop", nullptr);
    mTabUnloader = new MockTabUnloader;

    mWatcher = CreateAvailableMemoryWatcher();
    mWatcher->RegisterTabUnloader(mTabUnloader);

    mLowMemoryHandle.own(
        ::CreateMemoryResourceNotification(LowMemoryResourceNotification));
    ASSERT_TRUE(mLowMemoryHandle);

    // We set the threshold to 50% of the current available commit space.
    // This means we declare low-memory when the available commit space
    // gets lower than this threshold, otherwise we declare high-memory.
    SetThresholdAsPercentageOfCommitSpace(50);
  }

  void TearDown() override {
    StopUserInteraction();
    Preferences::ClearUser(kPrefLowCommitSpaceThreshold);
  }

  bool WaitForMemoryResourceNotification() {
    if (::WaitForSingleObject(mLowMemoryHandle, kNotificationTimeoutMs) !=
        WAIT_OBJECT_0) {
      fprintf(stderr, "The memory notification was not triggered.\n");
      return false;
    }
    return true;
  }

  void StartUserInteraction() {
    mObserverSvc->NotifyObservers(nullptr, "user-interaction-active", nullptr);
  }

  void StopUserInteraction() {
    mObserverSvc->NotifyObservers(nullptr, "user-interaction-inactive",
                                  nullptr);
  }
};

const char AvailableMemoryWatcherFixture::kPrefLowCommitSpaceThreshold[] =
    "browser.low_commit_space_threshold_mb";

TEST_F(AvailableMemoryWatcherFixture, AlwaysActive) {
  StartUserInteraction();

  const size_t allocSize = GetAllocationSizeToTriggerMemoryNotification();
  if (!allocSize) {
    // Not enough memory to safely create a low-memory situation.
    // Aborting the test without failure.
    return;
  }

  mTabUnloader->ResetCounter();
  mMemEater.RequestAlloc(allocSize);
  if (!WaitForMemoryResourceNotification()) {
    // If the notification was not triggered, abort the test without failure
    // because it's not a fault in nsAvailableMemoryWatcher.
    return;
  }

  EXPECT_TRUE(WaitUntil([this]() { return mTabUnloader->GetCounter() >= 1; },
                        kStateChangeTimeoutMs));

  mHighMemoryObserver->StartListening();
  mMemEater.RequestFree();
  EXPECT_TRUE(mHighMemoryObserver->Wait(kStateChangeTimeoutMs));
}

TEST_F(AvailableMemoryWatcherFixture, InactiveToActive) {
  const size_t allocSize = GetAllocationSizeToTriggerMemoryNotification();
  if (!allocSize) {
    // Not enough memory to safely create a low-memory situation.
    // Aborting the test without failure.
    return;
  }

  mTabUnloader->ResetCounter();
  mMemEater.RequestAlloc(allocSize);
  if (!WaitForMemoryResourceNotification()) {
    // If the notification was not triggered, abort the test without failure
    // because it's not a fault in nsAvailableMemoryWatcher.
    return;
  }

  mHighMemoryObserver->StartListening();
  EXPECT_TRUE(WaitUntil([this]() { return mTabUnloader->GetCounter() >= 1; },
                        kStateChangeTimeoutMs));

  mMemEater.RequestFree();

  // OnHighMemory should not be triggered during no user interaction
  // eve after all memory was freed.  Expecting false.
  EXPECT_FALSE(mHighMemoryObserver->Wait(3000));

  StartUserInteraction();

  // After user is active, we expect true.
  EXPECT_TRUE(mHighMemoryObserver->Wait(kStateChangeTimeoutMs));
}
