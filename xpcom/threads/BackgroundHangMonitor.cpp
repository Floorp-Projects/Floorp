/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThreadHangStats.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/SystemGroup.h"

#include "prinrval.h"
#include "prthread.h"
#include "ThreadStackHelper.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "GeckoProfiler.h"
#include "nsNetCID.h"

#include <algorithm>

// Activate BHR only for one every BHR_BETA_MOD users.
// This is now 100% of Beta population for the Beta 45/46 e10s A/B trials
// It can be scaled back again in the future
#define BHR_BETA_MOD 1;

// Maximum depth of the call stack in the reported thread hangs. This value represents
// the 99.9th percentile of the thread hangs stack depths reported by Telemetry.
static const size_t kMaxThreadHangStackDepth = 30;

// An utility comparator function used by std::unique to collapse "(* script)" entries in
// a vector representing a call stack.
bool StackScriptEntriesCollapser(const char* aStackEntry, const char *aAnotherStackEntry)
{
  return !strcmp(aStackEntry, aAnotherStackEntry) &&
         (!strcmp(aStackEntry, "(chrome script)") || !strcmp(aStackEntry, "(content script)"));
}

namespace mozilla {

class ProcessHangRunnable;

/**
 * BackgroundHangManager is the global object that
 * manages all instances of BackgroundHangThread.
 */
class BackgroundHangManager : public nsIObserver
{
private:
  // Background hang monitor thread function
  static void MonitorThread(void* aData)
  {
    AutoProfilerRegisterThread registerThread("BgHangMonitor");
    NS_SetCurrentThreadName("BgHangManager");

    /* We do not hold a reference to BackgroundHangManager here
       because the monitor thread only exists as long as the
       BackgroundHangManager instance exists. We stop the monitor
       thread in the BackgroundHangManager destructor, and we can
       only get to the destructor if we don't hold a reference here. */
    static_cast<BackgroundHangManager*>(aData)->RunMonitorThread();
  }

  // Hang monitor thread
  PRThread* mHangMonitorThread;
  // Stop hang monitoring
  bool mShutdown;

  BackgroundHangManager(const BackgroundHangManager&);
  BackgroundHangManager& operator=(const BackgroundHangManager&);
  void RunMonitorThread();

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  static StaticRefPtr<BackgroundHangManager> sInstance;
  static bool sDisabled;

  // Lock for access to members of this class
  Monitor mLock;
  // Current time as seen by hang monitors
  PRIntervalTime mIntervalNow;
  // List of BackgroundHangThread instances associated with each thread
  LinkedList<BackgroundHangThread> mHangThreads;
  // A reference to the StreamTransportService. This is gotten on the main
  // thread, and carried around, as nsStreamTransportService::Init is
  // non-threadsafe.
  nsCOMPtr<nsIEventTarget> mSTS;

  void Shutdown()
  {
    MonitorAutoLock autoLock(mLock);
    mShutdown = true;
    autoLock.Notify();
  }

  // Attempt to wakeup the hang monitor thread.
  void Wakeup()
  {
    mLock.AssertCurrentThreadOwns();
    mLock.NotifyAll();
  }

  BackgroundHangManager();
private:
  virtual ~BackgroundHangManager();
};

NS_IMPL_ISUPPORTS(BackgroundHangManager, nsIObserver)

NS_IMETHODIMP
BackgroundHangManager::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData) {
  NS_ENSURE_TRUE(!strcmp(aTopic, "profile-after-change"), NS_ERROR_UNEXPECTED);
  BackgroundHangMonitor::DisableOnBeta();

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);
  observerService->RemoveObserver(this, "profile-after-change");

  return NS_OK;
}

/**
 * BackgroundHangThread is a per-thread object that is used
 * by all instances of BackgroundHangMonitor to monitor hangs.
 */
class BackgroundHangThread : public LinkedListElement<BackgroundHangThread>
{
private:
  static MOZ_THREAD_LOCAL(BackgroundHangThread*) sTlsKey;
  static bool sTlsKeyInitialized;

  BackgroundHangThread(const BackgroundHangThread&);
  BackgroundHangThread& operator=(const BackgroundHangThread&);
  ~BackgroundHangThread();

  /* Keep a reference to the manager, so we can keep going even
     after BackgroundHangManager::Shutdown is called. */
  const RefPtr<BackgroundHangManager> mManager;
  // Unique thread ID for identification
  const PRThread* mThreadID;

