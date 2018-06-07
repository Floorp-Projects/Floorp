/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/CPUUsageWatcher.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"

#include "prinrval.h"
#include "prthread.h"
#include "ThreadStackHelper.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "GeckoProfiler.h"
#include "HangDetails.h"

#ifdef MOZ_GECKO_PROFILER
#include "ProfilerMarkerPayload.h"
#endif

#include <algorithm>

// Activate BHR only for one every BHR_BETA_MOD users.
// We're doing experimentation with collecting a lot more data from BHR, and
// don't want to enable it for beta users at the moment. We can scale this up in
// the future.
#define BHR_BETA_MOD INT32_MAX;

// Maximum depth of the call stack in the reported thread hangs. This value represents
// the 99.9th percentile of the thread hangs stack depths reported by Telemetry.
static const size_t kMaxThreadHangStackDepth = 30;

// Interval at which we check the global and per-process CPU usage in order to determine
// if there is high external CPU usage.
static const int32_t kCheckCPUIntervalMilliseconds = 2000;

// An utility comparator function used by std::unique to collapse "(* script)" entries in
// a vector representing a call stack.
bool StackScriptEntriesCollapser(const char* aStackEntry, const char *aAnotherStackEntry)
{
  return !strcmp(aStackEntry, aAnotherStackEntry) &&
         (!strcmp(aStackEntry, "(chrome script)") || !strcmp(aStackEntry, "(content script)"));
}

namespace mozilla {

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
    AUTO_PROFILER_REGISTER_THREAD("BgHangMonitor");
    NS_SetCurrentThreadName("BHMgr Monitor");

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
  TimeStamp mNow;
  // List of BackgroundHangThread instances associated with each thread
  LinkedList<BackgroundHangThread> mHangThreads;

  // Unwinding and reporting of hangs is despatched to this thread.
  nsCOMPtr<nsIThread> mHangProcessingThread;

  // Allows us to watch CPU usage and annotate hangs when the system is
  // under high external load.
  CPUUsageWatcher mCPUUsageWatcher;

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

  // Hang timeout
  const TimeDuration mTimeout;
  // PermaHang timeout
  const TimeDuration mMaxTimeout;
  // Time at last activity
  TimeStamp mLastActivity;
  // Time when a hang started
  TimeStamp mHangStart;
  // Is the thread in a hang
  bool mHanging;
  // Is the thread in a waiting state
  bool mWaiting;
  // Is the thread dedicated to a single BackgroundHangMonitor
  BackgroundHangMonitor::ThreadType mThreadType;
#ifdef MOZ_GECKO_PROFILER
  // Platform-specific helper to get hang stacks
  ThreadStackHelper mStackHelper;
#endif
  // Stack of current hang
  HangStack mHangStack;
  // Annotations for the current hang
  HangMonitor::HangAnnotations mAnnotations;
  // Annotators registered for this thread
  HangMonitor::Observer::Annotators mAnnotators;
  // The name of the runnable which is hanging the current process
  nsCString mRunnableName;
  // The name of the thread which is being monitored
  nsCString mThreadName;

  BackgroundHangThread(const char* aName,
                       uint32_t aTimeoutMs,
                       uint32_t aMaxTimeoutMs,
                       BackgroundHangMonitor::ThreadType aThreadType = BackgroundHangMonitor::THREAD_SHARED);

  // Report a hang; aManager->mLock IS locked. The hang will be processed
  // off-main-thread, and will then be submitted back.
  void ReportHang(TimeDuration aHangTime);
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
    if (mHanging) {
      // We were hanging! We're done with that now, so let's report it.
      // ReportHang() doesn't do much work on the current thread, and is
      // safe to call from any thread as long as we're holding the lock.
      ReportHang(mLastActivity - mHangStart);
      mHanging = false;
    }
    mWaiting = true;
  }

  // Returns true if this thread is (or might be) shared between other
  // BackgroundHangMonitors for the monitored thread.
  bool IsShared() {
    return mThreadType == BackgroundHangMonitor::THREAD_SHARED;
  }
};

