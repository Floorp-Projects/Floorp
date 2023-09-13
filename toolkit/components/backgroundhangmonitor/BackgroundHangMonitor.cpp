/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BackgroundHangMonitor.h"

#include <string_view>
#include <utility>

#include "GeckoProfiler.h"
#include "HangDetails.h"
#include "ThreadStackHelper.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CPUUsageWatcher.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_toolkit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Unused.h"
#if defined(XP_WIN)
#  include "mozilla/WindowsStackWalkInitialization.h"
#endif  // XP_WIN
#include "mozilla/dom/RemoteType.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prinrval.h"
#include "prthread.h"

#include <algorithm>

#if defined(XP_WIN)
#  include "mozilla/NativeNt.h"
#endif

// Activate BHR only for one every BHR_BETA_MOD users.
// We're doing experimentation with collecting a lot more data from BHR, and
// don't want to enable it for beta users at the moment. We can scale this up in
// the future.
#define BHR_BETA_MOD INT32_MAX;

// Interval at which we check the global and per-process CPU usage in order to
// determine if there is high external CPU usage.
static const int32_t kCheckCPUIntervalMilliseconds = 2000;

// An utility comparator function used by std::unique to collapse "(* script)"
// entries in a vector representing a call stack.
bool StackScriptEntriesCollapser(const char* aStackEntry,
                                 const char* aAnotherStackEntry) {
  return !strcmp(aStackEntry, aAnotherStackEntry) &&
         (!strcmp(aStackEntry, "(chrome script)") ||
          !strcmp(aStackEntry, "(content script)"));
}

namespace mozilla {

/**
 * BackgroundHangManager is the global object that
 * manages all instances of BackgroundHangThread.
 */
class BackgroundHangManager : public nsIObserver {
 private:
  // Stop hang monitoring
  bool mShutdown;

  BackgroundHangManager(const BackgroundHangManager&);
  BackgroundHangManager& operator=(const BackgroundHangManager&);

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  static StaticRefPtr<BackgroundHangManager> sInstance;
  static bool sDisabled;

  // Lock for access to members of this class
  Monitor mLock MOZ_UNANNOTATED;
  // List of BackgroundHangThread instances associated with each thread
  LinkedList<BackgroundHangThread> mHangThreads;

  // Unwinding and reporting of hangs is despatched to this thread.
  nsCOMPtr<nsIThread> mHangProcessingThread;

  // Hang monitor thread
  nsCOMPtr<nsIThread> mHangMonitorThread;

  ProfilerThreadId mHangMonitorProfilerThreadId;

  void InitMonitorThread() {
    mHangMonitorProfilerThreadId = profiler_current_thread_id();
#if defined(MOZ_GECKO_PROFILER) && defined(XP_WIN) && defined(_M_X64)
    // Pre-commit 5 more pages of stack to guarantee enough commited stack
    // space on this thread upon hang detection, when we will need to run
    // profiler_suspend_and_sample_thread (bug 1840164).
    mozilla::nt::CheckStack(5 * 0x1000);
#endif
  }

  // Used for recording a permahang in case we don't ever make it back to
  // the main thread to record/send it.
  nsCOMPtr<nsIFile> mPermahangFile;

  // Allows us to watch CPU usage and annotate hangs when the system is
  // under high external load.
  CPUUsageWatcher mCPUUsageWatcher;

  TimeStamp mLastCheckedCPUUsage;

  void CollectCPUUsage(TimeStamp aNow, bool aForce = false) {
    if (aForce ||
        aNow - mLastCheckedCPUUsage >
            TimeDuration::FromMilliseconds(kCheckCPUIntervalMilliseconds)) {
      Unused << NS_WARN_IF(mCPUUsageWatcher.CollectCPUUsage().isErr());
      mLastCheckedCPUUsage = aNow;
    }
  }

  void Shutdown() {
    MonitorAutoLock autoLock(mLock);
    mShutdown = true;
  }

  BackgroundHangManager();

