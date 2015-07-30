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
#include "mozilla/Atomics.h"
#include "mozilla/Likely.h"
#include "mozilla/TypeTraits.h"

//-----------------------------------------------------------------------------
// These methods are alternatives to the methods on nsIThreadManager, provided
// for convenience.

/**
 * Set name of the target thread.  This operation is asynchronous.
 */
extern void NS_SetThreadName(nsIThread* aThread, const nsACString& aName);

/**
 * Static length version of the above function checking length of the
 * name at compile time.
 */
template<size_t LEN>
inline void
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
extern NS_METHOD
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
extern NS_METHOD NS_GetCurrentThread(nsIThread** aResult);

/**
 * Dispatch the given event to the current thread.
 *
 * @param aEvent
 *   The event to dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern NS_METHOD NS_DispatchToCurrentThread(nsIRunnable* aEvent);

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
extern NS_METHOD
NS_DispatchToMainThread(nsIRunnable* aEvent,
                        uint32_t aDispatchFlags = NS_DISPATCH_NORMAL);
extern NS_METHOD
NS_DispatchToMainThread(already_AddRefed<nsIRunnable>&& aEvent,
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
extern NS_METHOD
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
#endif

//-----------------------------------------------------------------------------

#ifndef XPCOM_GLUE_AVOID_NSPR

// This class is designed to be subclassed.
class nsRunnable : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsRunnable() {}

protected:
  virtual ~nsRunnable() {}
};

// This class is designed to be subclassed.
class nsCancelableRunnable : public nsICancelableRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSICANCELABLERUNNABLE

  nsCancelableRunnable() {}

protected:
  virtual ~nsCancelableRunnable() {}
};

// An event that can be used to call a C++11 functions or function objects,
// including lambdas. The function must have no required arguments, and must
// return void.
template<typename Function>
class nsRunnableFunction : public nsRunnable
{
public:
  explicit nsRunnableFunction(const Function& aFunction)
    : mFunction(aFunction)
  { }

  NS_IMETHOD Run() {
    static_assert(mozilla::IsVoid<decltype(mFunction())>::value,
                  "The lambda must return void!");
    mFunction();
    return NS_OK;
  }
private:
  Function mFunction;
};

template<typename Function>
nsRunnableFunction<Function>* NS_NewRunnableFunction(const Function& aFunction)
{
  return new nsRunnableFunction<Function>(aFunction);
}

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

template<class ClassType, bool Owning>
struct nsRunnableMethodReceiver
{
  nsRefPtr<ClassType> mObj;
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

template<typename Method, bool Owning> struct nsRunnableMethodTraits;

template<class C, typename R, bool Owning, typename... As>
struct nsRunnableMethodTraits<R(C::*)(As...), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};

#ifdef NS_HAVE_STDCALL
template<class C, typename R, bool Owning, typename... As>
struct nsRunnableMethodTraits<R(__stdcall C::*)(As...), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};

template<class C, typename R, bool Owning>
struct nsRunnableMethodTraits<R(NS_STDCALL C::*)(), Owning>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
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
  explicit StoreCopyPassByValue(A&& a) : m(mozilla::Forward<A>(a)) {}
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
  explicit StoreCopyPassByConstLRef(A&& a) : m(mozilla::Forward<A>(a)) {}
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
  explicit StoreCopyPassByLRef(A&& a) : m(mozilla::Forward<A>(a)) {}
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
  explicit StoreCopyPassByRRef(A&& a) : m(mozilla::Forward<A>(a)) {}
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
  explicit StoreRefPassByLRef(A& a) : m(a) {}
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
  explicit StoreConstRefPassByConstLRef(const A& a) : m(a) {}
  passed_type PassAsParameter() { return m; }
};
template<typename S>
struct IsParameterStorageClass<StoreConstRefPassByConstLRef<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StorensRefPtrPassByPtr
{
  typedef nsRefPtr<T> stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  explicit StorensRefPtrPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return m.get(); }
};
template<typename S>
struct IsParameterStorageClass<StorensRefPtrPassByPtr<S>>
  : public mozilla::TrueType {};

template<typename T>
struct StorePtrPassByPtr
{
  typedef T* stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  explicit StorePtrPassByPtr(A a) : m(a) {}
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
  explicit StoreConstPtrPassByConstPtr(A a) : m(a) {}
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
  explicit StoreCopyPassByConstPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
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
  explicit StoreCopyPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
  passed_type PassAsParameter() { return &m; }
};
template<typename S>
struct IsParameterStorageClass<StoreCopyPassByPtr<S>>
  : public mozilla::TrueType {};

namespace detail {

template<typename TWithoutPointer>
struct NonnsISupportsPointerStorageClass
  : mozilla::Conditional<mozilla::IsConst<TWithoutPointer>::value,
                         StoreConstPtrPassByConstPtr<
                           typename mozilla::RemoveConst<TWithoutPointer>::Type>,
                         StorePtrPassByPtr<TWithoutPointer>>
{};

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

template<typename T>
struct IsRefcountedSmartPointer : public mozilla::FalseType
{};

template<typename T>
struct IsRefcountedSmartPointer<nsRefPtr<T>> : public mozilla::TrueType
{};

template<typename T>
struct IsRefcountedSmartPointer<nsCOMPtr<T>> : public mozilla::TrueType
{};

template<typename T>
struct StripSmartPointer
{
  typedef void Type;
};

template<typename T>
struct StripSmartPointer<nsRefPtr<T>>
{
  typedef T Type;
};

template<typename T>
struct StripSmartPointer<nsCOMPtr<T>>
{
  typedef T Type;
};

template<typename TWithoutPointer>
struct PointerStorageClass
  : mozilla::Conditional<HasRefCountMethods<TWithoutPointer>::value,
                         StorensRefPtrPassByPtr<TWithoutPointer>,
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
  : mozilla::Conditional<IsRefcountedSmartPointer<T>::value,
                         StorensRefPtrPassByPtr<
                           typename StripSmartPointer<T>::Type>,
                         StoreCopyPassByValue<T>>
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
// - RC*       -> StorensRefPtrPassByPtr<RC>     : Store nsRefPtr<RC>, pass RC*
//   ^^ RC quacks like a ref-counted type (i.e., has AddRef and Release methods)
// - const T*  -> StoreConstPtrPassByConstPtr<T> : Store const T*, pass const T*
// - T*        -> StorePtrPassByPtr<T>           : Store T*, pass T*.
// - const T&  -> StoreConstRefPassByConstLRef<T>: Store const T&, pass const T&.
// - T&        -> StoreRefPassByLRef<T>          : Store T&, pass T&.
// - T&&       -> StoreCopyPassByRRef<T>         : Store T, pass Move(T).
// - nsRefPtr<T>, nsCOMPtr<T>
//             -> StorensRefPtrPassByPtr<T>      : Store nsRefPtr<T>, pass T*
// - Other T   -> StoreCopyPassByValue<T>        : Store T, pass T.
// Other available explicit options:
// -              StoreCopyPassByConstLRef<T>    : Store T, pass const T&.
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

} /* namespace detail */

