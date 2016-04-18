/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MozPromise_h_)
#define MozPromise_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "mozilla/Tuple.h"

#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

extern LazyLogModule gMozPromiseLog;

#define PROMISE_LOG(x, ...) \
  MOZ_LOG(gMozPromiseLog, mozilla::LogLevel::Debug, (x, ##__VA_ARGS__))

namespace detail {
template<typename ThisType, typename Ret, typename ArgType>
static TrueType TakesArgumentHelper(Ret (ThisType::*)(ArgType));
template<typename ThisType, typename Ret, typename ArgType>
static TrueType TakesArgumentHelper(Ret (ThisType::*)(ArgType) const);
template<typename ThisType, typename Ret>
static FalseType TakesArgumentHelper(Ret (ThisType::*)());
template<typename ThisType, typename Ret>
static FalseType TakesArgumentHelper(Ret (ThisType::*)() const);

template<typename ThisType, typename Ret, typename ArgType>
static Ret ReturnTypeHelper(Ret (ThisType::*)(ArgType));
template<typename ThisType, typename Ret, typename ArgType>
static Ret ReturnTypeHelper(Ret (ThisType::*)(ArgType) const);
template<typename ThisType, typename Ret>
static Ret ReturnTypeHelper(Ret (ThisType::*)());
template<typename ThisType, typename Ret>
static Ret ReturnTypeHelper(Ret (ThisType::*)() const);

template<typename MethodType>
struct ReturnType {
  typedef decltype(detail::ReturnTypeHelper(DeclVal<MethodType>())) Type;
};

} // namespace detail

template<typename MethodType>
struct TakesArgument {
  static const bool value = decltype(detail::TakesArgumentHelper(DeclVal<MethodType>()))::value;
};

template<typename MethodType, typename TargetType>
struct ReturnTypeIs {
  static const bool value = IsConvertible<typename detail::ReturnType<MethodType>::Type, TargetType>::value;
};

/*
 * A promise manages an asynchronous request that may or may not be able to be
 * fulfilled immediately. When an API returns a promise, the consumer may attach
 * callbacks to be invoked (asynchronously, on a specified thread) when the
 * request is either completed (resolved) or cannot be completed (rejected).
 * Whereas JS promise callbacks are dispatched from Microtask checkpoints,
 * MozPromises resolution/rejection make a normal round-trip through the event
 * loop, which simplifies their ordering semantics relative to other native code.
 *
 * MozPromises attempt to mirror the spirit of JS Promises to the extent that
 * is possible (and desirable) in C++. While the intent is that MozPromises
 * feel familiar to programmers who are accustomed to their JS-implemented cousin,
 * we don't shy away from imposing restrictions and adding features that make
 * sense for the use cases we encounter.
 *
 * A MozPromise is ThreadSafe, and may be ->Then()ed on any thread. The Then()
 * call accepts resolve and reject callbacks, and returns a MozPromise::Request.
 * The Request object serves several purposes for the consumer.
 *
 *   (1) It allows the caller to cancel the delivery of the resolve/reject value
 *       if it has not already occurred, via Disconnect() (this must be done on
 *       the target thread to avoid racing).
 *
 *   (2) It provides access to a "Completion Promise", which is roughly analagous
 *       to the Promise returned directly by ->then() calls on JS promises. If
 *       the resolve/reject callback returns a new MozPromise, that promise is
 *       chained to the completion promise, such that its resolve/reject value
 *       will be forwarded along when it arrives. If the resolve/reject callback
 *       returns void, the completion promise is resolved/rejected with the same
 *       value that was passed to the callback.
 *
 * The MozPromise APIs skirt traditional XPCOM convention by returning nsRefPtrs
 * (rather than already_AddRefed) from various methods. This is done to allow elegant
 * chaining of calls without cluttering up the code with intermediate variables, and
 * without introducing separate API variants for callers that want a return value
 * (from, say, ->Then()) from those that don't.
 *
 * When IsExclusive is true, the MozPromise does a release-mode assertion that
 * there is at most one call to either Then(...) or ChainTo(...).
 */

class MozPromiseRefcountable
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MozPromiseRefcountable)
protected:
  virtual ~MozPromiseRefcountable() {}
};

