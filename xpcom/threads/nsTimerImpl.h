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
#include "mozilla/Logging.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

#ifdef MOZ_TASK_TRACER
#include "TracedTaskCommon.h"
#endif

extern mozilla::LogModule* GetTimerLog();

#define NS_TIMER_CID \
{ /* 5ff24248-1dd2-11b2-8427-fbab44f29bc8 */         \
     0x5ff24248,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x84, 0x27, 0xfb, 0xab, 0x44, 0xf2, 0x9b, 0xc8} \
}

// TimerThread, nsTimerEvent, and nsTimer have references to these. nsTimer has
// a separate lifecycle so we can Cancel() the underlying timer when the user of
// the nsTimer has let go of its last reference.
class nsTimerImpl
{
  ~nsTimerImpl();
public:
  typedef mozilla::TimeStamp TimeStamp;

  explicit nsTimerImpl(nsITimer* aTimer);
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsTimerImpl)
  NS_DECL_NON_VIRTUAL_NSITIMER

  static nsresult Startup();
  static void Shutdown();

  void SetDelayInternal(uint32_t aDelay, TimeStamp aBase = TimeStamp::Now());

  void Fire();

#ifdef MOZ_TASK_TRACER
  void GetTLSTraceInfo();
  mozilla::tasktracer::TracedTaskCommon GetTracedTask();
#endif

  int32_t GetGeneration()
  {
    return mGeneration;
  }

  enum class CallbackType : uint8_t {
    Unknown = 0,
    Interface = 1,
    Function = 2,
    Observer = 3,
  };

  nsresult InitCommon(uint32_t aDelay, uint32_t aType);

  void ReleaseCallback()
  {
    // if we're the last owner of the callback object, make
    // sure that we don't recurse into ReleaseCallback in case
    // the callback's destructor calls Cancel() or similar.
    CallbackType cbType = mCallbackType;
    mCallbackType = CallbackType::Unknown;

    if (cbType == CallbackType::Interface) {
      NS_RELEASE(mCallback.i);
    } else if (cbType == CallbackType::Observer) {
      NS_RELEASE(mCallback.o);
    }
  }

  // Permanently disables this timer. This gets called when the last nsTimer
  // ref (besides mITimer) goes away. If called from the target thread, it
  // guarantees that the timer will not fire again. If called from anywhere
  // else, it could race.
  void Neuter();

  bool IsRepeating() const
  {
    static_assert(nsITimer::TYPE_ONE_SHOT < nsITimer::TYPE_REPEATING_SLACK,
                  "invalid ordering of timer types!");
    static_assert(
        nsITimer::TYPE_REPEATING_SLACK < nsITimer::TYPE_REPEATING_PRECISE,
        "invalid ordering of timer types!");
    static_assert(
        nsITimer::TYPE_REPEATING_PRECISE <
          nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
        "invalid ordering of timer types!");
    return mType >= nsITimer::TYPE_REPEATING_SLACK;
  }

  bool IsRepeatingPrecisely() const
  {
    return mType >= nsITimer::TYPE_REPEATING_PRECISE;
  }

  nsCOMPtr<nsIEventTarget> mEventTarget;

  void*                 mClosure;

  union CallbackUnion
  {
    nsTimerCallbackFunc c;
    // These refcounted references are managed manually, as they are in a union
    nsITimerCallback* MOZ_OWNING_REF i;
    nsIObserver* MOZ_OWNING_REF o;
  } mCallback;

  void LogFiring(CallbackType aCallbackType, CallbackUnion aCallbackUnion);

  // |Name| is a tagged union type representing one of (a) nothing, (b) a
  // string, or (c) a function. mozilla::Variant doesn't naturally handle the
  // "nothing" case, so we define a dummy type and value (which is unused and
  // so the exact value doesn't matter) for it.
  typedef const int NameNothing;
  typedef const char* NameString;
  typedef nsTimerNameCallbackFunc NameFunc;
  typedef mozilla::Variant<NameNothing, NameString, NameFunc> Name;
  static const NameNothing Nothing;

  nsresult InitWithFuncCallbackCommon(nsTimerCallbackFunc aFunc,
                                      void* aClosure,
                                      uint32_t aDelay,
                                      uint32_t aType,
                                      Name aName);

  // This is set by Init. It records the name (if there is one) for the timer,
  // for use when logging timer firings.
  Name mName;

  // These members are set by the initiating thread, when the timer's type is
  // changed and during the period where it fires on that thread.
  CallbackType          mCallbackType;
  uint8_t               mType;

  // The generation number of this timer, re-generated each time the timer is
  // initialized so one-shot timers can be canceled and re-initialized by the
  // arming thread without any bad race conditions.
  // This is only modified on the target thread, and only after removing the
  // timer from the TimerThread. Is set on the arming thread, initially.
  int32_t               mGeneration;

  uint32_t              mDelay;
  TimeStamp             mTimeout;

#ifdef MOZ_TASK_TRACER
  mozilla::tasktracer::TracedTaskCommon mTracedTask;
#endif

  TimeStamp             mStart, mStart2;
  static double         sDeltaSum;
  static double         sDeltaSumSquared;
  static double         sDeltaNum;
  RefPtr<nsITimer>      mITimer;
};

class nsTimer final : public nsITimer
{
  virtual ~nsTimer();
public:
  nsTimer() : mImpl(new nsTimerImpl(this)) {}

  friend class TimerThread;
  friend class nsTimerEvent;
  friend struct TimerAdditionComparator;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSITIMER(mImpl);

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  // nsTimerImpl holds a strong ref to us. When our refcount goes to 1, we will
  // null this to break the cycle.
  RefPtr<nsTimerImpl> mImpl;
};

#endif /* nsTimerImpl_h___ */
