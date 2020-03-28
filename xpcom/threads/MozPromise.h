/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MozPromise_h_)
#  define MozPromise_h_

#  include "mozilla/Logging.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/Monitor.h"
#  include "mozilla/Mutex.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/Tuple.h"
#  include "mozilla/TypeTraits.h"
#  include "mozilla/UniquePtr.h"
#  include "mozilla/Variant.h"

#  include "nsISerialEventTarget.h"
#  include "nsTArray.h"
#  include "nsThreadUtils.h"

#  ifdef MOZ_WIDGET_ANDROID
#    include "mozilla/jni/GeckoResultUtils.h"
#  endif

#  if MOZ_DIAGNOSTIC_ASSERT_ENABLED
#    define PROMISE_DEBUG
#  endif

#  ifdef PROMISE_DEBUG
#    define PROMISE_ASSERT MOZ_RELEASE_ASSERT
#  else
#    define PROMISE_ASSERT(...) \
      do {                      \
      } while (0)
#  endif

namespace mozilla {

namespace dom {
class Promise;
}

extern LazyLogModule gMozPromiseLog;

#  define PROMISE_LOG(x, ...) \
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
struct MethodTrait : MethodTraitsHelper<typename RemoveReference<T>::Type> {};

}  // namespace detail

template <typename MethodType>
using TakesArgument =
    IntegralConstant<bool, detail::MethodTrait<MethodType>::ArgSize != 0>;

template <typename MethodType, typename TargetType>
using ReturnTypeIs =
    IsConvertible<typename detail::MethodTrait<MethodType>::ReturnType,
                  TargetType>;

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise;

template <typename Return>
struct IsMozPromise : FalseType {};

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
struct IsMozPromise<MozPromise<ResolveValueT, RejectValueT, IsExclusive>>
    : TrueType {};

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
            typename R = typename Conditional<IsExclusive, T&&, const T&>::Type>
  static R MaybeMove(T& aX) {
    return static_cast<R>(aX);
  }

 public:
  typedef ResolveValueT ResolveValueType;
  typedef RejectValueT RejectValueType;
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
  MozPromise(const char* aCreationSite, bool aIsCompletionPromise)
      : mCreationSite(aCreationSite),
        mMutex("MozPromise Mutex"),
        mHaveRequest(false),
        mIsCompletionPromise(aIsCompletionPromise)
#  ifdef PROMISE_DEBUG
        ,
        mMagic4(&mMutex)
