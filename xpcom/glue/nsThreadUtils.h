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
#include "nsIIncrementalRunnable.h"
#include "nsINamed.h"
#include "nsIRunnable.h"
#include "nsIThreadManager.h"
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

extern nsresult
NS_IdleDispatchToCurrentThread(already_AddRefed<nsIRunnable>&& aEvent);

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

// This class is designed to be subclassed.
class Runnable : public nsIRunnable, public nsINamed
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSINAMED

  Runnable() {}

protected:
  virtual ~Runnable() {}
private:
  Runnable(const Runnable&) = delete;
  Runnable& operator=(const Runnable&) = delete;
  Runnable& operator=(const Runnable&&) = delete;

#ifndef RELEASE_OR_BETA
  const char* mName = nullptr;
#endif
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

protected:
  virtual ~CancelableRunnable() {}
private:
  CancelableRunnable(const CancelableRunnable&) = delete;
  CancelableRunnable& operator=(const CancelableRunnable&) = delete;
  CancelableRunnable& operator=(const CancelableRunnable&&) = delete;
};

// This class is designed to be subclassed.
class IncrementalRunnable : public CancelableRunnable,
                            public nsIIncrementalRunnable
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  // nsIIncrementalRunnable
  virtual void SetDeadline(TimeStamp aDeadline) override;

  IncrementalRunnable() {}

protected:
  virtual ~IncrementalRunnable() {}
private:
  IncrementalRunnable(const IncrementalRunnable&) = delete;
  IncrementalRunnable& operator=(const IncrementalRunnable&) = delete;
  IncrementalRunnable& operator=(const IncrementalRunnable&&) = delete;
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

} // namespace detail

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
  return do_AddRef(new mozilla::detail::RunnableFunction
                   // Make sure we store a non-reference in nsRunnableFunction.
                   <typename mozilla::RemoveReference<Function>::Type>
                   // But still forward aFunction to move if possible.
                   (mozilla::Forward<Function>(aFunction)));
}

// An event that can be used to call a method on a class.  The class type must
// support reference counting. This event supports Revoke for use
// with nsRevocableEventPtr.
template<class ClassType,
         typename ReturnType = void,
         bool Owning = true,
         bool Cancelable = false>
class nsRunnableMethod : public mozilla::Conditional<!Cancelable,
                                                     mozilla::Runnable,
                                                     mozilla::CancelableRunnable>::Type
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

template<typename Method, bool Owning, bool Cancelable> struct nsRunnableMethodTraits;

template<class C, typename R, bool Owning, bool Cancelable, typename... As>
struct nsRunnableMethodTraits<R(C::*)(As...), Owning, Cancelable>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Cancelable> base_type;
  static const bool can_cancel = Cancelable;
};

template<class C, typename R, bool Owning, bool Cancelable, typename... As>
struct nsRunnableMethodTraits<R(C::*)(As...) const, Owning, Cancelable>
{
  typedef const C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Cancelable> base_type;
  static const bool can_cancel = Cancelable;
};

#ifdef NS_HAVE_STDCALL
template<class C, typename R, bool Owning, bool Cancelable, typename... As>
struct nsRunnableMethodTraits<R(__stdcall C::*)(As...), Owning, Cancelable>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Cancelable> base_type;
  static const bool can_cancel = Cancelable;
};

template<class C, typename R, bool Owning, bool Cancelable>
struct nsRunnableMethodTraits<R(NS_STDCALL C::*)(), Owning, Cancelable>
{
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Cancelable> base_type;
  static const bool can_cancel = Cancelable;
};
template<class C, typename R, bool Owning, bool Cancelable, typename... As>
struct nsRunnableMethodTraits<R(__stdcall C::*)(As...) const, Owning, Cancelable>
{
  typedef const C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Cancelable> base_type;
  static const bool can_cancel = Cancelable;
};