  void Update();

public:
  NS_INLINE_DECL_REFCOUNTING(BackgroundHangThread)
  /**
   * Returns the BackgroundHangThread associated with the
   * running thread. Note that this will not find private
   * BackgroundHangThread threads.
   *
   * @return BackgroundHangThread*, or nullptr if no thread
   *         is found.
   */
  static BackgroundHangThread* FindThread();

  static void Startup()
  {
    /* We can tolerate init() failing. */
    sTlsKeyInitialized = sTlsKey.init();
  }

  // Hang timeout in ticks
  const PRIntervalTime mTimeout;
  // PermaHang timeout in ticks
  const PRIntervalTime mMaxTimeout;
  // Time at last activity
  PRIntervalTime mInterval;
  // Time when a hang started
  PRIntervalTime mHangStart;
  // Is the thread in a hang
  bool mHanging;
  // Is the thread in a waiting state
  bool mWaiting;
  // Is the thread dedicated to a single BackgroundHangMonitor
  BackgroundHangMonitor::ThreadType mThreadType;
  // Platform-specific helper to get hang stacks
  ThreadStackHelper mStackHelper;
  // Stack of current hang
  Telemetry::HangStack mHangStack;
  // Native stack of current hang
  Telemetry::NativeHangStack mNativeHangStack;
  // Statistics for telemetry
  Telemetry::ThreadHangStats mStats;
  // Annotations for the current hang
  UniquePtr<HangMonitor::HangAnnotations> mAnnotations;
  // Annotators registered for this thread
  HangMonitor::Observer::Annotators mAnnotators;
  // List of runnables which can hold a reference to us which need to be
  // canceled before we can go away.
  LinkedList<RefPtr<ProcessHangRunnable>> mProcessHangRunnables;
  // The name of the runnable which is hanging the current process
  nsCString mRunnableName;

  BackgroundHangThread(const char* aName,
                       uint32_t aTimeoutMs,
                       uint32_t aMaxTimeoutMs,
                       BackgroundHangMonitor::ThreadType aThreadType = BackgroundHangMonitor::THREAD_SHARED);

  // Report a hang; aManager->mLock IS locked. The hang will be processed
  // off-main-thread, and will then be submitted back.
  void ReportHang(PRIntervalTime aHangTime);
  // Report a permanent hang; aManager->mLock IS locked
  void ReportPermaHang();
  // Called by BackgroundHangMonitor::NotifyActivity
  void NotifyActivity()
  {
    MonitorAutoLock autoLock(mManager->mLock);
    Update();
  }
  // Called by BackgroundHangMonitor::NotifyWait
  void NotifyWait()
  {
    MonitorAutoLock autoLock(mManager->mLock);

    if (mWaiting) {
      return;
    }

    Update();
    mWaiting = true;
  }

  // Returns true if this thread is (or might be) shared between other
  // BackgroundHangMonitors for the monitored thread.
  bool IsShared() {
    return mThreadType == BackgroundHangMonitor::THREAD_SHARED;
  }
};


StaticRefPtr<BackgroundHangManager> BackgroundHangManager::sInstance;
bool BackgroundHangManager::sDisabled = true;

MOZ_THREAD_LOCAL(BackgroundHangThread*) BackgroundHangThread::sTlsKey;
bool BackgroundHangThread::sTlsKeyInitialized;

BackgroundHangManager::BackgroundHangManager()
  : mShutdown(false)
  , mLock("BackgroundHangManager")
  , mIntervalNow(0)
  , mSTS(do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID))
{
  // Lock so we don't race against the new monitor thread
  MonitorAutoLock autoLock(mLock);

  mHangMonitorThread = PR_CreateThread(
    PR_USER_THREAD, MonitorThread, this,
    PR_PRIORITY_LOW, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);

  MOZ_ASSERT(mHangMonitorThread, "Failed to create monitor thread");
}

BackgroundHangManager::~BackgroundHangManager()
{
  MOZ_ASSERT(mShutdown, "Destruction without Shutdown call");
  MOZ_ASSERT(mHangThreads.isEmpty(), "Destruction with outstanding monitors");
  MOZ_ASSERT(mHangMonitorThread, "No monitor thread");

  // PR_CreateThread could have failed above due to resource limitation
  if (mHangMonitorThread) {
    // The monitor thread can only live as long as the instance lives
    PR_JoinThread(mHangMonitorThread);
  }
}

