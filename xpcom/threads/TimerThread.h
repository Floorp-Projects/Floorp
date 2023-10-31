/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TimerThread_h___
#define TimerThread_h___

#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"

#include "nsTimerImpl.h"
#include "nsThreadUtils.h"

#include "nsTArray.h"

#include "mozilla/Attributes.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Monitor.h"
#include "mozilla/ProfilerUtils.h"

// Enable this to compute lots of interesting statistics and print them out when
// PrintStatistics() is called.
#define TIMER_THREAD_STATISTICS 0

class TimerThread final : public mozilla::Runnable, public nsIObserver {
 public:
  typedef mozilla::Monitor Monitor;
  typedef mozilla::MutexAutoLock MutexAutoLock;
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  TimerThread();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER

  nsresult Shutdown();

  nsresult AddTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  nsresult RemoveTimer(nsTimerImpl* aTimer, const MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(aTimer->mMutex);
  // Considering only the first 'aSearchBound' timers (in firing order), returns
  // the timeout of the first non-low-priority timer, on the current thread,
  // that will fire before 'aDefault'. If no such timer exists, 'aDefault' is
  // returned.
  TimeStamp FindNextFireTimeForCurrentThread(TimeStamp aDefault,
                                             uint32_t aSearchBound);

  void DoBeforeSleep();
  void DoAfterSleep();

  bool IsOnTimerThread() const { return mThread->IsOnCurrentThread(); }

  uint32_t AllowedEarlyFiringMicroseconds();
  nsresult GetTimers(nsTArray<RefPtr<nsITimer>>& aRetVal);

 private:
  ~TimerThread();

  bool mInitialized;

  // These internal helper methods must be called while mMonitor is held.
  // AddTimerInternal returns false if the insertion failed.
  bool AddTimerInternal(nsTimerImpl& aTimer) MOZ_REQUIRES(mMonitor);
  bool RemoveTimerInternal(nsTimerImpl& aTimer)
      MOZ_REQUIRES(mMonitor, aTimer.mMutex);
  void RemoveLeadingCanceledTimersInternal() MOZ_REQUIRES(mMonitor);
  void RemoveFirstTimerInternal() MOZ_REQUIRES(mMonitor);
  nsresult Init() MOZ_REQUIRES(mMonitor);

  void PostTimerEvent(already_AddRefed<nsTimerImpl> aTimerRef)
      MOZ_REQUIRES(mMonitor);

  // Using atomic because this value is written to in one place, and read from
  // in another, and those two locations are likely to be executed from separate
  // threads. Reads/writes to an aligned value this size should be atomic even
  // without using std::atomic, but doing this explicitly provides a good
  // reminder that this is accessed from multiple threads.
  std::atomic<mozilla::hal::ProcessPriority> mCachedPriority =
      mozilla::hal::PROCESS_PRIORITY_UNKNOWN;

  nsCOMPtr<nsIThread> mThread;
  // Lock ordering requirements:
  // (optional) ThreadWrapper::sMutex ->
  // (optional) nsTimerImpl::mMutex   ->
  // TimerThread::mMonitor
  Monitor mMonitor;

  bool mShutdown MOZ_GUARDED_BY(mMonitor);
  bool mWaiting MOZ_GUARDED_BY(mMonitor);
  bool mNotified MOZ_GUARDED_BY(mMonitor);
  bool mSleeping MOZ_GUARDED_BY(mMonitor);

  class Entry final {
   public:
    explicit Entry(nsTimerImpl& aTimerImpl)
        : mTimeout(aTimerImpl.mTimeout),
          mDelay(aTimerImpl.mDelay),
          mTimerImpl(&aTimerImpl) {
      aTimerImpl.SetIsInTimerThread(true);
    }

    // Create an already-canceled entry with the given timeout.
    explicit Entry(TimeStamp aTimeout)
        : mTimeout(std::move(aTimeout)), mTimerImpl(nullptr) {}

    // Don't allow copies, otherwise which one would manage `IsInTimerThread`?
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    // Move-only.
    Entry(Entry&&) = default;
    Entry& operator=(Entry&&) = default;

    ~Entry() {
      if (mTimerImpl) {
        mTimerImpl->mMutex.AssertCurrentThreadOwns();
        mTimerImpl->SetIsInTimerThread(false);
      }
    }

    nsTimerImpl* Value() const { return mTimerImpl; }

    void Forget() {
      if (MOZ_UNLIKELY(!mTimerImpl)) {
        return;
      }
      mTimerImpl->mMutex.AssertCurrentThreadOwns();
      mTimerImpl->SetIsInTimerThread(false);
      mTimerImpl = nullptr;
    }

    // Called with the Monitor held, but not the TimerImpl's mutex
    already_AddRefed<nsTimerImpl> Take() {
      if (MOZ_LIKELY(mTimerImpl)) {
        MOZ_ASSERT(mTimerImpl->IsInTimerThread());
        mTimerImpl->SetIsInTimerThread(false);
      }
      return mTimerImpl.forget();
    }

    const TimeStamp& Timeout() const { return mTimeout; }
    const TimeDuration& Delay() const { return mDelay; }

   private:
    // These values are simply cached from the timer. Keeping them here is good
    // for cache usage and allows us to avoid worrying about locking conflicts
    // with the timer.
    TimeStamp mTimeout;
    TimeDuration mDelay;

    RefPtr<nsTimerImpl> mTimerImpl;
  };

  // Computes and returns the index in mTimers at which a new timer with the
  // specified timeout should be inserted in order to maintain "sorted" order.
  size_t ComputeTimerInsertionIndex(const TimeStamp& timeout) const
      MOZ_REQUIRES(mMonitor);

  // Computes and returns when we should next try to wake up in order to handle
  // the triggering of the timers in mTimers. Currently this is very simple and
  // we always just plan to wake up for the next timer in the list. In the
  // future this will be more sophisticated.
  TimeStamp ComputeWakeupTimeFromTimers() const MOZ_REQUIRES(mMonitor);

  // Computes how late a timer can acceptably fire.
  // timerDuration is the duration of the timer whose delay we are calculating.
  // Longer timers can tolerate longer firing delays.
  // minDelay is an amount by which any timer can be delayed.
  // This function will never return a value smaller than minDelay (unless this
  // conflicts with maxDelay). maxDelay is the upper limit on the amount by
  // which we will ever delay any timer. Takes precedence over minDelay if there
  // is a conflict. (Zero will effectively disable timer coalescing.)
  TimeDuration ComputeAcceptableFiringDelay(TimeDuration timerDuration,
                                            TimeDuration minDelay,
                                            TimeDuration maxDelay) const;

#ifdef XP_WIN
  UINT ComputeDesiredTimerPeriod() const;
#endif

#ifdef DEBUG
  // Checks mTimers to see if any entries are out of order or any cached
  // timeouts are incorrect and will assert if any inconsistency is found. Has
  // no side effects other than asserting so has no use in non-DEBUG builds.
  void VerifyTimerListConsistency() const MOZ_REQUIRES(mMonitor);
#endif

  // mTimers is maintained in a "pseudo-sorted" order wrt the timeouts.
  // Specifcally, mTimers is sorted according to the timeouts *if you ignore the
  // canceled entries* (those whose mTimerImpl is nullptr). Notably this means
  // that you cannot use a binary search on this list.
  nsTArray<Entry> mTimers MOZ_GUARDED_BY(mMonitor);

  // Set only at the start of the thread's Run():
  uint32_t mAllowedEarlyFiringMicroseconds MOZ_GUARDED_BY(mMonitor);

  ProfilerThreadId mProfilerThreadId MOZ_GUARDED_BY(mMonitor);

  // Time at which we were intending to wake up the last time that we slept.
  // Is "null" if we have never slept or if our last sleep was "forever".
  TimeStamp mIntendedWakeupTime;

#if TIMER_THREAD_STATISTICS
  static constexpr size_t sTimersFiredPerWakeupBucketCount = 16;
  static inline constexpr std::array<size_t, sTimersFiredPerWakeupBucketCount>
      sTimersFiredPerWakeupThresholds = {
          0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 20, 30, 40, 50, 70, (size_t)(-1)};

  mutable AutoTArray<size_t, sTimersFiredPerWakeupBucketCount>
      mTimersFiredPerWakeup MOZ_GUARDED_BY(mMonitor) = {0, 0, 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 0};
  mutable AutoTArray<size_t, sTimersFiredPerWakeupBucketCount>
      mTimersFiredPerUnnotifiedWakeup MOZ_GUARDED_BY(mMonitor) = {
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  mutable AutoTArray<size_t, sTimersFiredPerWakeupBucketCount>
      mTimersFiredPerNotifiedWakeup MOZ_GUARDED_BY(mMonitor) = {
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  mutable size_t mTotalTimersAdded MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalTimersRemoved MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalTimersFiredNotified MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalTimersFiredUnnotified MOZ_GUARDED_BY(mMonitor) = 0;

  mutable size_t mTotalWakeupCount MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalUnnotifiedWakeupCount MOZ_GUARDED_BY(mMonitor) = 0;
  mutable size_t mTotalNotifiedWakeupCount MOZ_GUARDED_BY(mMonitor) = 0;

  mutable double mTotalActualTimerFiringDelayNotified MOZ_GUARDED_BY(mMonitor) =
      0.0;
  mutable double mTotalActualTimerFiringDelayUnnotified
      MOZ_GUARDED_BY(mMonitor) = 0.0;

  mutable TimeStamp mFirstTimerAdded MOZ_GUARDED_BY(mMonitor);

  mutable size_t mEarlyWakeups MOZ_GUARDED_BY(mMonitor) = 0;
  mutable double mTotalEarlyWakeupTime MOZ_GUARDED_BY(mMonitor) = 0.0;

  void PrintStatistics() const;
#endif
};
#endif /* TimerThread_h___ */