// struct used to store arguments and later apply them to a method.
template <typename... Ts> struct nsRunnableMethodArguments;

// Specializations for 0-8 arguments, add more as required.
// TODO Use tuple instead; And/or use lambdas (see bug 1152753)
template <>
struct nsRunnableMethodArguments<>
{
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)();
  }
};
template <typename T0>
struct nsRunnableMethodArguments<T0>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  template<typename A0>
  explicit nsRunnableMethodArguments(A0&& a0)
    : m0(mozilla::Forward<A0>(a0))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter());
  }
};
template <typename T0, typename T1>
struct nsRunnableMethodArguments<T0, T1>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  template<typename A0, typename A1>
  nsRunnableMethodArguments(A0&& a0, A1&& a1)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter());
  }
};
template <typename T0, typename T1, typename T2>
struct nsRunnableMethodArguments<T0, T1, T2>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  typename ::detail::ParameterStorage<T2>::Type m2;
  template<typename A0, typename A1, typename A2>
  nsRunnableMethodArguments(A0&& a0, A1&& a1, A2&& a2)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
    , m2(mozilla::Forward<A2>(a2))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter(), m2.PassAsParameter());
  }
};
template <typename T0, typename T1, typename T2, typename T3>
struct nsRunnableMethodArguments<T0, T1, T2, T3>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  typename ::detail::ParameterStorage<T2>::Type m2;
  typename ::detail::ParameterStorage<T3>::Type m3;
  template<typename A0, typename A1, typename A2, typename A3>
  nsRunnableMethodArguments(A0&& a0, A1&& a1, A2&& a2, A3&& a3)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
    , m2(mozilla::Forward<A2>(a2))
    , m3(mozilla::Forward<A3>(a3))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter(),
              m2.PassAsParameter(), m3.PassAsParameter());
  }
};
template <typename T0, typename T1, typename T2, typename T3, typename T4>
struct nsRunnableMethodArguments<T0, T1, T2, T3, T4>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  typename ::detail::ParameterStorage<T2>::Type m2;
  typename ::detail::ParameterStorage<T3>::Type m3;
  typename ::detail::ParameterStorage<T4>::Type m4;
  template<typename A0, typename A1, typename A2, typename A3, typename A4>
  nsRunnableMethodArguments(A0&& a0, A1&& a1, A2&& a2, A3&& a3, A4&& a4)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
    , m2(mozilla::Forward<A2>(a2))
    , m3(mozilla::Forward<A3>(a3))
    , m4(mozilla::Forward<A4>(a4))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter(),
              m2.PassAsParameter(), m3.PassAsParameter(),
              m4.PassAsParameter());
  }
};
template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5>
struct nsRunnableMethodArguments<T0, T1, T2, T3, T4, T5>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  typename ::detail::ParameterStorage<T2>::Type m2;
  typename ::detail::ParameterStorage<T3>::Type m3;
  typename ::detail::ParameterStorage<T4>::Type m4;
  typename ::detail::ParameterStorage<T5>::Type m5;
  template<typename A0, typename A1, typename A2, typename A3, typename A4,
           typename A5>
  nsRunnableMethodArguments(A0&& a0, A1&& a1, A2&& a2, A3&& a3, A4&& a4,
        A5&& a5)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
    , m2(mozilla::Forward<A2>(a2))
    , m3(mozilla::Forward<A3>(a3))
    , m4(mozilla::Forward<A4>(a4))
    , m5(mozilla::Forward<A5>(a5))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter(),
              m2.PassAsParameter(), m3.PassAsParameter(),
              m4.PassAsParameter(), m5.PassAsParameter());
  }
};
template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5, typename T6>
struct nsRunnableMethodArguments<T0, T1, T2, T3, T4, T5, T6>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  typename ::detail::ParameterStorage<T2>::Type m2;
  typename ::detail::ParameterStorage<T3>::Type m3;
  typename ::detail::ParameterStorage<T4>::Type m4;
  typename ::detail::ParameterStorage<T5>::Type m5;
  typename ::detail::ParameterStorage<T6>::Type m6;
  template<typename A0, typename A1, typename A2, typename A3, typename A4,
           typename A5, typename A6>
  nsRunnableMethodArguments(A0&& a0, A1&& a1, A2&& a2, A3&& a3, A4&& a4,
        A5&& a5, A6&& a6)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
    , m2(mozilla::Forward<A2>(a2))
    , m3(mozilla::Forward<A3>(a3))
    , m4(mozilla::Forward<A4>(a4))
    , m5(mozilla::Forward<A5>(a5))
    , m6(mozilla::Forward<A6>(a6))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter(),
              m2.PassAsParameter(), m3.PassAsParameter(),
              m4.PassAsParameter(), m5.PassAsParameter(),
              m6.PassAsParameter());
  }
};
template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5, typename T6, typename T7>
struct nsRunnableMethodArguments<T0, T1, T2, T3, T4, T5, T6, T7>
{
  typename ::detail::ParameterStorage<T0>::Type m0;
  typename ::detail::ParameterStorage<T1>::Type m1;
  typename ::detail::ParameterStorage<T2>::Type m2;
  typename ::detail::ParameterStorage<T3>::Type m3;
  typename ::detail::ParameterStorage<T4>::Type m4;
  typename ::detail::ParameterStorage<T5>::Type m5;
  typename ::detail::ParameterStorage<T6>::Type m6;
  typename ::detail::ParameterStorage<T7>::Type m7;
  template<typename A0, typename A1, typename A2, typename A3, typename A4,
           typename A5, typename A6, typename A7>
  nsRunnableMethodArguments(A0&& a0, A1&& a1, A2&& a2, A3&& a3, A4&& a4,
        A5&& a5, A6&& a6, A7&& a7)
    : m0(mozilla::Forward<A0>(a0))
    , m1(mozilla::Forward<A1>(a1))
    , m2(mozilla::Forward<A2>(a2))
    , m3(mozilla::Forward<A3>(a3))
    , m4(mozilla::Forward<A4>(a4))
    , m5(mozilla::Forward<A5>(a5))
    , m6(mozilla::Forward<A6>(a6))
    , m7(mozilla::Forward<A7>(a7))
  {}
  template<class C, typename M> void apply(C* o, M m)
  {
    ((*o).*m)(m0.PassAsParameter(), m1.PassAsParameter(),
              m2.PassAsParameter(), m3.PassAsParameter(),
              m4.PassAsParameter(), m5.PassAsParameter(),
              m6.PassAsParameter(), m7.PassAsParameter());
  }
};

