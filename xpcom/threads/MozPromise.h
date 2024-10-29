/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_MOZPROMISE_H_
#define XPCOM_THREADS_MOZPROMISE_H_

#include <type_traits>
#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticString.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Variant.h"
#include "nsIDirectTaskDispatcher.h"
#include "nsISerialEventTarget.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/GeckoResultUtils.h"
#endif

#if MOZ_DIAGNOSTIC_ASSERT_ENABLED
#  define PROMISE_DEBUG
#endif

#ifdef PROMISE_DEBUG
#  define PROMISE_ASSERT MOZ_RELEASE_ASSERT
#else
#  define PROMISE_ASSERT(...) \
    do {                      \
    } while (0)
#endif

#if DEBUG
#  include "nsPrintfCString.h"
#endif

namespace mozilla {

namespace dom {
class Promise;
}

extern LazyLogModule gMozPromiseLog;

#define PROMISE_LOG(x, ...) \
  MOZ_LOG(gMozPromiseLog, mozilla::LogLevel::Debug, (x, ##__VA_ARGS__))

namespace detail {
template <typename F>
struct MethodTraitsHelper : MethodTraitsHelper<decltype(&F::operator())> {};
template <typename ThisType, typename Ret, typename... ArgTypes>
struct MethodTraitsHelper<Ret (ThisType::*)(ArgTypes...)> {
  using ReturnType = Ret;
  static const size_t ArgSize = sizeof...(ArgTypes);
};
template <typename ThisType, typename Ret, typename... ArgTypes>
struct MethodTraitsHelper<Ret (ThisType::*)(ArgTypes...) const> {
  using ReturnType = Ret;
  static const size_t ArgSize = sizeof...(ArgTypes);
};
template <typename ThisType, typename Ret, typename... ArgTypes>
struct MethodTraitsHelper<Ret (ThisType::*)(ArgTypes...) volatile> {
  using ReturnType = Ret;
  static const size_t ArgSize = sizeof...(ArgTypes);
};
template <typename ThisType, typename Ret, typename... ArgTypes>
struct MethodTraitsHelper<Ret (ThisType::*)(ArgTypes...) const volatile> {
  using ReturnType = Ret;
  static const size_t ArgSize = sizeof...(ArgTypes);
};
template <typename T>
struct MethodTrait : MethodTraitsHelper<std::remove_reference_t<T>> {};

}  // namespace detail

template <typename T>
using MethodReturnType = typename detail::MethodTrait<T>::ReturnType;

template <typename MethodType>
constexpr bool TakesAnyArguments =
    detail::MethodTrait<MethodType>::ArgSize != 0;

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise;

template <typename T>
constexpr bool IsMozPromise = false;

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
constexpr bool
    IsMozPromise<MozPromise<ResolveValueT, RejectValueT, IsExclusive>> = true;

/*
 * A promise manages an asynchronous request that may or may not be able to be
 * fulfilled immediately. When an API returns a promise, the consumer may attach
 * callbacks to be invoked (asynchronously, on a specified thread) when the
 * request is either completed (resolved) or cannot be completed (rejected).
 * Whereas JS promise callbacks are dispatched from Microtask checkpoints,
 * MozPromises resolution/rejection make a normal round-trip through the event
 * loop, which simplifies their ordering semantics relative to other native
 * code.
 *
 * MozPromises attempt to mirror the spirit of JS Promises to the extent that
 * is possible (and desirable) in C++. While the intent is that MozPromises
 * feel familiar to programmers who are accustomed to their JS-implemented
 * cousin, we don't shy away from imposing restrictions and adding features that
 * make sense for the use cases we encounter.
 *
 * A MozPromise is ThreadSafe, and may be ->Then()ed on any thread. The Then()
 * call accepts resolve and reject callbacks, and returns a magic object which
 * will be implicitly converted to a MozPromise::Request or a MozPromise object
 * depending on how the return value is used. The magic object serves several
 * purposes for the consumer.
 *
 *   (1) When converting to a MozPromise::Request, it allows the caller to
 *       cancel the delivery of the resolve/reject value if it has not already
 *       occurred, via Disconnect() (this must be done on the target thread to
 *       avoid racing).
 *
 *   (2) When converting to a MozPromise (which is called a completion promise),
 *       it allows promise chaining so ->Then() can be called again to attach
 *       more resolve and reject callbacks. If the resolve/reject callback
 *       returns a new MozPromise, that promise is chained to the completion
 *       promise, such that its resolve/reject value will be forwarded along
 *       when it arrives. If the resolve/reject callback returns void, the
 *       completion promise is resolved/rejected with the same value that was
 *       passed to the callback.
 *
 * The MozPromise APIs skirt traditional XPCOM convention by returning nsRefPtrs
 * (rather than already_AddRefed) from various methods. This is done to allow
 * elegant chaining of calls without cluttering up the code with intermediate
 * variables, and without introducing separate API variants for callers that
 * want a return value (from, say, ->Then()) from those that don't.
 *
 * When IsExclusive is true, the MozPromise does a release-mode assertion that
 * there is at most one call to either Then(...) or ChainTo(...).
 */

class MozPromiseRefcountable {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MozPromiseRefcountable)
 protected:
  virtual ~MozPromiseRefcountable() = default;
};

class MozPromiseBase : public MozPromiseRefcountable {
 public:
  virtual void AssertIsDead() = 0;
};

template <typename T>
class MozPromiseHolder;
template <typename T>
class MozPromiseRequestHolder;
template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise : public MozPromiseBase {
  static const uint32_t sMagic = 0xcecace11;

  // Return a |T&&| to enable move when IsExclusive is true or
  // a |const T&| to enforce copy otherwise.
  template <typename T,
            typename R = std::conditional_t<IsExclusive, T&&, const T&>>
  static R MaybeMove(T& aX) {
    return static_cast<R>(aX);
  }

 public:
  using ResolveValueType = ResolveValueT;
  using RejectValueType = RejectValueT;
  class ResolveOrRejectValue {
   public:
    template <typename ResolveValueType_>
    void SetResolve(ResolveValueType_&& aResolveValue) {
      MOZ_ASSERT(IsNothing());
      mValue = Storage(VariantIndex<ResolveIndex>{},
                       std::forward<ResolveValueType_>(aResolveValue));
    }

    template <typename RejectValueType_>
    void SetReject(RejectValueType_&& aRejectValue) {
      MOZ_ASSERT(IsNothing());
      mValue = Storage(VariantIndex<RejectIndex>{},
                       std::forward<RejectValueType_>(aRejectValue));
    }

    template <typename ResolveValueType_>
    static ResolveOrRejectValue MakeResolve(ResolveValueType_&& aResolveValue) {
      ResolveOrRejectValue val;
      val.SetResolve(std::forward<ResolveValueType_>(aResolveValue));
      return val;
    }

    template <typename RejectValueType_>
    static ResolveOrRejectValue MakeReject(RejectValueType_&& aRejectValue) {
      ResolveOrRejectValue val;
      val.SetReject(std::forward<RejectValueType_>(aRejectValue));
      return val;
    }

    bool IsResolve() const { return mValue.template is<ResolveIndex>(); }
    bool IsReject() const { return mValue.template is<RejectIndex>(); }
    bool IsNothing() const { return mValue.template is<NothingIndex>(); }

    const ResolveValueType& ResolveValue() const {
      return mValue.template as<ResolveIndex>();
    }
    ResolveValueType& ResolveValue() {
      return mValue.template as<ResolveIndex>();
    }
    const RejectValueType& RejectValue() const {
      return mValue.template as<RejectIndex>();
    }
    RejectValueType& RejectValue() { return mValue.template as<RejectIndex>(); }

   private:
    enum { NothingIndex, ResolveIndex, RejectIndex };
    using Storage = Variant<Nothing, ResolveValueType, RejectValueType>;
    Storage mValue = Storage(VariantIndex<NothingIndex>{});
  };

 protected:
  // MozPromise is the public type, and never constructed directly. Construct
  // a MozPromise::Private, defined below.
  MozPromise(StaticString aCreationSite, bool aIsCompletionPromise)
      : mCreationSite(aCreationSite),
        mMutex("MozPromise Mutex"),
        mHaveRequest(false),
        mIsCompletionPromise(aIsCompletionPromise)
#ifdef PROMISE_DEBUG
        ,
        mMagic4(&mMutex)
#endif
  {
    PROMISE_LOG("%s creating MozPromise (%p)", mCreationSite.get(), this);
  }

 public:
  // MozPromise::Private allows us to separate the public interface (upon which
  // consumers of the promise may invoke methods like Then()) from the private
  // interface (upon which the creator of the promise may invoke Resolve() or
  // Reject()). APIs should create and store a MozPromise::Private (usually
  // via a MozPromiseHolder), and return a MozPromise to consumers.
  //
  // NB: We can include the definition of this class inline once B2G ICS is
  // gone.
  class Private;

  template <typename ResolveValueType_>
  [[nodiscard]] static RefPtr<MozPromise> CreateAndResolve(
      ResolveValueType_&& aResolveValue, StaticString aResolveSite) {
    static_assert(std::is_convertible_v<ResolveValueType_, ResolveValueT>,
                  "Resolve() argument must be implicitly convertible to "
                  "MozPromise's ResolveValueT");
    RefPtr<typename MozPromise::Private> p =
        new MozPromise::Private(aResolveSite);
    p->Resolve(std::forward<ResolveValueType_>(aResolveValue), aResolveSite);
    return p;
  }

  template <typename RejectValueType_>
  [[nodiscard]] static RefPtr<MozPromise> CreateAndReject(
      RejectValueType_&& aRejectValue, StaticString aRejectSite) {
    static_assert(std::is_convertible_v<RejectValueType_, RejectValueT>,
                  "Reject() argument must be implicitly convertible to "
                  "MozPromise's RejectValueT");
    RefPtr<typename MozPromise::Private> p =
        new MozPromise::Private(aRejectSite);
    p->Reject(std::forward<RejectValueType_>(aRejectValue), aRejectSite);
    return p;
  }

  template <typename ResolveOrRejectValueType_>
  [[nodiscard]] static RefPtr<MozPromise> CreateAndResolveOrReject(
      ResolveOrRejectValueType_&& aValue, StaticString aSite) {
    RefPtr<typename MozPromise::Private> p = new MozPromise::Private(aSite);
    p->ResolveOrReject(std::forward<ResolveOrRejectValueType_>(aValue), aSite);
    return p;
  }

  using AllPromiseType = MozPromise<CopyableTArray<ResolveValueType>,
                                    RejectValueType, IsExclusive>;
  using AllSettledPromiseType =
      MozPromise<CopyableTArray<ResolveOrRejectValue>, bool, IsExclusive>;

 private:
  class AllPromiseHolder : public MozPromiseRefcountable {
   public:
    explicit AllPromiseHolder(size_t aDependentPromises)
        : mPromise(new typename AllPromiseType::Private(__func__)),
          mOutstandingPromises(aDependentPromises) {
      MOZ_ASSERT(aDependentPromises > 0);
      mResolveValues.SetLength(aDependentPromises);
    }

    template <typename ResolveValueType_>
    void Resolve(size_t aIndex, ResolveValueType_&& aResolveValue) {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mResolveValues[aIndex].emplace(
          std::forward<ResolveValueType_>(aResolveValue));
      if (--mOutstandingPromises == 0) {
        nsTArray<ResolveValueType> resolveValues;
        resolveValues.SetCapacity(mResolveValues.Length());
        for (auto&& resolveValue : mResolveValues) {
          resolveValues.AppendElement(std::move(resolveValue.ref()));
        }

        mPromise->Resolve(std::move(resolveValues), __func__);
        mPromise = nullptr;
        mResolveValues.Clear();
      }
    }

    template <typename RejectValueType_>
    void Reject(RejectValueType_&& aRejectValue) {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mPromise->Reject(std::forward<RejectValueType_>(aRejectValue), __func__);
      mPromise = nullptr;
      mResolveValues.Clear();
    }

    AllPromiseType* Promise() { return mPromise; }

   private:
    nsTArray<Maybe<ResolveValueType>> mResolveValues;
    RefPtr<typename AllPromiseType::Private> mPromise;
    size_t mOutstandingPromises;
  };

  // Trying to pass ResolveOrRejectValue by value fails static analysis checks,
  // so we need to use either a const& or an rvalue reference, depending on
  // whether IsExclusive is true or not.
  using ResolveOrRejectValueParam =
      std::conditional_t<IsExclusive, ResolveOrRejectValue&&,
                         const ResolveOrRejectValue&>;

  using ResolveValueTypeParam =
      std::conditional_t<IsExclusive, ResolveValueType&&,
                         const ResolveValueType&>;

  using RejectValueTypeParam =
      std::conditional_t<IsExclusive, RejectValueType&&,
                         const RejectValueType&>;

  class AllSettledPromiseHolder : public MozPromiseRefcountable {
   public:
    explicit AllSettledPromiseHolder(size_t aDependentPromises)
        : mPromise(new typename AllSettledPromiseType::Private(__func__)),
          mOutstandingPromises(aDependentPromises) {
      MOZ_ASSERT(aDependentPromises > 0);
      mValues.SetLength(aDependentPromises);
    }

    void Settle(size_t aIndex, ResolveOrRejectValueParam aValue) {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mValues[aIndex].emplace(MaybeMove(aValue));
      if (--mOutstandingPromises == 0) {
        nsTArray<ResolveOrRejectValue> values;
        values.SetCapacity(mValues.Length());
        for (auto&& value : mValues) {
          values.AppendElement(std::move(value.ref()));
        }

        mPromise->Resolve(std::move(values), __func__);
        mPromise = nullptr;
        mValues.Clear();
      }
    }

    AllSettledPromiseType* Promise() { return mPromise; }

   private:
    nsTArray<Maybe<ResolveOrRejectValue>> mValues;
    RefPtr<typename AllSettledPromiseType::Private> mPromise;
    size_t mOutstandingPromises;
  };

 public:
  [[nodiscard]] static RefPtr<AllPromiseType> All(
      nsISerialEventTarget* aProcessingTarget,
      nsTArray<RefPtr<MozPromise>>& aPromises) {
    if (aPromises.Length() == 0) {
      return AllPromiseType::CreateAndResolve(
          CopyableTArray<ResolveValueType>(), __func__);
    }

    RefPtr<AllPromiseHolder> holder = new AllPromiseHolder(aPromises.Length());
    RefPtr<AllPromiseType> promise = holder->Promise();
    for (size_t i = 0; i < aPromises.Length(); ++i) {
      aPromises[i]->Then(
          aProcessingTarget, __func__,
          [holder, i](ResolveValueTypeParam aResolveValue) -> void {
            holder->Resolve(i, MaybeMove(aResolveValue));
          },
          [holder](RejectValueTypeParam aRejectValue) -> void {
            holder->Reject(MaybeMove(aRejectValue));
          });
    }
    return promise;
  }

  [[nodiscard]] static RefPtr<AllSettledPromiseType> AllSettled(
      nsISerialEventTarget* aProcessingTarget,
      nsTArray<RefPtr<MozPromise>>& aPromises) {
    if (aPromises.Length() == 0) {
      return AllSettledPromiseType::CreateAndResolve(
          CopyableTArray<ResolveOrRejectValue>(), __func__);
    }

    RefPtr<AllSettledPromiseHolder> holder =
        new AllSettledPromiseHolder(aPromises.Length());
    RefPtr<AllSettledPromiseType> promise = holder->Promise();
    for (size_t i = 0; i < aPromises.Length(); ++i) {
      aPromises[i]->Then(aProcessingTarget, __func__,
                         [holder, i](ResolveOrRejectValueParam aValue) -> void {
                           holder->Settle(i, MaybeMove(aValue));
                         });
    }
    return promise;
  }

  class Request : public MozPromiseRefcountable {
   public:
    virtual void Disconnect() = 0;

   protected:
    Request() : mComplete(false), mDisconnected(false) {}
    virtual ~Request() = default;

    bool mComplete;
    bool mDisconnected;
  };

 protected:
  /*
   * A ThenValue tracks a single consumer waiting on the promise. When a
   * consumer invokes promise->Then(...), a ThenValue is created. Once the
   * Promise is resolved or rejected, a {Resolve,Reject}Runnable is dispatched,
   * which invokes the resolve/reject method and then deletes the ThenValue.
   */
  class ThenValueBase : public Request {
    friend class MozPromise;
    static const uint32_t sMagic = 0xfadece11;

   public:
    class ResolveOrRejectRunnable final
        : public PrioritizableCancelableRunnable {
     public:
      ResolveOrRejectRunnable(ThenValueBase* aThenValue, MozPromise* aPromise)
          : PrioritizableCancelableRunnable(
                aPromise->mPriority,
                "MozPromise::ThenValueBase::ResolveOrRejectRunnable"),
            mThenValue(aThenValue),
            mPromise(aPromise) {
        MOZ_DIAGNOSTIC_ASSERT(!mPromise->IsPending());
      }

      ~ResolveOrRejectRunnable() {
        if (mThenValue) {
          mThenValue->AssertIsDead();
        }
      }

      NS_IMETHOD Run() override {
        PROMISE_LOG("ResolveOrRejectRunnable::Run() [this=%p]", this);
        mThenValue->DoResolveOrReject(mPromise->Value());
        mThenValue = nullptr;
        mPromise = nullptr;
        return NS_OK;
      }

      nsresult Cancel() override { return Run(); }

     private:
      RefPtr<ThenValueBase> mThenValue;
      RefPtr<MozPromise> mPromise;
    };

    ThenValueBase(nsISerialEventTarget* aResponseTarget, StaticString aCallSite)
        : mResponseTarget(aResponseTarget), mCallSite(aCallSite) {
      MOZ_ASSERT(aResponseTarget);
    }

#ifdef PROMISE_DEBUG
    ~ThenValueBase() {
      mMagic1 = 0;
      mMagic2 = 0;
    }
#endif

    void AssertIsDead() {
      PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic);
      // We want to assert that this ThenValues is dead - that is to say, that
      // there are no consumers waiting for the result. In the case of a normal
      // ThenValue, we check that it has been disconnected, which is the way
      // that the consumer signals that it no longer wishes to hear about the
      // result. If this ThenValue has a completion promise (which is mutually
      // exclusive with being disconnectable), we recursively assert that every
      // ThenValue associated with the completion promise is dead.
      if (MozPromiseBase* p = CompletionPromise()) {
        p->AssertIsDead();
      } else {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
        if (MOZ_UNLIKELY(!Request::mDisconnected)) {
          MOZ_CRASH_UNSAFE_PRINTF(
              "MozPromise::ThenValue created from '%s' destroyed without being "
              "either disconnected, resolved, or rejected (dispatchRv: %s)",
              mCallSite.get(),
              mDispatchRv ? GetStaticErrorName(*mDispatchRv)
                          : "not dispatched");
        }
#endif
      }
    }

    void Dispatch(MozPromise* aPromise) {
      PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic);
      aPromise->mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!aPromise->IsPending());

      nsCOMPtr<nsIRunnable> r = new ResolveOrRejectRunnable(this, aPromise);
      PROMISE_LOG(
          "%s Then() call made from %s [Runnable=%p, Promise=%p, ThenValue=%p] "
          "%s dispatch",
          aPromise->mValue.IsResolve() ? "Resolving" : "Rejecting",
          mCallSite.get(), r.get(), aPromise, this,
          aPromise->mUseSynchronousTaskDispatch ? "synchronous"
          : aPromise->mUseDirectTaskDispatch    ? "directtask"
                                                : "normal");

      if (aPromise->mUseSynchronousTaskDispatch &&
          mResponseTarget->IsOnCurrentThread()) {
        PROMISE_LOG("ThenValue::Dispatch running task synchronously [this=%p]",
                    this);
        r->Run();
        return;
      }

      if (aPromise->mUseDirectTaskDispatch &&
          mResponseTarget->IsOnCurrentThread()) {
        PROMISE_LOG(
            "ThenValue::Dispatch dispatch task via direct task queue [this=%p]",
            this);
        nsCOMPtr<nsIDirectTaskDispatcher> dispatcher =
            do_QueryInterface(mResponseTarget);
        if (dispatcher) {
          SetDispatchRv(dispatcher->DispatchDirectTask(r.forget()));
          return;
        }
        NS_WARNING(
            nsPrintfCString(
                "Direct Task dispatching not available for thread \"%s\"",
                PR_GetThreadName(PR_GetCurrentThread()))
                .get());
        MOZ_DIAGNOSTIC_ASSERT(
            false,
            "mResponseTarget must implement nsIDirectTaskDispatcher for direct "
            "task dispatching");
      }

      // Promise consumers are allowed to disconnect the Request object and
      // then shut down the thread or task queue that the promise result would
      // be dispatched on. So we unfortunately can't assert that promise
      // dispatch succeeds. :-(
      // We do record whether or not it succeeds so that if the ThenValueBase is
      // then destroyed and it was not disconnected, we can include that
      // information in the assertion message.
      SetDispatchRv(mResponseTarget->Dispatch(r.forget()));
    }