void
BackgroundHangManager::RunMonitorThread()
{
  // Make sure to initialize any state required to perform stack walking as soon
  // as possible, so it does not interfere with us collecting hang stacks. We
  // don't want to be on the main thread, or holding the BHR lock, because this
  // can take a long time, so we do it before grabbing the lock off-main-thread.
  profiler_initialize_stackwalk();

  // Keep us locked except when waiting
  MonitorAutoLock autoLock(mLock);

  /* mIntervalNow is updated at various intervals determined by waitTime.
     However, if an update latency is too long (due to CPU scheduling, system
     sleep, etc.), we don't update mIntervalNow at all. This is done so that
     long latencies in our timing are not detected as hangs. systemTime is
     used to track PR_IntervalNow() and determine our latency. */

  PRIntervalTime systemTime = PR_IntervalNow();
  // Default values for the first iteration of thread loop
  PRIntervalTime waitTime = PR_INTERVAL_NO_WAIT;
  PRIntervalTime recheckTimeout = PR_INTERVAL_NO_WAIT;

  while (!mShutdown) {
    nsresult rv = autoLock.Wait(waitTime);

    PRIntervalTime newTime = PR_IntervalNow();
    PRIntervalTime systemInterval = newTime - systemTime;
    systemTime = newTime;

    /* waitTime is a quarter of the shortest timeout value; If our timing
       latency is low enough (less than half the shortest timeout value),
       we can update mIntervalNow. */
    if (MOZ_LIKELY(waitTime != PR_INTERVAL_NO_TIMEOUT &&
                   systemInterval < 2 * waitTime)) {
      mIntervalNow += systemInterval;
    }

    /* If it's before the next recheck timeout, and our wait did not get
       interrupted, we can keep the current waitTime and skip iterating
       through hang monitors. */
    if (MOZ_LIKELY(systemInterval < recheckTimeout &&
                   systemInterval >= waitTime &&
                   rv == NS_OK)) {
      recheckTimeout -= systemInterval;
      continue;
    }

    /* We are in one of the following scenarios,
     - Hang or permahang recheck timeout
     - Thread added/removed
     - Thread wait or hang ended
       In all cases, we want to go through our list of hang
       monitors and update waitTime and recheckTimeout. */
    waitTime = PR_INTERVAL_NO_TIMEOUT;
    recheckTimeout = PR_INTERVAL_NO_TIMEOUT;

    // Locally hold mIntervalNow
    PRIntervalTime intervalNow = mIntervalNow;

    // iterate through hang monitors
    for (BackgroundHangThread* currentThread = mHangThreads.getFirst();
         currentThread; currentThread = currentThread->getNext()) {

      if (currentThread->mWaiting) {
        // Thread is waiting, not hanging
        continue;
      }
      PRIntervalTime interval = currentThread->mInterval;
      PRIntervalTime hangTime = intervalNow - interval;
      if (MOZ_UNLIKELY(hangTime >= currentThread->mMaxTimeout)) {
        // A permahang started
        // Skip subsequent iterations and tolerate a race on mWaiting here
        currentThread->mWaiting = true;
        currentThread->mHanging = false;
        currentThread->ReportPermaHang();
        continue;
      }

      if (MOZ_LIKELY(!currentThread->mHanging)) {
        if (MOZ_UNLIKELY(hangTime >= currentThread->mTimeout)) {
          // A hang started
#ifdef NIGHTLY_BUILD
          if (currentThread->mStats.mNativeStackCnt < Telemetry::kMaximumNativeHangStacks) {
            // NOTE: In nightly builds of firefox we want to collect native stacks
            // for all hangs, not just permahangs.
            currentThread->mStats.mNativeStackCnt += 1;
            currentThread->mStackHelper.GetPseudoAndNativeStack(
              currentThread->mHangStack,
              currentThread->mNativeHangStack,
              currentThread->mRunnableName);
          } else {
            currentThread->mStackHelper.GetPseudoStack(currentThread->mHangStack,
                                                       currentThread->mRunnableName);
          }
#else
          currentThread->mStackHelper.GetPseudoStack(currentThread->mHangStack,
                                                     currentThread->mRunnableName);
#endif
          currentThread->mHangStart = interval;
          currentThread->mHanging = true;
          currentThread->mAnnotations =
            currentThread->mAnnotators.GatherAnnotations();
        }
      } else {
        if (MOZ_LIKELY(interval != currentThread->mHangStart)) {
          // A hang ended
          currentThread->ReportHang(intervalNow - currentThread->mHangStart);
          currentThread->mHanging = false;
        }
      }

      /* If we are hanging, the next time we check for hang status is when
         the hang turns into a permahang. If we're not hanging, the next
         recheck timeout is when we may be entering a hang. */
      PRIntervalTime nextRecheck;
      if (currentThread->mHanging) {
        nextRecheck = currentThread->mMaxTimeout;
      } else {
        nextRecheck = currentThread->mTimeout;
      }
      recheckTimeout = std::min(recheckTimeout, nextRecheck - hangTime);

      if (currentThread->mTimeout != PR_INTERVAL_NO_TIMEOUT) {
        /* We wait for a quarter of the shortest timeout
           value to give mIntervalNow enough granularity. */
        waitTime = std::min(waitTime, currentThread->mTimeout / 4);
      }
    }
  }

  /* We are shutting down now.
     Wait for all outstanding monitors to unregister. */
  while (!mHangThreads.isEmpty()) {
    autoLock.Wait(PR_INTERVAL_NO_TIMEOUT);
  }
}


