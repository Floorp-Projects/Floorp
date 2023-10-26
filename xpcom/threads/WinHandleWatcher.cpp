/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <threadpoolapiset.h>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ThreadSafety.h"
#include "mozilla/WinHandleWatcher.h"

#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsISerialEventTarget.h"
#include "nsISupportsImpl.h"
#include "nsITargetShutdownTask.h"
#include "nsIWeakReferenceUtils.h"
#include "nsThreadUtils.h"

mozilla::LazyLogModule sHWLog("HandleWatcher");

namespace mozilla {
namespace details {
struct WaitHandleDeleter {
  void operator()(PTP_WAIT waitHandle) {
    MOZ_LOG(sHWLog, LogLevel::Debug, ("Closing PTP_WAIT %p", waitHandle));
    ::CloseThreadpoolWait(waitHandle);
  }
};
}  // namespace details
using WaitHandlePtr = UniquePtr<TP_WAIT, details::WaitHandleDeleter>;

// HandleWatcher::Impl
//
// The backing implementation of HandleWatcher is a PTP_WAIT, an OS-threadpool
// wait-object. Windows doesn't actually create a new thread per wait-object;
// OS-threadpool threads are assigned to wait-objects only when their associated
// handle become signaled -- although explicit documentation of this fact is
// somewhat obscurely placed. [1]
//
// Throughout this class, we use manual locking and unlocking guarded by Clang's
// thread-safety warnings, rather than scope-based lock-guards. See `Replace()`
// for an explanation and justification.
//
// [1]https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects#remarks
class HandleWatcher::Impl final : public nsITargetShutdownTask {
  NS_DECL_THREADSAFE_ISUPPORTS

 public:
  Impl() = default;

 private:
  ~Impl() { MOZ_ASSERT(IsStopped()); }

  struct Data {
    // The watched handle and its callback.
    HANDLE handle;
    RefPtr<nsIEventTarget> target;
    nsCOMPtr<nsIRunnable> runnable;

    // Handle to the threadpool wait-object.
    WaitHandlePtr waitHandle;
    // A pointer to ourselves, notionally owned by the wait-object.
    RefPtr<Impl> self;

    // (We can't actually do this because a) it has annoying consequences in
    // C++20 thanks to P1008R1, and b) Clang just ignores it anyway.)
    //
    // ~Data() MOZ_EXCLUDES(mMutex) = default;
  };

  mozilla::Mutex mMutex{"HandleWatcher::Impl"};
  Data mData MOZ_GUARDED_BY(mMutex) = {};

  // Callback from OS threadpool wait-object.
  static void CALLBACK WaitCallback(PTP_CALLBACK_INSTANCE, void* ctx,
                                    PTP_WAIT aWaitHandle,
                                    TP_WAIT_RESULT aResult) {
    static_cast<Impl*>(ctx)->OnWaitCompleted(aWaitHandle, aResult);
  }

  void OnWaitCompleted(PTP_WAIT aWaitHandle, TP_WAIT_RESULT aResult)
      MOZ_EXCLUDES(mMutex) {
    MOZ_ASSERT(aResult == WAIT_OBJECT_0);

    mMutex.Lock();
    // If this callback is no longer the active callback, skip out.
    // All cleanup is someone else's problem.
    if (aWaitHandle != mData.waitHandle.get()) {
      MOZ_LOG(sHWLog, LogLevel::Debug,
              ("Recv'd already-stopped callback: HW %p | PTP_WAIT %p", this,
               aWaitHandle));
      mMutex.Unlock();
      return;
    }

    // Take our self-pointer so that we release it on exit.
    RefPtr<Impl> self = std::move(mData.self);

    MOZ_LOG(sHWLog, LogLevel::Info,
            ("Recv'd callback: HW %p | handle %p | target %p | PTP_WAIT %p",
             this, mData.handle, mData.target.get(), aWaitHandle));

    // This may fail if (for example) `mData.target` is being shut down, but we
    // have not yet received the shutdown callback.
    mData.target->Dispatch(mData.runnable.forget());
    Replace(Data{});
  }

 public:
  static RefPtr<Impl> Create(HANDLE aHandle, nsIEventTarget* aTarget,
                             already_AddRefed<nsIRunnable> aRunnable) {
    auto impl = MakeRefPtr<Impl>();
    bool const ok [[maybe_unused]] =
        impl->Watch(aHandle, aTarget, std::move(aRunnable));
    MOZ_ASSERT(ok);
    return impl;
  }

 private:
  bool Watch(HANDLE aHandle, nsIEventTarget* aTarget,
             already_AddRefed<nsIRunnable> aRunnable) MOZ_EXCLUDES(mMutex) {
    MOZ_ASSERT(aHandle);
    MOZ_ASSERT(aTarget);

    RefPtr<nsIEventTarget> target(aTarget);

    WaitHandlePtr waitHandle{
        ::CreateThreadpoolWait(&WaitCallback, this, nullptr)};
    if (!waitHandle) {
      return false;
    }

    {
      mMutex.Lock();

      nsresult const ret = aTarget->RegisterShutdownTask(this);
      if (NS_FAILED(ret)) {
        mMutex.Unlock();
        return false;
      }

      MOZ_LOG(sHWLog, LogLevel::Info,
              ("Setting callback: HW %p | handle %p | target %p | PTP_WAIT %p",
               this, aHandle, aTarget, waitHandle.get()));

      // returns `void`; presumably always succeeds given a successful
      // `::CreateThreadpoolWait()`
      ::SetThreadpoolWait(waitHandle.get(), aHandle, nullptr);
      // After this point, you must call `FlushWaitHandle(waitHandle.get())`
      // before destroying the wait handle. (Note that this must be done while
      // *not* holding `mMutex`!)

      Replace(Data{.handle = aHandle,
                   .target = std::move(target),
                   .runnable = aRunnable,
                   .waitHandle = std::move(waitHandle),
                   .self = this});
    }

    return true;
  }

