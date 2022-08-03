/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTimerImpl_h___
#define nsTimerImpl_h___

#include "nsITimer.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"

#include "nsCOMPtr.h"

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"
#include "mozilla/Logging.h"

extern mozilla::LogModule* GetTimerLog();

#define NS_TIMER_CID                                 \
  { /* 5ff24248-1dd2-11b2-8427-fbab44f29bc8 */       \
    0x5ff24248, 0x1dd2, 0x11b2, {                    \
      0x84, 0x27, 0xfb, 0xab, 0x44, 0xf2, 0x9b, 0xc8 \
    }                                                \
  }

class nsIObserver;
class nsTimerImplHolder;

namespace mozilla {
class LogModule;
}

// TimerThread, nsTimerEvent, and nsTimer have references to these. nsTimer has
// a separate lifecycle so we can Cancel() the underlying timer when the user of
// the nsTimer has let go of its last reference.
class nsTimerImpl {
  ~nsTimerImpl() {
    MOZ_ASSERT(!mHolder);

    // The nsITimer interface requires that its users keep a reference to the
    // timers they use while those timers are initialized but have not yet
    // fired. If this assert ever fails, it is a bug in the code that created
    // and used the timer.
    //
    // Further, note that this should never fail even with a misbehaving user,
    // because nsTimer::Release checks for a refcount of 1 with an armed timer
    // (a timer whose only reference is from the timer thread) and when it hits
    // this will remove the timer from the timer thread and thus destroy the
    // last reference, preventing this situation from occurring.
    MOZ_ASSERT(
        mCallback.is<UnknownCallback>() || mEventTarget->IsOnCurrentThread(),
        "Must not release mCallback off-target without canceling");
  }

 public:
  typedef mozilla::TimeStamp TimeStamp;

  nsTimerImpl(nsITimer* aTimer, nsIEventTarget* aTarget);
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsTimerImpl)
  NS_DECL_NON_VIRTUAL_NSITIMER

  static nsresult Startup();
  static void Shutdown();

  void SetDelayInternal(uint32_t aDelay, TimeStamp aBase = TimeStamp::Now());
  void CancelImpl(bool aClearITimer);

  void Fire(int32_t aGeneration);

  int32_t GetGeneration() { return mGeneration; }

  struct UnknownCallback {};

  using InterfaceCallback = nsCOMPtr<nsITimerCallback>;

  using ObserverCallback = nsCOMPtr<nsIObserver>;

  /// A raw function pointer and its closed-over state, along with its name for
  /// logging purposes.
  struct FuncCallback {
    nsTimerCallbackFunc mFunc;
    void* mClosure;
    const char* mName;
  };

  /// A callback defined by an owned closure and its name for logging purposes.
  struct ClosureCallback {
    std::function<void(nsITimer*)> mFunc;
    const char* mName;
  };

  using Callback =
      mozilla::Variant<UnknownCallback, InterfaceCallback, ObserverCallback,
                       FuncCallback, ClosureCallback>;

  nsresult InitCommon(const mozilla::TimeDuration& aDelay, uint32_t aType,
                      Callback&& newCallback,
                      const mozilla::MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(mMutex);

  Callback& GetCallback() MOZ_REQUIRES(mMutex) {
    mMutex.AssertCurrentThreadOwns();
    return mCallback;
  }

  bool IsRepeating() const {
    static_assert(nsITimer::TYPE_ONE_SHOT < nsITimer::TYPE_REPEATING_SLACK,
                  "invalid ordering of timer types!");
    static_assert(
        nsITimer::TYPE_REPEATING_SLACK < nsITimer::TYPE_REPEATING_PRECISE,
        "invalid ordering of timer types!");
    static_assert(nsITimer::TYPE_REPEATING_PRECISE <
                      nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
                  "invalid ordering of timer types!");
    return mType >= nsITimer::TYPE_REPEATING_SLACK &&
           mType < nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY;
  }

  bool IsLowPriority() const {
    return mType == nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY ||
           mType == nsITimer::TYPE_REPEATING_SLACK_LOW_PRIORITY;
  }

  bool IsSlack() const {
    return mType == nsITimer::TYPE_REPEATING_SLACK ||
           mType == nsITimer::TYPE_REPEATING_SLACK_LOW_PRIORITY;
  }

  void GetName(nsACString& aName, const mozilla::MutexAutoLock& aProofOfLock)
      MOZ_REQUIRES(mMutex);

  void GetName(nsACString& aName);

  void SetHolder(nsTimerImplHolder* aHolder);

  nsCOMPtr<nsIEventTarget> mEventTarget;

  void LogFiring(const Callback& aCallback, uint8_t aType, uint32_t aDelay);

  nsresult InitWithClosureCallback(std::function<void(nsITimer*)>&& aCallback,
                                   const mozilla::TimeDuration& aDelay,
                                   uint32_t aType, const char* aNameString);

  // This weak reference must be cleared by the nsTimerImplHolder by
  // calling SetHolder(nullptr) before the holder is destroyed.  Take()
  // also sets this to null, to indicate it's no longer in the
  // TimerThread's list.  This Take() call is NOT made under the
  // nsTimerImpl's mutex (all other SetHolder calls are under the mutex,
  // and all references other than in the constructor or destructor of
  // nsTimerImpl).  However, ALL uses and references to the holder are
  // under the TimerThread's Monitor lock, so consistency is guaranteed by that.
  nsTimerImplHolder* mHolder;

  // These members are set by the initiating thread, when the timer's type is
  // changed and during the period where it fires on that thread.
  uint8_t mType;

  // The generation number of this timer, re-generated each time the timer is
  // initialized so one-shot timers can be canceled and re-initialized by the
  // arming thread without any bad race conditions.
  // Updated only after this timer has been removed from the timer thread.
  int32_t mGeneration;

  mozilla::TimeDuration mDelay MOZ_GUARDED_BY(mMutex);
  // Never updated while in the TimerThread's timer list.  Only updated
  // before adding to that list or during nsTimerImpl::Fire(), when it has
  // been removed from the TimerThread's list.  TimerThread can access
  // mTimeout of any timer in the list safely
  mozilla::TimeStamp mTimeout;

  RefPtr<nsITimer> mITimer MOZ_GUARDED_BY(mMutex);
  mozilla::Mutex mMutex;
  Callback mCallback MOZ_GUARDED_BY(mMutex);
  // Counter because in rare cases we can Fire reentrantly
  unsigned int mFiring MOZ_GUARDED_BY(mMutex);

  static mozilla::StaticMutex sDeltaMutex;
  static double sDeltaSum MOZ_GUARDED_BY(sDeltaMutex);
  static double sDeltaSumSquared MOZ_GUARDED_BY(sDeltaMutex);
  static double sDeltaNum MOZ_GUARDED_BY(sDeltaMutex);
};