BackgroundHangThread::BackgroundHangThread(const char* aName,
                                           uint32_t aTimeoutMs,
                                           uint32_t aMaxTimeoutMs,
                                           BackgroundHangMonitor::ThreadType aThreadType)
  : mManager(BackgroundHangManager::sInstance)
  , mThreadID(PR_GetCurrentThread())
  , mTimeout(aTimeoutMs == BackgroundHangMonitor::kNoTimeout
             ? PR_INTERVAL_NO_TIMEOUT
             : PR_MillisecondsToInterval(aTimeoutMs))
  , mMaxTimeout(aMaxTimeoutMs == BackgroundHangMonitor::kNoTimeout
                ? PR_INTERVAL_NO_TIMEOUT
                : PR_MillisecondsToInterval(aMaxTimeoutMs))
  , mInterval(mManager->mIntervalNow)
  , mHangStart(mInterval)
  , mHanging(false)
  , mWaiting(true)
  , mThreadType(aThreadType)
  , mStats(aName)
{
  if (sTlsKeyInitialized && IsShared()) {
    sTlsKey.set(this);
  }
  // Lock here because LinkedList is not thread-safe
  MonitorAutoLock autoLock(mManager->mLock);
  // Add to thread list
  mManager->mHangThreads.insertBack(this);
  // Wake up monitor thread to process new thread
  autoLock.Notify();
}

// This runnable is used to pre-process a hang, performing any expensive
// operations on it, before submitting it into the BackgroundHangThread object
// for Telemetry.
//
// If this object is canceled, it will submit its payload to the
// BackgroundHangThread without performing the processing.
class ProcessHangRunnable final
  : public CancelableRunnable
  , public LinkedListElement<RefPtr<ProcessHangRunnable>>
{
public:
  ProcessHangRunnable(BackgroundHangManager* aManager,
                      BackgroundHangThread* aThread,
                      Telemetry::HangHistogram&& aHistogram,
                      Telemetry::NativeHangStack&& aNativeStack)
    : CancelableRunnable("ProcessHangRunnable")
    , mManager(aManager)
    , mNativeStack(mozilla::Move(aNativeStack))
    , mThread(aThread)
    , mHistogram(mozilla::Move(aHistogram))
  {
    MOZ_ASSERT(mThread);
  }

  NS_IMETHOD
  Run() override
  {
    // Start processing this histogram's native hang stack before we try to lock
    // anything, as we can do this without any locks held. This is the expensive
    // part of the operation.
    Telemetry::ProcessedStack processed;
    if (!mNativeStack.empty()) {
       processed = Telemetry::GetStackAndModules(mNativeStack);
    }

    // Lock the manager's lock, so that we can take a look at our mThread
    {
      MonitorAutoLock autoLock(mManager->mLock);
      if (NS_WARN_IF(!mThread)) {
        return NS_OK;
      }

      // If we have a stack, check if we can add it to combined stacks. This is
      // a relatively cheap operation, and must occur with the lock held.
      if (!mNativeStack.empty() &&
          mThread->mStats.mCombinedStacks.GetStackCount() < Telemetry::kMaximumNativeHangStacks) {
        mHistogram.SetNativeStackIndex(mThread->mStats.mCombinedStacks.AddStack(processed));
      }

      // Submit, remove ourselves from the list, and clear out mThread so we
      // don't run again.
      MOZ_ALWAYS_TRUE(mThread->mStats.mHangs.append(Move(mHistogram)));
      remove();
      mThread = nullptr;
    }

    return NS_OK;
  }

  // Submits hang, and removes from list.
  nsresult
  Cancel() override
  {
    mManager->mLock.AssertCurrentThreadOwns();
    if (NS_WARN_IF(!mThread)) {
      return NS_OK;
    }

    // Submit, remove ourselves from the list, and clear out mThread so we
    // don't run again.
    MOZ_ALWAYS_TRUE(mThread->mStats.mHangs.append(Move(mHistogram)));
    if (isInList()) {
      remove();
    }
    mThread = nullptr;
    return NS_OK;
  }

private:
  // These variables are constant after initialization, and do not need
  // synchronization.
  RefPtr<BackgroundHangManager> mManager;
  const Telemetry::NativeHangStack mNativeStack;
  // These variables are guarded by mManager->mLock.
  BackgroundHangThread* MOZ_NON_OWNING_REF mThread; // Will Cancel us before it dies
  Telemetry::HangHistogram mHistogram;
};