    void Disconnect() override {
      MOZ_DIAGNOSTIC_ASSERT(mResponseTarget->IsOnCurrentThread());
      MOZ_DIAGNOSTIC_ASSERT(!Request::mComplete);
      Request::mDisconnected = true;

      // We could support rejecting the completion promise on disconnection, but
      // then we'd need to have some sort of default reject value. The use cases
      // of disconnection and completion promise chaining seem pretty
      // orthogonal, so let's use assert against it.
      MOZ_DIAGNOSTIC_ASSERT(!CompletionPromise());
    }

   protected:
    virtual MozPromiseBase* CompletionPromise() const = 0;
    virtual void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) = 0;

    void DoResolveOrReject(ResolveOrRejectValue& aValue) {
      PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic);
      MOZ_DIAGNOSTIC_ASSERT(mResponseTarget->IsOnCurrentThread());
      Request::mComplete = true;
      if (Request::mDisconnected) {
        PROMISE_LOG(
            "ThenValue::DoResolveOrReject disconnected - bailing out [this=%p]",
            this);
        return;
      }

      // Invoke the resolve or reject method.
      DoResolveOrRejectInternal(aValue);
    }

    void SetDispatchRv(nsresult aRv) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      mDispatchRv = Some(aRv);
#endif
    }

    nsCOMPtr<nsISerialEventTarget>
        mResponseTarget;  // May be released on any thread.