template<typename T> class MozPromiseHolder;
template<typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise : public MozPromiseRefcountable
{
public:
  typedef ResolveValueT ResolveValueType;
  typedef RejectValueT RejectValueType;
  class ResolveOrRejectValue
  {
  public:
    template<typename ResolveValueType_>
    void SetResolve(ResolveValueType_&& aResolveValue)
    {
      MOZ_ASSERT(IsNothing());
      mResolveValue.emplace(Forward<ResolveValueType_>(aResolveValue));
    }

    template<typename RejectValueType_>
    void SetReject(RejectValueType_&& aRejectValue)
    {
      MOZ_ASSERT(IsNothing());
      mRejectValue.emplace(Forward<RejectValueType_>(aRejectValue));
    }

    template<typename ResolveValueType_>
    static ResolveOrRejectValue MakeResolve(ResolveValueType_&& aResolveValue)
    {
      ResolveOrRejectValue val;
      val.SetResolve(Forward<ResolveValueType_>(aResolveValue));
      return val;
    }

    template<typename RejectValueType_>
    static ResolveOrRejectValue MakeReject(RejectValueType_&& aRejectValue)
    {
      ResolveOrRejectValue val;
      val.SetReject(Forward<RejectValueType_>(aRejectValue));
      return val;
    }

    bool IsResolve() const { return mResolveValue.isSome(); }
    bool IsReject() const { return mRejectValue.isSome(); }
    bool IsNothing() const { return mResolveValue.isNothing() && mRejectValue.isNothing(); }

    const ResolveValueType& ResolveValue() const { return mResolveValue.ref(); }
    const RejectValueType& RejectValue() const { return mRejectValue.ref(); }

  private:
    Maybe<ResolveValueType> mResolveValue;
    Maybe<RejectValueType> mRejectValue;
  };

protected:
  // MozPromise is the public type, and never constructed directly. Construct
  // a MozPromise::Private, defined below.
  MozPromise(const char* aCreationSite, bool aIsCompletionPromise)
    : mCreationSite(aCreationSite)
    , mMutex("MozPromise Mutex")
    , mHaveRequest(false)
    , mIsCompletionPromise(aIsCompletionPromise)
  {
    PROMISE_LOG("%s creating MozPromise (%p)", mCreationSite, this);
  }

public:
  // MozPromise::Private allows us to separate the public interface (upon which
  // consumers of the promise may invoke methods like Then()) from the private
  // interface (upon which the creator of the promise may invoke Resolve() or
  // Reject()). APIs should create and store a MozPromise::Private (usually
  // via a MozPromiseHolder), and return a MozPromise to consumers.
  //
  // NB: We can include the definition of this class inline once B2G ICS is gone.
  class Private;

  template<typename ResolveValueType_>
  static RefPtr<MozPromise>
  CreateAndResolve(ResolveValueType_&& aResolveValue, const char* aResolveSite)
  {
    RefPtr<typename MozPromise::Private> p = new MozPromise::Private(aResolveSite);
    p->Resolve(Forward<ResolveValueType_>(aResolveValue), aResolveSite);
    return p.forget();
  }

  template<typename RejectValueType_>
  static RefPtr<MozPromise>
  CreateAndReject(RejectValueType_&& aRejectValue, const char* aRejectSite)
  {
    RefPtr<typename MozPromise::Private> p = new MozPromise::Private(aRejectSite);
    p->Reject(Forward<RejectValueType_>(aRejectValue), aRejectSite);
    return p.forget();
  }

  typedef MozPromise<nsTArray<ResolveValueType>, RejectValueType, IsExclusive> AllPromiseType;
private:
  class AllPromiseHolder : public MozPromiseRefcountable
  {
  public:
    explicit AllPromiseHolder(size_t aDependentPromises)
      : mPromise(new typename AllPromiseType::Private(__func__))
      , mOutstandingPromises(aDependentPromises)
    {
      mResolveValues.SetLength(aDependentPromises);
    }

    void Resolve(size_t aIndex, const ResolveValueType& aResolveValue)
    {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mResolveValues[aIndex].emplace(aResolveValue);
      if (--mOutstandingPromises == 0) {
        nsTArray<ResolveValueType> resolveValues;
        resolveValues.SetCapacity(mResolveValues.Length());
        for (size_t i = 0; i < mResolveValues.Length(); ++i) {
          resolveValues.AppendElement(mResolveValues[i].ref());
        }

        mPromise->Resolve(resolveValues, __func__);
        mPromise = nullptr;
        mResolveValues.Clear();
      }
    }

    void Reject(const RejectValueType& aRejectValue)
    {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mPromise->Reject(aRejectValue, __func__);
      mPromise = nullptr;
      mResolveValues.Clear();
    }

    AllPromiseType* Promise() { return mPromise; }

  private:
    nsTArray<Maybe<ResolveValueType>> mResolveValues;
    RefPtr<typename AllPromiseType::Private> mPromise;
    size_t mOutstandingPromises;
  };
public:

  static RefPtr<AllPromiseType> All(AbstractThread* aProcessingThread, nsTArray<RefPtr<MozPromise>>& aPromises)
  {
    RefPtr<AllPromiseHolder> holder = new AllPromiseHolder(aPromises.Length());
    for (size_t i = 0; i < aPromises.Length(); ++i) {
      aPromises[i]->Then(aProcessingThread, __func__,
        [holder, i] (ResolveValueType aResolveValue) -> void { holder->Resolve(i, aResolveValue); },
        [holder] (RejectValueType aRejectValue) -> void { holder->Reject(aRejectValue); }
      );
    }
    return holder->Promise();
  }

  class Request : public MozPromiseRefcountable
  {
  public:
    virtual void Disconnect() = 0;

    // MSVC complains when an inner class (ThenValueBase::{Resolve,Reject}Runnable)
    // tries to access an inherited protected member.
    bool IsDisconnected() const { return mDisconnected; }

    virtual MozPromise* CompletionPromise() = 0;

    virtual void AssertIsDead() = 0;

  protected:
    Request() : mComplete(false), mDisconnected(false) {}
    virtual ~Request() {}

    bool mComplete;
    bool mDisconnected;
  };

protected:

  /*
   * A ThenValue tracks a single consumer waiting on the promise. When a consumer
   * invokes promise->Then(...), a ThenValue is created. Once the Promise is
   * resolved or rejected, a {Resolve,Reject}Runnable is dispatched, which
   * invokes the resolve/reject method and then deletes the ThenValue.
   */
  class ThenValueBase : public Request
  {
  public:
    class ResolveOrRejectRunnable : public Runnable
    {
    public:
      ResolveOrRejectRunnable(ThenValueBase* aThenValue, MozPromise* aPromise)
        : mThenValue(aThenValue)
        , mPromise(aPromise)
      {
        MOZ_DIAGNOSTIC_ASSERT(!mPromise->IsPending());
      }

      ~ResolveOrRejectRunnable()
      {
        if (mThenValue) {
          mThenValue->AssertIsDead();
        }
      }

      NS_IMETHODIMP Run()
      {
        PROMISE_LOG("ResolveOrRejectRunnable::Run() [this=%p]", this);
        mThenValue->DoResolveOrReject(mPromise->Value());
        mThenValue = nullptr;
        mPromise = nullptr;
        return NS_OK;
      }

    private:
      RefPtr<ThenValueBase> mThenValue;
      RefPtr<MozPromise> mPromise;
    };

    explicit ThenValueBase(AbstractThread* aResponseTarget, const char* aCallSite)
      : mResponseTarget(aResponseTarget), mCallSite(aCallSite) {}

    MozPromise* CompletionPromise() override
    {
      MOZ_DIAGNOSTIC_ASSERT(mResponseTarget->IsCurrentThreadIn());
      MOZ_DIAGNOSTIC_ASSERT(!Request::mComplete);
      if (!mCompletionPromise) {
        mCompletionPromise = new MozPromise::Private(
          "<completion promise>", true /* aIsCompletionPromise */);
      }
      return mCompletionPromise;
    }

    void AssertIsDead() override
    {
      // We want to assert that this ThenValues is dead - that is to say, that
      // there are no consumers waiting for the result. In the case of a normal
      // ThenValue, we check that it has been disconnected, which is the way
      // that the consumer signals that it no longer wishes to hear about the
      // result. If this ThenValue has a completion promise (which is mutually
      // exclusive with being disconnectable), we recursively assert that every
      // ThenValue associated with the completion promise is dead.
      if (mCompletionPromise) {
        mCompletionPromise->AssertIsDead();
      } else {
        MOZ_DIAGNOSTIC_ASSERT(Request::mDisconnected);
      }
    }

    void Dispatch(MozPromise *aPromise)
    {
      aPromise->mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!aPromise->IsPending());

      RefPtr<Runnable> runnable =
        static_cast<Runnable*>(new (typename ThenValueBase::ResolveOrRejectRunnable)(this, aPromise));
      PROMISE_LOG("%s Then() call made from %s [Runnable=%p, Promise=%p, ThenValue=%p]",
                  aPromise->mValue.IsResolve() ? "Resolving" : "Rejecting", ThenValueBase::mCallSite,
                  runnable.get(), aPromise, this);

      // Promise consumers are allowed to disconnect the Request object and
      // then shut down the thread or task queue that the promise result would
      // be dispatched on. So we unfortunately can't assert that promise
      // dispatch succeeds. :-(
      mResponseTarget->Dispatch(runnable.forget(), AbstractThread::DontAssertDispatchSuccess);
    }

    virtual void Disconnect() override
    {
      MOZ_ASSERT(ThenValueBase::mResponseTarget->IsCurrentThreadIn());
      MOZ_DIAGNOSTIC_ASSERT(!Request::mComplete);
      Request::mDisconnected = true;

      // We could support rejecting the completion promise on disconnection, but
      // then we'd need to have some sort of default reject value. The use cases
      // of disconnection and completion promise chaining seem pretty orthogonal,
      // so let's use assert against it.
      MOZ_DIAGNOSTIC_ASSERT(!mCompletionPromise);
    }

  protected:
    virtual already_AddRefed<MozPromise> DoResolveOrRejectInternal(const ResolveOrRejectValue& aValue) = 0;

    void DoResolveOrReject(const ResolveOrRejectValue& aValue)
    {
      Request::mComplete = true;
      if (Request::mDisconnected) {
        PROMISE_LOG("ThenValue::DoResolveOrReject disconnected - bailing out [this=%p]", this);
        return;
      }

      // Invoke the resolve or reject method.
      RefPtr<MozPromise> p = DoResolveOrRejectInternal(aValue);

      // If there's a completion promise, resolve it appropriately with the
      // result of the method.
      //
      // We jump through some hoops to cast to MozPromise::Private here. This
      // can go away when we can just declare mCompletionPromise as
      // MozPromise::Private. See the declaration below.
      RefPtr<MozPromise::Private> completionPromise =
        dont_AddRef(static_cast<MozPromise::Private*>(mCompletionPromise.forget().take()));
      if (completionPromise) {
        if (p) {
          p->ChainTo(completionPromise.forget(), "<chained completion promise>");
        } else {
          completionPromise->ResolveOrReject(aValue, "<completion of non-promise-returning method>");
        }
      }
    }

    RefPtr<AbstractThread> mResponseTarget; // May be released on any thread.

    // Declaring RefPtr<MozPromise::Private> here causes build failures
    // on MSVC because MozPromise::Private is only forward-declared at this
    // point. This hack can go away when we inline-declare MozPromise::Private,
    // which is blocked on the B2G ICS compiler being too old.
    RefPtr<MozPromise> mCompletionPromise;

    const char* mCallSite;
  };

  /*
   * We create two overloads for invoking Resolve/Reject Methods so as to
   * make the resolve/reject value argument "optional".
   */

  template<typename ThisType, typename MethodType, typename ValueType>
  static typename EnableIf<ReturnTypeIs<MethodType, RefPtr<MozPromise>>::value &&
                           TakesArgument<MethodType>::value,
                           already_AddRefed<MozPromise>>::Type
  InvokeCallbackMethod(ThisType* aThisVal, MethodType aMethod, ValueType&& aValue)
  {
    return ((*aThisVal).*aMethod)(Forward<ValueType>(aValue)).forget();
  }

  template<typename ThisType, typename MethodType, typename ValueType>
  static typename EnableIf<ReturnTypeIs<MethodType, void>::value &&
                           TakesArgument<MethodType>::value,
                           already_AddRefed<MozPromise>>::Type
  InvokeCallbackMethod(ThisType* aThisVal, MethodType aMethod, ValueType&& aValue)
  {
    ((*aThisVal).*aMethod)(Forward<ValueType>(aValue));
    return nullptr;
  }

  template<typename ThisType, typename MethodType, typename ValueType>
  static typename EnableIf<ReturnTypeIs<MethodType, RefPtr<MozPromise>>::value &&
                           !TakesArgument<MethodType>::value,
                           already_AddRefed<MozPromise>>::Type
  InvokeCallbackMethod(ThisType* aThisVal, MethodType aMethod, ValueType&& aValue)
  {
    return ((*aThisVal).*aMethod)().forget();
  }

  template<typename ThisType, typename MethodType, typename ValueType>
  static typename EnableIf<ReturnTypeIs<MethodType, void>::value &&
                           !TakesArgument<MethodType>::value,
                           already_AddRefed<MozPromise>>::Type
  InvokeCallbackMethod(ThisType* aThisVal, MethodType aMethod, ValueType&& aValue)
  {
    ((*aThisVal).*aMethod)();
    return nullptr;
  }

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  class MethodThenValue : public ThenValueBase
  {
  public:
    MethodThenValue(AbstractThread* aResponseTarget, ThisType* aThisVal,
                    ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod,
                    const char* aCallSite)
      : ThenValueBase(aResponseTarget, aCallSite)
      , mThisVal(aThisVal)
      , mResolveMethod(aResolveMethod)
      , mRejectMethod(aRejectMethod) {}

  virtual void Disconnect() override
  {
    ThenValueBase::Disconnect();

    // If a Request has been disconnected, we don't guarantee that the
    // resolve/reject runnable will be dispatched. Null out our refcounted
    // this-value now so that it's released predictably on the dispatch thread.
    mThisVal = nullptr;
  }

  protected:
    virtual already_AddRefed<MozPromise> DoResolveOrRejectInternal(const ResolveOrRejectValue& aValue) override
    {
      RefPtr<MozPromise> completion;
      if (aValue.IsResolve()) {
        completion = InvokeCallbackMethod(mThisVal.get(), mResolveMethod, aValue.ResolveValue());
      } else {
        completion = InvokeCallbackMethod(mThisVal.get(), mRejectMethod, aValue.RejectValue());
      }

      // Null out mThisVal after invoking the callback so that any references are
      // released predictably on the dispatch thread. Otherwise, it would be
      // released on whatever thread last drops its reference to the ThenValue,
      // which may or may not be ok.
      mThisVal = nullptr;

      return completion.forget();
    }

  private:
    RefPtr<ThisType> mThisVal; // Only accessed and refcounted on dispatch thread.
    ResolveMethodType mResolveMethod;
    RejectMethodType mRejectMethod;
  };

  // NB: We could use std::function here instead of a template if it were supported. :-(
  template<typename ResolveFunction, typename RejectFunction>
  class FunctionThenValue : public ThenValueBase
  {
  public:
    FunctionThenValue(AbstractThread* aResponseTarget,
                      ResolveFunction&& aResolveFunction,
                      RejectFunction&& aRejectFunction,
                      const char* aCallSite)
      : ThenValueBase(aResponseTarget, aCallSite)
    {
      mResolveFunction.emplace(Move(aResolveFunction));
      mRejectFunction.emplace(Move(aRejectFunction));
    }

  virtual void Disconnect() override
  {
    ThenValueBase::Disconnect();

    // If a Request has been disconnected, we don't guarantee that the
    // resolve/reject runnable will be dispatched. Destroy our callbacks
    // now so that any references in closures are released predictable on
    // the dispatch thread.
    mResolveFunction.reset();
    mRejectFunction.reset();
  }

  protected:
    virtual already_AddRefed<MozPromise> DoResolveOrRejectInternal(const ResolveOrRejectValue& aValue) override
    {
      // Note: The usage of InvokeCallbackMethod here requires that
      // ResolveFunction/RejectFunction are capture-lambdas (i.e. anonymous
      // classes with ::operator()), since it allows us to share code more easily.
      // We could fix this if need be, though it's quite easy to work around by
      // just capturing something.
      RefPtr<MozPromise> completion;
      if (aValue.IsResolve()) {
        completion = InvokeCallbackMethod(mResolveFunction.ptr(), &ResolveFunction::operator(), aValue.ResolveValue());
      } else {
        completion = InvokeCallbackMethod(mRejectFunction.ptr(), &RejectFunction::operator(), aValue.RejectValue());
      }

      // Destroy callbacks after invocation so that any references in closures are
      // released predictably on the dispatch thread. Otherwise, they would be
      // released on whatever thread last drops its reference to the ThenValue,
      // which may or may not be ok.
      mResolveFunction.reset();
      mRejectFunction.reset();

      return completion.forget();
    }

  private:
    Maybe<ResolveFunction> mResolveFunction; // Only accessed and deleted on dispatch thread.
    Maybe<RejectFunction> mRejectFunction; // Only accessed and deleted on dispatch thread.
  };

