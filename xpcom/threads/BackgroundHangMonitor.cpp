/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"

#include "prinrval.h"
#include "prthread.h"

#include <algorithm>

namespace mozilla {

/**
 * BackgroundHangManager is the global object that
 * manages all instances of BackgroundHangThread.
 */
class BackgroundHangManager : public AtomicRefCounted<BackgroundHangManager>
{
private:
  // Background hang monitor thread function
  static void MonitorThread(void* aData)
  {
    PR_SetCurrentThreadName("BgHangManager");
    // Keep a strong reference throughout thread lifetime
    RefPtr<BackgroundHangManager>(
      static_cast<BackgroundHangManager*>(aData))->RunMonitorThread();
  }

  // Hang monitor thread
  PRThread* mHangMonitorThread;
  // Stop hang monitoring
  bool mShutdown;

  BackgroundHangManager(const BackgroundHangManager&);
  BackgroundHangManager& operator=(const BackgroundHangManager&);
  void RunMonitorThread();

public:
  static StaticRefPtr<BackgroundHangManager> sInstance;

  // Lock for access to members of this class
  Monitor mLock;
  // Current time as seen by hang monitors
  PRIntervalTime mIntervalNow;
  // List of BackgroundHangThread instances associated with each thread
  LinkedList<BackgroundHangThread> mHangThreads;

  void Shutdown()
  {
    MonitorAutoLock autoLock(mLock);
    mShutdown = true;
    autoLock.Notify();
  }

  void Wakeup()
  {
    // Use PR_Interrupt to avoid potentially taking a lock
    PR_Interrupt(mHangMonitorThread);
  }