template<typename Method, bool Owning, typename... Storages>
class nsRunnableMethodImpl
  : public nsRunnableMethodTraits<Method, Owning>::base_type
{
  typedef typename nsRunnableMethodTraits<Method, Owning>::class_type
      ClassType;
  nsRunnableMethodReceiver<ClassType, Owning> mReceiver;
  Method mMethod;
  nsRunnableMethodArguments<Storages...> mArgs;
public:
  virtual ~nsRunnableMethodImpl() { Revoke(); };
  template<typename... Args>
  explicit nsRunnableMethodImpl(ClassType* aObj, Method aMethod,
                                Args&&... aArgs)
    : mReceiver(aObj)
    , mMethod(aMethod)
    , mArgs(mozilla::Forward<Args>(aArgs)...)
  {
    static_assert(sizeof...(Storages) == sizeof...(Args), "Storages and Args should have equal sizes");
  }
  NS_IMETHOD Run()
  {
    if (MOZ_LIKELY(mReceiver.Get())) {
      mArgs.apply(mReceiver.Get(), mMethod);
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
  return new nsRunnableMethodImpl<Method, true>(aPtr, aMethod);
}

template<typename PtrType, typename Method>
typename nsRunnableMethodTraits<Method, false>::base_type*
NS_NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return new nsRunnableMethodImpl<Method, false>(aPtr, aMethod);
}

// Similar to NS_NewRunnableMethod. Call like so:
// nsCOMPtr<nsIRunnable> event =
//   NS_NewRunnableMethodWithArg<Type>(myObject, &MyClass::HandleEvent, myArg);
// 'Type' is the stored type for the argument, see ParameterStorage for details.
template<typename Storage, typename Method, typename PtrType, typename Arg>
typename nsRunnableMethodTraits<Method, true>::base_type*
NS_NewRunnableMethodWithArg(PtrType&& aPtr, Method aMethod, Arg&& aArg)
{
  return new nsRunnableMethodImpl<Method, true, Storage>(
      aPtr, aMethod, mozilla::Forward<Arg>(aArg));
}

// Similar to NS_NewRunnableMethod. Call like so:
// nsCOMPtr<nsIRunnable> event =
//   NS_NewRunnableMethodWithArg<Types,...>(myObject, &MyClass::HandleEvent, myArg1,...);
// 'Types' are the stored type for each argument, see ParameterStorage for details.
template<typename... Storages, typename Method, typename PtrType, typename... Args>
typename nsRunnableMethodTraits<Method, true>::base_type*
NS_NewRunnableMethodWithArgs(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return new nsRunnableMethodImpl<Method, true, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...);
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
typename nsRunnableMethodTraits<Method, false>::base_type*
NS_NewNonOwningRunnableMethodWithArgs(PtrType&& aPtr, Method aMethod,
                                      Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return new nsRunnableMethodImpl<Method, false, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...);
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

#endif  // nsThreadUtils_h__