public:
  void ThenInternal(AbstractThread* aResponseThread, ThenValueBase* aThenValue,
                    const char* aCallSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(aResponseThread->IsDispatchReliable());
    MOZ_DIAGNOSTIC_ASSERT(!IsExclusive || !mHaveRequest);
    mHaveRequest = true;
    PROMISE_LOG("%s invoking Then() [this=%p, aThenValue=%p, isPending=%d]",
                aCallSite, this, aThenValue, (int) IsPending());
    if (!IsPending()) {
      aThenValue->Dispatch(this);
    } else {
      mThenValues.AppendElement(aThenValue);
    }
  }

public:

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  RefPtr<Request> Then(AbstractThread* aResponseThread, const char* aCallSite, ThisType* aThisVal,
                         ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod)
  {
    RefPtr<ThenValueBase> thenValue = new MethodThenValue<ThisType, ResolveMethodType, RejectMethodType>(
                                              aResponseThread, aThisVal, aResolveMethod, aRejectMethod, aCallSite);
    ThenInternal(aResponseThread, thenValue, aCallSite);
    return thenValue.forget(); // Implicit conversion from already_AddRefed<ThenValueBase> to RefPtr<Request>.
  }

  template<typename ResolveFunction, typename RejectFunction>
  RefPtr<Request> Then(AbstractThread* aResponseThread, const char* aCallSite,
                         ResolveFunction&& aResolveFunction, RejectFunction&& aRejectFunction)
  {
    RefPtr<ThenValueBase> thenValue = new FunctionThenValue<ResolveFunction, RejectFunction>(aResponseThread,
                                              Move(aResolveFunction), Move(aRejectFunction), aCallSite);
    ThenInternal(aResponseThread, thenValue, aCallSite);
    return thenValue.forget(); // Implicit conversion from already_AddRefed<ThenValueBase> to RefPtr<Request>.
  }

  void ChainTo(already_AddRefed<Private> aChainedPromise, const char* aCallSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_DIAGNOSTIC_ASSERT(!IsExclusive || !mHaveRequest);
    mHaveRequest = true;
    RefPtr<Private> chainedPromise = aChainedPromise;
    PROMISE_LOG("%s invoking Chain() [this=%p, chainedPromise=%p, isPending=%d]",
                aCallSite, this, chainedPromise.get(), (int) IsPending());
    if (!IsPending()) {
      ForwardTo(chainedPromise);
    } else {
      mChainedPromises.AppendElement(chainedPromise);
    }
  }

  // Note we expose the function AssertIsDead() instead of IsDead() since
  // checking IsDead() is a data race in the situation where the request is not
  // dead. Therefore we enforce the form |Assert(IsDead())| by exposing
  // AssertIsDead() only.
  void AssertIsDead()
  {
    MutexAutoLock lock(mMutex);
    for (auto&& then : mThenValues) {
      then->AssertIsDead();
    }
    for (auto&& chained : mChainedPromises) {
      chained->AssertIsDead();
    }
  }