#ifdef PROMISE_DEBUG
    uint32_t mMagic1 = sMagic;
#endif
    StaticString mCallSite;
#ifdef PROMISE_DEBUG
    uint32_t mMagic2 = sMagic;
#endif
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    Maybe<nsresult> mDispatchRv;
#endif
  };

  /*
   * Helper to make the resolve/reject value argument "optional".
   */
  template <typename ThisType, typename MethodType, typename ValueType>
  static MethodReturnType<MethodType> InvokeMethod(ThisType* aThisVal,
                                                   MethodType aMethod,
                                                   ValueType&& aValue) {
    if constexpr (TakesAnyArguments<MethodType>) {
      return (aThisVal->*aMethod)(std::forward<ValueType>(aValue));
    } else {
      return (aThisVal->*aMethod)();
    }
  }

  template <bool SupportChaining, typename PromiseType, typename ThisType,
            typename MethodType, typename ValueType>
  static RefPtr<PromiseType> InvokeCallbackMethod(ThisType* aThisVal,
                                                  MethodType aMethod,
                                                  ValueType&& aValue) {
    if constexpr (SupportChaining) {
      return InvokeMethod(aThisVal, aMethod, std::forward<ValueType>(aValue));
    } else {
      InvokeMethod(aThisVal, aMethod, std::forward<ValueType>(aValue));
      return nullptr;
    }
  }

  template <typename PromiseType>
  static void MaybeChain(PromiseType* aFrom,
                         RefPtr<typename PromiseType::Private>&& aTo) {
    if (aTo) {
      MOZ_DIAGNOSTIC_ASSERT(
          aFrom,
          "Can't do promise chaining for a non-promise-returning method.");
      aFrom->ChainTo(aTo.forget(), "<chained completion promise>");
    }
  }

  template <typename>
  class ThenCommand;

  template <typename...>
  class ThenValue;

  template <typename ThisType, typename ResolveMethodType,
            typename RejectMethodType>
  class ThenValue<ThisType*, ResolveMethodType, RejectMethodType>
      : public ThenValueBase {
    friend class ThenCommand<ThenValue>;

    using R1 = RemoveSmartPointer<MethodReturnType<ResolveMethodType>>;
    using R2 = RemoveSmartPointer<MethodReturnType<RejectMethodType>>;
    constexpr static bool SupportChaining =
        IsMozPromise<R1> && std::is_same_v<R1, R2>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType = std::conditional_t<SupportChaining, R1, MozPromise>;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget, ThisType* aThisVal,
              ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod,
              StaticString aCallSite)
        : ThenValueBase(aResponseTarget, aCallSite),
          mThisVal(aThisVal),
          mResolveMethod(aResolveMethod),
          mRejectMethod(aRejectMethod) {}

    void Disconnect() override {
      ThenValueBase::Disconnect();

      // If a Request has been disconnected, we don't guarantee that the
      // resolve/reject runnable will be dispatched. Null out our refcounted
      // this-value now so that it's released predictably on the dispatch
      // thread.
      mThisVal = nullptr;
    }

   protected:
    MozPromiseBase* CompletionPromise() const override {
      return mCompletionPromise;
    }

    void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) override {
      RefPtr<PromiseType> result =
          aValue.IsResolve()
              ? InvokeCallbackMethod<SupportChaining, PromiseType>(
                    mThisVal.get(), mResolveMethod,
                    MaybeMove(aValue.ResolveValue()))
              : InvokeCallbackMethod<SupportChaining, PromiseType>(
                    mThisVal.get(), mRejectMethod,
                    MaybeMove(aValue.RejectValue()));

      // Null out mThisVal after invoking the callback so that any references
      // are released predictably on the dispatch thread. Otherwise, it would be
      // released on whatever thread last drops its reference to the ThenValue,
      // which may or may not be ok.
      mThisVal = nullptr;

      MaybeChain<PromiseType>(result, std::move(mCompletionPromise));
    }

   private:
    RefPtr<ThisType>
        mThisVal;  // Only accessed and refcounted on dispatch thread.
    ResolveMethodType mResolveMethod;
    RejectMethodType mRejectMethod;
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

  template <typename ThisType, typename ResolveRejectMethodType>
  class ThenValue<ThisType*, ResolveRejectMethodType> : public ThenValueBase {
    friend class ThenCommand<ThenValue>;

    using R1 = RemoveSmartPointer<MethodReturnType<ResolveRejectMethodType>>;
    constexpr static bool SupportChaining = IsMozPromise<R1>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType = std::conditional_t<SupportChaining, R1, MozPromise>;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget, ThisType* aThisVal,
              ResolveRejectMethodType aResolveRejectMethod,
              StaticString aCallSite)
        : ThenValueBase(aResponseTarget, aCallSite),
          mThisVal(aThisVal),
          mResolveRejectMethod(aResolveRejectMethod) {}

    void Disconnect() override {
      ThenValueBase::Disconnect();

      // If a Request has been disconnected, we don't guarantee that the
      // resolve/reject runnable will be dispatched. Null out our refcounted
      // this-value now so that it's released predictably on the dispatch
      // thread.
      mThisVal = nullptr;
    }

   protected:
    MozPromiseBase* CompletionPromise() const override {
      return mCompletionPromise;
    }

    void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) override {
      RefPtr<PromiseType> result =
          InvokeCallbackMethod<SupportChaining, PromiseType>(
              mThisVal.get(), mResolveRejectMethod, MaybeMove(aValue));

      // Null out mThisVal after invoking the callback so that any references
      // are released predictably on the dispatch thread. Otherwise, it would be
      // released on whatever thread last drops its reference to the ThenValue,
      // which may or may not be ok.
      mThisVal = nullptr;

      MaybeChain<PromiseType>(result, std::move(mCompletionPromise));
    }

   private:
    RefPtr<ThisType>
        mThisVal;  // Only accessed and refcounted on dispatch thread.
    ResolveRejectMethodType mResolveRejectMethod;
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

  // NB: We could use std::function here instead of a template if it were
  // supported. :-(
  template <typename ResolveFunction, typename RejectFunction>
  class ThenValue<ResolveFunction, RejectFunction> : public ThenValueBase {
    friend class ThenCommand<ThenValue>;

    using R1 = RemoveSmartPointer<MethodReturnType<ResolveFunction>>;
    using R2 = RemoveSmartPointer<MethodReturnType<RejectFunction>>;
    constexpr static bool SupportChaining =
        IsMozPromise<R1> && std::is_same_v<R1, R2>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType = std::conditional_t<SupportChaining, R1, MozPromise>;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget,
              ResolveFunction&& aResolveFunction,
              RejectFunction&& aRejectFunction, StaticString aCallSite)
        : ThenValueBase(aResponseTarget, aCallSite) {
      mResolveFunction.emplace(std::move(aResolveFunction));
      mRejectFunction.emplace(std::move(aRejectFunction));
    }

    void Disconnect() override {
      ThenValueBase::Disconnect();

      // If a Request has been disconnected, we don't guarantee that the
      // resolve/reject runnable will be dispatched. Destroy our callbacks
      // now so that any references in closures are released predictable on
      // the dispatch thread.
      mResolveFunction.reset();
      mRejectFunction.reset();
    }

   protected:
    MozPromiseBase* CompletionPromise() const override {
      return mCompletionPromise;
    }

    void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) override {
      // Note: The usage of InvokeCallbackMethod here requires that
      // ResolveFunction/RejectFunction are capture-lambdas (i.e. anonymous
      // classes with ::operator()), since it allows us to share code more
      // easily. We could fix this if need be, though it's quite easy to work
      // around by just capturing something.
      RefPtr<PromiseType> result =
          aValue.IsResolve()
              ? InvokeCallbackMethod<SupportChaining, PromiseType>(
                    mResolveFunction.ptr(), &ResolveFunction::operator(),
                    MaybeMove(aValue.ResolveValue()))
              : InvokeCallbackMethod<SupportChaining, PromiseType>(
                    mRejectFunction.ptr(), &RejectFunction::operator(),
                    MaybeMove(aValue.RejectValue()));

      // Destroy callbacks after invocation so that any references in closures
      // are released predictably on the dispatch thread. Otherwise, they would
      // be released on whatever thread last drops its reference to the
      // ThenValue, which may or may not be ok.
      mResolveFunction.reset();
      mRejectFunction.reset();

      MaybeChain<PromiseType>(result, std::move(mCompletionPromise));
    }

   private:
    Maybe<ResolveFunction>
        mResolveFunction;  // Only accessed and deleted on dispatch thread.
    Maybe<RejectFunction>
        mRejectFunction;  // Only accessed and deleted on dispatch thread.
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

  template <typename ResolveRejectFunction>
  class ThenValue<ResolveRejectFunction> : public ThenValueBase {
    friend class ThenCommand<ThenValue>;

    using R1 = RemoveSmartPointer<MethodReturnType<ResolveRejectFunction>>;
    constexpr static bool SupportChaining = IsMozPromise<R1>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType = std::conditional_t<SupportChaining, R1, MozPromise>;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget,
              ResolveRejectFunction&& aResolveRejectFunction,
              StaticString aCallSite)
        : ThenValueBase(aResponseTarget, aCallSite) {
      mResolveRejectFunction.emplace(std::move(aResolveRejectFunction));
    }

    void Disconnect() override {
      ThenValueBase::Disconnect();

      // If a Request has been disconnected, we don't guarantee that the
      // resolve/reject runnable will be dispatched. Destroy our callbacks
      // now so that any references in closures are released predictable on
      // the dispatch thread.
      mResolveRejectFunction.reset();
    }

   protected:
    MozPromiseBase* CompletionPromise() const override {
      return mCompletionPromise;
    }

    void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) override {
      // Note: The usage of InvokeCallbackMethod here requires that
      // ResolveRejectFunction is capture-lambdas (i.e. anonymous
      // classes with ::operator()), since it allows us to share code more
      // easily. We could fix this if need be, though it's quite easy to work
      // around by just capturing something.
      RefPtr<PromiseType> result =
          InvokeCallbackMethod<SupportChaining, PromiseType>(
              mResolveRejectFunction.ptr(), &ResolveRejectFunction::operator(),
              MaybeMove(aValue));

      // Destroy callbacks after invocation so that any references in closures
      // are released predictably on the dispatch thread. Otherwise, they would
      // be released on whatever thread last drops its reference to the
      // ThenValue, which may or may not be ok.
      mResolveRejectFunction.reset();

      MaybeChain<PromiseType>(result, std::move(mCompletionPromise));
    }

   private:
    Maybe<ResolveRejectFunction>
        mResolveRejectFunction;  // Only accessed and deleted on dispatch
                                 // thread.
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

  template <typename ResolveFunction>
  class MapValue final : public ThenValueBase {
    friend class ThenCommand<MapValue>;
    constexpr static const bool SupportChaining = true;
    using ResolveValueT_ = std::invoke_result_t<ResolveFunction, ResolveValueT>;
    using PromiseType = MozPromise<ResolveValueT_, RejectValueT, IsExclusive>;

   public:
    explicit MapValue(nsISerialEventTarget* aResponseTarget,
                      ResolveFunction&& f, StaticString aCallSite)
        : ThenValueBase(aResponseTarget, aCallSite),
          mResolveFunction(Some(std::forward<ResolveFunction>(f))) {}

   protected:
    void Disconnect() override {
      ThenValueBase::Disconnect();
      mResolveFunction.reset();
    }

    MozPromiseBase* CompletionPromise() const override {
      return mCompletionPromise;
    }

    void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) override {
      // Note that promise-chaining is always supported here; this function can
      // only transform from MozPromise<A, B, k> to MozPromise<A2, B, k>.
      auto value = MaybeMove(aValue);
      typename PromiseType::ResolveOrRejectValue output;

      if (value.IsResolve()) {
        output.SetResolve((*mResolveFunction)(std::move(value.ResolveValue())));
      } else {
        output.SetReject(std::move(value.RejectValue()));
      }

      if (mCompletionPromise) {
        mCompletionPromise->ResolveOrReject(std::move(output),
                                            ThenValueBase::mCallSite);
      }
    }

   private:
    Maybe<ResolveFunction> mResolveFunction;
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

  template <typename RejectFunction>
  class MapErrValue final : public ThenValueBase {
    friend class ThenCommand<MapErrValue>;
    constexpr static const bool SupportChaining = true;
    using RejectValueT_ = std::invoke_result_t<RejectFunction, RejectValueT>;
    using PromiseType = MozPromise<ResolveValueT, RejectValueT_, IsExclusive>;

   public:
    explicit MapErrValue(nsISerialEventTarget* aResponseTarget,
                         RejectFunction&& f, StaticString aCallSite)
        : ThenValueBase(aResponseTarget, aCallSite),
          mRejectFunction(Some(std::forward<RejectFunction>(f))) {}

   protected:
    void Disconnect() override {
      ThenValueBase::Disconnect();
      mRejectFunction.reset();
    }

    MozPromiseBase* CompletionPromise() const override {
      return mCompletionPromise;
    }

    void DoResolveOrRejectInternal(ResolveOrRejectValue& aValue) override {
      // Note that promise-chaining is always supported here; this function can
      // only transform from MozPromise<A, B, k> to MozPromise<A, B2, k>.
      auto value = MaybeMove(aValue);
      typename PromiseType::ResolveOrRejectValue output;

      if (value.IsResolve()) {
        output.SetResolve(std::move(value.ResolveValue()));
      } else {
        output.SetReject((*mRejectFunction)(std::move(value.RejectValue())));
      }

      if (mCompletionPromise) {
        mCompletionPromise->ResolveOrReject(std::move(output),
                                            ThenValueBase::mCallSite);
      }
    }

   private:
    Maybe<RejectFunction> mRejectFunction;
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

 public:
  void ThenInternal(already_AddRefed<ThenValueBase> aThenValue,
                    StaticString aCallSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    RefPtr<ThenValueBase> thenValue = aThenValue;
    MutexAutoLock lock(mMutex);
    MOZ_DIAGNOSTIC_ASSERT(
        !IsExclusive || !mHaveRequest,
        "Using an exclusive promise in a non-exclusive fashion");
    mHaveRequest = true;
    PROMISE_LOG("%s invoking Then() [this=%p, aThenValue=%p, isPending=%d]",
                aCallSite.get(), this, thenValue.get(), (int)IsPending());
    if (!IsPending()) {
      thenValue->Dispatch(this);
    } else {
      mThenValues.AppendElement(thenValue.forget());
    }
  }

 protected:
  /*
   * A command object to store all information needed to make a request to the
   * promise. This allows us to delay the request until further use is known
   * (whether it is ->Then() again for more promise chaining or ->Track() to
   * terminate chaining and issue the request).
   *
   * This allows a unified syntax for promise chaining and disconnection, and
   * feels more like its JS counterpart.
   *
   * Note that a ThenCommand is always exclusive, even if its source or result
   * promises are not. To attach multiple continuations, explicitly convert it
   * to a promise first.
   */
  template <typename ThenValueType>
  class MOZ_TEMPORARY_CLASS ThenCommand {
    // Allow Promise1::ThenCommand to access the private constructor
    // Promise2::ThenCommand(ThenCommand&&).
    template <typename, typename, bool>
    friend class MozPromise;

    using PromiseType = typename ThenValueType::PromiseType;
    using Private = typename PromiseType::Private;

    ThenCommand(StaticString aCallSite,
                already_AddRefed<ThenValueType> aThenValue,
                MozPromise* aReceiver)
        : mCallSite(aCallSite), mThenValue(aThenValue), mReceiver(aReceiver) {}

    ThenCommand(ThenCommand&& aOther) noexcept = default;

   public:
    ~ThenCommand() {
      // Issue the request now if the return value of Then() is not used.
      if (mThenValue) {
        mReceiver->ThenInternal(mThenValue.forget(), mCallSite);
      }
    }

    // Allow RefPtr<MozPromise> p = somePromise->Then();
    //       p->Then(thread1, ...);
    //       p->Then(thread2, ...);
    operator RefPtr<PromiseType>() {
      static_assert(
          ThenValueType::SupportChaining,
          "The resolve/reject callback needs to return a RefPtr<MozPromise> "
          "in order to do promise chaining.");

      // mCompletionPromise must be created before ThenInternal() to avoid race.
      RefPtr<Private> p =
          new Private("<completion promise>", true /* aIsCompletionPromise */);
      mThenValue->mCompletionPromise = p;
      // Note ThenInternal() might nullify mCompletionPromise before return.
      // So we need to return p instead of mCompletionPromise.
      mReceiver->ThenInternal(mThenValue.forget(), mCallSite);
      return p;
    }

    template <typename... Ts>
    auto Then(Ts&&... aArgs) -> decltype(std::declval<PromiseType>().Then(
        std::forward<Ts>(aArgs)...)) {
      return static_cast<RefPtr<PromiseType>>(*this)->Then(
          std::forward<Ts>(aArgs)...);
    }

    template <typename... Ts>
    auto Map(Ts&&... aArgs) -> decltype(std::declval<PromiseType>().Map(
        std::forward<Ts>(aArgs)...)) {
      return static_cast<RefPtr<PromiseType>>(*this)->Map(
          std::forward<Ts>(aArgs)...);
    }

    template <typename... Ts>
    auto MapErr(Ts&&... aArgs) -> decltype(std::declval<PromiseType>().MapErr(
        std::forward<Ts>(aArgs)...)) {
      return static_cast<RefPtr<PromiseType>>(*this)->MapErr(
          std::forward<Ts>(aArgs)...);
    }

    void Track(MozPromiseRequestHolder<MozPromise>& aRequestHolder) {
      aRequestHolder.Track(do_AddRef(mThenValue));
      mReceiver->ThenInternal(mThenValue.forget(), mCallSite);
    }

    // Allow calling ->Then() again for more promise chaining or ->Track() to
    // end chaining and track the request for future disconnection.
    ThenCommand* operator->() { return this; }

   private:
    StaticString mCallSite;
    RefPtr<ThenValueType> mThenValue;
    RefPtr<MozPromise> mReceiver;
  };

 public:
  template <typename ThisType, typename... Methods,
            typename ThenValueType = ThenValue<ThisType*, Methods...>,
            typename ReturnType = ThenCommand<ThenValueType>>
  ReturnType Then(nsISerialEventTarget* aResponseTarget, StaticString aCallSite,
                  ThisType* aThisVal, Methods... aMethods) {
    RefPtr<ThenValueType> thenValue =
        new ThenValueType(aResponseTarget, aThisVal, aMethods..., aCallSite);
    return ReturnType(aCallSite, thenValue.forget(), this);
  }

  template <typename... Functions,
            typename ThenValueType = ThenValue<Functions...>,
            typename ReturnType = ThenCommand<ThenValueType>>
  ReturnType Then(nsISerialEventTarget* aResponseTarget, StaticString aCallSite,
                  Functions&&... aFunctions) {
    RefPtr<ThenValueType> thenValue =
        new ThenValueType(aResponseTarget, std::move(aFunctions)..., aCallSite);
    return ReturnType(aCallSite, thenValue.forget(), this);
  }

  // Shorthand for a `Then` which simply forwards the reject-value, but performs
  // some additional work with the resolve-value.
  template <typename Function>
  auto Map(nsISerialEventTarget* aResponseTarget, StaticString aCallSite,
           Function&& function) {
    RefPtr<MapValue<Function>> thenValue = new MapValue<Function>(
        aResponseTarget, std::forward<Function>(function), aCallSite);
    return ThenCommand<MapValue<Function>>(aCallSite, thenValue.forget(), this);
  }

  // Shorthand for a `Then` which simply forwards the resolve-value, but
  // performs some additional work with the reject-value.
  template <typename Function>
  auto MapErr(nsISerialEventTarget* aResponseTarget, StaticString aCallSite,
              Function&& function) {
    RefPtr<MapErrValue<Function>> thenValue = new MapErrValue<Function>(
        aResponseTarget, std::forward<Function>(function), aCallSite);
    return ThenCommand<MapErrValue<Function>>(aCallSite, thenValue.forget(),
                                              this);
  }

  void ChainTo(already_AddRefed<Private> aChainedPromise,
               StaticString aCallSite) {
    MutexAutoLock lock(mMutex);
    MOZ_DIAGNOSTIC_ASSERT(
        !IsExclusive || !mHaveRequest,
        "Using an exclusive promise in a non-exclusive fashion");
    mHaveRequest = true;
    RefPtr<Private> chainedPromise = aChainedPromise;
    PROMISE_LOG(
        "%s invoking Chain() [this=%p, chainedPromise=%p, isPending=%d]",
        aCallSite.get(), this, chainedPromise.get(), (int)IsPending());

    // We want to use the same type of dispatching method with the chained
    // promises.

    // We need to ensure that the UseSynchronousTaskDispatch branch isn't taken
    // at compilation time to ensure we're not triggering the static_assert in
    // UseSynchronousTaskDispatch method. if constexpr (IsExclusive) ensures
    // that.
    if (mUseDirectTaskDispatch) {
      chainedPromise->UseDirectTaskDispatch(aCallSite);
    } else if constexpr (IsExclusive) {
      if (mUseSynchronousTaskDispatch) {
        chainedPromise->UseSynchronousTaskDispatch(aCallSite);
      }
    } else {
      chainedPromise->SetTaskPriority(mPriority, aCallSite);
    }

    if (!IsPending()) {
      ForwardTo(chainedPromise);
    } else {
      mChainedPromises.AppendElement(chainedPromise);
    }
  }

#ifdef MOZ_WIDGET_ANDROID
  // Creates a C++ MozPromise from its Java counterpart, GeckoResult.
  [[nodiscard]] static RefPtr<MozPromise> FromGeckoResult(
      java::GeckoResult::Param aGeckoResult) {
    using jni::GeckoResultCallback;
    RefPtr<Private> p = new Private("GeckoResult Glue", false);
    auto resolve = GeckoResultCallback::CreateAndAttach<ResolveValueType>(
        [p](ResolveValueType&& aArg) {
          p->Resolve(MaybeMove(aArg), __func__);
        });
    auto reject = GeckoResultCallback::CreateAndAttach<RejectValueType>(
        [p](RejectValueType&& aArg) { p->Reject(MaybeMove(aArg), __func__); });
    aGeckoResult->NativeThen(resolve, reject);
    return p;
  }
#endif

  // Note we expose the function AssertIsDead() instead of IsDead() since
  // checking IsDead() is a data race in the situation where the request is not
  // dead. Therefore we enforce the form |Assert(IsDead())| by exposing
  // AssertIsDead() only.
  void AssertIsDead() override {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    for (auto&& then : mThenValues) {
      then->AssertIsDead();
    }
    for (auto&& chained : mChainedPromises) {
      chained->AssertIsDead();
    }
  }

  bool IsResolved() const { return mValue.IsResolve(); }

 protected:
  bool IsPending() const { return mValue.IsNothing(); }

  ResolveOrRejectValue& Value() {
    // This method should only be called once the value has stabilized. As
    // such, we don't need to acquire the lock here.
    MOZ_DIAGNOSTIC_ASSERT(!IsPending());
    return mValue;
  }

  void DispatchAll() {
    mMutex.AssertCurrentThreadOwns();
    for (auto&& thenValue : mThenValues) {
      thenValue->Dispatch(this);
    }
    mThenValues.Clear();

    for (auto&& chainedPromise : mChainedPromises) {
      ForwardTo(chainedPromise);
    }
    mChainedPromises.Clear();
  }

  void ForwardTo(Private* aOther) {
    MOZ_ASSERT(!IsPending());
    if (mValue.IsResolve()) {
      aOther->Resolve(MaybeMove(mValue.ResolveValue()), "<chained promise>");
    } else {
      aOther->Reject(MaybeMove(mValue.RejectValue()), "<chained promise>");
    }
  }

  virtual ~MozPromise() {
    PROMISE_LOG("MozPromise::~MozPromise [this=%p]", this);
    AssertIsDead();
    // We can't guarantee a completion promise will always be revolved or
    // rejected since ResolveOrRejectRunnable might not run when dispatch fails.
    if (!mIsCompletionPromise) {
      MOZ_ASSERT(!IsPending());
      MOZ_ASSERT(mThenValues.IsEmpty());
      MOZ_ASSERT(mChainedPromises.IsEmpty());
    }
#ifdef PROMISE_DEBUG
    mMagic1 = 0;
    mMagic2 = 0;
    mMagic3 = 0;
    mMagic4 = nullptr;
#endif
  };

  StaticString mCreationSite;  // For logging
  Mutex mMutex MOZ_UNANNOTATED;
  ResolveOrRejectValue mValue;
  bool mUseSynchronousTaskDispatch = false;
  bool mUseDirectTaskDispatch = false;
  uint32_t mPriority = nsIRunnablePriority::PRIORITY_NORMAL;
#ifdef PROMISE_DEBUG
  uint32_t mMagic1 = sMagic;
#endif
  // Try shows we never have more than 3 elements when IsExclusive is false.
  // So '3' is a good value to avoid heap allocation in most cases.
  AutoTArray<RefPtr<ThenValueBase>, IsExclusive ? 1 : 3> mThenValues;
#ifdef PROMISE_DEBUG
  uint32_t mMagic2 = sMagic;
#endif
  nsTArray<RefPtr<Private>> mChainedPromises;
#ifdef PROMISE_DEBUG
  uint32_t mMagic3 = sMagic;
#endif
  bool mHaveRequest;
  const bool mIsCompletionPromise;
#ifdef PROMISE_DEBUG
  void* mMagic4;
#endif
};

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise<ResolveValueT, RejectValueT, IsExclusive>::Private
    : public MozPromise<ResolveValueT, RejectValueT, IsExclusive> {
 public:
  explicit Private(StaticString aCreationSite,
                   bool aIsCompletionPromise = false)
      : MozPromise(aCreationSite, aIsCompletionPromise) {}

  template <typename ResolveValueT_>
  void Resolve(ResolveValueT_&& aResolveValue, StaticString aResolveSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s resolving MozPromise (%p created at %s)",
                aResolveSite.get(), this, mCreationSite.get());
    if (!IsPending()) {
      PROMISE_LOG(
          "%s ignored already resolved or rejected MozPromise (%p created at "
          "%s)",
          aResolveSite.get(), this, mCreationSite.get());
      return;
    }
    mValue.SetResolve(std::forward<ResolveValueT_>(aResolveValue));
    DispatchAll();
  }

  template <typename RejectValueT_>
  void Reject(RejectValueT_&& aRejectValue, StaticString aRejectSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s rejecting MozPromise (%p created at %s)", aRejectSite.get(),
                this, mCreationSite.get());
    if (!IsPending()) {
      PROMISE_LOG(
          "%s ignored already resolved or rejected MozPromise (%p created at "
          "%s)",
          aRejectSite.get(), this, mCreationSite.get());
      return;
    }
    mValue.SetReject(std::forward<RejectValueT_>(aRejectValue));
    DispatchAll();
  }

  template <typename ResolveOrRejectValue_>
  void ResolveOrReject(ResolveOrRejectValue_&& aValue, StaticString aSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s resolveOrRejecting MozPromise (%p created at %s)",
                aSite.get(), this, mCreationSite.get());
    if (!IsPending()) {
      PROMISE_LOG(
          "%s ignored already resolved or rejected MozPromise (%p created at "
          "%s)",
          aSite.get(), this, mCreationSite.get());
      return;
    }
    mValue = std::forward<ResolveOrRejectValue_>(aValue);
    DispatchAll();
  }

  // If the caller and target are both on the same thread, run the the resolve
  // or reject callback synchronously. Otherwise, the task will be dispatched
  // via the target Dispatch method.
  void UseSynchronousTaskDispatch(const char* aSite) {
    static_assert(
        IsExclusive,
        "Synchronous dispatch can only be used with exclusive promises");
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s UseSynchronousTaskDispatch MozPromise (%p created at %s)",
                aSite, this, mCreationSite.get());
    MOZ_ASSERT(IsPending(),
               "A Promise must not have been already resolved or rejected to "
               "set dispatch state");
    mUseSynchronousTaskDispatch = true;
  }

  // If the caller and target are both on the same thread, run the
  // resolve/reject callback off the direct task queue instead. This avoids a
  // full trip to the back of the event queue for each additional asynchronous
  // step when using MozPromise, and is similar (but not identical to) the
  // microtask semantics of JS promises.
  void UseDirectTaskDispatch(const char* aSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s UseDirectTaskDispatch MozPromise (%p created at %s)", aSite,
                this, mCreationSite.get());
    MOZ_ASSERT(IsPending(),
               "A Promise must not have been already resolved or rejected to "
               "set dispatch state");
    MOZ_ASSERT(!mUseSynchronousTaskDispatch,
               "Promise already set for synchronous dispatch");
    mUseDirectTaskDispatch = true;
  }

  // If the resolve/reject will be handled on a thread supporting priorities,
  // one may want to tweak the priority of the task by passing a
  // nsIRunnablePriority::PRIORITY_* to SetTaskPriority.
  void SetTaskPriority(uint32_t aPriority, const char* aSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s TaskPriority MozPromise (%p created at %s)", aSite, this,
                mCreationSite.get());
    MOZ_ASSERT(IsPending(),
               "A Promise must not have been already resolved or rejected to "
               "set dispatch state");
    MOZ_ASSERT(!mUseSynchronousTaskDispatch,
               "Promise already set for synchronous dispatch");
    MOZ_ASSERT(!mUseDirectTaskDispatch,
               "Promise already set for direct dispatch");
    mPriority = aPriority;
  }
};