class nsTimer final : public nsITimer {
  explicit nsTimer(nsIEventTarget* aTarget)
      : mImpl(new nsTimerImpl(this, aTarget)) {}

  virtual ~nsTimer();

 public:
  friend class TimerThread;
  friend class nsTimerEvent;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSITIMER(mImpl);

  // NOTE: This constructor is not exposed on `nsITimer` as NS_FORWARD_SAFE_
  // does not support forwarding rvalue references.
  nsresult InitWithClosureCallback(std::function<void(nsITimer*)>&& aCallback,
                                   const mozilla::TimeDuration& aDelay,
                                   uint32_t aType, const char* aNameString) {
    return mImpl ? mImpl->InitWithClosureCallback(std::move(aCallback), aDelay,
                                                  aType, aNameString)
                 : NS_ERROR_NULL_POINTER;
  }

  // Create a timer targeting the given target.  nullptr indicates that the
  // current thread should be used as the timer's target.
  static RefPtr<nsTimer> WithEventTarget(nsIEventTarget* aTarget);

  static nsresult XPCOMConstructor(REFNSIID aIID, void** aResult);

 private:
  // nsTimerImpl holds a strong ref to us. When our refcount goes to 1, we will
  // null this to break the cycle.
  RefPtr<nsTimerImpl> mImpl;
};

// A class that holds on to an nsTimerImpl.  This lets the nsTimerImpl object
// directly instruct its holder to forget the timer, avoiding list lookups.
class nsTimerImplHolder {
 public:
  explicit nsTimerImplHolder(nsTimerImpl* aTimerImpl) : mTimerImpl(aTimerImpl) {
    if (mTimerImpl) {
      mTimerImpl->mMutex.AssertCurrentThreadOwns();
      mTimerImpl->SetHolder(this);
    }
  }

  ~nsTimerImplHolder() {
    if (mTimerImpl) {
      mTimerImpl->mMutex.AssertCurrentThreadOwns();
      mTimerImpl->SetHolder(nullptr);
    }
  }

  void Forget(nsTimerImpl* aTimerImpl) {
    if (MOZ_UNLIKELY(!mTimerImpl)) {
      return;
    }
    MOZ_ASSERT(aTimerImpl == mTimerImpl);
    mTimerImpl->mMutex.AssertCurrentThreadOwns();
    mTimerImpl->SetHolder(nullptr);
    mTimerImpl = nullptr;
  }

 protected:
  RefPtr<nsTimerImpl> mTimerImpl;
};

#endif /* nsTimerImpl_h___ */