protected:
  bool IsPending() const { return mValue.IsNothing(); }
  const ResolveOrRejectValue& Value() const
  {
    // This method should only be called once the value has stabilized. As
    // such, we don't need to acquire the lock here.
    MOZ_DIAGNOSTIC_ASSERT(!IsPending());
    return mValue;
  }

  void DispatchAll()
  {
    mMutex.AssertCurrentThreadOwns();
    for (size_t i = 0; i < mThenValues.Length(); ++i) {
      mThenValues[i]->Dispatch(this);
    }
    mThenValues.Clear();

    for (size_t i = 0; i < mChainedPromises.Length(); ++i) {
      ForwardTo(mChainedPromises[i]);
    }
    mChainedPromises.Clear();
  }

  void ForwardTo(Private* aOther)
  {
    MOZ_ASSERT(!IsPending());
    if (mValue.IsResolve()) {
      aOther->Resolve(mValue.ResolveValue(), "<chained promise>");
    } else {
      aOther->Reject(mValue.RejectValue(), "<chained promise>");
    }
  }

  virtual ~MozPromise()
  {
    PROMISE_LOG("MozPromise::~MozPromise [this=%p]", this);
    AssertIsDead();
    // We can't guarantee a completion promise will always be revolved or
    // rejected since ResolveOrRejectRunnable might not run when dispatch fails.
    if (!mIsCompletionPromise) {
      MOZ_ASSERT(!IsPending());
      MOZ_ASSERT(mThenValues.IsEmpty());
      MOZ_ASSERT(mChainedPromises.IsEmpty());
    }
  };

  const char* mCreationSite; // For logging
  Mutex mMutex;
  ResolveOrRejectValue mValue;
  nsTArray<RefPtr<ThenValueBase>> mThenValues;
  nsTArray<RefPtr<Private>> mChainedPromises;
  bool mHaveRequest;
  const bool mIsCompletionPromise;
};