StaticRefPtr<BackgroundHangManager> BackgroundHangManager::sInstance;
bool BackgroundHangManager::sDisabled = false;

MOZ_THREAD_LOCAL(BackgroundHangThread*) BackgroundHangThread::sTlsKey;
bool BackgroundHangThread::sTlsKeyInitialized;

BackgroundHangManager::BackgroundHangManager()
  : mShutdown(false)
  , mLock("BackgroundHangManager")
{
  // Lock so we don't race against the new monitor thread
  MonitorAutoLock autoLock(mLock);

  mHangMonitorThread = PR_CreateThread(
    PR_USER_THREAD, MonitorThread, this,
    PR_PRIORITY_LOW, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);

  MOZ_ASSERT(mHangMonitorThread, "Failed to create BHR monitor thread");

  DebugOnly<nsresult> rv
    = NS_NewNamedThread("BHMgr Processor",
                        getter_AddRefs(mHangProcessingThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && mHangProcessingThread,
             "Failed to create BHR processing thread");
}

BackgroundHangManager::~BackgroundHangManager()
{
  MOZ_ASSERT(mShutdown, "Destruction without Shutdown call");
  MOZ_ASSERT(mHangThreads.isEmpty(), "Destruction with outstanding monitors");
  MOZ_ASSERT(mHangMonitorThread, "No monitor thread");
  MOZ_ASSERT(mHangProcessingThread, "No processing thread");

  // PR_CreateThread could have failed above due to resource limitation
  if (mHangMonitorThread) {
    // The monitor thread can only live as long as the instance lives
    PR_JoinThread(mHangMonitorThread);
  }

  // Similarly, NS_NewNamedThread above could have failed.
  if (mHangProcessingThread) {
    mHangProcessingThread->Shutdown();
  }
}

void
BackgroundHangManager::RunMonitorThread()
{
  // Keep us locked except when waiting
  MonitorAutoLock autoLock(mLock);

  /* mNow is updated at various intervals determined by waitTime.
     However, if an update latency is too long (due to CPU scheduling, system
     sleep, etc.), we don't update mNow at all. This is done so that
     long latencies in our timing are not detected as hangs. systemTime is
     used to track TimeStamp::Now() and determine our latency. */

  TimeStamp systemTime = TimeStamp::Now();
  // Default values for the first iteration of thread loop
  TimeDuration waitTime;
  TimeDuration recheckTimeout;
  TimeStamp lastCheckedCPUUsage = systemTime;
  TimeDuration checkCPUUsageInterval =
    TimeDuration::FromMilliseconds(kCheckCPUIntervalMilliseconds);

  while (!mShutdown) {
    autoLock.Wait(waitTime);

    TimeStamp newTime = TimeStamp::Now();
    TimeDuration systemInterval = newTime - systemTime;
    systemTime = newTime;

    if (systemTime - lastCheckedCPUUsage > checkCPUUsageInterval) {
      Unused << NS_WARN_IF(mCPUUsageWatcher.CollectCPUUsage().isErr());
      lastCheckedCPUUsage = systemTime;
    }

    /* waitTime is a quarter of the shortest timeout value; If our timing
       latency is low enough (less than half the shortest timeout value),
       we can update mNow. */
    if (MOZ_LIKELY(waitTime != TimeDuration::Forever() &&
                   systemInterval < waitTime * 2)) {
      mNow += systemInterval;
    }

    /* If it's before the next recheck timeout, and our wait did not get
       interrupted, we can keep the current waitTime and skip iterating
       through hang monitors. */
    if (MOZ_LIKELY(systemInterval < recheckTimeout &&
                   systemInterval >= waitTime)) {
      recheckTimeout -= systemInterval;
      continue;
    }

    /* We are in one of the following scenarios,
     - Hang or permahang recheck timeout
     - Thread added/removed
     - Thread wait or hang ended
       In all cases, we want to go through our list of hang
       monitors and update waitTime and recheckTimeout. */
    waitTime = TimeDuration::Forever();
    recheckTimeout = TimeDuration::Forever();

    // Locally hold mNow
    TimeStamp now = mNow;

    // iterate through hang monitors
    for (BackgroundHangThread* currentThread = mHangThreads.getFirst();
         currentThread; currentThread = currentThread->getNext()) {

      if (currentThread->mWaiting) {
        // Thread is waiting, not hanging
        continue;
      }
      TimeStamp lastActivity = currentThread->mLastActivity;
      TimeDuration hangTime = now - lastActivity;
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
#ifdef MOZ_GECKO_PROFILER
          // A hang started, collect a stack
          currentThread->mStackHelper.GetStack(
            currentThread->mHangStack,
            currentThread->mRunnableName,
            true);
#endif

          // If we hang immediately on waking, then the most recently collected
          // CPU usage is going to be an average across the whole time we were
          // sleeping. Accordingly, we want to make sure that when we hang, we
          // collect a fresh value.
          if (systemTime != lastCheckedCPUUsage) {
            Unused << NS_WARN_IF(mCPUUsageWatcher.CollectCPUUsage().isErr());
            lastCheckedCPUUsage = systemTime;
          }

          currentThread->mHangStart = lastActivity;
          currentThread->mHanging = true;
          currentThread->mAnnotations =
            currentThread->mAnnotators.GatherAnnotations();
        }
      } else {
        if (MOZ_LIKELY(lastActivity != currentThread->mHangStart)) {
          // A hang ended
          currentThread->ReportHang(now - currentThread->mHangStart);
          currentThread->mHanging = false;
        }
      }

      /* If we are hanging, the next time we check for hang status is when
         the hang turns into a permahang. If we're not hanging, the next
         recheck timeout is when we may be entering a hang. */
      TimeDuration nextRecheck;
      if (currentThread->mHanging) {
        nextRecheck = currentThread->mMaxTimeout;
      } else {
        nextRecheck = currentThread->mTimeout;
      }
      recheckTimeout = TimeDuration::Min(recheckTimeout, nextRecheck - hangTime);

      if (currentThread->mTimeout != TimeDuration::Forever()) {
        /* We wait for a quarter of the shortest timeout
           value to give mNow enough granularity. */
        waitTime = TimeDuration::Min(waitTime, currentThread->mTimeout / (int64_t) 4);
      }
    }
  }

  /* We are shutting down now.
     Wait for all outstanding monitors to unregister. */
  while (!mHangThreads.isEmpty()) {
    autoLock.Wait();
  }
}