  void TargetShutdown() MOZ_EXCLUDES(mMutex) override final {
    mMutex.Lock();

    MOZ_LOG(sHWLog, LogLevel::Debug,
            ("Target shutdown: HW %p | handle %p | target %p | PTP_WAIT %p",
             this, mData.handle, mData.target.get(), mData.waitHandle.get()));

    // Clear mData.target, since there's no need to unregister the shutdown task
    // anymore. Hold onto it until we release the mutex, though, to avoid any
    // reentrancy issues.
    //
    // This is more for internal consistency than safety: someone has to be
    // shutting `target` down, and that someone isn't us, so there's necessarily
    // another reference out there. (Although decrementing the refcount might
    // still have arbitrary effects if someone's been excessively clever with
    // nsISupports::Release...)
    auto const oldTarget = std::move(mData.target);
    Replace(Data{});
    // (Static-assert that the mutex has indeed been released.)
    ([&]() MOZ_EXCLUDES(mMutex) {})();
  }

 public:
  void Stop() MOZ_EXCLUDES(mMutex) {
    mMutex.Lock();
    Replace(Data{});
  }

  bool IsStopped() MOZ_EXCLUDES(mMutex) {
    mozilla::MutexAutoLock lock(mMutex);
    return !mData.handle;
  }

 private:
  // Throughout this class, we use manual locking and unlocking guarded by
  // Clang's thread-safety warnings, rather than scope-based lock-guards. This
  // is largely driven by `Replace()`, below, which performs both operations
  // which require the mutex to be held and operations which require it to not
  // be held, and therefore must explicitly sequence the mutex release.
  //
  // These explicit locks, unlocks, and annotations are both alien to C++ and
  // offensively tedious; but they _are_ still checked for state consistency at
  // scope boundaries. (The concerned reader is invited to test this by
  // deliberately removing an `mMutex.Unlock()` call from anywhere in the class
  // and viewing the resultant compiler diagnostics.)
  //
  // A more principled, or at least differently-principled, implementation might
  // create a scope-based lock-guard and pass it to `Replace()` to dispose of at
  // the proper time. Alas, it cannot be communicated to Clang's thread-safety
  // checker that such a guard is associated with `mMutex`.
  //
  void Replace(Data&& aData) MOZ_CAPABILITY_RELEASE(mMutex) {
    // either both handles are NULL, or neither is
    MOZ_ASSERT(!!aData.handle == !!aData.waitHandle);

    if (mData.handle) {
      MOZ_LOG(sHWLog, LogLevel::Info,
              ("Stop callback: HW %p | handle %p | target %p | PTP_WAIT %p",
               this, mData.handle, mData.target.get(), mData.waitHandle.get()));
    }

    if (mData.target) {
      mData.target->UnregisterShutdownTask(this);
    }

    // Extract the old data and insert the new -- but hold onto the old data for
    // now. (See [1] and [2], below.)
    Data oldData = std::exchange(mData, std::move(aData));

    ////////////////////////////////////////////////////////////////////////////
    // Release the mutex.
    mMutex.Unlock();
    ////////////////////////////////////////////////////////////////////////////

    // [1] `oldData.self` will be unset if the old callback already ran (or if
    // there was no old callback in the first place). If it's set, though, we
    // need to explicitly clear out the wait-object first.
    if (oldData.self) {
      MOZ_ASSERT(oldData.waitHandle);
      FlushWaitHandle(oldData.waitHandle.get());
    }

    // [2] oldData also includes several other reference-counted pointers. It's
    // possible that these may be the last pointer to something, so releasing
    // them may have arbitrary side-effects -- like calling this->Stop(), which
    // will try to reacquire the mutex.
    //
    // Now that we've released the mutex, we can (implicitly) release them all
    // here.
  }

  // Either confirm as complete or cancel any callbacks on aWaitHandle. Block
  // until this is done. (See documentation for ::CloseThreadpoolWait().)
  void FlushWaitHandle(PTP_WAIT aWaitHandle) MOZ_EXCLUDES(mMutex) {
    ::SetThreadpoolWait(aWaitHandle, nullptr, nullptr);
    // This might block on `OnWaitCompleted()`, so we can't hold `mMutex` here.
    ::WaitForThreadpoolWaitCallbacks(aWaitHandle, TRUE);
    // ::CloseThreadpoolWait() itself is the caller's responsibility.
  }
};

NS_IMPL_ISUPPORTS(HandleWatcher::Impl, nsITargetShutdownTask)

//////
// HandleWatcher member function implementations

HandleWatcher::HandleWatcher() : mImpl{} {}
HandleWatcher::~HandleWatcher() {
  if (mImpl) {
    MOZ_ASSERT(mImpl->IsStopped());
    mImpl->Stop();  // just in case, in release
  }
}

HandleWatcher::HandleWatcher(HandleWatcher&&) noexcept = default;
HandleWatcher& HandleWatcher::operator=(HandleWatcher&&) noexcept = default;

void HandleWatcher::Watch(HANDLE aHandle, nsIEventTarget* aTarget,
                          already_AddRefed<nsIRunnable> aRunnable) {
  auto impl = Impl::Create(aHandle, aTarget, std::move(aRunnable));
  MOZ_ASSERT(impl);

  if (mImpl) {
    mImpl->Stop();
  }
  mImpl = std::move(impl);
}

void HandleWatcher::Stop() {
  if (mImpl) {
    mImpl->Stop();
  }
}

bool HandleWatcher::IsStopped() { return !mImpl || mImpl->IsStopped(); }

}  // namespace mozilla