template<typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise<ResolveValueT, RejectValueT, IsExclusive>::Private
  : public MozPromise<ResolveValueT, RejectValueT, IsExclusive>
{
public:
  explicit Private(const char* aCreationSite, bool aIsCompletionPromise = false)
    : MozPromise(aCreationSite, aIsCompletionPromise) {}

  template<typename ResolveValueT_>
  void Resolve(ResolveValueT_&& aResolveValue, const char* aResolveSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(IsPending());
    PROMISE_LOG("%s resolving MozPromise (%p created at %s)", aResolveSite, this, mCreationSite);
    mValue.SetResolve(Forward<ResolveValueT_>(aResolveValue));
    DispatchAll();
  }

  template<typename RejectValueT_>
  void Reject(RejectValueT_&& aRejectValue, const char* aRejectSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(IsPending());
    PROMISE_LOG("%s rejecting MozPromise (%p created at %s)", aRejectSite, this, mCreationSite);
    mValue.SetReject(Forward<RejectValueT_>(aRejectValue));
    DispatchAll();
  }

  template<typename ResolveOrRejectValue_>
  void ResolveOrReject(ResolveOrRejectValue_&& aValue, const char* aSite)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(IsPending());
    PROMISE_LOG("%s resolveOrRejecting MozPromise (%p created at %s)", aSite, this, mCreationSite);
    mValue = Forward<ResolveOrRejectValue_>(aValue);
    DispatchAll();
  }
};

