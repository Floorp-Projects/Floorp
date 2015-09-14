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

#include "mozilla/Logging.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Attributes.h"

#ifdef MOZ_TASK_TRACER
#include "TracedTaskCommon.h"
#endif

extern PRLogModuleInfo* GetTimerLog();

#define NS_TIMER_CID \
{ /* 5ff24248-1dd2-11b2-8427-fbab44f29bc8 */         \
     0x5ff24248,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x84, 0x27, 0xfb, 0xab, 0x44, 0xf2, 0x9b, 0xc8} \
}

class nsTimerImpl final : public nsITimer
{
public:
  typedef mozilla::TimeStamp TimeStamp;

  nsTimerImpl();

  static nsresult Startup();
  static void Shutdown();

  friend class TimerThread;
  friend class nsTimerEvent;
  friend struct TimerAdditionComparator;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMER

  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  void SetDelayInternal(uint32_t aDelay);

  void Fire();

#ifdef MOZ_TASK_TRACER
  void GetTLSTraceInfo();
  mozilla::tasktracer::TracedTaskCommon GetTracedTask();
#endif

  // If a failure is encountered, the reference is returned to the caller
  static already_AddRefed<nsTimerImpl> PostTimerEvent(
    already_AddRefed<nsTimerImpl> aTimerRef);

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

  ~nsTimerImpl();
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

  bool IsRepeating() const
  {
    static_assert(TYPE_ONE_SHOT < TYPE_REPEATING_SLACK,
                  "invalid ordering of timer types!");
    static_assert(TYPE_REPEATING_SLACK < TYPE_REPEATING_PRECISE,
                  "invalid ordering of timer types!");
    static_assert(TYPE_REPEATING_PRECISE < TYPE_REPEATING_PRECISE_CAN_SKIP,
                  "invalid ordering of timer types!");
    return mType >= TYPE_REPEATING_SLACK;
  }

  bool IsRepeatingPrecisely() const
  {
    return mType >= TYPE_REPEATING_PRECISE;
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

  // Some callers expect to be able to access the callback while the
  // timer is firing.
  nsCOMPtr<nsITimerCallback> mTimerCallbackWhileFiring;

  // These members are set by Init and never reset.
  CallbackType          mCallbackType;

  // These members are set by the initiating thread, when the timer's type is
  // changed and during the period where it fires on that thread.
  uint8_t               mType;
  bool                  mFiring;


  // Use a bool (int) here to isolate loads and stores of these two members
  // done on various threads under the protection of TimerThread::mLock, from
  // loads and stores done on the initiating/type-changing/timer-firing thread
  // to the above uint8_t/bool members.
  bool                  mArmed;
  bool                  mCanceled;

  // The generation number of this timer, re-generated each time the timer is
  // initialized so one-shot timers can be canceled and re-initialized by the
  // arming thread without any bad race conditions.
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
};

#endif /* nsTimerImpl_h___ */
