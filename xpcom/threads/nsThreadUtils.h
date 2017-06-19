/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsThreadUtils_h__
#define nsThreadUtils_h__

#include "prthread.h"
#include "prinrval.h"
#include "MainThreadUtils.h"
#include "nsICancelableRunnable.h"
#include "nsIIdlePeriod.h"
#include "nsIIdleRunnable.h"
#include "nsINamed.h"
#include "nsIRunnable.h"
#include "nsIThreadManager.h"
#include "nsITimer.h"
#include "nsIThread.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Atomics.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Likely.h"
#include "mozilla/Move.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"

//-----------------------------------------------------------------------------
// These methods are alternatives to the methods on nsIThreadManager, provided
// for convenience.

/**
 * Create a new thread, and optionally provide an initial event for the thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 * @param aInitialEvent
 *   The initial event to run on this thread.  This parameter may be null.
 * @param aStackSize
 *   The size in bytes to reserve for the thread's stack.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   Indicates that the given name is not unique.
 */
extern nsresult
NS_NewThread(nsIThread** aResult,
             nsIRunnable* aInitialEvent = nullptr,
             uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE);

/**
 * Creates a named thread, otherwise the same as NS_NewThread
 */
extern nsresult
NS_NewNamedThread(const nsACString& aName,
                  nsIThread** aResult,
                  nsIRunnable* aInitialEvent = nullptr,
                  uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE);

template<size_t LEN>
inline nsresult
NS_NewNamedThread(const char (&aName)[LEN],
                  nsIThread** aResult,
                  nsIRunnable* aInitialEvent = nullptr,
                  uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE)
{
  static_assert(LEN <= 16,
                "Thread name must be no more than 16 characters");
  return NS_NewNamedThread(nsDependentCString(aName, LEN - 1),
                           aResult, aInitialEvent, aStackSize);
}

/**
 * Get a reference to the current thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 */
extern nsresult NS_GetCurrentThread(nsIThread** aResult);

/**
 * Dispatch the given event to the current thread.
 *
 * @param aEvent
 *   The event to dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern nsresult NS_DispatchToCurrentThread(nsIRunnable* aEvent);
extern nsresult
NS_DispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent);

/**
 * Dispatch the given event to the main thread.
 *
 * @param aEvent
 *   The event to dispatch.
 * @param aDispatchFlags
 *   The flags to pass to the main thread's dispatch method.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern nsresult
NS_DispatchToMainThread(nsIRunnable* aEvent,
                        uint32_t aDispatchFlags = NS_DISPATCH_NORMAL);
extern nsresult
NS_DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent,
                        uint32_t aDispatchFlags = NS_DISPATCH_NORMAL);

extern nsresult
NS_DelayedDispatchToCurrentThread(
  already_AddRefed<nsIRunnable>&& aEvent, uint32_t aDelayMs);

/**
 * Dispatch the given event to the idle queue of the current thread.
 *
 * @param aEvent
 *   The event to dispatch.
  *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 * @returns NS_ERROR_UNEXPECTED
 *   If the thread is shutting down.
 */
extern nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent);

/**
 * Dispatch the given event to the idle queue of the current thread.
 *
 * @param aEvent The event to dispatch. If the event implements
 *   nsIIdleRunnable, it will receive a call on
 *   nsIIdleRunnable::SetTimer when dispatched, with the value of
 *   aTimeout.
 *
 * @param aTimeout The time in milliseconds until the event should be
 *   moved from the idle queue to the regular queue, if it hasn't been
 *   executed. If aEvent is also an nsIIdleRunnable, it is expected
 *   that it should handle the timeout itself, after a call to
 *   nsIIdleRunnable::SetTimer.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 * @returns NS_ERROR_UNEXPECTED
 *   If the thread is shutting down.
 */
extern nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent, uint32_t aTimeout);

/**
 * Dispatch the given event to the idle queue of a thread.
 *
 * @param aEvent The event to dispatch.
 *
 * @param aThread The target thread for the dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 * @returns NS_ERROR_UNEXPECTED
 *   If the thread is shutting down.
 */
extern nsresult
NS_IdleDispatchToThread(already_AddRefed<nsIRunnable>&& aEvent,
                        nsIThread* aThread);

/**
 * Dispatch the given event to the idle queue of a thread.
 *
 * @param aEvent The event to dispatch. If the event implements
 *   nsIIdleRunnable, it will receive a call on
 *   nsIIdleRunnable::SetTimer when dispatched, with the value of
 *   aTimeout.
 *
 * @param aTimeout The time in milliseconds until the event should be
 *   moved from the idle queue to the regular queue, if it hasn't been
 *   executed. If aEvent is also an nsIIdleRunnable, it is expected
 *   that it should handle the timeout itself, after a call to
 *   nsIIdleRunnable::SetTimer.
 *
 * @param aThread The target thread for the dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 * @returns NS_ERROR_UNEXPECTED
 *   If the thread is shutting down.
 */
extern nsresult
NS_IdleDispatchToThread(already_AddRefed<nsIRunnable>&& aEvent,
                        uint32_t aTimeout,
                        nsIThread* aThread);

#ifndef XPCOM_GLUE_AVOID_NSPR
/**
 * Process all pending events for the given thread before returning.  This
 * method simply calls ProcessNextEvent on the thread while HasPendingEvents
 * continues to return true and the time spent in NS_ProcessPendingEvents
 * does not exceed the given timeout value.
 *
 * @param aThread
 *   The thread object for which to process pending events.  If null, then
 *   events will be processed for the current thread.
 * @param aTimeout
 *   The maximum number of milliseconds to spend processing pending events.
 *   Events are not pre-empted to honor this timeout.  Rather, the timeout
 *   value is simply used to determine whether or not to process another event.
 *   Pass PR_INTERVAL_NO_TIMEOUT to specify no timeout.
 */
extern nsresult
NS_ProcessPendingEvents(nsIThread* aThread,
                        PRIntervalTime aTimeout = PR_INTERVAL_NO_TIMEOUT);
#endif

/**
 * Shortcut for nsIThread::HasPendingEvents.
 *
 * It is an error to call this function when the given thread is not the
 * current thread.  This function will return false if called from some
 * other thread.
 *
 * @param aThread
 *   The current thread or null.
 *
 * @returns
 *   A boolean value that if "true" indicates that there are pending events
 *   in the current thread's event queue.
 */
extern bool NS_HasPendingEvents(nsIThread* aThread = nullptr);