 private:
  virtual ~BackgroundHangManager();
};

NS_IMPL_ISUPPORTS(BackgroundHangManager, nsIObserver)

NS_IMETHODIMP
BackgroundHangManager::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  if (!strcmp(aTopic, "browser-delayed-startup-finished")) {
    MonitorAutoLock autoLock(mLock);
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(mPermahangFile));
    if (NS_SUCCEEDED(rv)) {
      mPermahangFile->AppendNative("last_permahang.bin"_ns);
    } else {
      mPermahangFile = nullptr;
    }

    if (mHangProcessingThread && mPermahangFile) {
      nsCOMPtr<nsIRunnable> submitRunnable =
          new SubmitPersistedPermahangRunnable(mPermahangFile);
      mHangProcessingThread->Dispatch(submitRunnable.forget());
    }
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(observerService);
    observerService->RemoveObserver(BackgroundHangManager::sInstance,
                                    "browser-delayed-startup-finished");
  } else if (!strcmp(aTopic, "profile-after-change")) {
    BackgroundHangMonitor::DisableOnBeta();
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    MOZ_ASSERT(observerService);
    observerService->RemoveObserver(BackgroundHangManager::sInstance,
                                    "profile-after-change");
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

/**
 * BackgroundHangThread is a per-thread object that is used
 * by all instances of BackgroundHangMonitor to monitor hangs.
 */
class BackgroundHangThread final
    : public LinkedListElement<BackgroundHangThread>,
      public nsITimerCallback,
      public nsINamed {
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
  RefPtr<nsITimer> mTimer;
  TimeStamp mExpectedTimerNotification;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  /**
   * Returns the BackgroundHangThread associated with the
   * running thread. Note that this will not find private
   * BackgroundHangThread threads.
   *
   * @return BackgroundHangThread*, or nullptr if no thread
   *         is found.
   */
  static BackgroundHangThread* FindThread();

  static void Startup() {
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
  BackgroundHangAnnotations mAnnotations;
  // Annotators registered for this thread
  BackgroundHangAnnotators mAnnotators;
  // The name of the runnable which is hanging the current process
  nsCString mRunnableName;
  // The name of the thread which is being monitored
  nsCString mThreadName;

  BackgroundHangThread(const char* aName, uint32_t aTimeoutMs,
                       uint32_t aMaxTimeoutMs,
                       BackgroundHangMonitor::ThreadType aThreadType =
                           BackgroundHangMonitor::THREAD_SHARED);

  // Report a hang; aManager->mLock IS locked. The hang will be processed
  // off-main-thread, and will then be submitted back.
  void ReportHang(TimeDuration aHangTime,
                  PersistedToDisk aPersistedToDisk = PersistedToDisk::No);
  // Report a permanent hang; aManager->mLock IS locked
  void ReportPermaHang();
  // Called by BackgroundHangMonitor::NotifyActivity
  void NotifyActivity() {
    if (MOZ_UNLIKELY(!mTimer)) {
      return;
    }

    MonitorAutoLock autoLock(mManager->mLock);
    PROFILER_MARKER_UNTYPED(
        "NotifyActivity", OTHER,
        MarkerThreadId(mManager->mHangMonitorProfilerThreadId));

    TimeStamp now = TimeStamp::Now();
    if (mWaiting) {
      mWaiting = false;
    } else if (mHanging) {
      // A hang ended.
      ReportHang(now - mHangStart);
      mHanging = false;
    }
    mLastActivity = now;
    BackgroundHangManager::sInstance->CollectCPUUsage(now);

    // Set or reset the timer.
    mExpectedTimerNotification = now + mTimeout;
    if (mTimeout != TimeDuration::Forever()) {
      mTimer->InitHighResolutionWithCallback(this, mTimeout,
                                             nsITimer::TYPE_ONE_SHOT);
    }
  }
  // Called by BackgroundHangMonitor::NotifyWait
  void NotifyWait() {
    if (MOZ_UNLIKELY(!mTimer)) {
      return;
    }

    MonitorAutoLock autoLock(mManager->mLock);
    PROFILER_MARKER_UNTYPED(
        "NotifyWait", OTHER,
        MarkerThreadId(mManager->mHangMonitorProfilerThreadId));

    if (mWaiting) {
      return;
    }

    mTimer->Cancel();

    mLastActivity = TimeStamp::Now();

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

NS_IMPL_ISUPPORTS(BackgroundHangThread, nsITimerCallback, nsINamed)

NS_IMETHODIMP
BackgroundHangThread::GetName(nsACString& aName) {
  aName.AssignLiteral("BackgroundHangThread_timer");
  return NS_OK;
}

StaticRefPtr<BackgroundHangManager> BackgroundHangManager::sInstance;
bool BackgroundHangManager::sDisabled = false;

MOZ_THREAD_LOCAL(BackgroundHangThread*) BackgroundHangThread::sTlsKey;
bool BackgroundHangThread::sTlsKeyInitialized;

BackgroundHangManager::BackgroundHangManager()
    : mShutdown(false), mLock("BackgroundHangManager") {
  // Save a reference to sInstance now so that the destructor is not triggered
  // if the InitMonitorThread RunnableMethod is released before we are done.
  sInstance = this;

  DebugOnly<nsresult> rv =
      NS_NewNamedThread("BHMgr Monitor", getter_AddRefs(mHangMonitorThread),
                        mozilla::NewRunnableMethod(
                            "BackgroundHangManager::InitMonitorThread", this,
                            &BackgroundHangManager::InitMonitorThread));

  MOZ_ASSERT(NS_SUCCEEDED(rv) && mHangMonitorThread,
             "Failed to create BHR processing thread");

  rv = NS_NewNamedThread("BHMgr Processor",
                         getter_AddRefs(mHangProcessingThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv) && mHangProcessingThread,
             "Failed to create BHR processing thread");
}

BackgroundHangManager::~BackgroundHangManager() {
  MOZ_ASSERT(mShutdown, "Destruction without Shutdown call");
  MOZ_ASSERT(mHangThreads.isEmpty(), "Destruction with outstanding monitors");
  MOZ_ASSERT(mHangMonitorThread, "No monitor thread");
  MOZ_ASSERT(mHangProcessingThread, "No processing thread");

  // NS_NewNamedThread could have failed above due to resource limitation
  if (mHangMonitorThread) {
    // The monitor thread can only live as long as the instance lives
    mHangMonitorThread->Shutdown();
  }

  // Similarly, NS_NewNamedThread above could have failed.
  if (mHangProcessingThread) {
    mHangProcessingThread->Shutdown();
  }
}

BackgroundHangThread::BackgroundHangThread(
    const char* aName, uint32_t aTimeoutMs, uint32_t aMaxTimeoutMs,
    BackgroundHangMonitor::ThreadType aThreadType)
    : mManager(BackgroundHangManager::sInstance),
      mThreadID(PR_GetCurrentThread()),
      mTimeout(aTimeoutMs == BackgroundHangMonitor::kNoTimeout
                   ? TimeDuration::Forever()
                   : TimeDuration::FromMilliseconds(aTimeoutMs)),
      mMaxTimeout(aMaxTimeoutMs == BackgroundHangMonitor::kNoTimeout
                      ? TimeDuration::Forever()
                      : TimeDuration::FromMilliseconds(aMaxTimeoutMs)),
      mHanging(false),
      mWaiting(true),
      mThreadType(aThreadType),
      mThreadName(aName) {
  if (sTlsKeyInitialized && IsShared()) {
    sTlsKey.set(this);
  }
  if (mManager->mHangMonitorThread) {
    mTimer = NS_NewTimer(mManager->mHangMonitorThread);
  }
  // Lock here because LinkedList is not thread-safe
  MonitorAutoLock autoLock(mManager->mLock);
  // Add to thread list
  mManager->mHangThreads.insertBack(this);
}

BackgroundHangThread::~BackgroundHangThread() {
  // Lock here because LinkedList is not thread-safe
  MonitorAutoLock autoLock(mManager->mLock);
  // Remove from thread list
  remove();

  // We no longer have a thread
  if (sTlsKeyInitialized && IsShared()) {
    sTlsKey.set(nullptr);
  }
}

void BackgroundHangThread::ReportHang(TimeDuration aHangTime,
                                      PersistedToDisk aPersistedToDisk) {
  // Recovered from a hang; called on the monitor thread
  // mManager->mLock IS locked

  HangDetails hangDetails(aHangTime,
                          nsDependentCString(XRE_GetProcessTypeString()),
                          NOT_REMOTE_TYPE, mThreadName, mRunnableName,
                          std::move(mHangStack), std::move(mAnnotations));

  PersistedToDisk persistedToDisk = aPersistedToDisk;
  if (aPersistedToDisk == PersistedToDisk::Yes && XRE_IsParentProcess() &&
      mManager->mPermahangFile) {
    auto res = WriteHangDetailsToFile(hangDetails, mManager->mPermahangFile);
    persistedToDisk = res.isOk() ? PersistedToDisk::Yes : PersistedToDisk::No;
  }

  // If the hang processing thread exists, we can process the native stack
  // on it. Otherwise, we are unable to report a native stack, so we just
  // report without one.
  if (mManager->mHangProcessingThread) {
    nsCOMPtr<nsIRunnable> processHangStackRunnable =
        new ProcessHangStackRunnable(std::move(hangDetails), persistedToDisk);
    mManager->mHangProcessingThread->Dispatch(
        processHangStackRunnable.forget());
  } else {
    NS_WARNING("Unable to report native stack without a BHR processing thread");
    RefPtr<nsHangDetails> hd =
        new nsHangDetails(std::move(hangDetails), persistedToDisk);
    hd->Submit();
  }

  // If the profiler is enabled, add a marker.
#ifdef MOZ_GECKO_PROFILER
  if (profiler_thread_is_being_profiled_for_markers(
          mStackHelper.GetThreadId())) {
    struct HangMarker {
      static constexpr Span<const char> MarkerTypeName() {
        return MakeStringSpan("BHR-detected hang");
      }
      static void StreamJSONMarkerData(
          baseprofiler::SpliceableJSONWriter& aWriter) {}
      static MarkerSchema MarkerTypeDisplay() {
        using MS = MarkerSchema;
        MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
        return schema;
      }
    };

    const TimeStamp endTime = TimeStamp::Now();
    const TimeStamp startTime = endTime - aHangTime;
    profiler_add_marker("BHR-detected hang", geckoprofiler::category::OTHER,
                        {MarkerThreadId(mStackHelper.GetThreadId()),
                         MarkerTiming::Interval(startTime, endTime)},
                        HangMarker{});
  }
#endif
}

void BackgroundHangThread::ReportPermaHang() {
  // Permanently hanged; called on the monitor thread
  // mManager->mLock IS locked

  // The significance of a permahang is that it's likely that we won't ever
  // recover and be allowed to submit this hang. On the parent thread, we
  // compensate for this by writing the hang details to disk on this thread,
  // and in our next session we'll try to read those details
  ReportHang(mMaxTimeout, PersistedToDisk::Yes);
}

NS_IMETHODIMP BackgroundHangThread::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(profiler_current_thread_id() ==
             mManager->mHangMonitorProfilerThreadId);

  MonitorAutoLock autoLock(mManager->mLock);
  PROFILER_MARKER_UNTYPED("TimerNotify", OTHER, {});

  TimeStamp now = TimeStamp::Now();
  if (MOZ_UNLIKELY((now - mExpectedTimerNotification) * 2 > mTimeout)) {
    // If the timer notification has been delayed by more than half the timeout
    // time, assume the machine is not scheduling tasks correctly and ignore
    // this hang.
    mWaiting = true;
    mHanging = false;
    return NS_OK;
  }

  TimeDuration hangTime = now - mLastActivity;
  if (MOZ_UNLIKELY(hangTime >= mMaxTimeout)) {
    // A permahang started.  No point in trying to find its exact
    // duration, so avoid restarting the timer until there is new
    // activity.
    mWaiting = true;
    mHanging = false;
    ReportPermaHang();
    return NS_OK;
  }

  if (MOZ_LIKELY(!mHanging && hangTime >= mTimeout)) {
#ifdef MOZ_GECKO_PROFILER
    // A hang started, collect a stack
    mStackHelper.GetStack(mHangStack, mRunnableName, true);
#endif

    // If we hang immediately on waking, then the most recently collected
    // CPU usage is going to be an average across the whole time we were
    // sleeping. Accordingly, we want to make sure that when we hang, we
    // collect a fresh value.
    BackgroundHangManager::sInstance->CollectCPUUsage(now, true);

    mHangStart = mLastActivity;
    mHanging = true;
    mAnnotations = mAnnotators.GatherAnnotations();
  }

  TimeDuration nextRecheck = mMaxTimeout - hangTime;
  mExpectedTimerNotification = now + nextRecheck;
  return mTimer->InitHighResolutionWithCallback(this, nextRecheck,
                                                nsITimer::TYPE_ONE_SHOT);
}

BackgroundHangThread* BackgroundHangThread::FindThread() {
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
  for (BackgroundHangThread* thread = manager->mHangThreads.getFirst(); thread;
       thread = thread->getNext()) {
    if (thread->mThreadID == threadID && thread->IsShared()) {
      return thread;
    }
  }
#endif
  // Current thread is not initialized
  return nullptr;
}

bool BackgroundHangMonitor::ShouldDisableOnBeta(const nsCString& clientID) {
  MOZ_ASSERT(clientID.Length() == 36, "clientID is invalid");
  const char* suffix = clientID.get() + clientID.Length() - 4;
  return strtol(suffix, NULL, 16) % BHR_BETA_MOD;
}

bool BackgroundHangMonitor::DisableOnBeta() {
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

void BackgroundHangMonitor::Startup() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  MOZ_ASSERT(!BackgroundHangManager::sInstance, "Already initialized");

  if (XRE_IsContentProcess() &&
      StaticPrefs::toolkit_content_background_hang_monitor_disabled()) {
    BackgroundHangManager::sDisabled = true;
    return;
  }

#  if defined(MOZ_GECKO_PROFILER) && defined(XP_WIN)
#    if defined(_M_AMD64) || defined(_M_ARM64)
  mozilla::WindowsStackWalkInitialization();
#    endif  // _M_AMD64 || _M_ARM64
#  endif    // MOZ_GECKO_PROFILER && XP_WIN

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wunreachable-code"
#  endif
  if constexpr (std::string_view(MOZ_STRINGIFY(MOZ_UPDATE_CHANNEL)) == "beta") {
    if (XRE_IsParentProcess()) {  // cached ClientID hasn't been read yet
      BackgroundHangThread::Startup();
      new BackgroundHangManager();
      Unused << NS_WARN_IF(
          BackgroundHangManager::sInstance->mCPUUsageWatcher.Init().isErr());
      observerService->AddObserver(BackgroundHangManager::sInstance,
                                   "profile-after-change", false);
      return;
    } else if (DisableOnBeta()) {
      return;
    }
  }
#  ifdef __clang__
#    pragma clang diagnostic pop
#  endif

  BackgroundHangThread::Startup();
  new BackgroundHangManager();
  Unused << NS_WARN_IF(
      BackgroundHangManager::sInstance->mCPUUsageWatcher.Init().isErr());
  if (XRE_IsParentProcess()) {
    observerService->AddObserver(BackgroundHangManager::sInstance,
                                 "browser-delayed-startup-finished", false);
  }
#endif
}

void BackgroundHangMonitor::Shutdown() {
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
    : mThread(aThreadType == THREAD_SHARED ? BackgroundHangThread::FindThread()
                                           : nullptr) {
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
#  ifdef MOZ_VALGRIND
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
#  endif

  if (!BackgroundHangManager::sDisabled && !mThread) {
    mThread =
        new BackgroundHangThread(aName, aTimeoutMs, aMaxTimeoutMs, aThreadType);
  }
#endif
}

BackgroundHangMonitor::BackgroundHangMonitor()
    : mThread(BackgroundHangThread::FindThread()) {
#ifdef MOZ_ENABLE_BACKGROUND_HANG_MONITOR
  if (BackgroundHangManager::sDisabled) {
    return;
  }
#endif
}

BackgroundHangMonitor::~BackgroundHangMonitor() = default;

void BackgroundHangMonitor::NotifyActivity() {
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

void BackgroundHangMonitor::NotifyWait() {
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

bool BackgroundHangMonitor::RegisterAnnotator(
    BackgroundHangAnnotator& aAnnotator) {
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

bool BackgroundHangMonitor::UnregisterAnnotator(
    BackgroundHangAnnotator& aAnnotator) {
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

}  // namespace mozilla