// A generic promise type that does the trick for simple use cases.
typedef MozPromise<bool, nsresult, /* IsExclusive = */ false> GenericPromise;

/*
 * Class to encapsulate a promise for a particular role. Use this as the member
 * variable for a class whose method returns a promise.
 */
template<typename PromiseType>
class MozPromiseHolder
{
public:
  MozPromiseHolder()
    : mMonitor(nullptr) {}

  // Move semantics.
  MozPromiseHolder& operator=(MozPromiseHolder&& aOther)
  {
    MOZ_ASSERT(!mMonitor && !aOther.mMonitor);
    MOZ_DIAGNOSTIC_ASSERT(!mPromise);
    mPromise = aOther.mPromise;
    aOther.mPromise = nullptr;
    return *this;
  }

  ~MozPromiseHolder() { MOZ_ASSERT(!mPromise); }

  already_AddRefed<PromiseType> Ensure(const char* aMethodName) {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    if (!mPromise) {
      mPromise = new (typename PromiseType::Private)(aMethodName);
    }
    RefPtr<PromiseType> p = mPromise.get();
    return p.forget();
  }

  // Provide a Monitor that should always be held when accessing this instance.
  void SetMonitor(Monitor* aMonitor) { mMonitor = aMonitor; }

  bool IsEmpty() const
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    return !mPromise;
  }

  already_AddRefed<typename PromiseType::Private> Steal()
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }

    RefPtr<typename PromiseType::Private> p = mPromise;
    mPromise = nullptr;
    return p.forget();
  }

  void Resolve(typename PromiseType::ResolveValueType aResolveValue,
               const char* aMethodName)
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    MOZ_ASSERT(mPromise);
    mPromise->Resolve(aResolveValue, aMethodName);
    mPromise = nullptr;
  }


  void ResolveIfExists(typename PromiseType::ResolveValueType aResolveValue,
                       const char* aMethodName)
  {
    if (!IsEmpty()) {
      Resolve(aResolveValue, aMethodName);
    }
  }

  void Reject(typename PromiseType::RejectValueType aRejectValue,
              const char* aMethodName)
  {
    if (mMonitor) {
      mMonitor->AssertCurrentThreadOwns();
    }
    MOZ_ASSERT(mPromise);
    mPromise->Reject(aRejectValue, aMethodName);
    mPromise = nullptr;
  }


  void RejectIfExists(typename PromiseType::RejectValueType aRejectValue,
                      const char* aMethodName)
  {
    if (!IsEmpty()) {
      Reject(aRejectValue, aMethodName);
    }
  }