/**
 * Shortcut for nsIThread::ProcessNextEvent.
 *
 * It is an error to call this function when the given thread is not the
 * current thread.  This function will simply return false if called
 * from some other thread.
 *
 * @param aThread
 *   The current thread or null.
 * @param aMayWait
 *   A boolean parameter that if "true" indicates that the method may block
 *   the calling thread to wait for a pending event.
 *
 * @returns
 *   A boolean value that if "true" indicates that an event from the current
 *   thread's event queue was processed.
 */
extern bool NS_ProcessNextEvent(nsIThread* aThread = nullptr,
                                bool aMayWait = true);

// A wrapper for nested event loops.
//
// This function is intended to make code more obvious (do you remember
// what NS_ProcessNextEvent(nullptr, true) means?) and slightly more
// efficient, as people often pass nullptr or NS_GetCurrentThread to
// NS_ProcessNextEvent, which results in needless querying of the current
// thread every time through the loop.
//
// You should use this function in preference to NS_ProcessNextEvent inside
// a loop unless one of the following is true:
//
// * You need to pass `false` to NS_ProcessNextEvent; or
// * You need to do unusual things around the call to NS_ProcessNextEvent,
//   such as unlocking mutexes that you are holding.
//
// If you *do* need to call NS_ProcessNextEvent manually, please do call
// NS_GetCurrentThread() outside of your loop and pass the returned pointer
// into NS_ProcessNextEvent for a tiny efficiency win.
namespace mozilla {

// You should normally not need to deal with this template parameter.  If
// you enjoy esoteric event loop details, read on.
//
// If you specify that NS_ProcessNextEvent wait for an event, it is possible
// for NS_ProcessNextEvent to return false, i.e. to indicate that an event
// was not processed.  This can only happen when the thread has been shut
// down by another thread, but is still attempting to process events outside
// of a nested event loop.
//
// This behavior is admittedly strange.  The scenario it deals with is the
// following:
//
// * The current thread has been shut down by some owner thread.
// * The current thread is spinning an event loop waiting for some condition
//   to become true.
// * Said condition is actually being fulfilled by another thread, so there
//   are timing issues in play.
//
// Thus, there is a small window where the current thread's event loop
// spinning can check the condition, find it false, and call
// NS_ProcessNextEvent to wait for another event.  But we don't actually
// want it to wait indefinitely, because there might not be any other events
// in the event loop, and the current thread can't accept dispatched events
// because it's being shut down.  Thus, actually blocking would hang the
// thread, which is bad.  The solution, then, is to detect such a scenario
// and not actually block inside NS_ProcessNextEvent.
//
// But this is a problem, because we want to return the status of
// NS_ProcessNextEvent to the caller of SpinEventLoopUntil if possible.  In
// the above scenario, however, we'd stop spinning prematurely and cause
// all sorts of havoc.  We therefore have this template parameter to
// control whether errors are ignored or passed out to the caller of
// SpinEventLoopUntil.  The latter is the default; if you find yourself
// wanting to use the former, you should think long and hard before doing
// so, and write a comment like this defending your choice.

enum class ProcessFailureBehavior {
  IgnoreAndContinue,
  ReportToCaller,
};

template<ProcessFailureBehavior Behavior = ProcessFailureBehavior::ReportToCaller,
         typename Pred>
bool
SpinEventLoopUntil(Pred&& aPredicate, nsIThread* aThread = nullptr)
{
  nsIThread* thread = aThread ? aThread : NS_GetCurrentThread();

  while (!aPredicate()) {
    bool didSomething = NS_ProcessNextEvent(thread, true);

    if (Behavior == ProcessFailureBehavior::IgnoreAndContinue) {
      // Don't care what happened, continue on.
      continue;
    } else if (!didSomething) {
      return false;
    }
  }

  return true;
}

} // namespace mozilla

/**
 * Returns true if we're in the compositor thread.
 *
 * We declare this here because the headers required to invoke
 * CompositorThreadHolder::IsInCompositorThread() also pull in a bunch of system
 * headers that #define various tokens in a way that can break the build.
 */
extern bool NS_IsInCompositorThread();

//-----------------------------------------------------------------------------
// Helpers that work with nsCOMPtr:

inline already_AddRefed<nsIThread>
do_GetCurrentThread()
{
  nsIThread* thread = nullptr;
  NS_GetCurrentThread(&thread);
  return already_AddRefed<nsIThread>(thread);
}

inline already_AddRefed<nsIThread>
do_GetMainThread()
{
  nsIThread* thread = nullptr;
  NS_GetMainThread(&thread);
  return already_AddRefed<nsIThread>(thread);
}

//-----------------------------------------------------------------------------

#ifdef MOZILLA_INTERNAL_API
// Fast access to the current thread.  Do not release the returned pointer!  If
// you want to use this pointer from some other thread, then you will need to
// AddRef it.  Otherwise, you should only consider this pointer valid from code
// running on the current thread.
extern nsIThread* NS_GetCurrentThread();

/**
 * Set the name of the current thread. Prefer this function over
 * PR_SetCurrentThreadName() if possible. The name will also be included in the
 * crash report.
 *
 * @param aName
 *   Name of the thread. A C language null-terminated string.
 */
extern void NS_SetCurrentThreadName(const char* aName);
#endif

//-----------------------------------------------------------------------------

#ifndef XPCOM_GLUE_AVOID_NSPR

namespace mozilla {

// This class is designed to be subclassed.
class IdlePeriod : public nsIIdlePeriod
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIDLEPERIOD

  IdlePeriod() {}

protected:
  virtual ~IdlePeriod() {}
private:
  IdlePeriod(const IdlePeriod&) = delete;
  IdlePeriod& operator=(const IdlePeriod&) = delete;
  IdlePeriod& operator=(const IdlePeriod&&) = delete;
};

// Cancelable runnable methods implement nsICancelableRunnable, and
// Idle and IdleWithTimer also nsIIdleRunnable.
enum RunnableKind
{
  Standard,
  Cancelable,
  Idle,
  IdleWithTimer
};

// This class is designed to be subclassed.
class Runnable : public nsIRunnable, public nsINamed
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSINAMED

  Runnable() {}

#ifdef RELEASE_OR_BETA
  explicit Runnable(const char* aName) {}
#else
  explicit Runnable(const char* aName) : mName(aName) {}
#endif

protected:
  virtual ~Runnable() {}

#ifndef RELEASE_OR_BETA
  const char* mName = nullptr;
#endif

private:
  Runnable(const Runnable&) = delete;
  Runnable& operator=(const Runnable&) = delete;
  Runnable& operator=(const Runnable&&) = delete;
};