  BackgroundHangManager();
  ~BackgroundHangManager();
};

/**
 * BackgroundHangThread is a per-thread object that is used
 * by all instances of BackgroundHangMonitor to monitor hangs.
 */
class BackgroundHangThread : public RefCounted<BackgroundHangThread>
                           , public LinkedListElement<BackgroundHangThread>
{
private:
  static ThreadLocal<BackgroundHangThread*> sTlsKey;

  BackgroundHangThread(const BackgroundHangThread&);
  BackgroundHangThread& operator=(const BackgroundHangThread&);

  /* Keep a reference to the manager, so we can keep going even
     after BackgroundHangManager::Shutdown is called. */
  const RefPtr<BackgroundHangManager> mManager;
  // Unique thread ID for identification
  const PRThread* mThreadID;

public:
  static BackgroundHangThread* FindThread();

  static void Startup()
  {
    /* We can tolerate init() failing.
       The if block turns off warn_unused_result. */
    if (!sTlsKey.init()) {}
  }

  // Name of the thread
  const nsAutoCString mThreadName;
  // Hang timeout in ticks
  const PRIntervalTime mTimeout;
  // PermaHang timeout in ticks
  const PRIntervalTime mMaxTimeout;
  // Time at last activity
  PRIntervalTime mInterval;
  // Is the thread in a waiting state
  bool mWaiting;

  BackgroundHangThread(const char* aName,
                       uint32_t aTimeoutMs,
                       uint32_t aMaxTimeoutMs);
  ~BackgroundHangThread();

  // Report a hang; aManager->mLock is NOT locked
  void ReportHang(PRIntervalTime aHangTime) const;
  // Report a permanent hang; aManager->mLock IS locked
  void ReportPermaHang() const;
  // Called by BackgroundHangMonitor::NotifyActivity
  void NotifyActivity();
  // Called by BackgroundHangMonitor::NotifyWait
  void NotifyWait()
  {
    NotifyActivity();
    mWaiting = true;
  }
};


StaticRefPtr<BackgroundHangManager> BackgroundHangManager::sInstance;

ThreadLocal<BackgroundHangThread*> BackgroundHangThread::sTlsKey;


BackgroundHangManager::BackgroundHangManager()
  : mShutdown(false)
  , mLock("BackgroundHangManager")
  , mIntervalNow(0)
{
  // Lock so we don't race against the new monitor thread
  MonitorAutoLock autoLock(mLock);
  mHangMonitorThread = PR_CreateThread(
    PR_USER_THREAD, MonitorThread, this,
    PR_PRIORITY_LOW, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
}

BackgroundHangManager::~BackgroundHangManager()
{
  MOZ_ASSERT(mShutdown,
    "Destruction without Shutdown call");
  MOZ_ASSERT(mHangThreads.isEmpty(),
    "Destruction with outstanding monitors");
}

void
BackgroundHangManager::RunMonitorThread()
{
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
  PRIntervalTime permaHangTimeout = PR_INTERVAL_NO_WAIT;

  while (!mShutdown) {

    PR_ClearInterrupt();
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

    /* If it's before the next permahang timeout, and our wait did not
       get interrupted (either through Notify or PR_Interrupt), we can
       keep the current waitTime and skip iterating through hang monitors. */
    if (MOZ_LIKELY(systemInterval < permaHangTimeout &&
                   systemInterval >= waitTime &&
                   rv == NS_OK)) {
      permaHangTimeout -= systemInterval;
      continue;
    }

    /* We are in one of the following scenarios,
     - Permahang timeout
     - Thread added/removed
     - Thread wait ended
       In all cases, we want to go through our list of hang
       monitors and update waitTime and permaHangTimeout. */
    waitTime = PR_INTERVAL_NO_TIMEOUT;
    permaHangTimeout = PR_INTERVAL_NO_TIMEOUT;

    // Locally hold mIntervalNow
    PRIntervalTime intervalNow = mIntervalNow;

    // iterate through hang monitors
    for (BackgroundHangThread* currentThread = mHangThreads.getFirst();
         currentThread; currentThread = currentThread->getNext()) {

      if (currentThread->mWaiting) {
        // Thread is waiting, not hanging
        continue;
      }
      PRIntervalTime hangTime = intervalNow - currentThread->mInterval;
      if (MOZ_UNLIKELY(hangTime >= currentThread->mMaxTimeout)) {
        // Skip subsequent iterations and tolerate a race on mWaiting here
        currentThread->mWaiting = true;
        currentThread->ReportPermaHang();
        continue;
      }
      /* We wait for a quarter of the shortest timeout
         value to give mIntervalNow enough granularity. */
      waitTime = std::min(waitTime, currentThread->mTimeout / 4);
      permaHangTimeout = std::min(
        permaHangTimeout, currentThread->mMaxTimeout - hangTime);
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
                                           uint32_t aMaxTimeoutMs)
  : mManager(BackgroundHangManager::sInstance)
  , mThreadID(PR_GetCurrentThread())
  , mThreadName(aName)
  , mTimeout(PR_MillisecondsToInterval(aTimeoutMs))
  , mMaxTimeout(PR_MillisecondsToInterval(aMaxTimeoutMs))
  , mInterval(mManager->mIntervalNow)
  , mWaiting(true)
{
  if (sTlsKey.initialized()) {
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
  if (sTlsKey.initialized()) {
    sTlsKey.set(nullptr);
  }
}

void
BackgroundHangThread::ReportHang(PRIntervalTime aHangTime) const
{
  // Recovered from a hang; called on the hanged thread
  // mManager->mLock is NOT locked

  // TODO: Add telemetry reporting for hangs
}

void
BackgroundHangThread::ReportPermaHang() const
{
  // Permanently hanged; called on the monitor thread
  // mManager->mLock IS locked

  // TODO: Add telemetry reporting for perma-hangs
}

MOZ_ALWAYS_INLINE void
BackgroundHangThread::NotifyActivity()
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
    if (MOZ_UNLIKELY(duration >= mTimeout)) {
      ReportHang(duration);
    }
    mInterval = intervalNow;
  }
}

BackgroundHangThread*
BackgroundHangThread::FindThread()
{
  if (sTlsKey.initialized()) {
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
    if (thread->mThreadID == threadID) {
      return thread;
    }
  }
  // Current thread is not initialized
  return nullptr;
}


void
BackgroundHangMonitor::Startup()
{
  MOZ_ASSERT(!BackgroundHangManager::sInstance, "Already initialized");
  BackgroundHangThread::Startup();
  BackgroundHangManager::sInstance = new BackgroundHangManager();
}

void
BackgroundHangMonitor::Shutdown()
{
  MOZ_ASSERT(BackgroundHangManager::sInstance, "Not initialized");
  /* Scope our lock inside Shutdown() because the sInstance object can
     be destroyed as soon as we set sInstance to nullptr below, and
     we don't want to hold the lock when it's being destroyed. */
  BackgroundHangManager::sInstance->Shutdown();
  BackgroundHangManager::sInstance = nullptr;
}

BackgroundHangMonitor::BackgroundHangMonitor(const char* aName,
                                             uint32_t aTimeoutMs,
                                             uint32_t aMaxTimeoutMs)
  : mThread(BackgroundHangThread::FindThread())
{
  if (!mThread) {
    mThread = new BackgroundHangThread(aName, aTimeoutMs, aMaxTimeoutMs);
  }
}

BackgroundHangMonitor::BackgroundHangMonitor()
  : mThread(BackgroundHangThread::FindThread())
{
  MOZ_ASSERT(mThread, "Thread not initialized for hang monitoring");
}

BackgroundHangMonitor::~BackgroundHangMonitor()
{
}

void
BackgroundHangMonitor::NotifyActivity()
{
  mThread->NotifyActivity();
}

void
BackgroundHangMonitor::NotifyWait()
{
  mThread->NotifyWait();
}

} // namespace mozilla
