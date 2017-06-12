/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(StateMirroring_h_)
#define StateMirroring_h_

#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StateWatching.h"
#include "mozilla/TaskDispatcher.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include "mozilla/Logging.h"
#include "nsISupportsImpl.h"

/*
 * The state-mirroring machinery allows pieces of interesting state to be
 * observed on multiple thread without locking. The basic strategy is to track
 * changes in a canonical value and post updates to other threads that hold
 * mirrors for that value.
 *
 * One problem with the naive implementation of such a system is that some pieces
 * of state need to be updated atomically, and certain other operations need to
 * wait for these atomic updates to complete before executing. The state-mirroring
 * machinery solves this problem by requiring that its owner thread uses tail
 * dispatch, and posting state update events (which should always be run first by
 * TaskDispatcher implementations) to that tail dispatcher. This ensures that
 * state changes are always atomic from the perspective of observing threads.
 *
 * Given that state-mirroring is an automatic background process, we try to avoid
 * burdening the caller with worrying too much about teardown. To that end, we
 * don't assert dispatch success for any of the notifications, and assume that
 * any canonical or mirror owned by a thread for whom dispatch fails will soon
 * be disconnected by its holder anyway.
 *
 * Given that semantics may change and comments tend to go out of date, we
 * deliberately don't provide usage examples here. Grep around to find them.
 */

namespace mozilla {

// Mirror<T> and Canonical<T> inherit WatchTarget, so we piggy-back on the
// logging that WatchTarget already does. Given that, it makes sense to share
// the same log module.
#define MIRROR_LOG(x, ...) \
  MOZ_ASSERT(gStateWatchingLog); \
  MOZ_LOG(gStateWatchingLog, LogLevel::Debug, (x, ##__VA_ARGS__))

template<typename T> class AbstractMirror;

/*
 * AbstractCanonical is a superclass from which all Canonical values must
 * inherit. It serves as the interface of operations which may be performed (via
 * asynchronous dispatch) by other threads, in particular by the corresponding
 * Mirror value.
 */
template<typename T>
class AbstractCanonical
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractCanonical)
  AbstractCanonical(AbstractThread* aThread) : mOwnerThread(aThread) {}
  virtual void AddMirror(AbstractMirror<T>* aMirror) = 0;
  virtual void RemoveMirror(AbstractMirror<T>* aMirror) = 0;

  AbstractThread* OwnerThread() const { return mOwnerThread; }
protected:
  virtual ~AbstractCanonical() {}
  RefPtr<AbstractThread> mOwnerThread;
};

/*
 * AbstractMirror is a superclass from which all Mirror values must
 * inherit. It serves as the interface of operations which may be performed (via
 * asynchronous dispatch) by other threads, in particular by the corresponding
 * Canonical value.
 */
template<typename T>
class AbstractMirror
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AbstractMirror)
  AbstractMirror(AbstractThread* aThread) : mOwnerThread(aThread) {}
  virtual void UpdateValue(const T& aNewValue) = 0;
  virtual void NotifyDisconnected() = 0;

  AbstractThread* OwnerThread() const { return mOwnerThread; }
protected:
  virtual ~AbstractMirror() {}
  RefPtr<AbstractThread> mOwnerThread;
};

/*
 * Canonical<T> is a wrapper class that allows a given value to be mirrored by other
 * threads. It maintains a list of active mirrors, and queues updates for them
 * when the internal value changes. When changing the value, the caller needs to
 * pass a TaskDispatcher object, which fires the updates at the appropriate time.
 * Canonical<T> is also a WatchTarget, and may be set up to trigger other routines
 * (on the same thread) when the canonical value changes.
 *
 * Canonical<T> is intended to be used as a member variable, so it doesn't actually
 * inherit AbstractCanonical<T> (a refcounted type). Rather, it contains an inner
 * class called |Impl| that implements most of the interesting logic.
 */
template<typename T>
class Canonical
{
public:
  Canonical(AbstractThread* aThread, const T& aInitialValue, const char* aName)
  {
    mImpl = new Impl(aThread, aInitialValue, aName);
  }


  ~Canonical() {}

private:
  class Impl : public AbstractCanonical<T>, public WatchTarget
  {
  public:
    using AbstractCanonical<T>::OwnerThread;

    Impl(AbstractThread* aThread, const T& aInitialValue, const char* aName)
      : AbstractCanonical<T>(aThread), WatchTarget(aName), mValue(aInitialValue)
    {
      MIRROR_LOG("%s [%p] initialized", mName, this);
      MOZ_ASSERT(aThread->SupportsTailDispatch(), "Can't get coherency without tail dispatch");
    }