// This class is designed to be subclassed.
class CancelableRunnable : public Runnable,
                           public nsICancelableRunnable
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  // nsICancelableRunnable
  virtual nsresult Cancel() override;

  CancelableRunnable() {}
  explicit CancelableRunnable(const char* aName) : Runnable(aName) {}

protected:
  virtual ~CancelableRunnable() {}
private:
  CancelableRunnable(const CancelableRunnable&) = delete;
  CancelableRunnable& operator=(const CancelableRunnable&) = delete;
  CancelableRunnable& operator=(const CancelableRunnable&&) = delete;
};

// This class is designed to be subclassed.
class IdleRunnable : public CancelableRunnable,
                     public nsIIdleRunnable
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  IdleRunnable() {}
  explicit IdleRunnable(const char* aName) : CancelableRunnable(aName) {}

protected:
  virtual ~IdleRunnable() {}
private:
  IdleRunnable(const IdleRunnable&) = delete;
  IdleRunnable& operator=(const IdleRunnable&) = delete;
  IdleRunnable& operator=(const IdleRunnable&&) = delete;
};

namespace detail {

// An event that can be used to call a C++11 functions or function objects,
// including lambdas. The function must have no required arguments, and must
// return void.
template<typename StoredFunction>
class RunnableFunction : public Runnable
{
public:
  template <typename F>
  explicit RunnableFunction(F&& aFunction)
    : mFunction(Forward<F>(aFunction))
  { }

  NS_IMETHOD Run() override {
    static_assert(IsVoid<decltype(mFunction())>::value,
                  "The lambda must return void!");
    mFunction();
    return NS_OK;
  }
private:
  StoredFunction mFunction;
};

// Type alias for NS_NewRunnableFunction
template<typename Function>
using RunnableFunctionImpl =
  // Make sure we store a non-reference in nsRunnableFunction.
  typename detail::RunnableFunction<typename RemoveReference<Function>::Type>;

template <typename T>
inline already_AddRefed<T>
SetRunnableName(already_AddRefed<T>&& aObj, const char* aName)
{
  MOZ_RELEASE_ASSERT(aName);
  RefPtr<T> ref(aObj);
  ref->SetName(aName);
  return ref.forget();
}

} // namespace detail

namespace detail {

template<typename CVRemoved>
struct IsRefcountedSmartPointerHelper : FalseType {};

template<typename Pointee>
struct IsRefcountedSmartPointerHelper<RefPtr<Pointee>> : TrueType {};

template<typename Pointee>
struct IsRefcountedSmartPointerHelper<nsCOMPtr<Pointee>> : TrueType {};

} // namespace detail

template<typename T>
struct IsRefcountedSmartPointer
  : detail::IsRefcountedSmartPointerHelper<typename RemoveCV<T>::Type>
{};

namespace detail {

template<typename T, typename CVRemoved>
struct RemoveSmartPointerHelper
{
  typedef T Type;
};

template<typename T, typename Pointee>
struct RemoveSmartPointerHelper<T, RefPtr<Pointee>>
{
  typedef Pointee Type;
};

template<typename T, typename Pointee>
struct RemoveSmartPointerHelper<T, nsCOMPtr<Pointee>>
{
  typedef Pointee Type;
};

} // namespace detail

template<typename T>
struct RemoveSmartPointer
  : detail::RemoveSmartPointerHelper<T, typename RemoveCV<T>::Type>
{};

namespace detail {

template<typename T, typename CVRemoved>
struct RemoveRawOrSmartPointerHelper
{
  typedef T Type;
};

template<typename T, typename Pointee>
struct RemoveRawOrSmartPointerHelper<T, Pointee*>
{
  typedef Pointee Type;
};

template<typename T, typename Pointee>
struct RemoveRawOrSmartPointerHelper<T, RefPtr<Pointee>>
{
  typedef Pointee Type;
};

template<typename T, typename Pointee>
struct RemoveRawOrSmartPointerHelper<T, nsCOMPtr<Pointee>>
{
  typedef Pointee Type;
};

} // namespace detail

template<typename T>
struct RemoveRawOrSmartPointer
  : detail::RemoveRawOrSmartPointerHelper<T, typename RemoveCV<T>::Type>
{};

} // namespace mozilla

inline nsISupports*
ToSupports(mozilla::Runnable *p)
{
  return static_cast<nsIRunnable*>(p);
}

template<typename Function>
already_AddRefed<mozilla::Runnable>
NS_NewRunnableFunction(Function&& aFunction)
{
  // We store a non-reference in RunnableFunction, but still forward aFunction
  // to move if possible.
  return do_AddRef(new mozilla::detail::RunnableFunctionImpl<Function>
    (mozilla::Forward<Function>(aFunction)));
}

template<typename Function>
already_AddRefed<mozilla::Runnable>
NS_NewRunnableFunction(const char* aName, Function&& aFunction)
{
  return mozilla::detail::SetRunnableName(
    NS_NewRunnableFunction(mozilla::Forward<Function>(aFunction)), aName);
}

namespace mozilla {
namespace detail {

already_AddRefed<nsITimer> CreateTimer();

template <RunnableKind Kind>
class TimerBehaviour
{
public:
  nsITimer* GetTimer() { return nullptr; }
  void CancelTimer() {}

protected:
  ~TimerBehaviour() {}
};

template <>
class TimerBehaviour<IdleWithTimer>
{
public:
  nsITimer* GetTimer()
  {
    if (!mTimer) {
      mTimer = CreateTimer();
    }

    return mTimer;
  }

  void CancelTimer()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

protected:
  ~TimerBehaviour()
  {
    CancelTimer();
  }
private:
  nsCOMPtr<nsITimer> mTimer;
};

} // namespace detail
} // namespace mozilla

// An event that can be used to call a method on a class.  The class type must
// support reference counting. This event supports Revoke for use
// with nsRevocableEventPtr.
template<class ClassType,
         typename ReturnType = void,
         bool Owning = true,
         mozilla::RunnableKind Kind = mozilla::Standard>
class nsRunnableMethod
  : public mozilla::Conditional<Kind == mozilla::Standard,
                                mozilla::Runnable,
                                typename mozilla::Conditional<
                                  Kind == mozilla::Cancelable,
                                  mozilla::CancelableRunnable,
                                  mozilla::IdleRunnable>::Type>::Type,
    protected mozilla::detail::TimerBehaviour<Kind>
{
public:
  virtual void Revoke() = 0;

  // These ReturnTypeEnforcer classes set up a blacklist for return types that
  // we know are not safe. The default ReturnTypeEnforcer compiles just fine but
  // already_AddRefed will not.
  template<typename OtherReturnType>
  class ReturnTypeEnforcer
  {
  public:
    typedef int ReturnTypeIsSafe;
  };

  template<class T>
  class ReturnTypeEnforcer<already_AddRefed<T>>
  {
    // No ReturnTypeIsSafe makes this illegal!
  };

  // Make sure this return type is safe.
  typedef typename ReturnTypeEnforcer<ReturnType>::ReturnTypeIsSafe check;
};

