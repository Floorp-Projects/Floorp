/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidVsync.h"

#include "AndroidBridge.h"
#include "nsTArray.h"

/**
 * Implementation for the AndroidVsync class.
 */

namespace mozilla {
namespace widget {

StaticDataMutex<ThreadSafeWeakPtr<AndroidVsync>> AndroidVsync::sInstance(
    "AndroidVsync::sInstance");

/* static */ RefPtr<AndroidVsync> AndroidVsync::GetInstance() {
  auto weakInstance = sInstance.Lock();
  RefPtr<AndroidVsync> instance(*weakInstance);
  if (!instance) {
    instance = new AndroidVsync();
    *weakInstance = instance;
  }
  return instance;
}

/**
 * Owned by the Java AndroidVsync instance.
 */
class AndroidVsyncSupport final
    : public java::AndroidVsync::Natives<AndroidVsyncSupport> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AndroidVsyncSupport)

  using Base = java::AndroidVsync::Natives<AndroidVsyncSupport>;
  using Base::AttachNative;
  using Base::DisposeNative;

  explicit AndroidVsyncSupport(AndroidVsync* aAndroidVsync)
      : mAndroidVsync(std::move(aAndroidVsync),
                      "AndroidVsyncSupport::mAndroidVsync") {}

  // Called by Java
  void NotifyVsync(const java::AndroidVsync::LocalRef& aInstance,
                   int64_t aFrameTimeNanos) {
    auto androidVsync = mAndroidVsync.Lock();
    if (*androidVsync) {
      (*androidVsync)->NotifyVsync(aFrameTimeNanos);
    }
  }

  // Called by the AndroidVsync destructor
  void Unlink() {
    auto androidVsync = mAndroidVsync.Lock();
    *androidVsync = nullptr;
  }

 protected:
  ~AndroidVsyncSupport() = default;

  DataMutex<AndroidVsync*> mAndroidVsync;
};

AndroidVsync::AndroidVsync() : mImpl("AndroidVsync.mImpl") {
  AndroidVsyncSupport::Init();

  auto impl = mImpl.Lock();
  impl->mSupport = new AndroidVsyncSupport(this);
  impl->mSupportJava = java::AndroidVsync::New();
  AndroidVsyncSupport::AttachNative(impl->mSupportJava, impl->mSupport);
}

AndroidVsync::~AndroidVsync() {
  auto impl = mImpl.Lock();
  impl->mInputObservers.Clear();
  impl->mRenderObservers.Clear();
  impl->UpdateObservingVsync();
  impl->mSupport->Unlink();
}

void AndroidVsync::RegisterObserver(Observer* aObserver, ObserverType aType) {
  auto impl = mImpl.Lock();
  if (aType == AndroidVsync::INPUT) {
    impl->mInputObservers.AppendElement(aObserver);
  } else {
    impl->mRenderObservers.AppendElement(aObserver);
  }
  impl->UpdateObservingVsync();
}

void AndroidVsync::UnregisterObserver(Observer* aObserver, ObserverType aType) {
  auto impl = mImpl.Lock();
  if (aType == AndroidVsync::INPUT) {
    impl->mInputObservers.RemoveElement(aObserver);
  } else {
    impl->mRenderObservers.RemoveElement(aObserver);
  }
  aObserver->Dispose();
  impl->UpdateObservingVsync();
}

void AndroidVsync::Impl::UpdateObservingVsync() {
  bool shouldObserve =
      !mInputObservers.IsEmpty() || !mRenderObservers.IsEmpty();
  if (shouldObserve != mObservingVsync) {
    mObservingVsync = mSupportJava->ObserveVsync(shouldObserve);
  }
}

// Always called on the Java UI thread.
void AndroidVsync::NotifyVsync(int64_t aFrameTimeNanos) {
  MOZ_ASSERT(AndroidBridge::IsJavaUiThread());

  // Convert aFrameTimeNanos to a TimeStamp. The value converts trivially to
  // the internal ticks representation of TimeStamp_posix; both use the
  // monotonic clock and are in nanoseconds.
  TimeStamp timeStamp = TimeStamp::FromSystemTime(aFrameTimeNanos);

  // Do not keep the lock held while calling OnVsync.
  nsTArray<Observer*> observers;
  {
    auto impl = mImpl.Lock();
    observers.AppendElements(impl->mInputObservers);
    observers.AppendElements(impl->mRenderObservers);
  }
  for (Observer* observer : observers) {
    observer->OnVsync(timeStamp);
  }
}

void AndroidVsync::OnMaybeUpdateRefreshRate() {
  MOZ_ASSERT(NS_IsMainThread());

  auto impl = mImpl.Lock();

  nsTArray<Observer*> observers;
  observers.AppendElements(impl->mRenderObservers);

  for (Observer* observer : observers) {
    observer->OnMaybeUpdateRefreshRate();
  }
}

}  // namespace widget
}  // namespace mozilla