    void AddMirror(AbstractMirror<T>* aMirror) override
    {
      MIRROR_LOG("%s [%p] adding mirror %p", mName, this, aMirror);
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      MOZ_ASSERT(!mMirrors.Contains(aMirror));
      mMirrors.AppendElement(aMirror);
      aMirror->OwnerThread()->DispatchStateChange(MakeNotifier(aMirror));
    }

    void RemoveMirror(AbstractMirror<T>* aMirror) override
    {
      MIRROR_LOG("%s [%p] removing mirror %p", mName, this, aMirror);
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      MOZ_ASSERT(mMirrors.Contains(aMirror));
      mMirrors.RemoveElement(aMirror);
    }

    void DisconnectAll()
    {
      MIRROR_LOG("%s [%p] Disconnecting all mirrors", mName, this);
      for (size_t i = 0; i < mMirrors.Length(); ++i) {
        mMirrors[i]->OwnerThread()->Dispatch(
          NewRunnableMethod("AbstractMirror::NotifyDisconnected",
                            mMirrors[i],
                            &AbstractMirror<T>::NotifyDisconnected),
          AbstractThread::DontAssertDispatchSuccess);
      }
      mMirrors.Clear();
    }

    operator const T&()
    {
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      return mValue;
    }

    void Set(const T& aNewValue)
    {
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());

      if (aNewValue == mValue) {
        return;
      }

      // Notify same-thread watchers. The state watching machinery will make sure
      // that notifications run at the right time.
      NotifyWatchers();

      // Check if we've already got a pending update. If so we won't schedule another
      // one.
      bool alreadyNotifying = mInitialValue.isSome();

      // Stash the initial value if needed, then update to the new value.
      if (mInitialValue.isNothing()) {
        mInitialValue.emplace(mValue);
      }
      mValue = aNewValue;

      // We wait until things have stablized before sending state updates so that
      // we can avoid sending multiple updates, and possibly avoid sending any
      // updates at all if the value ends up where it started.
      if (!alreadyNotifying) {
        AbstractThread::DispatchDirectTask(NewRunnableMethod(
          "Canonical::Impl::DoNotify", this, &Impl::DoNotify));
      }
    }

    Impl& operator=(const T& aNewValue) { Set(aNewValue); return *this; }
    Impl& operator=(const Impl& aOther) { Set(aOther); return *this; }
    Impl(const Impl& aOther) = delete;

  protected:
    ~Impl() { MOZ_DIAGNOSTIC_ASSERT(mMirrors.IsEmpty()); }

  private:
    void DoNotify()
    {
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      MOZ_ASSERT(mInitialValue.isSome());
      bool same = mInitialValue.ref() == mValue;
      mInitialValue.reset();

      if (same) {
        MIRROR_LOG("%s [%p] unchanged - not sending update", mName, this);
        return;
      }

      for (size_t i = 0; i < mMirrors.Length(); ++i) {
        mMirrors[i]->OwnerThread()->DispatchStateChange(MakeNotifier(mMirrors[i]));
      }
    }

    already_AddRefed<nsIRunnable> MakeNotifier(AbstractMirror<T>* aMirror)
    {
      return NewRunnableMethod<T>("AbstractMirror::UpdateValue",
                                  aMirror,
                                  &AbstractMirror<T>::UpdateValue,
                                  mValue);
      ;
    }

    T mValue;
    Maybe<T> mInitialValue;
    nsTArray<RefPtr<AbstractMirror<T>>> mMirrors;
  };
public:

  // NB: Because mirror-initiated disconnection can race with canonical-
  // initiated disconnection, a canonical should never be reinitialized.
  // Forward control operations to the Impl.
  void DisconnectAll() { return mImpl->DisconnectAll(); }

  // Access to the Impl.
  operator Impl&() { return *mImpl; }
  Impl* operator&() { return mImpl; }

  // Access to the T.
  const T& Ref() const { return *mImpl; }
  operator const T&() const { return Ref(); }
  void Set(const T& aNewValue) { mImpl->Set(aNewValue); }
  Canonical& operator=(const T& aNewValue) { Set(aNewValue); return *this; }
  Canonical& operator=(const Canonical& aOther) { Set(aOther); return *this; }
  Canonical(const Canonical& aOther) = delete;

private:
  RefPtr<Impl> mImpl;
};