BackgroundHangThread::~BackgroundHangThread()
{
  // Lock here because LinkedList is not thread-safe
  MonitorAutoLock autoLock(mManager->mLock);
  // Remove from thread list
  remove();
  // Wake up monitor thread to process removed thread
  autoLock.Notify();

  // We no longer have a thread
  if (sTlsKeyInitialized && IsShared()) {
    sTlsKey.set(nullptr);
  }

  // Cancel any remaining process hang runnables, as they hold a weak reference
  // into our mStats variable, which we're about to move.
  while (RefPtr<ProcessHangRunnable> runnable = mProcessHangRunnables.popFirst()) {
    runnable->Cancel();
  }

  // Record the ThreadHangStats for this thread before we go away. All stats
  // should be in this method now, as we canceled any pending runnables.
  Telemetry::RecordThreadHangStats(Move(mStats));
}

void
BackgroundHangThread::ReportHang(PRIntervalTime aHangTime)
{
  // Recovered from a hang; called on the monitor thread
  // mManager->mLock IS locked

  // Remove unwanted "js::RunScript" frame from the stack
  for (size_t i = 0; i < mHangStack.length(); ) {
    const char** f = mHangStack.begin() + i;
    if (!mHangStack.IsInBuffer(*f) && !strcmp(*f, "js::RunScript")) {
      mHangStack.erase(f);
    } else {
      i++;
    }
  }

  // Collapse duplicated "(chrome script)" and "(content script)" entries in the stack.
  auto it = std::unique(mHangStack.begin(), mHangStack.end(), StackScriptEntriesCollapser);
  mHangStack.erase(it, mHangStack.end());

  // Limit the depth of the reported stack if greater than our limit. Only keep its
  // last entries, since the most recent frames are at the end of the vector.
  if (mHangStack.length() > kMaxThreadHangStackDepth) {
    const int elementsToRemove = mHangStack.length() - kMaxThreadHangStackDepth;
    // Replace the oldest frame with a known label so that we can tell this stack
    // was limited.
    mHangStack[0] = "(reduced stack)";
    mHangStack.erase(mHangStack.begin() + 1, mHangStack.begin() + elementsToRemove);
  }

  Telemetry::HangHistogram newHistogram(Move(mHangStack), mRunnableName);
  for (Telemetry::HangHistogram* oldHistogram = mStats.mHangs.begin();
       oldHistogram != mStats.mHangs.end(); oldHistogram++) {
    if (newHistogram == *oldHistogram) {
      // New histogram matches old one
      oldHistogram->Add(aHangTime, Move(mAnnotations));
      return;
    }
  }
  newHistogram.Add(aHangTime, Move(mAnnotations));

  // Process the hang off-main thread. We record a reference to the runnable in
  // mProcessHangRunnables so we can abort this preprocessing and just submit
  // the message if the processing takes too long and our thread is going away.
  RefPtr<ProcessHangRunnable> processHang =
    new ProcessHangRunnable(mManager, this, Move(newHistogram), Move(mNativeHangStack));
  mProcessHangRunnables.insertFront(processHang);

  // Try to dispatch the runnable to the StreamTransportService threadpool. If
  // we fail, cancel our runnable.
  if (!mManager->mSTS || NS_FAILED(mManager->mSTS->Dispatch(processHang.forget()))) {
    RefPtr<ProcessHangRunnable> runnable = mProcessHangRunnables.popFirst();
    runnable->Cancel();
  }
}