BackgroundHangThread::BackgroundHangThread(const char* aName,
                                           uint32_t aTimeoutMs,
                                           uint32_t aMaxTimeoutMs,
                                           BackgroundHangMonitor::ThreadType aThreadType)
  : mManager(BackgroundHangManager::sInstance)
  , mThreadID(PR_GetCurrentThread())
  , mTimeout(aTimeoutMs == BackgroundHangMonitor::kNoTimeout
             ? TimeDuration::Forever()
             : TimeDuration::FromMilliseconds(aTimeoutMs))
  , mMaxTimeout(aMaxTimeoutMs == BackgroundHangMonitor::kNoTimeout
                ? TimeDuration::Forever()
                : TimeDuration::FromMilliseconds(aMaxTimeoutMs))
  , mLastActivity(mManager->mNow)
  , mHangStart(mLastActivity)
  , mHanging(false)
  , mWaiting(true)
  , mThreadType(aThreadType)
  , mThreadName(aName)
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
}

void
BackgroundHangThread::ReportHang(TimeDuration aHangTime)
{
  // Recovered from a hang; called on the monitor thread
  // mManager->mLock IS locked

  nsTArray<HangAnnotation> annotations;
  for (auto& annotation : mAnnotations) {
    HangAnnotation annot(annotation.mName, annotation.mValue);
    annotations.AppendElement(std::move(annot));
  }

  HangDetails hangDetails(
    aHangTime,
    nsDependentCString(XRE_ChildProcessTypeToString(XRE_GetProcessType())),
    VoidString(),
    mThreadName,
    mRunnableName,
    std::move(mHangStack),
    std::move(annotations)
  );

  // If the hang processing thread exists, we can process the native stack
  // on it. Otherwise, we are unable to report a native stack, so we just
  // report without one.
  if (mManager->mHangProcessingThread) {
    nsCOMPtr<nsIRunnable> processHangStackRunnable =
      new ProcessHangStackRunnable(std::move(hangDetails));
    mManager->mHangProcessingThread
            ->Dispatch(processHangStackRunnable.forget());
  } else {
    NS_WARNING("Unable to report native stack without a BHR processing thread");
    RefPtr<nsHangDetails> hd = new nsHangDetails(std::move(hangDetails));
    hd->Submit();
  }

  // If the profiler is enabled, add a marker.
#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    TimeStamp endTime = TimeStamp::Now();
    TimeStamp startTime = endTime - aHangTime;
    profiler_add_marker_for_thread(
      mStackHelper.GetThreadId(),
      "BHR-detected hang",
      MakeUnique<HangMarkerPayload>(startTime, endTime));
  }