// A generic promise type that does the trick for simple use cases.
//
// Vaguely deprecated: prefer explicitly naming the resolve- and reject-type.
// Additionally, prefer `mozilla::Ok` as the resolve-type if the boolean's value
// is irrelevant.
using GenericPromise = MozPromise<bool, nsresult, /* IsExclusive = */ true>;

// A generic, non-exclusive promise type that does the trick for simple use
// cases.
//
// Vaguely deprecated, as above.
using GenericNonExclusivePromise =
    MozPromise<bool, nsresult, /* IsExclusive = */ false>;

/*
 * Class to encapsulate a promise for a particular role. Use this as the member
 * variable for a class whose method returns a promise.
 */
template <typename PromiseType, typename ImplType>
class MozPromiseHolderBase {
 public:
  MozPromiseHolderBase() = default;

  MozPromiseHolderBase(MozPromiseHolderBase&& aOther) noexcept = default;
  MozPromiseHolderBase& operator=(MozPromiseHolderBase&& aOther) noexcept =
      default;

  ~MozPromiseHolderBase() { MOZ_ASSERT(!mPromise); }

  already_AddRefed<PromiseType> Ensure(StaticString aMethodName) {
    static_cast<ImplType*>(this)->Check();
    if (!mPromise) {
      mPromise = new (typename PromiseType::Private)(aMethodName);
    }
    RefPtr<PromiseType> p = mPromise.get();
    return p.forget();
  }