void
BackgroundHangThread::ReportPermaHang()
{
  // Permanently hanged; called on the monitor thread
  // mManager->mLock IS locked

  // NOTE: We used to capture a native stack in this situation if one had not
  // already been captured, but with the new ReportHang design that is less
  // practical.
  //
  // We currently don't look at hang reports outside of nightly, and already
  // collect native stacks eagerly on nightly, so this should be OK.
  ReportHang(mMaxTimeout);
}

MOZ_ALWAYS_INLINE void
BackgroundHangThread::Update()
{
  PRIntervalTime intervalNow = mManager->mIntervalNow;
  if (mWaiting) {
    mInterval = intervalNow;
    mWaiting = false;
    /* We have to wake up the manager thread because when all threads
       are waiting, the manager thread waits indefinitely as well. */
    mManager->Wakeup();
  } else {
    PRIntervalTime duration = intervalNow - mInterval;
    mStats.mActivity.Add(duration);
    if (MOZ_UNLIKELY(duration >= mTimeout)) {
      /* Wake up the manager thread to tell it that a hang ended */
      mManager->Wakeup();
    }
    mInterval = intervalNow;
  }
}

BackgroundHangThread*
BackgroundHangThread::FindThread()
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (BackgroundHangManager::sInstance == nullptr) {
    MOZ_ASSERT(BackgroundHangManager::sDisabled,
               "BackgroundHandleManager is not initialized");
    return nullptr;
  }

  if (sTlsKeyInitialized) {
    // Use TLS if available
    return sTlsKey.get();
  }
  // If TLS is unavailable, we can search through the thread list
  RefPtr<BackgroundHangManager> manager(BackgroundHangManager::sInstance);
  MOZ_ASSERT(manager, "Creating BackgroundHangMonitor after shutdown");

  PRThread* threadID = PR_GetCurrentThread();
  // Lock thread list for traversal
  MonitorAutoLock autoLock(manager->mLock);
  for (BackgroundHangThread* thread = manager->mHangThreads.getFirst();
       thread; thread = thread->getNext()) {
    if (thread->mThreadID == threadID && thread->IsShared()) {
      return thread;
    }
  }
#endif
  // Current thread is not initialized
  return nullptr;
}

bool
BackgroundHangMonitor::ShouldDisableOnBeta(const nsCString &clientID) {
  MOZ_ASSERT(clientID.Length() == 36, "clientID is invalid");
  const char *suffix = clientID.get() + clientID.Length() - 4;
  return strtol(suffix, NULL, 16) % BHR_BETA_MOD;
}

bool
BackgroundHangMonitor::IsDisabled() {
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  return BackgroundHangManager::sDisabled;
#else
  return true;
#endif
}

bool
BackgroundHangMonitor::DisableOnBeta() {
  nsAdoptingCString clientID = Preferences::GetCString("toolkit.telemetry.cachedClientID");
  bool telemetryEnabled = Preferences::GetBool("toolkit.telemetry.enabled");

  if (!telemetryEnabled || !clientID || BackgroundHangMonitor::ShouldDisableOnBeta(clientID)) {
    if (XRE_IsParentProcess()) {
      BackgroundHangMonitor::Shutdown();
    } else {
      BackgroundHangManager::sDisabled = true;
    }
    return true;
  }

  return false;
}