#endif
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
  TimeStamp now = mManager->mNow;
  if (mWaiting) {
    mLastActivity = now;
    mWaiting = false;
    /* We have to wake up the manager thread because when all threads
       are waiting, the manager thread waits indefinitely as well. */
    mManager->Wakeup();
  } else {
    TimeDuration duration = now - mLastActivity;
    if (MOZ_UNLIKELY(duration >= mTimeout)) {
      /* Wake up the manager thread to tell it that a hang ended */
      mManager->Wakeup();
    }
    mLastActivity = now;
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
  nsAutoCString clientID;
  nsresult rv =
    Preferences::GetCString("toolkit.telemetry.cachedClientID", clientID);
  bool telemetryEnabled = Telemetry::CanRecordPrereleaseData();

  if (!telemetryEnabled || NS_FAILED(rv) ||
      BackgroundHangMonitor::ShouldDisableOnBeta(clientID)) {
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

  if (!strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "beta")) {
    if (XRE_IsParentProcess()) { // cached ClientID hasn't been read yet
      BackgroundHangThread::Startup();
      BackgroundHangManager::sInstance = new BackgroundHangManager();
      Unused << NS_WARN_IF(BackgroundHangManager::sInstance->mCPUUsageWatcher.Init().isErr());

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
  Unused << NS_WARN_IF(BackgroundHangManager::sInstance->mCPUUsageWatcher.Init().isErr());
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
  BackgroundHangManager::sInstance->mCPUUsageWatcher.Uninit();
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
# ifdef MOZ_VALGRIND
  // If we're running on Valgrind, we'll be making forward progress at a
  // rate of somewhere between 1/25th and 1/50th of normal.  This causes the
  // BHR to capture a lot of stacks, which slows us down even more.  As an
  // attempt to avoid the worst of this, scale up all presented timeouts by
  // a factor of thirty, and add six seconds so as to impose a six second
  // floor on all timeouts.  For a non-Valgrind-enabled build, or for an
  // enabled build which isn't running on Valgrind, the timeouts are
  // unchanged.
  if (RUNNING_ON_VALGRIND) {
    const uint32_t scaleUp = 30;
    const uint32_t extraMs = 6000;
    if (aTimeoutMs != BackgroundHangMonitor::kNoTimeout) {
      aTimeoutMs *= scaleUp;
      aTimeoutMs += extraMs;
    }
    if (aMaxTimeoutMs != BackgroundHangMonitor::kNoTimeout) {
      aMaxTimeoutMs *= scaleUp;
      aMaxTimeoutMs += extraMs;
    }
  }
# endif

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

} // namespace mozilla