#  endif
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
  // NB: We can include the definition of this class inline once B2G ICS is
  // gone.
  class Private;

  template <typename ResolveValueType_>
  [[nodiscard]] static RefPtr<MozPromise> CreateAndResolve(
      ResolveValueType_&& aResolveValue, const char* aResolveSite) {
    static_assert(IsConvertible<ResolveValueType_, ResolveValueT>::value,
                  "Resolve() argument must be implicitly convertible to "
                  "MozPromise's ResolveValueT");
    RefPtr<typename MozPromise::Private> p =
        new MozPromise::Private(aResolveSite);
    p->Resolve(std::forward<ResolveValueType_>(aResolveValue), aResolveSite);
    return p;
  }

  template <typename RejectValueType_>
  [[nodiscard]] static RefPtr<MozPromise> CreateAndReject(
      RejectValueType_&& aRejectValue, const char* aRejectSite) {
    static_assert(IsConvertible<RejectValueType_, RejectValueT>::value,
                  "Reject() argument must be implicitly convertible to "
                  "MozPromise's RejectValueT");
    RefPtr<typename MozPromise::Private> p =
        new MozPromise::Private(aRejectSite);
    p->Reject(std::forward<RejectValueType_>(aRejectValue), aRejectSite);
    return p;
  }

  template <typename ResolveOrRejectValueType_>
  [[nodiscard]] static RefPtr<MozPromise> CreateAndResolveOrReject(
      ResolveOrRejectValueType_&& aValue, const char* aSite) {
    RefPtr<typename MozPromise::Private> p = new MozPromise::Private(aSite);
    p->ResolveOrReject(std::forward<ResolveOrRejectValueType_>(aValue), aSite);
    return p;
  }

  typedef MozPromise<nsTArray<ResolveValueType>, RejectValueType, IsExclusive>
      AllPromiseType;

 private:
  class AllPromiseHolder : public MozPromiseRefcountable {
   public:
    explicit AllPromiseHolder(size_t aDependentPromises)
        : mPromise(new typename AllPromiseType::Private(__func__)),
          mOutstandingPromises(aDependentPromises) {
      MOZ_ASSERT(aDependentPromises > 0);
      mResolveValues.SetLength(aDependentPromises);
    }

    void Resolve(size_t aIndex, ResolveValueType&& aResolveValue) {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mResolveValues[aIndex].emplace(std::move(aResolveValue));
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

    void Reject(RejectValueType&& aRejectValue) {
      if (!mPromise) {
        // Already rejected.
        return;
      }

      mPromise->Reject(std::move(aRejectValue), __func__);
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
  [[nodiscard]] static RefPtr<AllPromiseType> All(
      nsISerialEventTarget* aProcessingTarget,
      nsTArray<RefPtr<MozPromise>>& aPromises) {
    if (aPromises.Length() == 0) {
      return AllPromiseType::CreateAndResolve(nsTArray<ResolveValueType>(),
                                              __func__);
    }

    RefPtr<AllPromiseHolder> holder = new AllPromiseHolder(aPromises.Length());
    RefPtr<AllPromiseType> promise = holder->Promise();
    for (size_t i = 0; i < aPromises.Length(); ++i) {
      aPromises[i]->Then(
          aProcessingTarget, __func__,
          [holder, i](ResolveValueType aResolveValue) -> void {
            holder->Resolve(i, std::move(aResolveValue));
          },
          [holder](RejectValueType aRejectValue) -> void {
            holder->Reject(std::move(aRejectValue));
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
    class ResolveOrRejectRunnable : public CancelableRunnable {
     public:
      ResolveOrRejectRunnable(ThenValueBase* aThenValue, MozPromise* aPromise)
          : CancelableRunnable(
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

    ThenValueBase(nsISerialEventTarget* aResponseTarget, const char* aCallSite)
        : mResponseTarget(aResponseTarget), mCallSite(aCallSite) {
      MOZ_ASSERT(aResponseTarget);
    }

#  ifdef PROMISE_DEBUG
    ~ThenValueBase() {
      mMagic1 = 0;
      mMagic2 = 0;
    }
#  endif

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
        MOZ_DIAGNOSTIC_ASSERT(Request::mDisconnected);
      }
    }

    void Dispatch(MozPromise* aPromise) {
      PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic);
      aPromise->mMutex.AssertCurrentThreadOwns();
      MOZ_ASSERT(!aPromise->IsPending());

      nsCOMPtr<nsIRunnable> r = new ResolveOrRejectRunnable(this, aPromise);
      PROMISE_LOG(
          "%s Then() call made from %s [Runnable=%p, Promise=%p, ThenValue=%p]",
          aPromise->mValue.IsResolve() ? "Resolving" : "Rejecting", mCallSite,
          r.get(), aPromise, this);

      // Promise consumers are allowed to disconnect the Request object and
      // then shut down the thread or task queue that the promise result would
      // be dispatched on. So we unfortunately can't assert that promise
      // dispatch succeeds. :-(
      mResponseTarget->Dispatch(r.forget());
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

    nsCOMPtr<nsISerialEventTarget>
        mResponseTarget;  // May be released on any thread.
#  ifdef PROMISE_DEBUG
    uint32_t mMagic1 = sMagic;
#  endif
    const char* mCallSite;
#  ifdef PROMISE_DEBUG
    uint32_t mMagic2 = sMagic;
#  endif
  };

  /*
   * We create two overloads for invoking Resolve/Reject Methods so as to
   * make the resolve/reject value argument "optional".
   */
  template <typename ThisType, typename MethodType, typename ValueType>
  static typename EnableIf<
      TakesArgument<MethodType>::value,
      typename detail::MethodTrait<MethodType>::ReturnType>::Type
  InvokeMethod(ThisType* aThisVal, MethodType aMethod, ValueType&& aValue) {
    return (aThisVal->*aMethod)(std::forward<ValueType>(aValue));
  }

  template <typename ThisType, typename MethodType, typename ValueType>
  static typename EnableIf<
      !TakesArgument<MethodType>::value,
      typename detail::MethodTrait<MethodType>::ReturnType>::Type
  InvokeMethod(ThisType* aThisVal, MethodType aMethod, ValueType&& aValue) {
    return (aThisVal->*aMethod)();
  }

  // Called when promise chaining is supported.
  template <bool SupportChaining, typename ThisType, typename MethodType,
            typename ValueType, typename CompletionPromiseType>
  static typename EnableIf<SupportChaining, void>::Type InvokeCallbackMethod(
      ThisType* aThisVal, MethodType aMethod, ValueType&& aValue,
      CompletionPromiseType&& aCompletionPromise) {
    auto p = InvokeMethod(aThisVal, aMethod, std::forward<ValueType>(aValue));
    if (aCompletionPromise) {
      p->ChainTo(aCompletionPromise.forget(), "<chained completion promise>");
    }
  }

  // Called when promise chaining is not supported.
  template <bool SupportChaining, typename ThisType, typename MethodType,
            typename ValueType, typename CompletionPromiseType>
  static typename EnableIf<!SupportChaining, void>::Type InvokeCallbackMethod(
      ThisType* aThisVal, MethodType aMethod, ValueType&& aValue,
      CompletionPromiseType&& aCompletionPromise) {
    MOZ_DIAGNOSTIC_ASSERT(
        !aCompletionPromise,
        "Can't do promise chaining for a non-promise-returning method.");
    InvokeMethod(aThisVal, aMethod, std::forward<ValueType>(aValue));
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

    using R1 = typename RemoveSmartPointer<
        typename detail::MethodTrait<ResolveMethodType>::ReturnType>::Type;
    using R2 = typename RemoveSmartPointer<
        typename detail::MethodTrait<RejectMethodType>::ReturnType>::Type;
    using SupportChaining = IntegralConstant<bool, IsMozPromise<R1>::value &&
                                                       IsSame<R1, R2>::value>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType =
        typename Conditional<SupportChaining::value, R1, MozPromise>::Type;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget, ThisType* aThisVal,
              ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod,
              const char* aCallSite)
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
      if (aValue.IsResolve()) {
        InvokeCallbackMethod<SupportChaining::value>(
            mThisVal.get(), mResolveMethod, MaybeMove(aValue.ResolveValue()),
            std::move(mCompletionPromise));
      } else {
        InvokeCallbackMethod<SupportChaining::value>(
            mThisVal.get(), mRejectMethod, MaybeMove(aValue.RejectValue()),
            std::move(mCompletionPromise));
      }

      // Null out mThisVal after invoking the callback so that any references
      // are released predictably on the dispatch thread. Otherwise, it would be
      // released on whatever thread last drops its reference to the ThenValue,
      // which may or may not be ok.
      mThisVal = nullptr;
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

    using R1 = typename RemoveSmartPointer<typename detail::MethodTrait<
        ResolveRejectMethodType>::ReturnType>::Type;
    using SupportChaining = IntegralConstant<bool, IsMozPromise<R1>::value>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType =
        typename Conditional<SupportChaining::value, R1, MozPromise>::Type;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget, ThisType* aThisVal,
              ResolveRejectMethodType aResolveRejectMethod,
              const char* aCallSite)
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
      InvokeCallbackMethod<SupportChaining::value>(
          mThisVal.get(), mResolveRejectMethod, MaybeMove(aValue),
          std::move(mCompletionPromise));

      // Null out mThisVal after invoking the callback so that any references
      // are released predictably on the dispatch thread. Otherwise, it would be
      // released on whatever thread last drops its reference to the ThenValue,
      // which may or may not be ok.
      mThisVal = nullptr;
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

    using R1 = typename RemoveSmartPointer<
        typename detail::MethodTrait<ResolveFunction>::ReturnType>::Type;
    using R2 = typename RemoveSmartPointer<
        typename detail::MethodTrait<RejectFunction>::ReturnType>::Type;
    using SupportChaining = IntegralConstant<bool, IsMozPromise<R1>::value &&
                                                       IsSame<R1, R2>::value>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType =
        typename Conditional<SupportChaining::value, R1, MozPromise>::Type;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget,
              ResolveFunction&& aResolveFunction,
              RejectFunction&& aRejectFunction, const char* aCallSite)
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
      if (aValue.IsResolve()) {
        InvokeCallbackMethod<SupportChaining::value>(
            mResolveFunction.ptr(), &ResolveFunction::operator(),
            MaybeMove(aValue.ResolveValue()), std::move(mCompletionPromise));
      } else {
        InvokeCallbackMethod<SupportChaining::value>(
            mRejectFunction.ptr(), &RejectFunction::operator(),
            MaybeMove(aValue.RejectValue()), std::move(mCompletionPromise));
      }

      // Destroy callbacks after invocation so that any references in closures
      // are released predictably on the dispatch thread. Otherwise, they would
      // be released on whatever thread last drops its reference to the
      // ThenValue, which may or may not be ok.
      mResolveFunction.reset();
      mRejectFunction.reset();
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

    using R1 = typename RemoveSmartPointer<
        typename detail::MethodTrait<ResolveRejectFunction>::ReturnType>::Type;
    using SupportChaining = IntegralConstant<bool, IsMozPromise<R1>::value>;

    // Fall back to MozPromise when promise chaining is not supported to make
    // code compile.
    using PromiseType =
        typename Conditional<SupportChaining::value, R1, MozPromise>::Type;

   public:
    ThenValue(nsISerialEventTarget* aResponseTarget,
              ResolveRejectFunction&& aResolveRejectFunction,
              const char* aCallSite)
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
      InvokeCallbackMethod<SupportChaining::value>(
          mResolveRejectFunction.ptr(), &ResolveRejectFunction::operator(),
          MaybeMove(aValue), std::move(mCompletionPromise));

      // Destroy callbacks after invocation so that any references in closures
      // are released predictably on the dispatch thread. Otherwise, they would
      // be released on whatever thread last drops its reference to the
      // ThenValue, which may or may not be ok.
      mResolveRejectFunction.reset();
    }

   private:
    Maybe<ResolveRejectFunction>
        mResolveRejectFunction;  // Only accessed and deleted on dispatch
                                 // thread.
    RefPtr<typename PromiseType::Private> mCompletionPromise;
  };

 public:
  void ThenInternal(already_AddRefed<ThenValueBase> aThenValue,
                    const char* aCallSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    RefPtr<ThenValueBase> thenValue = aThenValue;
    MutexAutoLock lock(mMutex);
    MOZ_DIAGNOSTIC_ASSERT(
        !IsExclusive || !mHaveRequest,
        "Using an exclusive promise in a non-exclusive fashion");
    mHaveRequest = true;
    PROMISE_LOG("%s invoking Then() [this=%p, aThenValue=%p, isPending=%d]",
                aCallSite, this, thenValue.get(), (int)IsPending());
    if (!IsPending()) {
      thenValue->Dispatch(this);
    } else {
      mThenValues.AppendElement(thenValue.forget());
    }
  }

 protected:
  /*
   * A command object to store all information needed to make a request to
   * the promise. This allows us to delay the request until further use is
   * known (whether it is ->Then() again for more promise chaining or ->Track()
   * to terminate chaining and issue the request).
   *
   * This allows a unified syntax for promise chaining and disconnection
   * and feels more like its JS counterpart.
   */
  template <typename ThenValueType>
  class ThenCommand {
    // Allow Promise1::ThenCommand to access the private constructor,
    // Promise2::ThenCommand(ThenCommand&&).
    template <typename, typename, bool>
    friend class MozPromise;

    using PromiseType = typename ThenValueType::PromiseType;
    using Private = typename PromiseType::Private;

    ThenCommand(const char* aCallSite,
                already_AddRefed<ThenValueType> aThenValue,
                MozPromise* aReceiver)
        : mCallSite(aCallSite), mThenValue(aThenValue), mReceiver(aReceiver) {}

    ThenCommand(ThenCommand&& aOther) = default;

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
          ThenValueType::SupportChaining::value,
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
    auto Then(Ts&&... aArgs)
        -> decltype(DeclVal<PromiseType>().Then(std::forward<Ts>(aArgs)...)) {
      return static_cast<RefPtr<PromiseType>>(*this)->Then(
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
    const char* mCallSite;
    RefPtr<ThenValueType> mThenValue;
    RefPtr<MozPromise> mReceiver;
  };

 public:
  template <typename ThisType, typename... Methods,
            typename ThenValueType = ThenValue<ThisType*, Methods...>,
            typename ReturnType = ThenCommand<ThenValueType>>
  ReturnType Then(nsISerialEventTarget* aResponseTarget, const char* aCallSite,
                  ThisType* aThisVal, Methods... aMethods) {
    RefPtr<ThenValueType> thenValue =
        new ThenValueType(aResponseTarget, aThisVal, aMethods..., aCallSite);
    return ReturnType(aCallSite, thenValue.forget(), this);
  }

  template <typename... Functions,
            typename ThenValueType = ThenValue<Functions...>,
            typename ReturnType = ThenCommand<ThenValueType>>
  ReturnType Then(nsISerialEventTarget* aResponseTarget, const char* aCallSite,
                  Functions&&... aFunctions) {
    RefPtr<ThenValueType> thenValue =
        new ThenValueType(aResponseTarget, std::move(aFunctions)..., aCallSite);
    return ReturnType(aCallSite, thenValue.forget(), this);
  }

  void ChainTo(already_AddRefed<Private> aChainedPromise,
               const char* aCallSite) {
    MutexAutoLock lock(mMutex);
    MOZ_DIAGNOSTIC_ASSERT(
        !IsExclusive || !mHaveRequest,
        "Using an exclusive promise in a non-exclusive fashion");
    mHaveRequest = true;
    RefPtr<Private> chainedPromise = aChainedPromise;
    PROMISE_LOG(
        "%s invoking Chain() [this=%p, chainedPromise=%p, isPending=%d]",
        aCallSite, this, chainedPromise.get(), (int)IsPending());
    if (!IsPending()) {
      ForwardTo(chainedPromise);
    } else {
      mChainedPromises.AppendElement(chainedPromise);
    }
  }

#  ifdef MOZ_WIDGET_ANDROID
  // Creates a C++ MozPromise from its Java counterpart, GeckoResult.
  [[nodiscard]] static RefPtr<MozPromise> FromGeckoResult(
      java::GeckoResult::Param aGeckoResult) {
    using jni::GeckoResultCallback;
    RefPtr<Private> p = new Private("GeckoResult Glue", false);
    auto resolve = GeckoResultCallback::CreateAndAttach<ResolveValueType>(
        [p](ResolveValueType aArg) { p->Resolve(aArg, __func__); });
    auto reject = GeckoResultCallback::CreateAndAttach<RejectValueType>(
        [p](RejectValueType aArg) { p->Reject(aArg, __func__); });
    aGeckoResult->NativeThen(resolve, reject);
    return p;
  }
#  endif

  // Creates a C++ MozPromise from its JS counterpart, dom::Promise.
  // FromDomPromise currently only supports primitive types (int8/16/32, float,
  // double) And the reject value type must be a nsresult.
  // To use, please include MozPromiseInlines.h
  static RefPtr<MozPromise> FromDomPromise(dom::Promise* aDOMPromise);

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
#  ifdef PROMISE_DEBUG
    mMagic1 = 0;
    mMagic2 = 0;
    mMagic3 = 0;
    mMagic4 = nullptr;
#  endif
  };

  const char* mCreationSite;  // For logging
  Mutex mMutex;
  ResolveOrRejectValue mValue;
#  ifdef PROMISE_DEBUG
  uint32_t mMagic1 = sMagic;
#  endif
  // Try shows we never have more than 3 elements when IsExclusive is false.
  // So '3' is a good value to avoid heap allocation in most cases.
  AutoTArray<RefPtr<ThenValueBase>, IsExclusive ? 1 : 3> mThenValues;
#  ifdef PROMISE_DEBUG
  uint32_t mMagic2 = sMagic;
#  endif
  nsTArray<RefPtr<Private>> mChainedPromises;
#  ifdef PROMISE_DEBUG
  uint32_t mMagic3 = sMagic;
#  endif
  bool mHaveRequest;
  const bool mIsCompletionPromise;
#  ifdef PROMISE_DEBUG
  void* mMagic4;
#  endif
};

template <typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class MozPromise<ResolveValueT, RejectValueT, IsExclusive>::Private
    : public MozPromise<ResolveValueT, RejectValueT, IsExclusive> {
 public:
  explicit Private(const char* aCreationSite, bool aIsCompletionPromise = false)
      : MozPromise(aCreationSite, aIsCompletionPromise) {}

  template <typename ResolveValueT_>
  void Resolve(ResolveValueT_&& aResolveValue, const char* aResolveSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s resolving MozPromise (%p created at %s)", aResolveSite,
                this, mCreationSite);
    if (!IsPending()) {
      PROMISE_LOG(
          "%s ignored already resolved or rejected MozPromise (%p created at "
          "%s)",
          aResolveSite, this, mCreationSite);
      return;
    }
    mValue.SetResolve(std::forward<ResolveValueT_>(aResolveValue));
    DispatchAll();
  }

  template <typename RejectValueT_>
  void Reject(RejectValueT_&& aRejectValue, const char* aRejectSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s rejecting MozPromise (%p created at %s)", aRejectSite, this,
                mCreationSite);
    if (!IsPending()) {
      PROMISE_LOG(
          "%s ignored already resolved or rejected MozPromise (%p created at "
          "%s)",
          aRejectSite, this, mCreationSite);
      return;
    }
    mValue.SetReject(std::forward<RejectValueT_>(aRejectValue));
    DispatchAll();
  }

  template <typename ResolveOrRejectValue_>
  void ResolveOrReject(ResolveOrRejectValue_&& aValue, const char* aSite) {
    PROMISE_ASSERT(mMagic1 == sMagic && mMagic2 == sMagic &&
                   mMagic3 == sMagic && mMagic4 == &mMutex);
    MutexAutoLock lock(mMutex);
    PROMISE_LOG("%s resolveOrRejecting MozPromise (%p created at %s)", aSite,
                this, mCreationSite);
    if (!IsPending()) {
      PROMISE_LOG(
          "%s ignored already resolved or rejected MozPromise (%p created at "
          "%s)",
          aSite, this, mCreationSite);
      return;
    }
    mValue = std::forward<ResolveOrRejectValue_>(aValue);
    DispatchAll();
  }
};