template<class ClassType, bool Owning>
struct nsRunnableMethodReceiver
{
  RefPtr<ClassType> mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  ~nsRunnableMethodReceiver() { Revoke(); }
  ClassType* Get() const { return mObj.get(); }
  void Revoke() { mObj = nullptr; }
};

template<class ClassType>
struct nsRunnableMethodReceiver<ClassType, false>
{
  ClassType* MOZ_NON_OWNING_REF mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  ClassType* Get() const { return mObj; }
  void Revoke() { mObj = nullptr; }
};

static inline constexpr bool
IsIdle(mozilla::RunnableKind aKind)
{
  return aKind == mozilla::Idle || aKind == mozilla::IdleWithTimer;
}

template<typename PtrType, typename Method, bool Owning, mozilla::RunnableKind Kind>
struct nsRunnableMethodTraits;

template<typename PtrType, class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<PtrType, R(C::*)(As...), Owning, Kind>
{
  typedef typename mozilla::RemoveRawOrSmartPointer<PtrType>::Type class_type;
  static_assert(mozilla::IsBaseOf<C, class_type>::value,
                "Stored class must inherit from method's class");
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

template<typename PtrType, class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<PtrType, R(C::*)(As...) const, Owning, Kind>
{
  typedef const typename mozilla::RemoveRawOrSmartPointer<PtrType>::Type class_type;
  static_assert(mozilla::IsBaseOf<C, class_type>::value,
                "Stored class must inherit from method's class");
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

#ifdef NS_HAVE_STDCALL
template<typename PtrType, class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<PtrType, R(__stdcall C::*)(As...), Owning, Kind>
{
  typedef typename mozilla::RemoveRawOrSmartPointer<PtrType>::Type class_type;
  static_assert(mozilla::IsBaseOf<C, class_type>::value,
                "Stored class must inherit from method's class");
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

template<typename PtrType, class C, typename R, bool Owning, mozilla::RunnableKind Kind>
struct nsRunnableMethodTraits<PtrType, R(NS_STDCALL C::*)(), Owning, Kind>
{
  typedef typename mozilla::RemoveRawOrSmartPointer<PtrType>::Type class_type;
  static_assert(mozilla::IsBaseOf<C, class_type>::value,
                "Stored class must inherit from method's class");
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

template<typename PtrType, class C, typename R, bool Owning, mozilla::RunnableKind Kind, typename... As>
struct nsRunnableMethodTraits<PtrType, R(__stdcall C::*)(As...) const, Owning, Kind>
{
  typedef const typename mozilla::RemoveRawOrSmartPointer<PtrType>::Type class_type;
  static_assert(mozilla::IsBaseOf<C, class_type>::value,
                "Stored class must inherit from method's class");
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};

template<typename PtrType, class C, typename R, bool Owning, mozilla::RunnableKind Kind>
struct nsRunnableMethodTraits<PtrType, R(NS_STDCALL C::*)() const, Owning, Kind>
{
  typedef const typename mozilla::RemoveRawOrSmartPointer<PtrType>::Type class_type;
  static_assert(mozilla::IsBaseOf<C, class_type>::value,
                "Stored class must inherit from method's class");
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Kind> base_type;
  static const bool can_cancel = Kind == mozilla::Cancelable;
};
#endif


// IsParameterStorageClass<T>::value is true if T is a parameter-storage class
// that will be recognized by NS_New[NonOwning]RunnableMethodWithArg[s] to
// force a specific storage&passing strategy (instead of inferring one,
// see ParameterStorage).
// When creating a new storage class, add a specialization for it to be
// recognized.
template<typename T>
struct IsParameterStorageClass : public mozilla::FalseType {};

// StoreXPassByY structs used to inform nsRunnableMethodArguments how to
// store arguments, and how to pass them to the target method.

template<typename T>
struct StoreCopyPassByValue
{
  typedef T stored_type;
  typedef T passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByValue(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByValue<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByConstLRef
{
  typedef T stored_type;
  typedef const T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByConstLRef(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByConstLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByLRef
{
  typedef T stored_type;
  typedef T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByLRef(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByRRef
{
  typedef T stored_type;
  typedef T&& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByRRef(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return mozilla::Move(m); }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByRRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreRefPassByLRef
{
  typedef T& stored_type;
  typedef T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreRefPassByLRef(A& a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreRefPassByLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreConstRefPassByConstLRef
{
  typedef const T& stored_type;
  typedef const T& passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreConstRefPassByConstLRef(const A& a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreConstRefPassByConstLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreRefPtrPassByPtr
{
  typedef RefPtr<T> stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreRefPtrPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m.get(); }
};
template<typename S>
struct IsParameterStorageClass<StoreRefPtrPassByPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StorePtrPassByPtr
{
  typedef T* stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StorePtrPassByPtr(A a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StorePtrPassByPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreConstPtrPassByConstPtr
{
  typedef const T* stored_type;
  typedef const T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreConstPtrPassByConstPtr(A a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreConstPtrPassByConstPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByConstPtr
{
  typedef T stored_type;
  typedef const T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByConstPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return &m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByConstPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StoreCopyPassByPtr
{
  typedef T stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StoreCopyPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return &m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByPtr<S>>
  : public mozilla::TrueType {};

namespace detail {

template<typename>
struct SFINAE1True : mozilla::TrueType
{};

template<class T>
static auto HasRefCountMethodsTest(int)
    -> SFINAE1True<decltype(mozilla::DeclVal<T>().AddRef(),
                            mozilla::DeclVal<T>().Release())>;
template<class>
static auto HasRefCountMethodsTest(long) -> mozilla::FalseType;

template<class T>
struct HasRefCountMethods : decltype(HasRefCountMethodsTest<T>(0))
{};

template<typename TWithoutPointer>
struct NonnsISupportsPointerStorageClass
  : mozilla::Conditional<mozilla::IsConst<TWithoutPointer>::value,
                         StoreConstPtrPassByConstPtr<
                           typename mozilla::RemoveConst<TWithoutPointer>::Type>,
                         StorePtrPassByPtr<TWithoutPointer>>
{};

template<typename TWithoutPointer>
struct PointerStorageClass
  : mozilla::Conditional<HasRefCountMethods<TWithoutPointer>::value,
                         StoreRefPtrPassByPtr<TWithoutPointer>,
                         typename NonnsISupportsPointerStorageClass<
                           TWithoutPointer
                         >::Type>
{};

template<typename TWithoutRef>
struct LValueReferenceStorageClass
  : mozilla::Conditional<mozilla::IsConst<TWithoutRef>::value,
                         StoreConstRefPassByConstLRef<
                           typename mozilla::RemoveConst<TWithoutRef>::Type>,
                         StoreRefPassByLRef<TWithoutRef>>
{};

template<typename T>
struct SmartPointerStorageClass
  : mozilla::Conditional<mozilla::IsRefcountedSmartPointer<T>::value,
                         StoreRefPtrPassByPtr<
                           typename mozilla::RemoveSmartPointer<T>::Type>,
                         StoreCopyPassByConstLRef<T>>
{};

template<typename T>
struct NonLValueReferenceStorageClass
  : mozilla::Conditional<mozilla::IsRvalueReference<T>::value,
                         StoreCopyPassByRRef<
                           typename mozilla::RemoveReference<T>::Type>,
                         typename SmartPointerStorageClass<T>::Type>
{};

template<typename T>
struct NonPointerStorageClass
  : mozilla::Conditional<mozilla::IsLvalueReference<T>::value,
                         typename LValueReferenceStorageClass<
                           typename mozilla::RemoveReference<T>::Type
                         >::Type,
                         typename NonLValueReferenceStorageClass<T>::Type>
{};

template<typename T>
struct NonParameterStorageClass
  : mozilla::Conditional<mozilla::IsPointer<T>::value,
                         typename PointerStorageClass<
                           typename mozilla::RemovePointer<T>::Type
                         >::Type,
                         typename NonPointerStorageClass<T>::Type>
{};

// Choose storage&passing strategy based on preferred storage type:
// - If IsParameterStorageClass<T>::value is true, use as-is.
// - RC*       -> StoreRefPtrPassByPtr<RC>       : Store RefPtr<RC>, pass RC*
//   ^^ RC quacks like a ref-counted type (i.e., has AddRef and Release methods)
// - const T*  -> StoreConstPtrPassByConstPtr<T> : Store const T*, pass const T*
// - T*        -> StorePtrPassByPtr<T>           : Store T*, pass T*.
// - const T&  -> StoreConstRefPassByConstLRef<T>: Store const T&, pass const T&.
// - T&        -> StoreRefPassByLRef<T>          : Store T&, pass T&.
// - T&&       -> StoreCopyPassByRRef<T>         : Store T, pass Move(T).
// - RefPtr<T>, nsCOMPtr<T>
//             -> StoreRefPtrPassByPtr<T>        : Store RefPtr<T>, pass T*
// - Other T   -> StoreCopyPassByConstLRef<T>    : Store T, pass const T&.
// Other available explicit options:
// -              StoreCopyPassByValue<T>        : Store T, pass T.
// -              StoreCopyPassByLRef<T>         : Store T, pass T& (of copy!)
// -              StoreCopyPassByConstPtr<T>     : Store T, pass const T*
// -              StoreCopyPassByPtr<T>          : Store T, pass T* (of copy!)
// Or create your own class with PassAsParameter() method, optional
// clean-up in destructor, and with associated IsParameterStorageClass<>.
template<typename T>
struct ParameterStorage
  : mozilla::Conditional<IsParameterStorageClass<T>::value,
                         T,
                         typename NonParameterStorageClass<T>::Type>
{};

template<class T>
static auto
HasSetDeadlineTest(int) -> SFINAE1True<decltype(
  mozilla::DeclVal<T>().SetDeadline(mozilla::DeclVal<mozilla::TimeStamp>()))>;

template<class T>
static auto
HasSetDeadlineTest(long) -> mozilla::FalseType;

template<class T>
struct HasSetDeadline : decltype(HasSetDeadlineTest<T>(0))
{};

template <class T>
typename mozilla::EnableIf<::detail::HasSetDeadline<T>::value>::Type
SetDeadlineImpl(T* aObj, mozilla::TimeStamp aTimeStamp)
{
  aObj->SetDeadline(aTimeStamp);
}

template <class T>
typename mozilla::EnableIf<!::detail::HasSetDeadline<T>::value>::Type
SetDeadlineImpl(T* aObj, mozilla::TimeStamp aTimeStamp)
{
}
} /* namespace detail */

namespace mozilla {
namespace detail {

// struct used to store arguments and later apply them to a method.
template <typename... Ts>
struct RunnableMethodArguments final
{
  Tuple<typename ::detail::ParameterStorage<Ts>::Type...> mArguments;
  template <typename... As>
  explicit RunnableMethodArguments(As&&... aArguments)
    : mArguments(Forward<As>(aArguments)...)
  {}
  template<typename C, typename M, typename... Args, size_t... Indices>
  static auto
  applyImpl(C* o, M m, Tuple<Args...>& args, IndexSequence<Indices...>)
      -> decltype(((*o).*m)(Get<Indices>(args).PassAsParameter()...))
  {
    return ((*o).*m)(Get<Indices>(args).PassAsParameter()...);
  }
  template<class C, typename M> auto apply(C* o, M m)
      -> decltype(applyImpl(o, m, mArguments,
                  typename IndexSequenceFor<Ts...>::Type()))
  {
    return applyImpl(o, m, mArguments,
        typename IndexSequenceFor<Ts...>::Type());
  }
};

template<typename PtrType, typename Method, bool Owning, RunnableKind Kind, typename... Storages>
class RunnableMethodImpl final
  : public ::nsRunnableMethodTraits<PtrType, Method, Owning, Kind>::base_type
{
  typedef typename ::nsRunnableMethodTraits<PtrType, Method, Owning, Kind> Traits;

  typedef typename Traits::class_type ClassType;
  typedef typename Traits::base_type BaseType;
  ::nsRunnableMethodReceiver<ClassType, Owning> mReceiver;
  Method mMethod;
  RunnableMethodArguments<Storages...> mArgs;
  using BaseType::GetTimer;
  using BaseType::CancelTimer;
private:
  virtual ~RunnableMethodImpl() { Revoke(); };
  static void TimedOut(nsITimer* aTimer, void* aClosure)
  {
    static_assert(IsIdle(Kind), "Don't use me!");
    RefPtr<IdleRunnable> r = static_cast<IdleRunnable*>(aClosure);
    r->SetDeadline(TimeStamp());
    r->Run();
    r->Cancel();
  }
public:
  template<typename ForwardedPtrType, typename... Args>
  explicit RunnableMethodImpl(ForwardedPtrType&& aObj, Method aMethod,
                              Args&&... aArgs)
    : mReceiver(Forward<ForwardedPtrType>(aObj))
    , mMethod(aMethod)
    , mArgs(Forward<Args>(aArgs)...)
  {
    static_assert(sizeof...(Storages) == sizeof...(Args), "Storages and Args should have equal sizes");
  }
  NS_IMETHOD Run()
  {
    CancelTimer();

    if (MOZ_LIKELY(mReceiver.Get())) {
      mArgs.apply(mReceiver.Get(), mMethod);
    }

    return NS_OK;
  }

  nsresult Cancel()
  {
    static_assert(Kind >= Cancelable, "Don't use me!");
    Revoke();
    return NS_OK;
  }

  void Revoke()
  {
    CancelTimer();
    mReceiver.Revoke();
  }

  void SetDeadline(TimeStamp aDeadline)
  {
    if (MOZ_LIKELY(mReceiver.Get())) {
      ::detail::SetDeadlineImpl(mReceiver.Get(), aDeadline);
    }
  }

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget)
  {
    MOZ_ASSERT(aTarget);

    if (nsCOMPtr<nsITimer> timer = GetTimer()) {
      timer->Cancel();
      timer->SetTarget(aTarget);
      timer->InitWithFuncCallback(TimedOut, this, aDelay,
                                  nsITimer::TYPE_ONE_SHOT);
    }
  }
};

// Type aliases for NewRunnableMethod.
template<typename PtrType, typename Method>
using OwningRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, Standard>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using OwningRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, Standard, Storages...>;

// Type aliases for NewCancelableRunnableMethod.
template<typename PtrType, typename Method>
using CancelableRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, Cancelable>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using CancelableRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, Cancelable, Storages...>;

// Type aliases for NewIdleRunnableMethod.
template<typename PtrType, typename Method>
using IdleRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, Idle>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using IdleRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, Idle, Storages...>;

// Type aliases for NewIdleRunnableMethodWithTimer.
template<typename PtrType, typename Method>
using IdleRunnableMethodWithTimer = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, true, IdleWithTimer>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using IdleRunnableMethodWithTimerImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, true, IdleWithTimer, Storages...>;

// Type aliases for NewNonOwningRunnableMethod.
template<typename PtrType, typename Method>
using NonOwningRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, Standard>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, Standard, Storages...>;

// Type aliases for NonOwningCancelableRunnableMethod
template<typename PtrType, typename Method>
using NonOwningCancelableRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, Cancelable>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningCancelableRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, Cancelable, Storages...>;

// Type aliases for NonOwningIdleRunnableMethod
template<typename PtrType, typename Method>
using NonOwningIdleRunnableMethod = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, Idle>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningIdleRunnableMethodImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, Idle, Storages...>;

// Type aliases for NewIdleRunnableMethodWithTimer.
template<typename PtrType, typename Method>
using NonOwningIdleRunnableMethodWithTimer = typename ::nsRunnableMethodTraits<
  typename RemoveReference<PtrType>::Type, Method, false, IdleWithTimer>::base_type;
template<typename PtrType, typename Method, typename... Storages>
using NonOwningIdleRunnableMethodWithTimerImpl = RunnableMethodImpl<
  typename RemoveReference<PtrType>::Type, Method, false, IdleWithTimer, Storages...>;

} // namespace detail

// NewRunnableMethod and friends
//
// Very often in Gecko, you'll find yourself in a situation where you want
// to invoke a method (with or without arguments) asynchronously.  You
// could write a small helper class inheriting from nsRunnable to handle
// all these details, or you could let NewRunnableMethod take care of all
// those details for you.
//
// The simplest use of NewRunnableMethod looks like:
//
//   nsCOMPtr<nsIRunnable> event =
//     mozilla::NewRunnableMethod("description", myObject, &MyClass::HandleEvent);
//   NS_DispatchToCurrentThread(event);
//
// Statically enforced constraints:
//  - myObject must be of (or implicitly convertible to) type MyClass
//  - MyClass must define AddRef and Release methods
//
// The "description" string should specify a human-readable name for the
// runnable; the provided string is used by various introspection tools
// in the browser.
//
// The created runnable will take a strong reference to `myObject`.  For
// non-refcounted objects, or refcounted objects with unusual refcounting
// requirements, and if and only if you are 110% certain that `myObject`
// will live long enough, you can use NewNonOwningRunnableMethod instead,
// which will, as its name implies, take a non-owning reference.  If you
// find yourself having to use this function, you should accompany your use
// with a proof comment describing why the runnable will not lead to
// use-after-frees.
//
// (If you find yourself writing contorted code to Release() an object
// asynchronously on a different thread, you should use the
// NS_ProxyRelease function.)
//
// Invoking a method with arguments takes a little more care.  The
// natural extension of the above:
//
//   nsCOMPtr<nsIRunnable> event =
//     mozilla::NewRunnableMethod("description", myObject, &MyClass::HandleEvent,
//                                arg1, arg2, ...);
//
// can lead to security hazards (e.g. passing in raw pointers to refcounted
// objects and storing those raw pointers in the runnable).  We therefore
// require you to specify the storage types used by the runnable, just as
// you would if you were writing out the class by hand:
//
//   nsCOMPtr<nsIRunnable> event =
//     mozilla::NewRunnableMethod<RefPtr<T>, nsTArray<U>>
//         ("description", myObject, &MyClass::HandleEvent, arg1, arg2);
//
// Please note that you do not have to pass the same argument type as you
// specify in the template arguments.  For example, if you want to transfer
// ownership to a runnable, you can write:
//
//   RefPtr<T> ptr = ...;
//   nsTArray<U> array = ...;
//   nsCOMPtr<nsIRunnable> event =
//     mozilla::NewRunnableMethod<RefPtr<T>, nsTArray<U>>
//         ("description", myObject, &MyClass::DoSomething,
//          Move(ptr), Move(array));
//
// and there will be no extra AddRef/Release traffic, or copying of the array.
//
// Each type that you specify as a template argument to NewRunnableMethod
// comes with its own style of storage in the runnable and its own style
// of argument passing to the invoked method.  See the comment for
// ParameterStorage above for more details.
//
// If you need to customize the storage type and/or argument passing type,
// you can write your own class to use as a template argument to
// NewRunnableMethod.  If you find yourself having to do that frequently,
// please file a bug in Core::XPCOM about adding the custom type to the
// core code in this file, and/or for custom rules for ParameterStorage
// to select that strategy.
//
// For places that require you to use cancelable runnables, such as
// workers, there's also NewCancelableRunnableMethod and its non-owning
// counterpart.  The runnables returned by these methods additionally
// implement nsICancelableRunnable.
//
// Finally, all of the functions discussed above have additional overloads
// that do not take a `const char*` as their first parameter; you may see
// these in older code.  The `const char*` overload is preferred and
// should be used in new code exclusively.

template<typename PtrType, typename Method>
already_AddRefed<detail::OwningRunnableMethod<PtrType, Method>>
NewRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(
    new detail::OwningRunnableMethodImpl<PtrType, Method>
      (Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::OwningRunnableMethod<PtrType, Method>>
NewRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod)
{
  return detail::SetRunnableName(
    NewRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::CancelableRunnableMethod<PtrType, Method>>
NewCancelableRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(
    new detail::CancelableRunnableMethodImpl<PtrType, Method>
      (Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::CancelableRunnableMethod<PtrType, Method>>
NewCancelableRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod)
{
  return detail::SetRunnableName(
    NewCancelableRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::IdleRunnableMethodImpl<PtrType, Method>(
    Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod)
{
  return detail::SetRunnableName(
    NewIdleRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethodWithTimer<PtrType, Method>>
NewIdleRunnableMethodWithTimer(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::IdleRunnableMethodWithTimerImpl<PtrType, Method>(
    Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::IdleRunnableMethodWithTimer<PtrType, Method>>
NewIdleRunnableMethodWithTimer(const char* aName,
                               PtrType&& aPtr,
                               Method aMethod)
{
  return detail::SetRunnableName(
    NewIdleRunnableMethodWithTimer(Forward<PtrType>(aPtr), aMethod),
    aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningRunnableMethod<PtrType, Method>>
NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(
    new detail::NonOwningRunnableMethodImpl<PtrType, Method>
      (Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningRunnableMethod<PtrType, Method>>
NewNonOwningRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod)
{
  return detail::SetRunnableName(
    NewNonOwningRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningCancelableRunnableMethod<PtrType, Method>>
NewNonOwningCancelableRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(
    new detail::NonOwningCancelableRunnableMethodImpl<PtrType, Method>
      (Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningCancelableRunnableMethod<PtrType, Method>>
NewNonOwningCancelableRunnableMethod(const char* aName, PtrType&& aPtr,
                                     Method aMethod)
{
  return detail::SetRunnableName(
    NewNonOwningCancelableRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(
    new detail::NonOwningIdleRunnableMethodImpl<PtrType, Method>(
      Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(const char* aName,
                               PtrType&& aPtr,
                               Method aMethod)
{
  return detail::SetRunnableName(
    NewNonOwningIdleRunnableMethod(Forward<PtrType>(aPtr), aMethod), aName);
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethodWithTimer<PtrType, Method>>
NewNonOwningIdleRunnableMethodWithTimer(PtrType&& aPtr,
                                        Method aMethod)
{
  return do_AddRef(
      new detail::NonOwningIdleRunnableMethodWithTimerImpl<PtrType, Method>(
        Forward<PtrType>(aPtr), aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<detail::NonOwningIdleRunnableMethodWithTimer<PtrType, Method>>
NewNonOwningIdleRunnableMethodWithTimer(const char* aName,
                                        PtrType&& aPtr,
                                        Method aMethod)
{
  return detail::SetRunnableName(NewNonOwningIdleRunnableMethodWithTimer(
                                   Forward<PtrType>(aPtr), aMethod),
                                 aName);
}

// Similar to NewRunnableMethod. Call like so:
// nsCOMPtr<nsIRunnable> event =
//   NewRunnableMethod<Types,...>(myObject, &MyClass::HandleEvent, myArg1,...);
// 'Types' are the stored type for each argument, see ParameterStorage for details.
template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::OwningRunnableMethod<PtrType, Method>>
NewRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::OwningRunnableMethodImpl<PtrType, Method, Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::OwningRunnableMethod<PtrType, Method>>
NewRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewRunnableMethod<Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...), aName);
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::NonOwningRunnableMethod<PtrType, Method>>
NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
      new detail::NonOwningRunnableMethodImpl<PtrType, Method, Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::NonOwningRunnableMethod<PtrType, Method>>
NewNonOwningRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod,
                           Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewNonOwningRunnableMethod<Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...), aName);
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::CancelableRunnableMethod<PtrType, Method>>
NewCancelableRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::CancelableRunnableMethodImpl<PtrType, Method, Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::CancelableRunnableMethod<PtrType, Method>>
NewCancelableRunnableMethod(const char* aName, PtrType&& aPtr, Method aMethod,
                            Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewCancelableRunnableMethod<Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...), aName);
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::NonOwningCancelableRunnableMethod<PtrType, Method>>
NewNonOwningCancelableRunnableMethod(PtrType&& aPtr, Method aMethod,
                                     Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::NonOwningCancelableRunnableMethodImpl<PtrType, Method, Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename PtrType, typename Method, typename... Args>
already_AddRefed<detail::NonOwningCancelableRunnableMethod<PtrType, Method>>
NewNonOwningCancelableRunnableMethod(const char* aName, PtrType&& aPtr,
                                     Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewNonOwningCancelableRunnableMethod<Storages...>
      (Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...), aName);
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::IdleRunnableMethodImpl<PtrType, Method, Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::IdleRunnableMethod<PtrType, Method>>
NewIdleRunnableMethod(const char* aName,
                      PtrType&& aPtr,
                      Method aMethod,
                      Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewIdleRunnableMethod<Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...),
    aName);
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(
    new detail::NonOwningIdleRunnableMethodImpl<PtrType, Method, Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages,
         typename PtrType,
         typename Method,
         typename... Args>
already_AddRefed<detail::NonOwningIdleRunnableMethod<PtrType, Method>>
NewNonOwningIdleRunnableMethod(const char* aName,
                               PtrType&& aPtr,
                               Method aMethod,
                               Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return detail::SetRunnableName(
    NewNonOwningIdleRunnableMethod<Storages...>(
      Forward<PtrType>(aPtr), aMethod, mozilla::Forward<Args>(aArgs)...),
    aName);
}

} // namespace mozilla

#endif  // XPCOM_GLUE_AVOID_NSPR

// This class is designed to be used when you have an event class E that has a
// pointer back to resource class R.  If R goes away while E is still pending,
// then it is important to "revoke" E so that it does not try use R after R has
// been destroyed.  nsRevocableEventPtr makes it easy for R to manage such
// situations:
//
//   class R;
//
//   class E : public mozilla::Runnable {
//   public:
//     void Revoke() {
//       mResource = nullptr;
//     }
//   private:
//     R *mResource;
//   };
//
//   class R {
//   public:
//     void EventHandled() {
//       mEvent.Forget();
//     }
//   private:
//     nsRevocableEventPtr<E> mEvent;
//   };
//
//   void R::PostEvent() {
//     // Make sure any pending event is revoked.
//     mEvent->Revoke();
//
//     nsCOMPtr<nsIRunnable> event = new E();
//     if (NS_SUCCEEDED(NS_DispatchToCurrentThread(event))) {
//       // Keep pointer to event so we can revoke it.
//       mEvent = event;
//     }
//   }
//
//   NS_IMETHODIMP E::Run() {
//     if (!mResource)
//       return NS_OK;
//     ...
//     mResource->EventHandled();
//     return NS_OK;
//   }
//
template<class T>
class nsRevocableEventPtr
{
public:
  nsRevocableEventPtr() : mEvent(nullptr) {}
  ~nsRevocableEventPtr() { Revoke(); }

  const nsRevocableEventPtr& operator=(T* aEvent)
  {
    if (mEvent != aEvent) {
      Revoke();
      mEvent = aEvent;
    }
    return *this;
  }

  const nsRevocableEventPtr& operator=(already_AddRefed<T> aEvent)
  {
    RefPtr<T> event = aEvent;
    if (mEvent != event) {
      Revoke();
      mEvent = event.forget();
    }
    return *this;
  }

  void Revoke()
  {
    if (mEvent) {
      mEvent->Revoke();
      mEvent = nullptr;
    }
  }

  void Forget() { mEvent = nullptr; }
  bool IsPending() { return mEvent != nullptr; }
  T* get() { return mEvent; }

private:
  // Not implemented
  nsRevocableEventPtr(const nsRevocableEventPtr&);
  nsRevocableEventPtr& operator=(const nsRevocableEventPtr&);

  RefPtr<T> mEvent;
};

/**
 * A simple helper to suffix thread pool name
 * with incremental numbers.
 */
class nsThreadPoolNaming
{
public:
  nsThreadPoolNaming() : mCounter(0) {}

  /**
   * Returns a thread name as "<aPoolName> #<n>" and increments the counter.
   */
  nsCString GetNextThreadName(const nsACString& aPoolName);

  template<size_t LEN>
  nsCString GetNextThreadName(const char (&aPoolName)[LEN])
  {
    return GetNextThreadName(nsDependentCString(aPoolName, LEN - 1));
  }

private:
  mozilla::Atomic<uint32_t> mCounter;

  nsThreadPoolNaming(const nsThreadPoolNaming&) = delete;
  void operator=(const nsThreadPoolNaming&) = delete;
};

/**
 * Thread priority in most operating systems affect scheduling, not IO.  This
 * helper is used to set the current thread to low IO priority for the lifetime
 * of the created object.  You can only use this low priority IO setting within
 * the context of the current thread.
 */
class MOZ_STACK_CLASS nsAutoLowPriorityIO
{
public:
  nsAutoLowPriorityIO();
  ~nsAutoLowPriorityIO();

private:
  bool lowIOPrioritySet;
#if defined(XP_MACOSX)
  int oldPriority;
#endif
};

void
NS_SetMainThread();

/**
 * Return the expiration time of the next timer to run on the current
 * thread.  If that expiration time is greater than aDefault, then
 * return aDefault.  aSearchBound specifies a maximum number of timers
 * to examine to find a timer on the current thread.  If no timer that
 * will run on the current thread is found after examining
 * aSearchBound timers, return the highest seen expiration time as a
 * best effort guess.
 *
 * Timers with either the type nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY or
 * nsITIMER::TYPE_REPEATING_SLACK_LOW_PRIORITY will be skipped when
 * searching for the next expiration time.  This enables timers to
 * have lower priority than callbacks dispatched from
 * nsIThread::IdleDispatch.
 */
extern mozilla::TimeStamp
NS_GetTimerDeadlineHintOnCurrentThread(mozilla::TimeStamp aDefault, uint32_t aSearchBound);

namespace mozilla {

/**
 * Cooperative thread scheduling is governed by two rules:
 * - Only one thread in the pool of cooperatively scheduled threads runs at a
 *   time.
 * - Thread switching happens at well-understood safe points.
 *
 * In some cases we may want to treat all the threads in a cooperative pool as a
 * single thread, while other parts of the code may want to view them as separate
 * threads. GetCurrentVirtualThread() will return the same value for all
 * threads in a cooperative thread pool. GetCurrentPhysicalThread will return a
 * different value for each thread in the pool.
 *
 * Thread safety assertions are a concrete example where GetCurrentVirtualThread
 * should be used. An object may want to assert that it only can be used on the
 * thread that created it. Such assertions would normally prevent the object
 * from being used on different cooperative threads. However, the object might
 * really only care that it's used atomically. Cooperative scheduling guarantees
 * that it will be (assuming we don't yield in the middle of modifying the
 * object). So we can weaken the assertion to compare the virtual thread the
 * object was created on to the virtual thread on which it's being used. This
 * assertion allows the object to be used across threads in a cooperative thread
 * pool while preventing accesses across preemptively scheduled threads (which
 * would be unsafe).
 */

// Returns the PRThread on which this code is running.
PRThread*
GetCurrentPhysicalThread();

// Returns a "virtual" PRThread that should only be used for comparison with
// other calls to GetCurrentVirtualThread. Two threads in the same cooperative
// thread pool will return the same virtual thread. Threads that are not
// cooperatively scheduled will have their own unique virtual PRThread (which
// will be equal to their physical PRThread).
//
// The return value of GetCurrentVirtualThread() is guaranteed not to change
// throughout the lifetime of a thread.
//
// Note that the original main thread (the first one created in the process) is
// considered as part of the pool of cooperative threads, so the return value of
// GetCurrentVirtualThread() for this thread (throughout its lifetime, even
// during shutdown) is the same as the return value from any other thread in the
// cooperative pool.
PRThread*
GetCurrentVirtualThread();

// These functions return event targets that can be used to dispatch to the
// current or main thread. They can also be used to test if you're on those
// threads (via IsOnCurrentThread). These functions should be used in preference
// to the nsIThread-based NS_Get{Current,Main}Thread functions since they will
// return more useful answers in the case of threads sharing an event loop.

nsIEventTarget*
GetCurrentThreadEventTarget();

nsIEventTarget*
GetMainThreadEventTarget();

// These variants of the above functions assert that the given thread has a
// serial event target (i.e., that it's not part of a thread pool) and returns
// that.

nsISerialEventTarget*
GetCurrentThreadSerialEventTarget();

nsISerialEventTarget*
GetMainThreadSerialEventTarget();

} // namespace mozilla

#endif  // nsThreadUtils_h__