  bool IsEmpty() const {
    static_cast<const ImplType*>(this)->Check();
    return !mPromise;
  }

  already_AddRefed<typename PromiseType::Private> Steal() {
    static_cast<ImplType*>(this)->Check();
    return mPromise.forget();
  }

  template <typename ResolveValueType_>
  void Resolve(ResolveValueType_&& aResolveValue, StaticString aMethodName) {
    static_assert(std::is_convertible_v<ResolveValueType_,
                                        typename PromiseType::ResolveValueType>,
                  "Resolve() argument must be implicitly convertible to "
                  "MozPromise's ResolveValueT");

    static_cast<ImplType*>(this)->Check();
    MOZ_ASSERT(mPromise);
    mPromise->Resolve(std::forward<ResolveValueType_>(aResolveValue),
                      aMethodName);
    mPromise = nullptr;
  }

  template <typename ResolveValueType_>
  void ResolveIfExists(ResolveValueType_&& aResolveValue,
                       StaticString aMethodName) {
    if (!IsEmpty()) {
      Resolve(std::forward<ResolveValueType_>(aResolveValue), aMethodName);
    }
  }

  template <typename RejectValueType_>
  void Reject(RejectValueType_&& aRejectValue, StaticString aMethodName) {
    static_assert(std::is_convertible_v<RejectValueType_,
                                        typename PromiseType::RejectValueType>,
                  "Reject() argument must be implicitly convertible to "
                  "MozPromise's RejectValueT");

    static_cast<ImplType*>(this)->Check();
    MOZ_ASSERT(mPromise);
    mPromise->Reject(std::forward<RejectValueType_>(aRejectValue), aMethodName);
    mPromise = nullptr;
  }

