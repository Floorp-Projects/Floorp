/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidVsync_h
#define mozilla_widget_AndroidVsync_h

#include "mozilla/DataMutex.h"
#include "mozilla/java/AndroidVsyncNatives.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace widget {

class AndroidVsyncSupport;

/**
 * A thread-safe way to listen to vsync notifications on Android. All methods
 * can be called on any thread.
 * Observers must keep a strong reference to the AndroidVsync instance until
 * they unregister themselves.
 */
class AndroidVsync final : public SupportsThreadSafeWeakPtr<AndroidVsync> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(AndroidVsync)

  static RefPtr<AndroidVsync> GetInstance();

  ~AndroidVsync();

  class Observer {
   public:
    // Will be called on the Java UI thread.
    virtual void OnVsync(const TimeStamp& aTimeStamp) = 0;
    // Will be called on the Java UI thread.
    virtual void OnMaybeUpdateRefreshRate() {}
    // Called when the observer is unregistered, in case it wants to
    // manage its own lifetime.
    virtual void Dispose() {}
    virtual ~Observer() = default;
  };

  // INPUT observers are called before RENDER observers.
  enum ObserverType { INPUT, RENDER };
  void RegisterObserver(Observer* aObserver, ObserverType aType);
  void UnregisterObserver(Observer* aObserver, ObserverType aType);

  void OnMaybeUpdateRefreshRate();

 private:
  friend class AndroidVsyncSupport;

  AndroidVsync();

  // Called by Java, via AndroidVsyncSupport
  void NotifyVsync(int64_t aFrameTimeNanos);

  struct Impl {
    void UpdateObservingVsync();

    nsTArray<Observer*> mInputObservers;
    nsTArray<Observer*> mRenderObservers;
    RefPtr<AndroidVsyncSupport> mSupport;
    java::AndroidVsync::GlobalRef mSupportJava;
    bool mObservingVsync = false;
  };

  DataMutex<Impl> mImpl;

  static StaticDataMutex<ThreadSafeWeakPtr<AndroidVsync>> sInstance;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_AndroidVsync_h