void
BackgroundHangMonitor::Startup()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  MOZ_ASSERT(!BackgroundHangManager::sInstance, "Already initialized");

  // Enable the BackgroundHangManager by setting sDisabled to false in Startup.
  MOZ_ASSERT(BackgroundHangManager::sDisabled);
  BackgroundHangManager::sDisabled = false;

  if (!strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "beta")) {
    if (XRE_IsParentProcess()) { // cached ClientID hasn't been read yet
      BackgroundHangThread::Startup();
      BackgroundHangManager::sInstance = new BackgroundHangManager();

      nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
      MOZ_ASSERT(observerService);

      observerService->AddObserver(BackgroundHangManager::sInstance, "profile-after-change", false);
      return;
    } else if(DisableOnBeta()){
      return;
    }
  }

  BackgroundHangThread::Startup();
  BackgroundHangManager::sInstance = new BackgroundHangManager();
#endif
}

void
BackgroundHangMonitor::Shutdown()
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (BackgroundHangManager::sDisabled) {
    MOZ_ASSERT(!BackgroundHangManager::sInstance, "Initialized");
    return;
  }

  MOZ_ASSERT(BackgroundHangManager::sInstance, "Not initialized");
  /* Scope our lock inside Shutdown() because the sInstance object can
     be destroyed as soon as we set sInstance to nullptr below, and
     we don't want to hold the lock when it's being destroyed. */
  BackgroundHangManager::sInstance->Shutdown();
  BackgroundHangManager::sInstance = nullptr;
  BackgroundHangManager::sDisabled = true;
#endif
}

BackgroundHangMonitor::BackgroundHangMonitor(const char* aName,
                                             uint32_t aTimeoutMs,
                                             uint32_t aMaxTimeoutMs,
                                             ThreadType aThreadType)
  : mThread(aThreadType == THREAD_SHARED ? BackgroundHangThread::FindThread() : nullptr)
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (!BackgroundHangManager::sDisabled && !mThread) {
    mThread = new BackgroundHangThread(aName, aTimeoutMs, aMaxTimeoutMs,
                                       aThreadType);
  }
#endif
}

BackgroundHangMonitor::BackgroundHangMonitor()
  : mThread(BackgroundHangThread::FindThread())
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (BackgroundHangManager::sDisabled) {
    return;
  }
#endif
}

BackgroundHangMonitor::~BackgroundHangMonitor()
{
}

void
BackgroundHangMonitor::NotifyActivity()
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (mThread == nullptr) {
    MOZ_ASSERT(BackgroundHangManager::sDisabled,
               "This thread is not initialized for hang monitoring");
    return;
  }

  if (Telemetry::CanRecordExtended()) {
    mThread->NotifyActivity();
  }
#endif
}

void
BackgroundHangMonitor::NotifyWait()
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (mThread == nullptr) {
    MOZ_ASSERT(BackgroundHangManager::sDisabled,
               "This thread is not initialized for hang monitoring");
    return;
  }

  if (Telemetry::CanRecordExtended()) {
    mThread->NotifyWait();
  }
#endif
}

bool
BackgroundHangMonitor::RegisterAnnotator(HangMonitor::Annotator& aAnnotator)
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  BackgroundHangThread* thisThread = BackgroundHangThread::FindThread();
  if (!thisThread) {
    return false;
  }
  return thisThread->mAnnotators.Register(aAnnotator);
#else
  return false;
#endif
}

bool
BackgroundHangMonitor::UnregisterAnnotator(HangMonitor::Annotator& aAnnotator)
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  BackgroundHangThread* thisThread = BackgroundHangThread::FindThread();
  if (!thisThread) {
    return false;
  }
  return thisThread->mAnnotators.Unregister(aAnnotator);
#else
  return false;
#endif
}

/* Because we are iterating through the BackgroundHangThread linked list,
   we need to take a lock. Using MonitorAutoLock as a base class makes
   sure all of that is taken care of for us. */
BackgroundHangMonitor::ThreadHangStatsIterator::ThreadHangStatsIterator()
  : MonitorAutoLock(BackgroundHangManager::sInstance->mLock)
  , mThread(BackgroundHangManager::sInstance ?
            BackgroundHangManager::sInstance->mHangThreads.getFirst() :
            nullptr)
{
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  MOZ_ASSERT(BackgroundHangManager::sInstance ||
             BackgroundHangManager::sDisabled,
             "Inconsistent state");
#endif
}

Telemetry::ThreadHangStats*
BackgroundHangMonitor::ThreadHangStatsIterator::GetNext()
{
  if (!mThread) {
    return nullptr;
  }
  Telemetry::ThreadHangStats* stats = &mThread->mStats;
  mThread = mThread->getNext();
  return stats;
}

} // namespace mozilla