  template <typename RejectValueType_>
  void RejectIfExists(RejectValueType_&& aRejectValue,
                      StaticString aMethodName) {
    if (!IsEmpty()) {
      Reject(std::forward<RejectValueType_>(aRejectValue), aMethodName);
    }
  }

  template <typename ResolveOrRejectValueType_>
  void ResolveOrReject(ResolveOrRejectValueType_&& aValue,
                       StaticString aMethodName) {
    static_cast<ImplType*>(this)->Check();
    MOZ_ASSERT(mPromise);
    mPromise->ResolveOrReject(std::forward<ResolveOrRejectValueType_>(aValue),
                              aMethodName);
    mPromise = nullptr;
  }

  template <typename ResolveOrRejectValueType_>
  void ResolveOrRejectIfExists(ResolveOrRejectValueType_&& aValue,
                               StaticString aMethodName) {
    if (!IsEmpty()) {
      ResolveOrReject(std::forward<ResolveOrRejectValueType_>(aValue),
                      aMethodName);
    }
  }

  void UseSynchronousTaskDispatch(const char* aSite) {
    MOZ_ASSERT(mPromise);
    mPromise->UseSynchronousTaskDispatch(aSite);
  }

  void UseDirectTaskDispatch(const char* aSite) {
    MOZ_ASSERT(mPromise);
    mPromise->UseDirectTaskDispatch(aSite);
  }

