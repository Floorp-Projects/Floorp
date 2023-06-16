/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Natives.h"

#include "mozilla/MozPromise.h"

namespace mozilla::jni {

namespace details {

/**
 * When a NativeWeakPtr is detached from its owning Java object, the calling
 * thread invokes the implementation's OnWeakNonIntrusiveDetach to perform
 * cleanup. We complete the remainder of the cleanup sequence on the Gecko
 * main thread by expecting OnWeakNonIntrusiveDetach implementations to invoke
 * this Runnable before exiting. It will move itself to the main thread if it
 * is not already there.
 */
template <typename NativeImpl>
class NativeWeakPtrDetachRunnable final : public Runnable {
 public:
  NativeWeakPtrDetachRunnable(
      already_AddRefed<detail::NativeWeakPtrControlBlock<NativeImpl>> aCtlBlock,
      const Object::LocalRef& aOwner,
      typename NativeWeakPtrControlBlockStorageTraits<NativeImpl>::Type
          aNativeImpl)
      : Runnable("mozilla::jni::detail::NativeWeakPtrDetachRunnable"),
        mCtlBlock(aCtlBlock),
        mOwner(aOwner),
        mNativeImpl(std::move(aNativeImpl)),
        mHasRun(false) {
    MOZ_RELEASE_ASSERT(!!mCtlBlock);
    MOZ_RELEASE_ASSERT(!!mNativeImpl);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(NativeWeakPtrDetachRunnable, Runnable)

  NS_IMETHOD Run() override {
    mHasRun = true;

    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(this);
      return NS_OK;
    }

    // Get the owner object's native implementation
    auto owner = ToLocalRef(mOwner);
    auto attachedNativeImpl = NativePtrTraits<NativeImpl>::Get(owner);
    MOZ_RELEASE_ASSERT(!!attachedNativeImpl);

    // NativePtrTraits::ClearFinish cleans out the JNIObject's handle, which
    // obviously we don't want to attempt unless that handle still points to
    // our native implementation.
    if (attachedNativeImpl->IsSame(mCtlBlock)) {
      NativePtrTraits<NativeImpl>::ClearFinish(owner);
    }

    // Now we destroy that native object.
    mNativeImpl = nullptr;
    mHolder.Resolve(true, __func__);
    return NS_OK;
  }

  RefPtr<DetachPromise> GetPromise() { return mHolder.Ensure(__func__); }

 private:
  ~NativeWeakPtrDetachRunnable() {
    // Guard against somebody forgetting to call this runnable.
    MOZ_RELEASE_ASSERT(mHasRun, "You must run/dispatch this runnable!");
  }

 private:
  RefPtr<detail::NativeWeakPtrControlBlock<NativeImpl>> mCtlBlock;
  Object::GlobalRef mOwner;
  MozPromiseHolder<DetachPromise> mHolder;
  typename NativeWeakPtrControlBlockStorageTraits<NativeImpl>::Type mNativeImpl;
  bool mHasRun;
};

}  // namespace details

template <typename NativeImpl>
RefPtr<DetachPromise> NativeWeakPtr<NativeImpl>::Detach() {
  if (!IsAttached()) {
    // Never attached to begin with; no-op
    return DetachPromise::CreateAndResolve(true, __func__);
  }

  auto native = mCtlBlock->Clear();
  if (!native) {
    // Detach already in progress
    return DetachPromise::CreateAndResolve(true, __func__);
  }

  Object::LocalRef owner(mCtlBlock->GetJavaOwner());
  MOZ_RELEASE_ASSERT(!!owner);

  // Save the raw pointer before we move native into the runnable so that we
  // may call OnWeakNonIntrusiveDetach on it even after moving native into
  // the runnable.
  NativeImpl* rawImpl =
      detail::NativeWeakPtrControlBlock<NativeImpl>::StorageTraits::AsRaw(
          native);
  RefPtr<details::NativeWeakPtrDetachRunnable<NativeImpl>> runnable =
      new details::NativeWeakPtrDetachRunnable<NativeImpl>(
          mCtlBlock.forget(), owner, std::move(native));
  RefPtr<DetachPromise> promise = runnable->GetPromise();
  rawImpl->OnWeakNonIntrusiveDetach(runnable.forget());
  return promise;
}

}  // namespace mozilla::jni