// A generic promise type that does the trick for simple use cases.
typedef MozPromise<bool, nsresult, /* IsExclusive = */ true> GenericPromise;

// A generic, non-exclusive promise type that does the trick for simple use
// cases.
typedef MozPromise<bool, nsresult, /* IsExclusive = */ false>
    GenericNonExclusivePromise;

/*
 * Class to encapsulate a promise for a particular role. Use this as the member
 * variable for a class whose method returns a promise.
 */
template <typename PromiseType, typename ImplType>
class MozPromiseHolderBase {
 public:
  MozPromiseHolderBase() = default;

  MozPromiseHolderBase(MozPromiseHolderBase&& aOther) = default;
  MozPromiseHolderBase& operator=(MozPromiseHolderBase&& aOther) = default;

  ~MozPromiseHolderBase() { MOZ_ASSERT(!mPromise); }

  already_AddRefed<PromiseType> Ensure(const char* aMethodName) {
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
  void Resolve(ResolveValueType_&& aResolveValue, const char* aMethodName) {
    static_assert(IsConvertible<ResolveValueType_,
                                typename PromiseType::ResolveValueType>::value,
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
                       const char* aMethodName) {
    if (!IsEmpty()) {
      Resolve(std::forward<ResolveValueType_>(aResolveValue), aMethodName);
    }
  }

  template <typename RejectValueType_>
  void Reject(RejectValueType_&& aRejectValue, const char* aMethodName) {
    static_assert(IsConvertible<RejectValueType_,
                                typename PromiseType::RejectValueType>::value,
                  "Reject() argument must be implicitly convertible to "
                  "MozPromise's RejectValueT");

    static_cast<ImplType*>(this)->Check();
    MOZ_ASSERT(mPromise);
    mPromise->Reject(std::forward<RejectValueType_>(aRejectValue), aMethodName);
    mPromise = nullptr;
  }

  template <typename RejectValueType_>
  void RejectIfExists(RejectValueType_&& aRejectValue,
                      const char* aMethodName) {
    if (!IsEmpty()) {
      Reject(std::forward<RejectValueType_>(aRejectValue), aMethodName);
    }
  }

  template <typename ResolveOrRejectValueType_>
  void ResolveOrReject(ResolveOrRejectValueType_&& aValue,
                       const char* aMethodName) {
    static_cast<ImplType*>(this)->Check();
    MOZ_ASSERT(mPromise);
    mPromise->ResolveOrReject(std::forward<ResolveOrRejectValueType_>(aValue),
                              aMethodName);
    mPromise = nullptr;
  }

  template <typename ResolveOrRejectValueType_>
  void ResolveOrRejectIfExists(ResolveOrRejectValueType_&& aValue,
                               const char* aMethodName) {
    if (!IsEmpty()) {
      ResolveOrReject(std::forward<ResolveOrRejectValueType_>(aValue),
                      aMethodName);
    }
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
    nsISerialEventTarget* aTarget, ThisType* aThisVal, const char* aCallerName,
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
          typename EnableIf<sizeof...(Storages) != 0, int>::Type = 0>
static RefPtr<PromiseType> InvokeAsync(
    nsISerialEventTarget* aTarget, ThisType* aThisVal, const char* aCallerName,
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
          typename EnableIf<sizeof...(Storages) == 0, int>::Type = 0>
static RefPtr<PromiseType> InvokeAsync(
    nsISerialEventTarget* aTarget, ThisType* aThisVal, const char* aCallerName,
    RefPtr<PromiseType> (ThisType::*aMethod)(ArgTypes...),
    ActualArgTypes&&... aArgs) {
  static_assert(
      !detail::Any(
          IsPointer<typename RemoveReference<ActualArgTypes>::Type>::value...),
      "Cannot pass pointer types through InvokeAsync, Storages must be "
      "provided");
  static_assert(sizeof...(ArgTypes) == sizeof...(ActualArgTypes),
                "Method's ArgTypes and ActualArgTypes should have equal sizes");
  return detail::InvokeAsyncImpl<
      StoreCopyPassByRRef<typename Decay<ActualArgTypes>::Type>...>(
      aTarget, aThisVal, aCallerName, aMethod,
      std::forward<ActualArgTypes>(aArgs)...);
}

namespace detail {

template <typename Function, typename PromiseType>
class ProxyFunctionRunnable : public CancelableRunnable {
  typedef typename Decay<Function>::Type FunctionStorage;

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

// Note: The following struct and function are not for public consumption (yet?)
// as we would prefer all calls to pass on-the-spot lambdas (or at least moved
// function objects). They could be moved outside of detail if really needed.

// We prefer getting function objects by non-lvalue-ref (to avoid copying them
// and their captures). This struct is a tag that allows the use of objects
// through lvalue-refs where necessary.
struct AllowInvokeAsyncFunctionLVRef {};

// Invoke a function object (e.g., lambda or std/mozilla::function)
// asynchronously; note that the object will be copied if provided by
// lvalue-ref. Return a promise that the function should eventually resolve or
// reject.
template <typename Function>
static auto InvokeAsync(nsISerialEventTarget* aTarget, const char* aCallerName,
                        AllowInvokeAsyncFunctionLVRef, Function&& aFunction)
    -> decltype(aFunction()) {
  static_assert(
      IsRefcountedSmartPointer<decltype(aFunction())>::value &&
          IsMozPromise<
              typename RemoveSmartPointer<decltype(aFunction())>::Type>::value,
      "Function object must return RefPtr<MozPromise>");
  MOZ_ASSERT(aTarget);
  typedef typename RemoveSmartPointer<decltype(aFunction())>::Type PromiseType;
  typedef detail::ProxyFunctionRunnable<Function, PromiseType>
      ProxyRunnableType;

  auto p = MakeRefPtr<typename PromiseType::Private>(aCallerName);
  auto r = MakeRefPtr<ProxyRunnableType>(p, std::forward<Function>(aFunction));
  aTarget->Dispatch(r.forget());
  return p;
}

}  // namespace detail

// Invoke a function object (e.g., lambda) asynchronously.
// Return a promise that the function should eventually resolve or reject.
template <typename Function>
static auto InvokeAsync(nsISerialEventTarget* aTarget, const char* aCallerName,
                        Function&& aFunction) -> decltype(aFunction()) {
  static_assert(!std::is_lvalue_reference_v<Function>,
                "Function object must not be passed by lvalue-ref (to avoid "
                "unplanned copies); Consider move()ing the object.");
  return detail::InvokeAsync(aTarget, aCallerName,
                             detail::AllowInvokeAsyncFunctionLVRef(),
                             std::forward<Function>(aFunction));
}

#  undef PROMISE_LOG
#  undef PROMISE_ASSERT
#  undef PROMISE_DEBUG

}  // namespace mozilla

#endif