  void SetTaskPriority(uint32_t aPriority, const char* aSite) {
    MOZ_ASSERT(mPromise);
    mPromise->SetTaskPriority(aPriority, aSite);
  }

 private:
  RefPtr<typename PromiseType::Private> mPromise;
};

template <typename PromiseType>
class MozPromiseHolder
    : public MozPromiseHolderBase<PromiseType, MozPromiseHolder<PromiseType>> {
 public:
  using MozPromiseHolderBase<
      PromiseType, MozPromiseHolder<PromiseType>>::MozPromiseHolderBase;
  static constexpr void Check(){};
};

template <typename PromiseType>
class MozMonitoredPromiseHolder
    : public MozPromiseHolderBase<PromiseType,
                                  MozMonitoredPromiseHolder<PromiseType>> {
 public:
  // Provide a Monitor that should always be held when accessing this instance.
  explicit MozMonitoredPromiseHolder(Monitor* const aMonitor)
      : mMonitor(aMonitor) {
    MOZ_ASSERT(aMonitor);
  }

  MozMonitoredPromiseHolder(MozMonitoredPromiseHolder&& aOther) = delete;
  MozMonitoredPromiseHolder& operator=(MozMonitoredPromiseHolder&& aOther) =
      delete;

  void Check() const { mMonitor->AssertCurrentThreadOwns(); }

 private:
  Monitor* const mMonitor;
};

/*
 * Class to encapsulate a MozPromise::Request reference. Use this as the member
 * variable for a class waiting on a MozPromise.
 */
template <typename PromiseType>
class MozPromiseRequestHolder {
 public:
  MozPromiseRequestHolder() = default;
  ~MozPromiseRequestHolder() { MOZ_ASSERT(!mRequest); }

  void Track(already_AddRefed<typename PromiseType::Request> aRequest) {
    MOZ_DIAGNOSTIC_ASSERT(!Exists());
    mRequest = aRequest;
  }

  void Complete() {
    MOZ_DIAGNOSTIC_ASSERT(Exists());
    mRequest = nullptr;
  }