private:
  Monitor* mMonitor;
  RefPtr<typename PromiseType::Private> mPromise;
};

/*
 * Class to encapsulate a MozPromise::Request reference. Use this as the member
 * variable for a class waiting on a MozPromise.
 */
template<typename PromiseType>
class MozPromiseRequestHolder
{
public:
  MozPromiseRequestHolder() {}
  ~MozPromiseRequestHolder() { MOZ_ASSERT(!mRequest); }

  void Begin(RefPtr<typename PromiseType::Request>&& aRequest)
  {
    MOZ_DIAGNOSTIC_ASSERT(!Exists());
    mRequest = Move(aRequest);
  }

  void Begin(typename PromiseType::Request* aRequest)
  {
    MOZ_DIAGNOSTIC_ASSERT(!Exists());
    mRequest = aRequest;
  }

  void Complete()
  {
    MOZ_DIAGNOSTIC_ASSERT(Exists());
    mRequest = nullptr;
  }

  // Disconnects and forgets an outstanding promise. The resolve/reject methods
  // will never be called.
  void Disconnect() {
    MOZ_ASSERT(Exists());
    mRequest->Disconnect();
    mRequest = nullptr;
  }

  void DisconnectIfExists() {
    if (Exists()) {
      Disconnect();
    }
  }