template<class C, typename R, bool Owning, bool Cancelable>
struct nsRunnableMethodTraits<R(NS_STDCALL C::*)() const, Owning, Cancelable>
{
  typedef const C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning, Cancelable> base_type;
  static const bool can_cancel = Cancelable;
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
struct StorensRefPtrPassByPtr
{
  typedef RefPtr<T> stored_type;
  typedef T* passed_type;
  stored_type m;
  template <typename A>
  MOZ_IMPLICIT StorensRefPtrPassByPtr(A&& a) : m(mozilla::Forward<A>(a)) {}
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

namespace mozilla {

template<typename T>
struct IsRefcountedSmartPointer : public mozilla::FalseType
{};

template<typename T>
struct IsRefcountedSmartPointer<RefPtr<T>> : public mozilla::TrueType
{};

template<typename T>
struct IsRefcountedSmartPointer<nsCOMPtr<T>> : public mozilla::TrueType
{};

template<typename T>
struct RemoveSmartPointer
{
  typedef void Type;
};

template<typename T>
struct RemoveSmartPointer<RefPtr<T>>
{
  typedef T Type;
};

template<typename T>
struct RemoveSmartPointer<nsCOMPtr<T>>
{
  typedef T Type;
};

} // namespace mozilla

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
  : mozilla::Conditional<mozilla::IsRefcountedSmartPointer<T>::value,
                         StorensRefPtrPassByPtr<
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
// - RC*       -> StorensRefPtrPassByPtr<RC>     : Store RefPtr<RC>, pass RC*
//   ^^ RC quacks like a ref-counted type (i.e., has AddRef and Release methods)
// - const T*  -> StoreConstPtrPassByConstPtr<T> : Store const T*, pass const T*
// - T*        -> StorePtrPassByPtr<T>           : Store T*, pass T*.
// - const T&  -> StoreConstRefPassByConstLRef<T>: Store const T&, pass const T&.
// - T&        -> StoreRefPassByLRef<T>          : Store T&, pass T&.
// - T&&       -> StoreCopyPassByRRef<T>         : Store T, pass Move(T).
// - RefPtr<T>, nsCOMPtr<T>
//             -> StorensRefPtrPassByPtr<T>      : Store RefPtr<T>, pass T*
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

template<typename Method, bool Owning, bool Cancelable, typename... Storages>
class RunnableMethodImpl final
  : public ::nsRunnableMethodTraits<Method, Owning, Cancelable>::base_type
{
  typedef typename ::nsRunnableMethodTraits<Method, Owning, Cancelable>::class_type
      ClassType;
  ::nsRunnableMethodReceiver<ClassType, Owning> mReceiver;
  Method mMethod;
  RunnableMethodArguments<Storages...> mArgs;
private:
  virtual ~RunnableMethodImpl() { Revoke(); };
public:
  template<typename... Args>
  explicit RunnableMethodImpl(ClassType* aObj, Method aMethod,
                              Args&&... aArgs)
    : mReceiver(aObj)
    , mMethod(aMethod)
    , mArgs(Forward<Args>(aArgs)...)
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
  nsresult Cancel() {
    static_assert(Cancelable, "Don't use me!");
    Revoke();
    return NS_OK;
  }
  void Revoke() { mReceiver.Revoke(); }
};

} // namespace detail

// Use this template function like so:
//
//   nsCOMPtr<nsIRunnable> event =
//     mozilla::NewRunnableMethod(myObject, &MyClass::HandleEvent);
//   NS_DispatchToCurrentThread(event);
//
// Statically enforced constraints:
//  - myObject must be of (or implicitly convertible to) type MyClass
//  - MyClass must define AddRef and Release methods
//

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, false>::base_type>
NewRunnableMethod(PtrType aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, false>(aPtr, aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, true>::base_type>
NewCancelableRunnableMethod(PtrType aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, true>(aPtr, aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, false>::base_type>
NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, false>(aPtr, aMethod));
}

template<typename PtrType, typename Method>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, true>::base_type>
NewNonOwningCancelableRunnableMethod(PtrType&& aPtr, Method aMethod)
{
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, true>(aPtr, aMethod));
}

// Similar to NewRunnableMethod. Call like so:
// nsCOMPtr<nsIRunnable> event =
//   NewRunnableMethod<Types,...>(myObject, &MyClass::HandleEvent, myArg1,...);
// 'Types' are the stored type for each argument, see ParameterStorage for details.
template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, false>::base_type>
NewRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, false, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, false>::base_type>
NewNonOwningRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, false, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, true, true>::base_type>
NewCancelableRunnableMethod(PtrType&& aPtr, Method aMethod, Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, true, true, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
}

template<typename... Storages, typename Method, typename PtrType, typename... Args>
already_AddRefed<typename ::nsRunnableMethodTraits<Method, false, true>::base_type>
NewNonOwningCancelableRunnableMethod(PtrType&& aPtr, Method aMethod,
                                                Args&&... aArgs)
{
  static_assert(sizeof...(Storages) == sizeof...(Args),
                "<Storages...> size should be equal to number of arguments");
  return do_AddRef(new detail::RunnableMethodImpl<Method, false, true, Storages...>(
      aPtr, aMethod, mozilla::Forward<Args>(aArgs)...));
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

#endif  // nsThreadUtils_h__