  // Disconnects and forgets an outstanding promise. The resolve/reject methods
  // will never be called.
  void Disconnect() {
    MOZ_ASSERT(Exists());
    RefPtr request = std::move(mRequest);
    request->Disconnect();
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
// This machinery allows callers to schedule a promise-returning function
// (a method and object, or a function object like a lambda) to be invoked
// asynchronously on a given thread, while at the same time receiving a
// promise upon which to invoke Then() immediately. InvokeAsync dispatches a
// task to invoke the function on the proper thread and also chain the
// resulting promise to the one that the caller received, so that resolve/
// reject values are forwarded through.

namespace detail {

// Non-templated base class to allow us to use MOZ_COUNT_{C,D}TOR, which cause
// assertions when used on templated types.
class MethodCallBase {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(MethodCallBase)
  MOZ_COUNTED_DTOR_VIRTUAL(MethodCallBase)
};

template <typename PromiseType, typename MethodType, typename ThisType,
          typename... Storages>
class MethodCall : public MethodCallBase {
 public:
  template <typename... Args>
  MethodCall(MethodType aMethod, ThisType* aThisVal, Args&&... aArgs)
      : mMethod(aMethod),
        mThisVal(aThisVal),
        mArgs(std::forward<Args>(aArgs)...) {
    static_assert(sizeof...(Storages) == sizeof...(Args),
                  "Storages and Args should have equal sizes");
  }

  RefPtr<PromiseType> Invoke() { return mArgs.apply(mThisVal.get(), mMethod); }

 private:
  MethodType mMethod;
  RefPtr<ThisType> mThisVal;
  RunnableMethodArguments<Storages...> mArgs;
};

template <typename PromiseType, typename MethodType, typename ThisType,
          typename... Storages>
class ProxyRunnable : public CancelableRunnable {
 public:
  ProxyRunnable(
      typename PromiseType::Private* aProxyPromise,
      MethodCall<PromiseType, MethodType, ThisType, Storages...>* aMethodCall)
      : CancelableRunnable("detail::ProxyRunnable"),
        mProxyPromise(aProxyPromise),
        mMethodCall(aMethodCall) {}

  NS_IMETHOD Run() override {
    RefPtr<PromiseType> p = mMethodCall->Invoke();
    mMethodCall = nullptr;
    p->ChainTo(mProxyPromise.forget(), "<Proxy Promise>");
    return NS_OK;
  }

  nsresult Cancel() override { return Run(); }

 private:
  RefPtr<typename PromiseType::Private> mProxyPromise;
  UniquePtr<MethodCall<PromiseType, MethodType, ThisType, Storages...>>
      mMethodCall;
};

template <typename... Storages, typename PromiseType, typename ThisType,
          typename... ArgTypes, typename... ActualArgTypes>
static RefPtr<PromiseType> InvokeAsyncImpl(
    nsISerialEventTarget* aTarget, ThisType* aThisVal, StaticString aCallerName,
    RefPtr<PromiseType> (ThisType::*aMethod)(ArgTypes...),
    ActualArgTypes&&... aArgs) {
  MOZ_ASSERT(aTarget);

  typedef RefPtr<PromiseType> (ThisType::*MethodType)(ArgTypes...);
  typedef detail::MethodCall<PromiseType, MethodType, ThisType, Storages...>
      MethodCallType;
  typedef detail::ProxyRunnable<PromiseType, MethodType, ThisType, Storages...>
      ProxyRunnableType;

  MethodCallType* methodCall = new MethodCallType(
      aMethod, aThisVal, std::forward<ActualArgTypes>(aArgs)...);
  RefPtr<typename PromiseType::Private> p =
      new (typename PromiseType::Private)(aCallerName);
  RefPtr<ProxyRunnableType> r = new ProxyRunnableType(p, methodCall);
  aTarget->Dispatch(r.forget());
  return p;
}

constexpr bool Any() { return false; }

template <typename T1>
constexpr bool Any(T1 a) {
  return static_cast<bool>(a);
}

template <typename T1, typename... Ts>
constexpr bool Any(T1 a, Ts... aOthers) {
  return a || Any(aOthers...);
}

}  // namespace detail

// InvokeAsync with explicitly-specified storages.
// See ParameterStorage in nsThreadUtils.h for help.
template <typename... Storages, typename PromiseType, typename ThisType,
          typename... ArgTypes, typename... ActualArgTypes,
          std::enable_if_t<sizeof...(Storages) != 0, int> = 0>
static RefPtr<PromiseType> InvokeAsync(
    nsISerialEventTarget* aTarget, ThisType* aThisVal, StaticString aCallerName,
    RefPtr<PromiseType> (ThisType::*aMethod)(ArgTypes...),
    ActualArgTypes&&... aArgs) {
  static_assert(
      sizeof...(Storages) == sizeof...(ArgTypes),
      "Provided Storages and method's ArgTypes should have equal sizes");
  static_assert(sizeof...(Storages) == sizeof...(ActualArgTypes),
                "Provided Storages and ActualArgTypes should have equal sizes");
  return detail::InvokeAsyncImpl<Storages...>(
      aTarget, aThisVal, aCallerName, aMethod,
      std::forward<ActualArgTypes>(aArgs)...);
}

// InvokeAsync with no explicitly-specified storages, will copy arguments and
// then move them out of the runnable into the target method parameters.
template <typename... Storages, typename PromiseType, typename ThisType,
          typename... ArgTypes, typename... ActualArgTypes,
          std::enable_if_t<sizeof...(Storages) == 0, int> = 0>
static RefPtr<PromiseType> InvokeAsync(
    nsISerialEventTarget* aTarget, ThisType* aThisVal, StaticString aCallerName,
    RefPtr<PromiseType> (ThisType::*aMethod)(ArgTypes...),
    ActualArgTypes&&... aArgs) {
  static_assert(
      !detail::Any(
          std::is_pointer_v<std::remove_reference_t<ActualArgTypes>>...),
      "Cannot pass pointer types through InvokeAsync, Storages must be "
      "provided");
  static_assert(sizeof...(ArgTypes) == sizeof...(ActualArgTypes),
                "Method's ArgTypes and ActualArgTypes should have equal sizes");
  return detail::InvokeAsyncImpl<
      StoreCopyPassByRRef<std::decay_t<ActualArgTypes>>...>(
      aTarget, aThisVal, aCallerName, aMethod,
      std::forward<ActualArgTypes>(aArgs)...);
}

namespace detail {

template <typename Function, typename PromiseType>
class ProxyFunctionRunnable : public CancelableRunnable {
  using FunctionStorage = std::decay_t<Function>;

 public:
  template <typename F>
  ProxyFunctionRunnable(typename PromiseType::Private* aProxyPromise,
                        F&& aFunction)
      : CancelableRunnable("detail::ProxyFunctionRunnable"),
        mProxyPromise(aProxyPromise),
        mFunction(new FunctionStorage(std::forward<F>(aFunction))) {}

  NS_IMETHOD Run() override {
    RefPtr<PromiseType> p = (*mFunction)();
    mFunction = nullptr;
    p->ChainTo(mProxyPromise.forget(), "<Proxy Promise>");
    return NS_OK;
  }

  nsresult Cancel() override { return Run(); }

 private:
  RefPtr<typename PromiseType::Private> mProxyPromise;
  UniquePtr<FunctionStorage> mFunction;
};

template <typename T>
constexpr static bool IsRefPtrMozPromise = false;
template <typename T, typename U, bool B>
constexpr static bool IsRefPtrMozPromise<RefPtr<MozPromise<T, U, B>>> = true;

}  // namespace detail

// Invoke a function object (e.g., lambda) asynchronously.
// Return a promise that the function should eventually resolve or reject.
template <typename Function>
static auto InvokeAsync(nsISerialEventTarget* aTarget, StaticString aCallerName,
                        Function&& aFunction) -> decltype(aFunction()) {
  static_assert(!std::is_lvalue_reference_v<Function>,
                "Function object must not be passed by lvalue-ref (to avoid "
                "unplanned copies); Consider move()ing the object.");

  static_assert(detail::IsRefPtrMozPromise<decltype(aFunction())>,
                "Function object must return RefPtr<MozPromise>");
  MOZ_ASSERT(aTarget);
  typedef RemoveSmartPointer<decltype(aFunction())> PromiseType;
  typedef detail::ProxyFunctionRunnable<Function, PromiseType>
      ProxyRunnableType;

  auto p = MakeRefPtr<typename PromiseType::Private>(aCallerName);
  auto r = MakeRefPtr<ProxyRunnableType>(p, std::forward<Function>(aFunction));
  aTarget->Dispatch(r.forget());
  return p;
}

#undef PROMISE_LOG
#undef PROMISE_ASSERT
#undef PROMISE_DEBUG

}  // namespace mozilla

#endif
