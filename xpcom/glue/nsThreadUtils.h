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
#include "nsIThreadManager.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsICancelableRunnable.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Likely.h"

//-----------------------------------------------------------------------------
// These methods are alternatives to the methods on nsIThreadManager, provided
// for convenience.

/**
 * Set name of the target thread.  This operation is asynchronous.
 */
extern NS_COM_GLUE void NS_SetThreadName(nsIThread* aThread,
                                         const nsACString& aName);

/**
 * Static length version of the above function checking length of the
 * name at compile time.
 */
template<size_t LEN>
inline NS_COM_GLUE void
NS_SetThreadName(nsIThread* aThread, const char (&aName)[LEN])
{
  static_assert(LEN <= 16,
                "Thread name must be no more than 16 characters");
  NS_SetThreadName(aThread, nsDependentCString(aName));
}

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
extern NS_COM_GLUE NS_METHOD
NS_NewThread(nsIThread** aResult,
             nsIRunnable* aInitialEvent = nullptr,
             uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE);

/**
 * Creates a named thread, otherwise the same as NS_NewThread
 */
template<size_t LEN>
inline NS_METHOD
NS_NewNamedThread(const char (&aName)[LEN],
                  nsIThread** aResult,
                  nsIRunnable* aInitialEvent = nullptr,
                  uint32_t aStackSize = nsIThreadManager::DEFAULT_STACK_SIZE)
{
  // Hold a ref while dispatching the initial event to match NS_NewThread()
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewThread(getter_AddRefs(thread), nullptr, aStackSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  NS_SetThreadName<LEN>(thread, aName);
  if (aInitialEvent) {
    rv = thread->Dispatch(aInitialEvent, NS_DISPATCH_NORMAL);
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Initial event dispatch failed");
  }

  *aResult = nullptr;
  thread.swap(*aResult);
  return rv;
}

/**
 * Get a reference to the current thread.
 *
 * @param aResult
 *   The resulting nsIThread object.
 */
extern NS_COM_GLUE NS_METHOD NS_GetCurrentThread(nsIThread** aResult);

/**
 * Dispatch the given event to the current thread.
 *
 * @param aEvent
 *   The event to dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern NS_COM_GLUE NS_METHOD NS_DispatchToCurrentThread(nsIRunnable* aEvent);

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
extern NS_COM_GLUE NS_METHOD
NS_DispatchToMainThread(nsIRunnable* aEvent,
                        uint32_t aDispatchFlags = NS_DISPATCH_NORMAL);

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
extern NS_COM_GLUE NS_METHOD
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
extern NS_COM_GLUE bool NS_HasPendingEvents(nsIThread* aThread = nullptr);

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
extern NS_COM_GLUE bool NS_ProcessNextEvent(nsIThread* aThread = nullptr,
                                            bool aMayWait = true);

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
extern NS_COM_GLUE nsIThread* NS_GetCurrentThread();
#endif

//-----------------------------------------------------------------------------

#ifndef XPCOM_GLUE_AVOID_NSPR

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_COM_GLUE

// This class is designed to be subclassed.
class NS_COM_GLUE nsRunnable : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsRunnable() {}

protected:
  virtual ~nsRunnable() {}
};

// This class is designed to be subclassed.
class NS_COM_GLUE nsCancelableRunnable : public nsICancelableRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSICANCELABLERUNNABLE

  nsCancelableRunnable() {}

protected:
  virtual ~nsCancelableRunnable() {}
};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY

// An event that can be used to call a method on a class.  The class type must
// support reference counting. This event supports Revoke for use
// with nsRevocableEventPtr.
template<class ClassType,
         typename ReturnType = void,
         bool Owning = true>
class nsRunnableMethod : public nsRunnable
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

template<class ClassType, typename Arg, bool Owning>
struct nsRunnableMethodReceiver
{
  ClassType* mObj;
  Arg mArg;
  nsRunnableMethodReceiver(ClassType* aObj, Arg aArg)
    : mObj(aObj)
    , mArg(aArg)
  {
    NS_IF_ADDREF(mObj);
  }
  ~nsRunnableMethodReceiver() { Revoke(); }
  void Revoke() { NS_IF_RELEASE(mObj); }
};

template<class ClassType, bool Owning>
struct nsRunnableMethodReceiver<ClassType, void, Owning>
{
  ClassType* mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) { NS_IF_ADDREF(mObj); }
  ~nsRunnableMethodReceiver() { Revoke(); }
  void Revoke() { NS_IF_RELEASE(mObj); }
};

template<class ClassType>
struct nsRunnableMethodReceiver<ClassType, void, false>
{
  ClassType* mObj;
  explicit nsRunnableMethodReceiver(ClassType* aObj) : mObj(aObj) {}
  void Revoke() { mObj = nullptr; }
};

template<typename Method, bool Owning> struct nsRunnableMethodTraits;

template<class C, typename R, typename A, bool Owning>
struct nsRunnableMethodTraits<R(C::*)(A), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef A arg_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};

template<class C, typename R, bool Owning>
struct nsRunnableMethodTraits<R(C::*)(), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef void arg_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};

#ifdef NS_HAVE_STDCALL
template<class C, typename R, typename A, bool Owning>
struct nsRunnableMethodTraits<R(__stdcall C::*)(A), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef A arg_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};

template<class C, typename R, bool Owning>
struct nsRunnableMethodTraits<R(NS_STDCALL C::*)(), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef void arg_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};
#endif

template<typename Method, typename Arg, bool Owning>
class nsRunnableMethodImpl
  : public nsRunnableMethodTraits<Method, Owning>::base_type
{
  typedef typename nsRunnableMethodTraits<Method, Owning>::class_type ClassType;
  nsRunnableMethodReceiver<ClassType, Arg, Owning> mReceiver;
  Method mMethod;
public:
  nsRunnableMethodImpl(ClassType* aObj, Method aMethod, Arg aArg)
    : mReceiver(aObj, aArg)
    , mMethod(aMethod)
  {
  }
  NS_IMETHOD Run()
  {
    if (MOZ_LIKELY(mReceiver.mObj)) {
      ((*mReceiver.mObj).*mMethod)(mReceiver.mArg);
    }
    return NS_OK;
  }
  void Revoke() { mReceiver.Revoke(); }
};

template<typename Method, bool Owning>
class nsRunnableMethodImpl<Method, void, Owning>
  : public nsRunnableMethodTraits<Method, Owning>::base_type
{
  typedef typename nsRunnableMethodTraits<Method, Owning>::class_type ClassType;
  nsRunnableMethodReceiver<ClassType, void, Owning> mReceiver;
  Method mMethod;

public:
  nsRunnableMethodImpl(ClassType* aObj, Method aMethod)
    : mReceiver(aObj)
    , mMethod(aMethod)
  {
  }

  NS_IMETHOD Run()
  {
    if (MOZ_LIKELY(mReceiver.mObj)) {
      ((*mReceiver.mObj).*mMethod)();
    }
    return NS_OK;
  }

  void Revoke() { mReceiver.Revoke(); }
};

// Use this template function like so:
//
//   nsCOMPtr<nsIRunnable> event =
//     NS_NewRunnableMethod(myObject, &MyClass::HandleEvent);
//   NS_DispatchToCurrentThread(event);
//
// Statically enforced constraints:
//  - myObject must be of (or implicitly convertible to) type MyClass
//  - MyClass must defined AddRef and Release methods
//
template<typename PtrType, typename Method>
typename nsRunnableMethodTraits<Method, true>::base_type*
NS_NewRunnableMethod(PtrType aPtr, Method aMethod)
{
  return new nsRunnableMethodImpl<Method, void, true>(aPtr, aMethod);
}

template<typename T>
struct dependent_type
{
  typedef T type;
};


// Similar to NS_NewRunnableMethod. Call like so:
// Type myArg;
// nsCOMPtr<nsIRunnable> event =
//   NS_NewRunnableMethodWithArg<Type>(myObject, &MyClass::HandleEvent, myArg);
template<typename Arg, typename Method, typename PtrType>
typename nsRunnableMethodTraits<Method, true>::base_type*
NS_NewRunnableMethodWithArg(PtrType&& aPtr, Method aMethod,
                            typename dependent_type<Arg>::type aArg)
{
  return new nsRunnableMethodImpl<Method, Arg, true>(aPtr, aMethod, aArg);
}

template<typename PtrType, typename Method>
typename nsRunnableMethodTraits<Method, false>::base_type*
NS_NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return new nsRunnableMethodImpl<Method, void, false>(aPtr, aMethod);
}

#endif  // XPCOM_GLUE_AVOID_NSPR

// This class is designed to be used when you have an event class E that has a
// pointer back to resource class R.  If R goes away while E is still pending,
// then it is important to "revoke" E so that it does not try use R after R has
// been destroyed.  nsRevocableEventPtr makes it easy for R to manage such
// situations:
//
//   class R;
//
//   class E : public nsRunnable {
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

  nsRefPtr<T> mEvent;
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
   * Creates and sets next thread name as "<aPoolName> #<n>"
   * on the specified thread.  If no thread is specified (aThread
   * is null) then the name is synchronously set on the current thread.
   */
  void SetThreadPoolName(const nsACString& aPoolName,
                         nsIThread* aThread = nullptr);

private:
  volatile uint32_t mCounter;

  nsThreadPoolNaming(const nsThreadPoolNaming&) MOZ_DELETE;
  void operator=(const nsThreadPoolNaming&) MOZ_DELETE;
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

#endif  // nsThreadUtils_h__