  bool Exists() const { return !!mRequest; }

private:
  RefPtr<typename PromiseType::Request> mRequest;
};

// Asynchronous Potentially-Cross-Thread Method Calls.
//
// This machinery allows callers to schedule a promise-returning method to be
// invoked asynchronously on a given thread, while at the same time receiving
// a promise upon which to invoke Then() immediately. InvokeAsync dispatches
// a task to invoke the method on the proper thread and also chain the resulting
// promise to the one that the caller received, so that resolve/reject values
// are forwarded through.

namespace detail {

template<typename ReturnType, typename ThisType, typename... ArgTypes, size_t... Indices>
ReturnType
MethodCallInvokeHelper(ReturnType(ThisType::*aMethod)(ArgTypes...), ThisType* aThisVal,
                       Tuple<ArgTypes...>& aArgs, IndexSequence<Indices...>)
{
  return ((*aThisVal).*aMethod)(Get<Indices>(aArgs)...);
}

// Non-templated base class to allow us to use MOZ_COUNT_{C,D}TOR, which cause
// assertions when used on templated types.
class MethodCallBase
{
public:
  MethodCallBase() { MOZ_COUNT_CTOR(MethodCallBase); }
  virtual ~MethodCallBase() { MOZ_COUNT_DTOR(MethodCallBase); }
};

template<typename PromiseType, typename ThisType, typename... ArgTypes>
class MethodCall : public MethodCallBase
{
public:
  typedef RefPtr<PromiseType>(ThisType::*MethodType)(ArgTypes...);
  MethodCall(MethodType aMethod, ThisType* aThisVal, ArgTypes... aArgs)
    : mMethod(aMethod)
    , mThisVal(aThisVal)
    , mArgs(Forward<ArgTypes>(aArgs)...)
  {}

  RefPtr<PromiseType> Invoke()
  {
    return MethodCallInvokeHelper(mMethod, mThisVal.get(), mArgs, typename IndexSequenceFor<ArgTypes...>::Type());
  }

private:
  MethodType mMethod;
  RefPtr<ThisType> mThisVal;
  Tuple<ArgTypes...> mArgs;
};

template<typename PromiseType, typename ThisType, typename ...ArgTypes>
class ProxyRunnable : public Runnable
{
public:
  ProxyRunnable(typename PromiseType::Private* aProxyPromise, MethodCall<PromiseType, ThisType, ArgTypes...>* aMethodCall)
    : mProxyPromise(aProxyPromise), mMethodCall(aMethodCall) {}

  NS_IMETHODIMP Run()
  {
    RefPtr<PromiseType> p = mMethodCall->Invoke();
    mMethodCall = nullptr;
    p->ChainTo(mProxyPromise.forget(), "<Proxy Promise>");
    return NS_OK;
  }

private:
  RefPtr<typename PromiseType::Private> mProxyPromise;
  nsAutoPtr<MethodCall<PromiseType, ThisType, ArgTypes...>> mMethodCall;
};

} // namespace detail

template<typename PromiseType, typename ThisType, typename ...ArgTypes, typename ...ActualArgTypes>
static RefPtr<PromiseType>
InvokeAsync(AbstractThread* aTarget, ThisType* aThisVal, const char* aCallerName,
            RefPtr<PromiseType>(ThisType::*aMethod)(ArgTypes...), ActualArgTypes&&... aArgs)
{
  typedef detail::MethodCall<PromiseType, ThisType, ArgTypes...> MethodCallType;
  typedef detail::ProxyRunnable<PromiseType, ThisType, ArgTypes...> ProxyRunnableType;

  MethodCallType* methodCall = new MethodCallType(aMethod, aThisVal, Forward<ActualArgTypes>(aArgs)...);
  RefPtr<typename PromiseType::Private> p = new (typename PromiseType::Private)(aCallerName);
  RefPtr<ProxyRunnableType> r = new ProxyRunnableType(p, methodCall);
  MOZ_ASSERT(aTarget->IsDispatchReliable());
  aTarget->Dispatch(r.forget());
  return p.forget();
}

#undef PROMISE_LOG

} // namespace mozilla

#endif