/*
 * Mirror<T> is a wrapper class that allows a given value to mirror that of a
 * Canonical<T> owned by another thread. It registers itself with a Canonical<T>,
 * and is periodically updated with new values. Mirror<T> is also a WatchTarget,
 * and may be set up to trigger other routines (on the same thread) when the
 * mirrored value changes.
 *
 * Mirror<T> is intended to be used as a member variable, so it doesn't actually
 * inherit AbstractMirror<T> (a refcounted type). Rather, it contains an inner
 * class called |Impl| that implements most of the interesting logic.
 */
template<typename T>
class Mirror
{
public:
  Mirror(AbstractThread* aThread, const T& aInitialValue, const char* aName)
  {
    mImpl = new Impl(aThread, aInitialValue, aName);
  }

  ~Mirror()
  {
    // As a member of complex objects, a Mirror<T> may be destroyed on a
    // different thread than its owner, or late in shutdown during CC. Given
    // that, we require manual disconnection so that callers can put things in
    // the right place.
    MOZ_DIAGNOSTIC_ASSERT(!mImpl->IsConnected());
  }

private:
  class Impl : public AbstractMirror<T>, public WatchTarget
  {
  public:
    using AbstractMirror<T>::OwnerThread;

    Impl(AbstractThread* aThread, const T& aInitialValue, const char* aName)
      : AbstractMirror<T>(aThread), WatchTarget(aName), mValue(aInitialValue)
    {
      MIRROR_LOG("%s [%p] initialized", mName, this);
      MOZ_ASSERT(aThread->SupportsTailDispatch(), "Can't get coherency without tail dispatch");
    }

    operator const T&()
    {
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      return mValue;
    }

    virtual void UpdateValue(const T& aNewValue) override
    {
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      if (mValue != aNewValue) {
        mValue = aNewValue;
        WatchTarget::NotifyWatchers();
      }
    }

    virtual void NotifyDisconnected() override
    {
      MIRROR_LOG("%s [%p] Notifed of disconnection from %p", mName, this, mCanonical.get());
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      mCanonical = nullptr;
    }

    bool IsConnected() const { return !!mCanonical; }

    void Connect(AbstractCanonical<T>* aCanonical)
    {
      MIRROR_LOG("%s [%p] Connecting to %p", mName, this, aCanonical);
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      MOZ_ASSERT(!IsConnected());
      MOZ_ASSERT(OwnerThread()->RequiresTailDispatch(aCanonical->OwnerThread()), "Can't get coherency without tail dispatch");

      nsCOMPtr<nsIRunnable> r =
        NewRunnableMethod<StoreRefPtrPassByPtr<AbstractMirror<T>>>(
          "AbstractCanonical::AddMirror",
          aCanonical,
          &AbstractCanonical<T>::AddMirror,
          this);
      aCanonical->OwnerThread()->Dispatch(r.forget(), AbstractThread::DontAssertDispatchSuccess);
      mCanonical = aCanonical;
    }
  public:

    void DisconnectIfConnected()
    {
      MOZ_ASSERT(OwnerThread()->IsCurrentThreadIn());
      if (!IsConnected()) {
        return;
      }

      MIRROR_LOG("%s [%p] Disconnecting from %p", mName, this, mCanonical.get());
      nsCOMPtr<nsIRunnable> r =
        NewRunnableMethod<StoreRefPtrPassByPtr<AbstractMirror<T>>>(
          "AbstractCanonical::RemoveMirror",
          mCanonical,
          &AbstractCanonical<T>::RemoveMirror,
          this);
      mCanonical->OwnerThread()->Dispatch(r.forget(), AbstractThread::DontAssertDispatchSuccess);
      mCanonical = nullptr;
    }

  protected:
    ~Impl() { MOZ_DIAGNOSTIC_ASSERT(!IsConnected()); }

  private:
    T mValue;
    RefPtr<AbstractCanonical<T>> mCanonical;
  };
public:

  // Forward control operations to the Impl<T>.
  void Connect(AbstractCanonical<T>* aCanonical) { mImpl->Connect(aCanonical); }
  void DisconnectIfConnected() { mImpl->DisconnectIfConnected(); }

  // Access to the Impl<T>.
  operator Impl&() { return *mImpl; }
  Impl* operator&() { return mImpl; }

  // Access to the T.
  const T& Ref() const { return *mImpl; }
  operator const T&() const { return Ref(); }

private:
  RefPtr<Impl> mImpl;
};

#undef MIRROR_LOG

} // namespace mozilla

#endif
